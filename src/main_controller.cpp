#include "main_controller.h"
#include "third_party/base64.h"
#include <iostream>
#include <cstring>
#include <signal.h>
#include <unistd.h>

// ESP-NOWプロトコル用のCommunicationData構造体
struct __attribute__((packed)) CommunicationData {
    char signalCode[10];
    uint8_t hopCount;
    char contentName[100];
    char content[20];
};

MainController::MainController() {}

MainController::~MainController() {
    shutdown();
}

bool MainController::initialize(const std::string& uart_device, int baudrate) {
    // コンポーネント作成
    uart_ = std::make_unique<UARTReceiver>(uart_device, baudrate);
    parser_ = std::make_unique<PacketParser>();
    cefore_ = std::make_unique<CeforeInterface>();
    name_mapper_ = std::make_unique<NameMapper>();
    fib_ = std::make_unique<GatewayFIB>();

    // CEFORE初期化
    if (!cefore_->init()) {
        std::cerr << "CEFORE initialization failed" << std::endl;
        return false;
    }

    if (!cefore_->connect()) {
        std::cerr << "CEFORE connection failed" << std::endl;
        return false;
    }

    // コールバック設定
    uart_->setRxCallback([this](const RxPacket& packet) {
        onRxPacket(packet);
    });

    cefore_->setInterestCallback([this](const std::string& uri, uint32_t chunk_num) {
        onInterest(uri, chunk_num);
    });

    // UART受信開始
    uart_->start();

    // CEFORE Interest受信開始
    cefore_->startReceiving();

    std::cout << "Gateway initialized successfully" << std::endl;
    return true;
}

void MainController::run() {
    std::cout << "Gateway running... Press Ctrl+C to stop" << std::endl;

    // メインループ
    while (true) {
        sleep(1);
    }
}

void MainController::shutdown() {
    std::cout << "Shutting down gateway..." << std::endl;

    if (uart_) {
        uart_->stop();
    }

    if (cefore_) {
        cefore_->stopReceiving();
        cefore_->disconnect();
    }
}

void MainController::onRxPacket(const RxPacket& packet) {
    PacketParser::SensorData data;

    if (!parser_->parse(packet.payload, data)) {
        std::cerr << "Failed to parse packet from " << packet.sender_mac << std::endl;
        return;
    }

    std::cout << "Received " << data.signal_code << " from " << packet.sender_mac
              << ": " << data.content_name << " = " << data.content << std::endl;

    // DATAパケットかチェック
    if (std::string(data.signal_code) == "DATA") {
        // FIBエントリ学習（content_name → MAC）
        fib_->save(data.content_name, {packet.sender_mac});

        // コンテンツ名にタイムスタンプ付加
        std::string timestamped_uri = name_mapper_->addTimestamp(data.content_name);

        // CEFOREに公開
        std::vector<uint8_t> payload(data.content, data.content + strlen(data.content));
        if (cefore_->publishData(timestamped_uri, payload)) {
            std::cout << "Published to CEFORE: " << timestamped_uri << std::endl;
        } else {
            std::cerr << "Failed to publish to CEFORE" << std::endl;
        }
    }
}

void MainController::onInterest(const std::string& uri, uint32_t chunk_num) {
    std::cout << "Received Interest: " << uri << " (chunk=" << chunk_num << ")" << std::endl;

    // タイムスタンプを除去してICSNコンテンツ名取得
    std::string content_name = name_mapper_->removeTimestamp(uri);

    // FIB検索（最長プレフィックス一致）
    std::set<std::string> macs = fib_->lookup(content_name);

    if (macs.empty()) {
        std::cout << "No FIB entry found for: " << content_name << std::endl;
        return;
    }

    // ICSN Interestパケット作成
    CommunicationData interest_packet;
    memset(&interest_packet, 0, sizeof(interest_packet));

    strncpy(interest_packet.signalCode, "INTEREST", 9);
    interest_packet.hopCount = 1;
    strncpy(interest_packet.contentName, content_name.c_str(), 99);
    strncpy(interest_packet.content, "N/A", 19);

    // バイナリにシリアライズ
    std::vector<uint8_t> binary_data(sizeof(CommunicationData));
    memcpy(binary_data.data(), &interest_packet, sizeof(CommunicationData));

    // 各MACアドレスにInterest転送
    for (const auto& mac : macs) {
        if (uart_->sendTxCommand(mac, binary_data)) {
            std::cout << "Forwarded Interest to " << mac << ": " << content_name << std::endl;
        } else {
            std::cerr << "Failed to forward Interest to " << mac << std::endl;
        }
    }
}
