#include "main_controller.h"
#include "third_party/base64.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

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
    // std::cout << "[INFO] Creating components..." << std::endl;

    // コンポーネント作成
    uart_ = std::make_unique<UARTReceiver>(uart_device, baudrate);
    parser_ = std::make_unique<PacketParser>();
    cefore_ = std::make_unique<CeforeInterface>();
    fib_ = std::make_unique<GatewayFIB>();

    // 設定ファイルから初期 FIB エントリを読み込む（静的ルート）
    if (!fib_config_path.empty()) {
        loadFIBConfig(fib_config_path);
    }

    // CEFORE初期化（cefpyco方式: init()でconnectまで完了）
    // cefpycoのテストと同様に、空文字列を渡す (test_cefpyco.c:38)
    // std::cout << "[INFO] Initializing CEFORE..." << std::endl;
    if (!cefore_->init(0, "")) {
        // std::cerr << "[ERROR] CEFORE initialization failed" << std::endl;
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
    // std::cout << "[INFO] Registering CEFORE prefixes..." << std::endl;

    // ICSNセンサーのルートプレフィックスを登録
    // 実際のセンサー名に応じて変更
    const char* prefixes[] = {
        "ccnx:/iot/buildingA/room101",
        nullptr
    };

    for (int i = 0; prefixes[i] != nullptr; i++) {
        if (cefore_->registerName(prefixes[i])) {
            // std::cout << "[INFO] Registered prefix: " << prefixes[i] << std::endl;
        } else {
            // std::cerr << "[ERROR] Failed to register prefix: " << prefixes[i] << std::endl;
        }
    }

    // UART受信開始
    uart_->start();

    // 計測CSV初期化
    timing_csv_.open("latency_log.csv", std::ios::out | std::ios::app);
    if (timing_csv_.is_open()) {
        // ファイルが空の場合のみヘッダを書く
        timing_csv_.seekp(0, std::ios::end);
        if (timing_csv_.tellp() == 0) {
            timing_csv_ << "unix_time_us,content_name,chunk_num,latency_us\n";
        }
    }

    return true;
}

void MainController::run() {
    // std::cout << "[INFO] Gateway running... Press Ctrl+C to stop" << std::endl;

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
            // std::cerr << "[ERROR] Receive error: " << len << std::endl;
            break;
        }
        // len == 0 はタイムアウト、ループ継続
    }
}

void MainController::shutdown() {
    if (uart_) {
        uart_->stop();
    }

    if (cefore_) {
        cefore_->close();
    }

    if (timing_csv_.is_open()) {
        timing_csv_.close();
    }
}

void MainController::writeLatencyRecord(const std::string& content_name,
                                        uint32_t chunk_num,
                                        int64_t latency_us) {
    if (!timing_csv_.is_open()) return;

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    int64_t unix_us = static_cast<int64_t>(tv.tv_sec) * 1000000LL + tv.tv_usec;

    timing_csv_ << unix_us << ","
                << content_name << ","
                << chunk_num << ","
                << latency_us << "\n";
    timing_csv_.flush();
}

bool MainController::forwardToICSN(const std::string& content_name) {
    // FIB検索（最長プレフィックス一致）
    std::set<std::string> macs = fib_->lookup(content_name);

    if (macs.empty()) {
        // std::cout << "[WARN] No FIB entry found for: " << content_name << std::endl;
        // std::cout << "[INFO] Broadcasting Interest to all nodes" << std::endl;
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

    // 各MACアドレスにInterest転送
    bool forwarded = false;
    for (const auto& mac : macs) {
        if (uart_->sendTxCommand(mac, binary_data)) {
            // std::cout << "[INFO] Forwarded Interest to " << mac << ": " << content_name << std::endl;
            forwarded = true;
        } else {
            // std::cerr << "[ERROR] Failed to forward Interest to " << mac << std::endl;
        }
    }

    return forwarded;
}

void MainController::loadFIBConfig(const std::string& fib_config_path) {
    std::ifstream file(fib_config_path);
    if (!file.is_open()) {
        // std::cerr << "[WARN] Failed to open FIB config file: " << fib_config_path << std::endl;
        return;
    }

    // std::cout << "[INFO] Loading initial FIB from: " << fib_config_path << std::endl;
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
            // std::cerr << "[WARN] Skipping invalid FIB config line: " << line << std::endl;
            continue;
        }

        fib_->save(prefix, {mac});
        // std::cout << "[INFO] Static FIB: " << prefix << " -> " << mac << std::endl;
        ++count;
    }
    // std::cout << "[INFO] Loaded " << count << " static FIB entries" << std::endl;
}

