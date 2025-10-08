#include "uart_receiver.h"
#include "packet_parser.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <thread>

void printHex(const std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]) << " ";
        if ((i + 1) % 16 == 0) std::cout << std::endl;
    }
    std::cout << std::dec << std::endl;
}

void onRxPacket(const RxPacket& packet) {
    std::cout << "\n=== 受信パケット ===" << std::endl;
    std::cout << "送信元MAC: " << packet.sender_mac << std::endl;
    std::cout << "データ長: " << packet.data_len << " bytes" << std::endl;
    std::cout << "ペイロード(HEX):" << std::endl;
    printHex(packet.payload);

    // PacketParserでパース
    PacketParser parser;
    PacketParser::SensorData sensor_data;

    if (parser.parse(packet.payload, sensor_data)) {
        std::cout << "\n--- パース成功 ---" << std::endl;
        std::cout << "シグナルコード: " << sensor_data.signal_code << std::endl;
        std::cout << "ホップカウント: " << static_cast<int>(sensor_data.hop_count) << std::endl;
        std::cout << "コンテンツ名: " << sensor_data.content_name << std::endl;
        std::cout << "コンテンツ: " << sensor_data.content << std::endl;
    } else {
        std::cout << "\n--- パース失敗 ---" << std::endl;
        std::cout << "期待サイズ: " << sizeof(PacketParser::SensorData) << " bytes" << std::endl;
        std::cout << "実際のサイズ: " << packet.payload.size() << " bytes" << std::endl;
    }
    std::cout << "===================" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "使用方法: " << argv[0] << " <UARTデバイス> [ボーレート]" << std::endl;
        std::cerr << "例: " << argv[0] << " /dev/serial0 115200" << std::endl;
        return 1;
    }

    std::string device = argv[1];
    int baudrate = (argc >= 3) ? std::stoi(argv[2]) : 115200;

    std::cout << "=== UARTReceiver & PacketParser テスト ===" << std::endl;
    std::cout << "デバイス: " << device << std::endl;
    std::cout << "ボーレート: " << baudrate << " bps" << std::endl;
    std::cout << std::endl;

    // UARTReceiver初期化
    UARTReceiver uart(device, baudrate);
    uart.setRxCallback(onRxPacket);
    uart.start();

    if (!uart.sendTxCommand("FF:FF:FF:FF:FF:FF", {})) {
        std::cerr << "UART接続に失敗しました" << std::endl;
        return 1;
    }

    std::cout << "UART受信開始 (Ctrl+Cで終了)" << std::endl;
    std::cout << "ESP32からのデータを待機中..." << std::endl;
    std::cout << std::endl;

    // テスト送信（オプション）
    std::cout << "テスト送信を実行しますか？ (y/n): ";
    std::string answer;
    std::getline(std::cin, answer);

    if (answer == "y" || answer == "Y") {
        std::cout << "\nテストコマンド送信中..." << std::endl;

        // テストデータ（ICSNのCommunicationData構造体形式）
        struct __attribute__((packed)) CommunicationData {
            char signalCode[10];
            uint8_t hopCount;
            char contentName[100];
            char content[20];
        };

        CommunicationData test_data;
        memset(&test_data, 0, sizeof(test_data));
        strncpy(test_data.signalCode, "INTEREST", 9);
        test_data.hopCount = 1;
        strncpy(test_data.contentName, "/sensor/temp", 99);
        strncpy(test_data.content, "REQ", 19);

        std::vector<uint8_t> payload(sizeof(test_data));
        memcpy(payload.data(), &test_data, sizeof(test_data));

        if (uart.sendTxCommand("AA:BB:CC:DD:EE:FF", payload)) {
            std::cout << "テストコマンド送信成功" << std::endl;
        } else {
            std::cout << "テストコマンド送信失敗" << std::endl;
        }
    }

    // 受信待機
    std::cout << "\nパケット受信待機中... (Ctrl+Cで終了)" << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    uart.stop();
    return 0;
}
