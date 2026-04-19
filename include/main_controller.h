#pragma once

#include <memory>
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

    std::unique_ptr<UARTReceiver> uart_;
    std::unique_ptr<PacketParser> parser_;
    std::unique_ptr<CeforeInterface> cefore_;
    std::unique_ptr<NameMapper> name_mapper_;
    std::unique_ptr<GatewayFIB> fib_;

    // PIT: コンテンツ名ごとに最後にInterestを転送した時刻と
    //      待機中のチャンク番号セットを管理
    struct PitEntry {
        std::chrono::steady_clock::time_point time;
        std::set<uint32_t> pending_chunks;
    };
    std::unordered_map<std::string, PitEntry> pit_;
    static constexpr int PIT_TIMEOUT_MS = 5000;  // 同一名前の重複Interest抑制期間(ms)
};
