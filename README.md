# Raspberry Pi - ICSN ゲートウェイ

ESP-NOW（ICSN）センサーネットワークと CEFORE（NDN）ネットワークを接続する  
C++ ゲートウェイの Raspberry Pi 実装です。

## 概要

本ゲートウェイは以下の間で双方向通信を実現します。

- **ESP32 ベースのセンサーノード** — ESP-NOW プロトコルで通信
- **CEFORE (NDN) ネットワーク** — コンテンツ指向ネットワークでデータを配信

### 主な機能

| 機能 | 説明 |
|---|---|
| データフロー（ICSN → CEFORE） | ESP32 から UART 経由でセンサーデータを受信し、CEFORE ネットワークへ公開 |
| Interest フロー（CEFORE → ICSN） | CEFORE から Interest パケットを受信し、ESP32 センサーへ転送 |
| FIB（TwoStage アルゴリズム） | LRU キャッシュ（最大 100 エントリ）による高速ルーティング |
| 自動学習 | 受信データパケットを元に FIB を動的に更新 |
| タイムスタンプ管理 | コンテンツ名へのタイムスタンプ付加・除去 |

## アーキテクチャ

```
ESP32 ブリッジ <--UART--> Raspberry Pi ゲートウェイ <--CEFORE API--> cefnetd <--> NDN ネットワーク
  (ESP-NOW)                    (C++ 実装)                  (CEFORE)
```

### コンポーネント

| コンポーネント | 役割 |
|---|---|
| UARTReceiver | ESP32 との UART 通信 |
| PacketParser | ESP-NOW パケットの解析 |
| CeforeInterface | CEFORE API との連携 |
| NameMapper | タイムスタンプの付加・除去 |
| GatewayFIB | コンテンツ名ルーティング（最長プレフィックス一致） |
| MainController | 全体の統括制御 |

## クイックスタート

詳細は [BUILD.md](BUILD.md) を参照してください。

```bash
# ビルド
mkdir build && cd build
cmake -DBUILD_PROFILE=normal ..
make

# 起動（cefnetd が稼働していること）
sudo ./gateway /dev/serial0 115200
```

動作確認の詳細手順は [OPERATION_GUIDE.md](OPERATION_GUIDE.md) を参照してください。

## 通信プロトコル

### UART フォーマット（Raspberry Pi <-> ESP32）

**受信（ESP32 → RasPi）:**

```
RX:<送信者MAC>|<データ長>|<Base64エンコードデータ>\n
例: RX:AA:BB:CC:DD:EE:FF|42|SGVsbG8gV29ybGQhCg==\n
```

**送信（RasPi → ESP32）:**

```
TX:<宛先MAC>|<Base64エンコードデータ>\n
```

### ICSN パケット構造体

```cpp
struct __attribute__((packed)) CommunicationData {
    char     signalCode[10];  // "DATA" または "INTEREST"
    uint8_t  hopCount;
    char     contentName[100];
    char     content[20];
    uint32_t counter;         // リプレイ攻撃対策カウンタ
    uint8_t  hmac[32];        // HMAC-SHA256 認証値
};
// 合計: 167 バイト
```

## データフロー

### センサーデータ収集（ICSN → CEFORE）

1. ESP32 → UART → `UARTReceiver`
2. `PacketParser` がセンサーデータを抽出
3. `GatewayFIB` がルートを学習（content_name → MAC）
4. `NameMapper` がタイムスタンプを付加
5. `CeforeInterface` が cefnetd へ公開

### Interest 処理（CEFORE → ICSN）

1. NDN アプリ → cefnetd → `CeforeInterface`
2. `NameMapper` がタイムスタンプを除去
3. `GatewayFIB` が最長プレフィックス一致で MAC アドレスを検索
4. `UARTReceiver` が UART 経由で ESP32 へ Interest を転送

## ビルドプロファイル

本プロジェクトは `BUILD_PROFILE` で以下 3 種類のビルドを提供します。

| プロファイル | 指定方法 | ログ出力 | パフォーマンス計測 |
|---|---|---|---|
| normal | `-DBUILD_PROFILE=normal` | INFO/WARN/DEBUG | 無効 |
| perf | `-DBUILD_PROFILE=perf` | 最小（WARN/ERROR） | 有効 |
| release | `-DBUILD_PROFILE=release` | 最小（WARN/ERROR） | 無効 |

```bash
mkdir build && cd build

# normal
cmake -DBUILD_PROFILE=normal ..
cmake --build .

# perf
cmake -DBUILD_PROFILE=perf ..
cmake --build .

# release
cmake -DBUILD_PROFILE=release ..
cmake --build .
```

`perf` プロファイルでのみ `latency_log.csv` への計測記録が有効になります。

## テストトポロジー

### テスト経路

```
RasPi gateway --UART--> bridge --ESP-NOW--> sensor(A) --ESP-NOW--> sensor(B) --ESP-NOW--> sensor(C)
```

### テスト用 MAC アドレス

| ノード    | MAC アドレス        |
|-----------|---------------------|
| bridge    | `08:D1:F9:37:39:C0` |
| sensor A  | `CC:7B:5C:9A:F3:C4` |
| sensor B  | `CC:7B:5C:9A:F3:AC` |
| sensor C  | `9C:9C:1F:CF:F4:8C` |

### 責務分担（FIB/ルーティング）

各ノードは「自分の直接の次ホップ」だけを決定します。

| ノード     | 決定する次ホップ          | 設定箇所                                      |
|------------|--------------------------|-----------------------------------------------|
| **gateway**  | 次ホップ = bridge        | `config/test_fib.conf`（本リポジトリ）         |
| **bridge**   | 次ホップ = sensor(A)     | `ESP32-ICSN-bridge` 側設定 |
| **sensor(A)**| 次ホップ = sensor(B)     | `ESP32-ICSN-sensor-node` 側設定 |
| **sensor(B)**| 次ホップ = sensor(C)     | `ESP32-ICSN-sensor-node` 側設定 |
| **sensor(C)**| 末端（データ発信源）      | `ESP32-ICSN-sensor-node` 側設定 |

> **ポイント**: gateway は bridge の MAC アドレスしか直接知らない。  
> センサー A/B/C への Interest は、`/icsn` プレフィックスの最長一致で bridge 宛に転送される。  
> bridge 以降の転送（bridge→A, A→B, B→C）は各ノードが自身の FIB で解決する。

### 初期 FIB 設定（gateway）

`config/test_fib.conf` に静的 FIB エントリが記述されています。  
このファイルを起動時の第 3 引数に渡すことで初期 FIB を投入できます。

```bash
# テストトポロジーの初期 FIB を使って起動（cefnetd が稼働していること）
sudo ./gateway /dev/serial0 115200 ../config/test_fib.conf
```

FIB エントリの例:

```
# /icsn 配下のすべての Interest を bridge (08:D1:F9:37:39:C0) へ転送
/icsn 08:D1:F9:37:39:C0
```

---

## 設定

| パラメータ | デフォルト値 |
|---|---|
| UART デバイス | `/dev/serial0` |
| ボーレート | `115200` |
| FIB キャッシュサイズ | `100` エントリ |
| 最大仮想深度 | `3` |

## 依存関係

- CEFORE (libcefore)
- CMake 3.10+
- GCC 7.0+（C++17 対応）
- cpp-base64（同梱済み）

## ライセンス

[LICENSE](LICENSE)

## 関連ドキュメント

- [設計書](raspi-gateway-design.md)
- [ビルド手順](BUILD.md)
- [動作確認手順書](OPERATION_GUIDE.md)
- [CEFORE 公式サイト](https://cefore.net/)
