#include "main_controller.h"
#include "third_party/base64.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <signal.h>
#include <unistd.h>

// ESP-NOWプロトコル用のCommunicationData構造体
struct __attribute__((packed)) CommunicationData {
    char signalCode[10];
    uint8_t hopCount;
    char contentName[100];
    char content[20];
    uint32_t counter;        // リプレイ攻撃対策用カウンタ
    uint8_t hmac[32];        // HMAC-SHA256認証値
};

MainController::MainController() {}

MainController::~MainController() {
    shutdown();
}

bool MainController::initialize(const std::string& uart_device, int baudrate,
                                const std::string& fib_config_path) {
    std::cout << "[INFO] Creating components..." << std::endl;

    // コンポーネント作成
    uart_ = std::make_unique<UARTReceiver>(uart_device, baudrate);
    parser_ = std::make_unique<PacketParser>();
    cefore_ = std::make_unique<CeforeInterface>();
    name_mapper_ = std::make_unique<NameMapper>();
    fib_ = std::make_unique<GatewayFIB>();

    // 設定ファイルから初期 FIB エントリを読み込む（静的ルート）
    if (!fib_config_path.empty()) {
        loadFIBConfig(fib_config_path);
    }

    // CEFORE初期化（cefpyco方式: init()でconnectまで完了）
    // cefpycoのテストと同様に、空文字列を渡す (test_cefpyco.c:38)
    std::cout << "[INFO] Initializing CEFORE..." << std::endl;
    if (!cefore_->init(0, "")) {
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

    // プレフィックス登録（全センサーデータのルート）
    // ICSN側で使う名前空間を登録
    std::cout << "[INFO] Registering CEFORE prefixes..." << std::endl;

    // ICSNセンサーのルートプレフィックスを登録
    // 実際のセンサー名に応じて変更
    const char* prefixes[] = {
        "ccnx:/icsn",
        nullptr
    };

    for (int i = 0; prefixes[i] != nullptr; i++) {
        if (cefore_->registerName(prefixes[i])) {
            std::cout << "[INFO] Registered prefix: " << prefixes[i] << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to register prefix: " << prefixes[i] << std::endl;
        }
    }

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
            // Interest受信 - バッファ内の全メッセージを処理
            // 注: cef_client_request_get_with_info() は処理済みメッセージを削除し、
            //     残りのメッセージをバッファの先頭に詰めるため、常にbufferを渡す
            int remaining = len;

            while (remaining > 0) {
                std::string uri;
                uint32_t chunk_num;

                // 常にbufferの先頭を渡す（関数内でバッファが書き換えられる）
                int next_remaining = cefore_->parseInterest(buffer, remaining, uri, chunk_num);

                if (next_remaining > 0) {
                    // パース成功 - Interestを処理
                    onInterest(uri, chunk_num);

                    // 残りサイズを更新（バッファは既に関数内で詰められている）
                    remaining = next_remaining;
                } else {
                    // パース失敗、またはバッファ終端
                    break;
                }
            }
        } else if (len < 0) {
            std::cerr << "[ERROR] Receive error: " << len << std::endl;
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

void MainController::loadFIBConfig(const std::string& fib_config_path) {
    std::ifstream file(fib_config_path);
    if (!file.is_open()) {
        std::cerr << "[WARN] Failed to open FIB config file: " << fib_config_path << std::endl;
        return;
    }

    std::cout << "[INFO] Loading initial FIB from: " << fib_config_path << std::endl;
    int count = 0;
    std::string line;
    while (std::getline(file, line)) {
        // コメント行・空行をスキップ
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string prefix, mac;
        if (!(iss >> prefix >> mac)) {
            std::cerr << "[WARN] Skipping invalid FIB config line: " << line << std::endl;
            continue;
        }

        fib_->save(prefix, {mac});
        std::cout << "[INFO] Static FIB: " << prefix << " -> " << mac << std::endl;
        ++count;
    }
    std::cout << "[INFO] Loaded " << count << " static FIB entries" << std::endl;
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

        // PITエントリを削除（データが届いたので重複抑制を解除）
        pit_.erase(data.content_name);

        // コンテンツ名にタイムスタンプ付加
        std::string timestamped_uri = name_mapper_->addTimestamp(data.content_name);

        // 初回受信時は名前登録（冪等性あり）
        // 例: "/sensor/temp/12345" → "/sensor/temp" を登録
        cefore_->registerName(data.content_name);

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

    // PIT重複チェック: 同一コンテンツ名のInterestが既に転送済みであれば抑制
    auto now = std::chrono::steady_clock::now();
    auto it = pit_.find(content_name);
    if (it != pit_.end()) {
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
        if (elapsed_ms < PIT_TIMEOUT_MS) {
            std::cout << "[INFO] Suppressing duplicate Interest for: " << content_name
                      << " (pending since " << elapsed_ms << "ms ago)" << std::endl;
            return;
        }
        // タイムアウト済みのエントリを削除
        pit_.erase(it);
    }

    // FIB検索（最長プレフィックス一致）
    std::set<std::string> macs = fib_->lookup(content_name);

    if (macs.empty()) {
        std::cout << "[WARN] No FIB entry found for: " << content_name << std::endl;
        std::cout << "[INFO] Broadcasting Interest to all nodes" << std::endl;
        // FIBエントリがない場合はブロードキャスト
        macs.insert("FF:FF:FF:FF:FF:FF");
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

    // 各MACアドレスにInterest転送（少なくとも1つ成功した場合にPIT登録）
    bool forwarded = false;
    for (const auto& mac : macs) {
        if (uart_->sendTxCommand(mac, binary_data)) {
            std::cout << "[INFO] Forwarded Interest to " << mac << ": " << content_name << std::endl;
            forwarded = true;
        } else {
            std::cerr << "[ERROR] Failed to forward Interest to " << mac << std::endl;
        }
    }

    // 転送成功時のみPITに登録（重複抑制用）
    if (forwarded) {
        pit_[content_name] = now;
    }
}
