# Raspberry Pi - CEFORE Gateway 設計書

## 1. システム概要

### 1.1 目的

- ESP32ブリッジから受信したESP-NOWデータをCEFOREネットワークに転送
- CEFOREのPIT/CS/FIB機能を活用したNDN通信の実現
- センサーデータの効率的な収集と配信

### 1.2 基本方針

- C++による実装
- CEFOREのcefnetd API (`cef_client`, `cef_frame`) を使用
- **双方向通信**: Data送信（Producer）とInterest受信（Consumer的動作）の両対応
- シンプルな実装（まず動作させることを優先）

## 2. システムアーキテクチャ

### 2.1 全体構成

```
ESP32ブリッジ ←UART→ Raspberry Piゲートウェイ ←cef API→ cefnetd ←→ NDNネットワーク
  (ESP-NOW)              (C++実装)                 (CEFORE)
```

### 2.2 コンポーネント構成

| コンポーネント | 役割 | 主要機能 |
|---|---|---|
| UARTReceiver | ESP32通信 | UART受信、パケット解析、送信コマンド発行 |
| PacketParser | データ解析 | Base64デコード、構造体変換 |
| CeforeInterface | CEFORE連携 | cefnetd接続、Interest/Data送受信 |
| NameMapper | 名前変換 | タイムスタンプ付加・除去 |
| GatewayFIB | ルーティング | コンテンツ名→MAC変換、FIB管理 |
| MainController | 全体制御 | スレッド管理、イベント処理 |

## 3. クラス設計

### 3.1 クラス図

```
MainController
    ├── UARTReceiver
    ├── PacketParser
    ├── CeforeInterface
    ├── NameMapper
    └── GatewayFIB
```

### 3.2 各クラス詳細

#### 3.2.1 UARTReceiver

**責務：** ESP32との通信

```cpp
class UARTReceiver {
public:
    UARTReceiver(const std::string& device, int baudrate);
    ~UARTReceiver();

    void start();
    void stop();
    bool sendTxCommand(const std::string& mac, const std::vector<uint8_t>& data);
    void setRxCallback(std::function<void(const RxPacket&)> callback);

private:
    void receiveLoop();
    bool parseLine(const std::string& line, RxPacket& packet);

    int fd_;
    std::thread recv_thread_;
    std::atomic<bool> running_;
    std::function<void(const RxPacket&)> rx_callback_;
};

struct RxPacket {
    std::string sender_mac;
    uint16_t data_len;
    std::vector<uint8_t> payload;
};
```

**処理内容：**
1. UART受信ループ（別スレッド）
2. `RX:<MAC>|<len>|<Base64>\n` 形式のパース
3. Base64デコード
4. コールバック呼び出し

#### 3.2.2 PacketParser

**責務：** ESP-NOWペイロードの解析

```cpp
class PacketParser {
public:
    struct SensorData {
        char signal_code[10];      // シグナルコード
        uint8_t hop_count;         // ホップカウント
        char content_name[100];    // コンテンツ名
        char content[20];          // コンテンツ本体
    };

    bool parse(const std::vector<uint8_t>& raw_data, SensorData& output);
};
```

**処理内容：**
1. ICSNの`CommunicationData`構造体をそのままパース
2. Base64デコード済みのバイナリをメモリコピー

#### 3.2.3 CeforeInterface

**責務：** cefnetdとの通信

```cpp
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

    CefT_Client_Handle handle_;
    std::thread recv_thread_;
    std::atomic<bool> running_;
    std::function<void(const std::string&, uint32_t)> interest_callback_;
};
```

**処理内容：**
1. `cef_frame_init()` と `cef_client_init()` で初期化
2. `cef_client_connect()` でcefnetdへ接続
3. **Data送信**: `cef_frame_object_create()` でContent Object作成 → `cef_client_message_input()` で送信
4. **Interest受信**: `cef_client_read()` でポーリング → `cef_client_request_get_with_info()` で解析

#### 3.2.4 NameMapper

**責務：** ICSNのコンテンツ名にタイムスタンプを付加

```cpp
class NameMapper {
public:
    // ICSNコンテンツ名にタイムスタンプを付加
    std::string addTimestamp(const std::string& icsn_content_name);

    // タイムスタンプ付きの名前からICSNコンテンツ名を抽出
    std::string removeTimestamp(const std::string& timestamped_name);
};
```

**変換ルール：**
```
入力（ICSN）: /sensor/temp
タイムスタンプ: 1234567890

出力（CEFORE）: /sensor/temp/1234567890
```

#### 3.2.5 GatewayFIB

**責務：** コンテンツ名からMACアドレスへのルーティング

