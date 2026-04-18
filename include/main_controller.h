#pragma once

#include <memory>
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

    bool initialize(const std::string& uart_device, int baudrate);
    void run();
    void shutdown();

private:
    void onRxPacket(const RxPacket& packet);
    void onInterest(const std::string& uri, uint32_t chunk_num);

    std::unique_ptr<UARTReceiver> uart_;
    std::unique_ptr<PacketParser> parser_;
    std::unique_ptr<CeforeInterface> cefore_;
    std::unique_ptr<NameMapper> name_mapper_;
    std::unique_ptr<GatewayFIB> fib_;

    // PIT: コンテンツ名ごとに最後にInterestを転送した時刻を管理
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> pit_;
    static constexpr int PIT_TIMEOUT_MS = 5000;  // 同一名前の重複Interest抑制期間(ms)
};
