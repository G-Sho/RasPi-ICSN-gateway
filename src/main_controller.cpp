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
    std::cout << "[INFO] Creating components..." << std::endl;

    // コンポーネント作成
    uart_ = std::make_unique<UARTReceiver>(uart_device, baudrate);
    parser_ = std::make_unique<PacketParser>();
    cefore_ = std::make_unique<CeforeInterface>();
    name_mapper_ = std::make_unique<NameMapper>();
    fib_ = std::make_unique<GatewayFIB>();

    // CEFORE初期化（cefpyco方式: init()でconnectまで完了）
    std::cout << "[INFO] Initializing CEFORE..." << std::endl;
    if (!cefore_->init()) {
        std::cerr << "[ERROR] CEFORE initialization failed" << std::endl;
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

    std::cout << "[INFO] Gateway initialized successfully" << std::endl;
    return true;
}

void MainController::run() {
    std::cout << "[INFO] Gateway running... Press Ctrl+C to stop" << std::endl;

    // cefpyco方式: ポーリングで受信
    const int BUFFER_SIZE = 65536;
    uint8_t buffer[BUFFER_SIZE];

    while (true) {
        // 1秒タイムアウトで受信
        int len = cefore_->receive(buffer, BUFFER_SIZE, 1000);

        if (len > 0) {
            // Interest受信
            std::string uri;
            uint32_t chunk_num;

            if (cefore_->parseInterest(buffer, len, uri, chunk_num)) {
                // コールバック呼び出し
                onInterest(uri, chunk_num);
            }
        } else if (len < 0) {
            std::cerr << "[ERROR] Receive error" << std::endl;
            break;
        }
        // len == 0 はタイムアウト、ループ継続
    }
}

void MainController::shutdown() {
    std::cout << "[INFO] Shutting down gateway..." << std::endl;

    if (uart_) {
        uart_->stop();
    }

    if (cefore_) {
        cefore_->close();
    }
}

void MainController::onRxPacket(const RxPacket& packet) {
    PacketParser::SensorData data;

    if (!parser_->parse(packet.payload, data)) {
        std::cerr << "[ERROR] Failed to parse packet from " << packet.sender_mac << std::endl;
        return;
    }

    std::cout << "[INFO] Received " << data.signal_code << " from " << packet.sender_mac
              << ": " << data.content_name << " = " << data.content << std::endl;

    // DATAパケットかチェック
    if (std::string(data.signal_code) == "DATA") {
        // FIBエントリ学習（content_name → MAC）
        fib_->save(data.content_name, {packet.sender_mac});

        // コンテンツ名にタイムスタンプ付加
        std::string timestamped_uri = name_mapper_->addTimestamp(data.content_name);

        // CEFOREに公開（cefpyco方式: sendData）
        int payload_len = strlen(data.content);
        if (cefore_->sendData(timestamped_uri.c_str(),
                             0,  // chunk_num
                             (const uint8_t*)data.content,
                             payload_len)) {
            std::cout << "[INFO] Published to CEFORE: " << timestamped_uri << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to publish to CEFORE" << std::endl;
        }
    }
}

void MainController::onInterest(const std::string& uri, uint32_t chunk_num) {
    std::cout << "[INFO] Received Interest: " << uri << " (chunk=" << chunk_num << ")" << std::endl;

    // タイムスタンプを除去してICSNコンテンツ名取得
    std::string content_name = name_mapper_->removeTimestamp(uri);

    // FIB検索（最長プレフィックス一致）
    std::set<std::string> macs = fib_->lookup(content_name);

    if (macs.empty()) {
        std::cout << "[WARN] No FIB entry found for: " << content_name << std::endl;
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
            std::cout << "[INFO] Forwarded Interest to " << mac << ": " << content_name << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to forward Interest to " << mac << std::endl;
        }
    }
}
