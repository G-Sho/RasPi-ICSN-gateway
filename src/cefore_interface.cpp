#include "cefore_interface.h"
#include <iostream>
#include <cstring>
#include <sys/time.h>
#include <unistd.h>

CeforeInterface::CeforeInterface() : handle_(0) {}

CeforeInterface::~CeforeInterface() {
    close();
}

// CEFORE公式ツール方式の初期化 (cefore/tools/cefgetchunk/cefgetchunk.c:224-248)
bool CeforeInterface::init(int port_num, const char* config_path) {
    std::cout << "[INFO] Initializing Cefore..." << std::endl;

    // 1. cef_log_init2 を使用（CEFOREの公式ツールと同じ）
    cef_log_init2((char*)config_path, 1 /* for CEFNETD */);

    // 2. cef_frame_init
    cef_frame_init();

    // 3. cef_client_init
    int res = cef_client_init(port_num, (char*)config_path);
    if (res < 0) {
        std::cerr << "[ERROR] cef_client_init failed: " << res << std::endl;
        std::cerr << "[INFO] Check:" << std::endl;
        std::cerr << "  1. cefnetd is running (check with: cefstatus)" << std::endl;
        std::cerr << "  2. /usr/local/cefore/cefnetd.conf exists and is valid" << std::endl;
        std::cerr << "  3. PORT_NUM is set in cefnetd.conf (default: 9896)" << std::endl;
        return false;
    }

    // 4. cef_client_connect
    handle_ = cef_client_connect();
    if (handle_ < 1) {
        std::cerr << "[ERROR] cef_client_connect failed (handle=" << handle_ << ")" << std::endl;
        std::cerr << "[INFO] Make sure cefnetd is running: sudo cefnetdstart" << std::endl;
        return false;
    }

    std::cout << "[INFO] Connected to cefnetd (handle=" << handle_ << ")" << std::endl;
    return true;
}

// cefpyco: destroy_cef_handler (cefpyco.c:114-120)
void CeforeInterface::close() {
    if (handle_ > 0) {
        // cefpycoと同じく1msスリープしてからクローズ
        struct timespec ts = {0, 1000L * 1000L};
        nanosleep(&ts, nullptr);
        cef_client_close(handle_);
        handle_ = 0;
        std::cout << "[INFO] Closed cefore connection" << std::endl;
    }
}

uint64_t CeforeInterface::getCurrentTimeMs() {
    struct timeval now;
    gettimeofday(&now, nullptr);
    return now.tv_sec * 1000ULL + now.tv_usec / 1000ULL;
}

// cefpyco: send_data (cefpyco.c:84-92) + set_basic_data_params (cefpyco.c:233-263)
bool CeforeInterface::sendData(const char* uri,
                                int chunk_num,
                                const uint8_t* payload,
                                int payload_len,
                                uint64_t cache_time_ms,
                                uint64_t expiry_ms) {
    if (handle_ < 1) {
        std::cerr << "[ERROR] Handle not initialized" << std::endl;
        return false;
    }

    CefT_CcnMsg_OptHdr opt;
    CefT_CcnMsg_MsgBdy params;

    memset(&opt, 0, sizeof(opt));
    memset(&params, 0, sizeof(params));

    // URI → Name変換
    int res = cef_frame_conversion_uri_to_name(uri, params.name);
    if (res < 0) {
        std::cerr << "[ERROR] Invalid URI: " << uri << std::endl;
        return false;
    }
    params.name_len = res;

    // ペイロード設定
    if (payload_len > CefC_Max_Length) {
        std::cerr << "[ERROR] Payload too large: " << payload_len << std::endl;
        return false;
    }
    memcpy(params.payload, payload, payload_len);
    params.payload_len = payload_len;

    // チャンク番号設定
    params.chunk_num_f = 1;
    params.chunk_num = chunk_num;

    // 有効期限設定
    uint64_t now_ms = getCurrentTimeMs();
    params.expiry = now_ms + expiry_ms;

    // キャッシュ時間設定
    opt.cachetime_f = 1;
    opt.cachetime = now_ms + cache_time_ms;

    // Data送信 (cefpyco.c:150)
    res = cef_client_object_input(handle_, &opt, &params);
    if (res < 0) {
        std::cerr << "[ERROR] cef_client_object_input failed" << std::endl;
        return false;
    }

    return true;
}

