#pragma once

#include <string>
#include <vector>
#include <functional>

extern "C" {
#include <cefore/cef_client.h>
#include <cefore/cef_frame.h>
#include <cefore/cef_log.h>
}

// cefpyco方式のCeforeインターフェース
class CeforeInterface {
public:
    CeforeInterface();
    ~CeforeInterface();

    // 初期化・接続（cefpyco: create_cef_handler）
    bool init(int port_num = 0, const char* config_path = nullptr);

    // 終了処理（cefpyco: destroy_cef_handler）
    void close();

    // Data送信（cefpyco: send_data）
    bool sendData(const char* uri,
                  int chunk_num,
                  const uint8_t* payload,
                  int payload_len,
                  uint64_t cache_time_ms = 10000,
                  uint64_t expiry_ms = 3600000);

    // Interest送信（cefpyco: send_interest）
    bool sendInterest(const char* uri,
                      int chunk_num,
                      uint32_t lifetime_ms = 4000);

    // 名前登録（cefpyco: register_name）
    bool registerName(const char* uri);

    // 名前登録解除（cefpyco: deregister_name）
    bool deregisterName(const char* uri);

    // メッセージ受信（cefpyco: receive）
    // timeout_ms: タイムアウト時間（ミリ秒）
    // 戻り値: 受信したバイト数（0=タイムアウト、-1=エラー）
    int receive(uint8_t* buffer, int buffer_size, int timeout_ms = 1000);

    // Interest受信時のコールバック設定
    void setInterestCallback(std::function<void(const std::string& uri, uint32_t chunk_num)> callback);

    // 受信データのパース処理
    // 戻り値: 処理後の残りバッファサイズ（0=パース失敗またはバッファ終端）
    int parseInterest(const uint8_t* buffer, int len, std::string& uri, uint32_t& chunk_num);

private:
    // cefpyco: call_register_api相当
    bool callRegisterAPI(uint16_t func, const char* uri);

    // 現在時刻（ミリ秒）取得
    uint64_t getCurrentTimeMs();

    CefT_Client_Handle handle_;
    std::function<void(const std::string&, uint32_t)> interest_callback_;
};