void MainController::onRxPacket(const RxPacket& packet) {
    PacketParser::SensorData data;

    if (!parser_->parse(packet.payload, data)) {
        // std::cerr << "[ERROR] Failed to parse packet from " << packet.sender_mac << std::endl;
        return;
    }

    // std::cout << "[INFO] Received " << data.signal_code << " from " << packet.sender_mac
    //           << ": " << data.content_name << " = " << data.content << std::endl;

    // DATAパケットかチェック
    if (std::string(data.signal_code) == "DATA") {
        // FIBエントリ学習（content_name → MAC）
        fib_->save(data.content_name, {packet.sender_mac});

        // コンテンツ名にccnx:/スキームを付加
        std::string cefore_uri = NameMapper::addScheme(data.content_name);

        // CSキャッシュに保存（遅れたInterestに即答するため）
        cs_.insert_or_assign(data.content_name,
                             CsEntry{std::chrono::steady_clock::now(),
                                     std::string(data.content),
                                     cefore_uri});

        // 初回受信時は名前登録（冪等性あり）
        cefore_->registerName(cefore_uri.c_str());

        // PITキューの先頭チャンク番号を取得して1つのみ公開する
        // ICSNは1回のInterestに1つの値を返すため、ICSN応答1つ = CEFOREチャンク1つ に対応させる
        auto pit_it = pit_.find(data.content_name);
        uint32_t chunk_to_serve = 0;
        bool has_pit_entry = (pit_it != pit_.end());

        if (has_pit_entry && !pit_it->second.pending_chunks.empty()) {
            chunk_to_serve = pit_it->second.pending_chunks.front();
            pit_it->second.pending_chunks.pop();
        } else if (!has_pit_entry) {
            // PITエントリが存在しない場合はチャンク番号0でフォールバック
            // std::cerr << "[WARN] DATA received without matching PIT entry for: "
            //           << data.content_name << " — falling back to chunk 0" << std::endl;
        } else {
            // PITエントリは存在するがキューが空の場合（予期しない状態）はチャンク番号0でフォールバック
            // std::cerr << "[WARN] PIT entry exists but chunk queue is empty for: "
            //           << data.content_name << " — falling back to chunk 0" << std::endl;
        }

        // 対応するチャンク1つ分だけCEFOREへ公開
        // end_chunk_num = chunk_to_serve とすることで「このチャンクが唯一のチャンク」と宣言済み

        // 計測: PITエントリが存在する場合のみ開始時刻を取得（CS経由の応答は除外済み）
        auto timing_key = std::make_pair(std::string(data.content_name), chunk_to_serve);
        auto t_it = interest_times_.find(timing_key);
        bool has_timing = (t_it != interest_times_.end());
        auto t_start = has_timing ? t_it->second : std::chrono::steady_clock::time_point{};
        if (has_timing) interest_times_.erase(t_it);

        int payload_len = strlen(data.content);
        if (cefore_->sendData(cefore_uri.c_str(),
                              chunk_to_serve,
                              (const uint8_t*)data.content,
                              payload_len)) {
            if (has_timing) {
                auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - t_start).count();
                writeLatencyRecord(data.content_name, chunk_to_serve, latency_us);
            }
        }

        // PITキューに残りチャンクがある場合は次のICSN Interestを転送する
        // （キューが空になるまで1応答ごとに1クエリを順次送信）
        if (has_pit_entry && !pit_it->second.pending_chunks.empty()) {
            uint32_t next_chunk = pit_it->second.pending_chunks.front();
            auto now_fwd = std::chrono::steady_clock::now();
            if (forwardToICSN(data.content_name)) {
                pit_it->second.time = now_fwd;
                // 計測開始: 次チャンクのICSN転送時刻を記録
                interest_times_[{std::string(data.content_name), next_chunk}] = now_fwd;
            }
        } else if (has_pit_entry) {
            pit_.erase(pit_it);
        }
    }
}

void MainController::onInterest(const std::string& uri, uint32_t chunk_num) {
    // std::cout << "[INFO] Received Interest: " << uri << " (chunk=" << chunk_num << ")" << std::endl;

    // スキームを除去し、CEFOREが付加したTLVコンポーネントを取り除いてICSNコンテンツ名取得
    std::string content_name = NameMapper::stripCeforeComponents(NameMapper::removeScheme(uri));

    // PIT重複チェック: 同一コンテンツ名のInterestが既に転送済みであれば
    // チャンク番号をキューに追加して抑制（ICSNへの追加転送は行わない）
    // ICSN応答が返ってきた際にキューから順番に取り出してICSNへ再転送する
    auto now = std::chrono::steady_clock::now();
    auto it = pit_.find(content_name);
    if (it != pit_.end()) {
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.time).count();
        if (elapsed_ms < PIT_TIMEOUT_MS) {
            // 同一コンテンツ名への待機中Interestにチャンク番号を追加
            // チャンク数が多すぎる場合はログ警告（チャンク番号は追加しない）
            static constexpr size_t MAX_PENDING_CHUNKS = 256;
            if (it->second.pending_chunks.size() >= MAX_PENDING_CHUNKS) {
                // std::cerr << "[WARN] Too many pending chunks for: " << content_name
                //           << " — discarding chunk " << chunk_num << std::endl;
                return;
            }
            it->second.pending_chunks.push(chunk_num);
            // std::cout << "[INFO] Queuing chunk " << chunk_num << " for pending Interest: "
            //           << content_name << " (" << it->second.pending_chunks.size()
            //           << " chunks pending)" << std::endl;
            return;
        }
        // タイムアウト済みのエントリを削除
        pit_.erase(it);
    }

    // ICSNへInterest転送（転送成功時のみPITに登録）
    // まずCSキャッシュを確認する（遅れたchunkへの即答）
    auto cs_it = cs_.find(content_name);
    if (cs_it != cs_.end()) {
        auto cs_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - cs_it->second.time).count();
        if (cs_elapsed_ms < CS_TTL_MS) {
            // CSキャッシュヒット: ESP32への新規リクエストなしにキャッシュから即答
            int payload_len = static_cast<int>(cs_it->second.content.size());
            cefore_->sendData(cs_it->second.cefore_uri.c_str(),
                              chunk_num,
                              (const uint8_t*)cs_it->second.content.c_str(),
                              payload_len);
            return;
        }
        // キャッシュ期限切れ、削除して通常フローへ
        cs_.erase(cs_it);
    }

    if (forwardToICSN(content_name)) {
        auto& entry = pit_[content_name];
        entry.time = now;
        entry.pending_chunks.push(chunk_num);
        // 計測開始: CSヒット以外でICSNに転送した時刻を記録
        interest_times_[{content_name, chunk_num}] = now;
    }
}
