#include "uart_receiver.h"
#include "third_party/base64.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sstream>

UARTReceiver::UARTReceiver(const std::string& device, int baudrate)
    : device_(device), baudrate_(baudrate), fd_(-1), running_(false) {}

UARTReceiver::~UARTReceiver() {
    stop();
}

void UARTReceiver::start() {
    // Open UART device
    fd_ = open(device_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        std::cerr << "Error opening " << device_ << ": " << strerror(errno) << std::endl;
        return;
    }

    // Configure UART
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "Error from tcgetattr: " << strerror(errno) << std::endl;
        close(fd_);
        fd_ = -1;
        return;
    }

    // Set baud rate
    speed_t baud = B115200;
    if (baudrate_ == 9600) baud = B9600;
    else if (baudrate_ == 19200) baud = B19200;
    else if (baudrate_ == 38400) baud = B38400;
    else if (baudrate_ == 57600) baud = B57600;
    else if (baudrate_ == 115200) baud = B115200;

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    // 8N1 mode
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= (CLOCAL | CREAD);

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    tty.c_lflag = 0;
    tty.c_oflag = 0;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "Error from tcsetattr: " << strerror(errno) << std::endl;
        close(fd_);
        fd_ = -1;
        return;
    }

    // Start receive thread
    running_ = true;
    recv_thread_ = std::thread(&UARTReceiver::receiveLoop, this);
}

void UARTReceiver::stop() {
    running_ = false;
    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

bool UARTReceiver::sendTxCommand(const std::string& mac, const std::vector<uint8_t>& data) {
    if (fd_ < 0) {
        return false;
    }

    // Base64 encode
    std::string encoded = base64_encode(data.data(), data.size());

    // Format: TX:<MAC>|<Base64>\n
    std::string command = "TX:" + mac + "|" + encoded + "\n";

    ssize_t written = write(fd_, command.c_str(), command.size());
    if (written < 0) {
        std::cerr << "UART write error: " << strerror(errno) << std::endl;
        return false;
    }

    return true;
}

void UARTReceiver::setRxCallback(std::function<void(const RxPacket&)> callback) {
    rx_callback_ = callback;
}

void UARTReceiver::receiveLoop() {
    std::string buffer;
    char read_buf[256];

    while (running_) {
        ssize_t n = read(fd_, read_buf, sizeof(read_buf) - 1);
        if (n > 0) {
            read_buf[n] = '\0';
            buffer += read_buf;

            // Process complete lines
            size_t pos;
            while ((pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);

                RxPacket packet;
                if (parseLine(line, packet) && rx_callback_) {
                    rx_callback_(packet);
                }
            }
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "UART read error: " << strerror(errno) << std::endl;
            break;
        }
        usleep(1000); // 1ms sleep
    }
}

bool UARTReceiver::parseLine(const std::string& line, RxPacket& packet) {
    // Format: RX:<MAC>|<len>|<Base64>
    if (line.substr(0, 3) != "RX:") {
        return false;
    }

    size_t first_pipe = line.find('|', 3);
    if (first_pipe == std::string::npos) {
        return false;
    }

    size_t second_pipe = line.find('|', first_pipe + 1);
    if (second_pipe == std::string::npos) {
        return false;
    }

    // Extract MAC
    packet.sender_mac = line.substr(3, first_pipe - 3);

    // Extract length
    std::string len_str = line.substr(first_pipe + 1, second_pipe - first_pipe - 1);
    packet.data_len = std::stoi(len_str);

    // Extract and decode Base64
    std::string encoded = line.substr(second_pipe + 1);
    std::string decoded = base64_decode(encoded);

    packet.payload.assign(decoded.begin(), decoded.end());

    return true;
}