```cpp
class GatewayFIB {
public:
    // FIBエントリ登録
    void save(const std::string& content_name, const std::set<std::string>& mac_addresses);

    // 最長一致検索（LPM with TwoStage algorithm）
    std::set<std::string> lookup(const std::string& content_name);

    // エントリ削除
    void remove(const std::string& content_name);

    // 存在確認
    bool find(const std::string& content_name);

private:
    struct FIBEntry {
        bool isVirtual;
        int maximumDepth;
        std::set<std::string> macAddresses;
    };

    FixedSizeLRUCache<FIBEntry, 100> cache_;
    int maxVirtualDepth_ = 3;

    std::string extractPrefix(const std::string& name, int prefixDepth) const;
    bool lookupEntry(const std::string& name, int prefixDepth, FIBEntry& outEntry);
    bool fibLpmLookup(const std::string& name, int nameDepth, int maxVirtualDepth, FIBEntry& outEntry);
};
```

**処理内容：**
1. **TwoStage アルゴリズム**: ICSNと同じ高速検索
2. **Virtual Entry**: メモリ効率の良いプレフィックス管理
3. **LRUキャッシュ**: 最大100エントリ、アクセス頻度でエビクション
4. **マルチキャスト対応**: 1つのコンテンツ名に複数のMACアドレス

**FIBエントリ登録タイミング：**
- **起動時**: 設定ファイルから静的ルートを読み込み
- **実行時**: センサーからのData受信時に動的学習・更新

**使用例：**
```cpp
// 起動時の静的登録
fib_->save("/sensor/temp", {"AA:BB:CC:DD:EE:01"});
fib_->save("/sensor", {"FF:FF:FF:FF:FF:FF"});  // ブロードキャスト

// Data受信時の動的学習
void onRxPacket(const RxPacket& packet) {
    PacketParser::SensorData data;
    parser_->parse(packet.payload, data);

    // FIBに登録（学習）
    fib_->save(data.content_name, {packet.sender_mac});
}

// Interest受信時の検索
void onInterest(const std::string& uri, uint32_t chunk_num) {
    std::string content_name = name_mapper_->removeTimestamp(uri);

    // FIB検索（最長一致）
    std::set<std::string> macs = fib_->lookup(content_name);

    // 各MACアドレスにInterest転送
    for (const auto& mac : macs) {
        uart_->sendTxCommand(mac, content_name);
    }
}
```

#### 3.2.6 MainController

**責務：** 全体の統括管理

```cpp
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
};
```

**処理フロー：**
1. 各コンポーネント初期化
2. FIBに静的ルート登録（設定ファイルから）
3. UART受信コールバック登録
4. Interest受信コールバック登録
5. イベント駆動で処理

## 4. データフロー

### 4.1 センサーデータ収集フロー（ICSN → CEFORE）

```
1. ESP32 → UART → UARTReceiver::receiveLoop()
   ↓
2. UARTReceiver::parseLine() → Base64デコード
   ↓
3. rx_callback_ 呼び出し → MainController::onRxPacket()
   ↓
4. PacketParser::parse() → SensorData構造体（コンテンツ名 + ペイロード）
   ↓
5. GatewayFIB::save() → FIBに学習（コンテンツ名 → MAC）
   ↓
6. NameMapper::addTimestamp() → コンテンツ名にタイムスタンプ付加
   ↓
7. CeforeInterface::publishData() → cefnetd送信
   ↓
8. cefnetd → PIT/CS登録 → NDNネットワーク配信
```

### 4.2 Interest処理フロー（CEFORE → ICSN）

```
1. NDNアプリ → cefnetd → CeforeInterface::receiveLoop()
   ↓
2. cef_client_read() でInterest受信
   ↓
3. cef_client_request_get_with_info() で解析
   ↓
4. interest_callback_ 呼び出し → MainController::onInterest()
   ↓
5. NameMapper::removeTimestamp() → タイムスタンプ除去、ICSNコンテンツ名抽出
   ↓
6. GatewayFIB::lookup() → FIB検索（最長一致）、MACアドレス集合取得
   ↓
7. 各MACアドレスに対してUARTReceiver::sendTxCommand() → ESP32にInterest転送
   ↓
8. ESP32 → ESP-NOW → センサーノード（複数の場合はマルチキャスト）
   ↓
9. センサーノード応答 → ESP-NOW → ESP32 → UART
   ↓
10. (センサーデータ収集フローに戻る)
```

## 5. 通信プロトコル

### 5.1 UART通信仕様

**物理層：**
- デバイス: `/dev/serial0` (Raspberry Pi GPIO UART)
- ボーレート: 115200 bps
- データビット: 8、パリティ: None、ストップビット: 1

**受信フォーマット (ESP32 → RasPi):**
```
RX:<送信者MAC>|<データ長>|<Base64エンコードデータ>\n

例: RX:AA:BB:CC:DD:EE:FF|42|SGVsbG8gV29ybGQhCg==\n
```

