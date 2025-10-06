#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <cefore/cef_client.h>
#include <cefore/cef_frame.h>

class CeforeInterface {
public:
    CeforeInterface();
    ~CeforeInterface();

    bool init(int port_num = CefC_Unset_Port, const std::string& config_path = "");
    bool connect();
    void disconnect();

    // Dataパケット送信（Content Object公開）
    bool publishData(const std::string& uri,
                     const std::vector<uint8_t>& payload,
                     uint32_t chunk_num = 0,
                     uint32_t cache_time_sec = 300,
                     uint32_t expiry_sec = 3600);

    // Interest受信スレッド開始・停止
    void startReceiving();
    void stopReceiving();

    // Interest受信コールバック設定
    void setInterestCallback(std::function<void(const std::string& uri, uint32_t chunk_num)> callback);

private:
    void receiveLoop();
    uint64_t getCurrentTimeMs();

    CefT_Client_Handle handle_;
    std::thread recv_thread_;
    std::atomic<bool> running_;
    std::function<void(const std::string&, uint32_t)> interest_callback_;
};