// cefpyco: send_interest (cefpyco.c:74-82) + set_basic_interest_params (cefpyco.c:213-231)
bool CeforeInterface::sendInterest(const char* uri,
                                    int chunk_num,
                                    uint32_t lifetime_ms) {
    if (handle_ < 1) {
        std::cerr << "[ERROR] Handle not initialized" << std::endl;
        return false;
    }

    CefT_CcnMsg_OptHdr opt;
    CefT_CcnMsg_MsgBdy params;

    memset(&opt, 0, sizeof(opt));
    memset(&params, 0, sizeof(params));

    // URI → Name変換
    int res = cef_frame_conversion_uri_to_name(uri, params.name);
    if (res < 0) {
        std::cerr << "[ERROR] Invalid URI: " << uri << std::endl;
        return false;
    }
    params.name_len = res;

    // Interest パラメータ設定
    params.hoplimit = 32;
    opt.lifetime_f = 1;
    opt.lifetime = lifetime_ms;

    params.chunk_num_f = 1;
    params.chunk_num = chunk_num;

    // Interest送信 (cefpyco.c:139)
    res = cef_client_interest_input(handle_, &opt, &params);
    if (res < 0) {
        std::cerr << "[ERROR] cef_client_interest_input failed" << std::endl;
        return false;
    }

    return true;
}

// cefpyco: call_register_api (cefpyco.c:162-174)
bool CeforeInterface::callRegisterAPI(uint16_t func, const char* uri) {
    if (handle_ < 1) {
        std::cerr << "[ERROR] Handle not initialized" << std::endl;
        return false;
    }

    unsigned char name[CefC_Max_Length];
    int res = cef_frame_conversion_uri_to_name(uri, name);
    if (res < 0) {
        std::cerr << "[ERROR] Invalid URI: " << uri << std::endl;
        return false;
    }

    // cefpyco.c:172 - cef_client_prefix_regを使用
    cef_client_prefix_reg(handle_, func, name, (uint16_t)res);
    return true;
}

// cefpyco: register_name (cefpyco.c:94)
bool CeforeInterface::registerName(const char* uri) {
    std::cout << "[INFO] Registering name: " << uri << std::endl;
    return callRegisterAPI(CefC_App_Reg, uri);
}

// cefpyco: deregister_name (cefpyco.c:95)
bool CeforeInterface::deregisterName(const char* uri) {
    std::cout << "[INFO] Deregistering name: " << uri << std::endl;
    return callRegisterAPI(CefC_App_DeReg, uri);
}

// cefpyco: wait_receive相当 (cefpyco.c:319-346)
int CeforeInterface::receive(uint8_t* buffer, int buffer_size, int timeout_ms) {
    if (handle_ < 1) {
        std::cerr << "[ERROR] Handle not initialized" << std::endl;
        return -1;
    }

    int elapsed_ms = 0;
    int res = 0;

    // タイムアウトまでポーリング（cef_client_readは最大1秒ブロック）
    while (elapsed_ms < timeout_ms) {
        res = cef_client_read(handle_, buffer, buffer_size);
        if (res > 0) {
            return res;  // データ受信成功
        }

        // cef_client_readは約1秒ブロックする (cefpyco.c:329コメント参照)
        elapsed_ms += 1000;
    }

    // タイムアウト
    return 0;
}

void CeforeInterface::setInterestCallback(std::function<void(const std::string&, uint32_t)> callback) {
    interest_callback_ = callback;
}

// Interest受信データのパース
bool CeforeInterface::parseInterest(const uint8_t* buffer, int len, std::string& uri, uint32_t& chunk_num) {
    struct cef_app_request app_request;
    memset(&app_request, 0, sizeof(app_request));

    int res = cef_client_request_get_with_info((unsigned char*)buffer, len, &app_request);
    if (res <= 0 || app_request.version != CefC_App_Version) {
        return false;
    }

    // Name → URI変換
    char uri_buf[CefC_Max_Length];
    cef_frame_conversion_name_to_uri(app_request.name, app_request.name_len, uri_buf);
    uri = std::string(uri_buf);
    chunk_num = app_request.chunk_num;

    return true;
}
