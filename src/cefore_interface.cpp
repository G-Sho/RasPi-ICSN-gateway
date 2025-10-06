#include "cefore_interface.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <unistd.h>

CeforeInterface::CeforeInterface() : handle_(-1), running_(false) {}

CeforeInterface::~CeforeInterface() {
    stopReceiving();
    disconnect();
}

bool CeforeInterface::init(int port_num, const std::string& config_path) {
    cef_frame_init();

    const char* config_ptr = config_path.empty() ? nullptr : config_path.c_str();
    int res = cef_client_init(port_num, config_ptr);

    if (res < 0) {
        std::cerr << "cef_client_init failed: " << res << std::endl;
        return false;
    }

    return true;
}

bool CeforeInterface::connect() {
    handle_ = cef_client_connect();

    if (handle_ < 1) {
        std::cerr << "cef_client_connect failed" << std::endl;
        return false;
    }

    std::cout << "Connected to cefnetd (handle=" << handle_ << ")" << std::endl;
    return true;
}

void CeforeInterface::disconnect() {
    if (handle_ >= 1) {
        cef_client_close(handle_);
        handle_ = -1;
    }
}

uint64_t CeforeInterface::getCurrentTimeMs() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return ms.count();
}

bool CeforeInterface::publishData(const std::string& uri,
                                   const std::vector<uint8_t>& payload,
                                   uint32_t chunk_num,
                                   uint32_t cache_time_sec,
                                   uint32_t expiry_sec) {
    if (handle_ < 1) {
        return false;
    }

    CefT_CcnMsg_OptHdr opt;
    CefT_CcnMsg_MsgBdy params;
    unsigned char cob_buff[CefC_Max_Length];

    memset(&opt, 0, sizeof(opt));
    memset(&params, 0, sizeof(params));

    // 名前設定
    params.name_len = cef_frame_conversion_uri_to_name(uri.c_str(), params.name);
    if (params.name_len <= 0) {
        std::cerr << "Invalid URI: " << uri << std::endl;
        return false;
    }

    // チャンク番号設定
    params.chunk_num_f = 1;
    params.chunk_num = chunk_num;

    // ペイロード設定
    if (payload.size() > CefC_Max_Length) {
        std::cerr << "Payload too large: " << payload.size() << std::endl;
        return false;
    }
    params.payload_len = payload.size();
    memcpy(params.payload, payload.data(), payload.size());

    // 有効期限設定
    uint64_t now_ms = getCurrentTimeMs();
    params.expiry = now_ms + expiry_sec * 1000;

    // キャッシュ時間設定
    opt.cachetime_f = 1;
    opt.cachetime = now_ms + cache_time_sec * 1000;

    // Content Object作成
    int cob_len = cef_frame_object_create(cob_buff, &opt, &params);
    if (cob_len < 0) {
        std::cerr << "cef_frame_object_create failed" << std::endl;
        return false;
    }

    // cefnetdへ送信
    int res = cef_client_message_input(handle_, cob_buff, cob_len);
    if (res < 0) {
        std::cerr << "cef_client_message_input failed" << std::endl;
        return false;
    }

    return true;
}

void CeforeInterface::startReceiving() {
    if (running_) {
        return;
    }

    running_ = true;
    recv_thread_ = std::thread(&CeforeInterface::receiveLoop, this);
}

void CeforeInterface::stopReceiving() {
    running_ = false;
    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
}

void CeforeInterface::setInterestCallback(std::function<void(const std::string&, uint32_t)> callback) {
    interest_callback_ = callback;
}

void CeforeInterface::receiveLoop() {
    unsigned char recv_buff[CefC_Max_Length];
    struct cef_app_request app_request;

    while (running_) {
        int len = cef_client_read(handle_, recv_buff, CefC_Max_Length);

        if (len > 0) {
            int res = cef_client_request_get_with_info(recv_buff, len, &app_request);

            if (res > 0 && app_request.version == CefC_App_Version) {
                char uri[1024];
                cef_frame_conversion_name_to_uri(app_request.name, app_request.name_len, uri);
                uint32_t chunk_num = app_request.chunk_num;

                if (interest_callback_) {
                    interest_callback_(std::string(uri), chunk_num);
                }
            }
        }

        usleep(1000); // 1msポーリング
    }
}
