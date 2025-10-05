#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>

struct RxPacket {
    std::string sender_mac;
    uint16_t data_len;
    std::vector<uint8_t> payload;
};

class UARTReceiver {
public:
    UARTReceiver(const std::string& device, int baudrate);
    ~UARTReceiver();

    void start();
    void stop();
    bool sendTxCommand(const std::string& mac, const std::vector<uint8_t>& data);
    void setRxCallback(std::function<void(const RxPacket&)> callback);

private:
    void receiveLoop();
    bool parseLine(const std::string& line, RxPacket& packet);

    int fd_;
    std::string device_;
    int baudrate_;
    std::thread recv_thread_;
    std::atomic<bool> running_;
    std::function<void(const RxPacket&)> rx_callback_;
};
