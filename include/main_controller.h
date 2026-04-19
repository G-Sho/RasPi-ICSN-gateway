#pragma once

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <chrono>
#include "uart_receiver.h"
#include "packet_parser.h"
#include "cefore_interface.h"
#include "name_mapper.h"
#include "gateway_fib.h"

class MainController {
public:
    MainController();
    ~MainController();

    bool initialize(const std::string& uart_device, int baudrate,
                    const std::string& fib_config_path = "");
    void run();
    void shutdown();

private:
    void onRxPacket(const RxPacket& packet);
    void onInterest(const std::string& uri, uint32_t chunk_num);
    void loadFIBConfig(const std::string& fib_config_path);

    // ICSNセンサーへInterestを転送するヘルパー
    // FIB検索してUART経由で送信する。転送成功時にtrueを返す
    bool forwardToICSN(const std::string& content_name);

    std::unique_ptr<UARTReceiver> uart_;
    std::unique_ptr<PacketParser> parser_;
    std::unique_ptr<CeforeInterface> cefore_;
    std::unique_ptr<GatewayFIB> fib_;

    // PIT: コンテンツ名ごとに最後にInterestを転送した時刻と
    //      待機中のチャンク番号キューを管理（FIFO順でICSN応答に対応させる）
    struct PitEntry {
        std::chrono::steady_clock::time_point time;
        std::queue<uint32_t> pending_chunks;
    };
    std::unordered_map<std::string, PitEntry> pit_;
    static constexpr int PIT_TIMEOUT_MS = 5000;  // 同一名前の重複Interest抑制期間(ms)

    // CS (Content Store) キャッシュ: 直近のセンサー値を短時間保持する
    // chunk=3などが遅れて届いた際に、ESP32への再リクエストなしで即答するために使用
    struct CsEntry {
        std::chrono::steady_clock::time_point time;
        std::string content;      // センサー値（生文字列）
        std::string cefore_uri;   // ccnx:/ スキーム付きURI
    };
    std::unordered_map<std::string, CsEntry> cs_;
    static constexpr int CS_TTL_MS = 2000;  // CSキャッシュ有効期間(ms)
};