**送信フォーマット (RasPi → ESP32):**
```
TX:<宛先MAC>|<Base64エンコードデータ>\n
```

**応答フォーマット (ESP32 → RasPi):**
```
OK\n
ERR:INVALID_MAC\n
ERR:DECODE_FAIL\n
ERR:SEND_FAIL\n
```

### 5.2 CEFORE API使用方法

CEFORE APIは `cef_client.h` と `cef_frame.h` の2つのヘッダーで提供されます。

#### 初期化

```cpp
#include <cefore/cef_client.h>
#include <cefore/cef_frame.h>

cef_frame_init();

int res = cef_client_init(port_num, config_path);
if (res < 0) {
    return false;
}

CefT_Client_Handle handle = cef_client_connect();
if (handle < 1) {
    return false;
}
```

#### Dataパケット送信（Content Object作成）

```cpp
CefT_CcnMsg_OptHdr opt;
CefT_CcnMsg_MsgBdy params;
unsigned char cob_buff[CefC_Max_Length];

// 名前の設定
const char* uri = "/iot/node01/sensor/temp";
params.name_len = cef_frame_conversion_uri_to_name(uri, params.name);

// チャンクナンバー設定
params.chunk_num_f = 1;
params.chunk_num = 0;

// ペイロード設定
params.payload_len = data_size;
memcpy(params.payload, data_buffer, data_size);

// Expiry Time設定
uint64_t now_ms = get_current_time_ms();
params.expiry = now_ms + 3600 * 1000;  // 1時間

// Cache Time設定
opt.cachetime_f = 1;
opt.cachetime = now_ms + 300 * 1000;  // 5分

// Content Object作成
int cob_len = cef_frame_object_create(cob_buff, &opt, &params);

// cefnetdへ送信
cef_client_message_input(handle, cob_buff, cob_len);
```

#### Interest受信（CEFORE → ICSN方向）

```cpp
// Interest受信ループ
unsigned char recv_buff[CefC_Max_Length];
struct cef_app_request app_request;

while (running) {
    int len = cef_client_read(handle, recv_buff, CefC_Max_Length);

    if (len > 0) {
        int res = cef_client_request_get_with_info(recv_buff, len, &app_request);

        if (res > 0 && app_request.version == CefC_App_Version) {
            char uri[1024];
            cef_frame_conversion_name_to_uri(app_request.name, app_request.name_len, uri);
            uint32_t chunk_num = app_request.chunk_num;

            // コールバック呼び出し → ESP32経由でICSNセンサーにInterest転送
            onInterestReceived(uri, chunk_num);
        }
    }
    usleep(1000);  // 1msポーリング
}
```

#### 終了処理

```cpp
cef_client_close(handle);
```

## 6. スレッド設計

### 6.1 スレッド構成

| スレッド | 役割 |
|---|---|
| メインスレッド | 初期化、シャットダウン |
| UART受信スレッド | ESP32からのデータ受信 |
| CEFORE受信スレッド | cefnetdからのInterest受信 |

### 6.2 同期設計

- `std::mutex` による送信キューの保護
- `std::function` によるイベント駆動（コールバック）

## 7. ビルド環境

### 7.1 必要なライブラリ

- **CEFORE:** libcefore, libcefore-dev
- **Base64:** 軽量Base64ライブラリ (例: `cpp-base64`)
- **ビルドツール:** CMake 3.10+, GCC 7.0+

### 7.2 CMake設定

```cmake
cmake_minimum_required(VERSION 3.10)
project(raspi-cefore-gateway)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)
find_library(CEFORE_LIB cefore REQUIRED)
find_path(CEFORE_INCLUDE cefore/cef_frame.h REQUIRED)

include_directories(${CEFORE_INCLUDE})

add_executable(gateway
    src/main.cpp
    src/uart_receiver.cpp
    src/packet_parser.cpp
    src/cefore_interface.cpp
    src/name_mapper.cpp
    src/main_controller.cpp
)

target_link_libraries(gateway
    ${CEFORE_LIB}
    Threads::Threads
)
```

### 7.3 実行方法

```bash
# ビルド
mkdir build && cd build
cmake ..
make

# 実行
sudo ./gateway /dev/serial0 115200
```

## 8. ディレクトリ構成

```
raspi-cefore-gateway/
├── CMakeLists.txt
├── README.md
└── src/
    ├── main.cpp
    ├── uart_receiver.cpp
    ├── uart_receiver.h
    ├── packet_parser.cpp
    ├── packet_parser.h
    ├── cefore_interface.cpp
    ├── cefore_interface.h
    ├── name_mapper.cpp
    ├── name_mapper.h
    ├── main_controller.cpp
    └── main_controller.h
```
