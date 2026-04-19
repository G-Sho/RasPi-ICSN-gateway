# 動作確認手順書

Raspberry Pi - ICSN ゲートウェイの動作確認手順をまとめたドキュメントです。

---

## 目次

1. [前提条件・必要機材](#1-前提条件必要機材)
2. [環境セットアップ](#2-環境セットアップ)
3. [ビルド](#3-ビルド)
4. [起動前チェック](#4-起動前チェック)
5. [ゲートウェイ起動](#5-ゲートウェイ起動)
6. [動作確認手順](#6-動作確認手順)
   - 6.1 [UART 通信確認（ESP32 ↔ Raspberry Pi）](#61-uart-通信確認esp32--raspberry-pi)
   - 6.2 [ICSN → CEFORE データフロー確認](#62-icsn--cefore-データフロー確認)
   - 6.3 [CEFORE → ICSN Interest フロー確認](#63-cefore--icsn-interest-フロー確認)
7. [正常動作ログの見方](#7-正常動作ログの見方)
8. [トラブルシューティング](#8-トラブルシューティング)
9. [動作確認チェックリスト](#9-動作確認チェックリスト)

---

## 1. 前提条件・必要機材

### ハードウェア

| 機材 | 用途 |
|---|---|
| Raspberry Pi 5 | ゲートウェイ本体 |
| ESP32 ブリッジノード | UART 経由で Raspberry Pi と通信 |
| USB-UART 変換アダプタ（任意） | PC からの UART 確認用 |

> **補足**: Raspberry Pi 5 では I/O が RP1 チップ経由になっており、GPIO の UART（UART0）は `/dev/ttyAMA0` → `/dev/serial0` としてアクセスします。Bluetooth は RP1 内部の別 UART で動作するため、`dtoverlay=disable-bt` を適用することで UART0 をアプリケーション専用にできます。

### ソフトウェア

| ソフトウェア | バージョン | インストール先 |
|---|---|---|
| Raspberry Pi OS (64-bit) | Bookworm（Debian 12）以降 | Raspberry Pi |
| CEFORE | 最新版 | Raspberry Pi |
| CMake | 3.10+ | Raspberry Pi |
| GCC | 12.0+（C++17 対応） | Raspberry Pi |
| cefnetd | CEFORE 付属 | Raspberry Pi |

---

## 2. 環境セットアップ

### 2.1 ビルドツールのインストール

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git libssl-dev
```

### 2.2 CEFORE のインストール

```bash
# CEFORE 公式サイト (https://cefore.net/) の手順に従いインストール
# インストール後、以下を確認
cefnetd --version
```

### 2.3 Raspberry Pi の UART 有効化

GPIO ピンの UART（`/dev/serial0`）を有効にします。

```bash
sudo raspi-config
# 移動先: Interface Options -> Serial Port
#   「シリアルポートのログインシェルを有効にしますか？」-> No
#   「シリアルポートハードウェアを有効にしますか？」  -> Yes
```

> **Raspberry Pi 5 の設定ファイルパスについて**: Raspberry Pi OS Bookworm では、ブート設定ファイルのパスが `/boot/config.txt` から `/boot/firmware/config.txt` に変更されています。

`/boot/firmware/config.txt` を編集して Bluetooth を無効化し、UART0 をアプリケーション専用にします。

```bash
sudo nano /boot/firmware/config.txt
```

以下を追加（または既存行を変更）：

```ini
enable_uart=1
dtoverlay=disable-bt
```

再起動して設定を反映します。

```bash
sudo reboot
```

再起動後、デバイスファイルが存在することを確認します。

```bash
ls -l /dev/serial0
# 期待: /dev/serial0 -> ttyAMA0
```

### 2.4 UART ポートのアクセス権設定

```bash
sudo usermod -a -G dialout $USER
# ログアウト＆再ログインして反映
```

### 2.5 リポジトリのクローン

```bash
git clone <repository-url>
cd RasPi-ICSN-gateway
```

---

## 3. ビルド

```bash
# ビルドディレクトリを作成
mkdir build && cd build

# CMake 設定
cmake ..

# ビルド
make -j$(nproc)
```

ビルドに成功すると `build/gateway` が生成されます。

```bash
ls -lh gateway
# 期待: -rwxr-xr-x ... gateway
```

### CEFORE のインストールパスが非標準の場合

```bash
cmake -DCEFORE_ROOT=/path/to/cefore ..
make -j$(nproc)
```

---

## 4. 起動前チェック

### 4.1 cefnetd の起動確認

ゲートウェイを起動する前に cefnetd が稼働していることを確認します。

```bash
# cefnetd が動いているか確認
ps aux | grep cefnetd

# 動いていなければ起動
sudo cefnetd &
sleep 2

# 接続テスト (CEFORE の cefping 等を使う場合)
# cefping ccnx:/test
```

### 4.2 UART デバイスの確認

```bash
# ESP32 が接続されている場合
ls -l /dev/serial0 /dev/ttyAMA0 /dev/ttyUSB0 2>/dev/null

# ESP32 未接続でも動作検証する場合は仮想 UART を使用（後述）
```

### 4.3 ゲートウェイ実行ファイルの確認

```bash
# build ディレクトリ内にいることを確認
ls -lh gateway
```

---

## 5. ゲートウェイ起動

### 基本起動（デフォルト設定）

```bash
sudo ./gateway
# UART: /dev/serial0、ボーレート: 115200
# FIB設定ファイルは ../config/test_fib.conf → config/test_fib.conf の順に自動探索
```

### カスタム設定で起動

```bash
# 引数: <UARTデバイス> <ボーレート> [<FIB設定ファイル>]
sudo ./gateway /dev/serial0 115200 ../config/test_fib.conf

# USB-UART アダプタを使用する場合
sudo ./gateway /dev/ttyUSB0 115200 ../config/test_fib.conf
# 引数: <UARTデバイス> <ボーレート>
sudo ./gateway /dev/ttyAMA0 115200
```

### 正常起動時の出力例

```
=== Raspberry Pi CEFORE Gateway ===
UART Device: /dev/serial0
Baudrate: 115200
FIB Config: ../config/test_fib.conf
===================================
[INFO] Creating components...
[INFO] Initializing CEFORE...
[INFO] Loading initial FIB from: ../config/test_fib.conf
[INFO] Static FIB: /iot/buildingA/room101 -> CC:7B:5C:9A:F3:C4
[INFO] Loaded 1 static FIB entries
[INFO] Registered prefix: ccnx:/iot/buildingA/room101
[INFO] Gateway initialized successfully
[INFO] Gateway running... Press Ctrl+C to stop
```

起動に失敗する場合は [トラブルシューティング](#8-トラブルシューティング) を参照してください。

---

## 6. 動作確認手順

### 6.1 UART 通信確認（ESP32 ↔ Raspberry Pi）

#### ESP32 が手元にない場合（仮想 UART でテスト）

`socat` で仮想 UART ペアを作成し、テストデータを送信します。

```bash
# socat インストール
sudo apt-get install -y socat

# 仮想シリアルポートペアを作成（別ターミナルで実行）
socat PTY,link=/tmp/virtual-esp32,rawer PTY,link=/tmp/virtual-raspi,rawer &

# ゲートウェイをカスタムデバイスで起動（別ターミナル）
sudo ./gateway /tmp/virtual-raspi 115200 ../config/test_fib.conf

# テストパケット送信（Python で ICSN の CommunicationData 構造体を生成）
python3 /tmp/send_test_packet.py /tmp/virtual-esp32
```

テストパケット送信スクリプト（`/tmp/send_test_packet.py`）を作成して実行します。

```python
#!/usr/bin/env python3
"""UART テストパケット送信スクリプト"""
import serial
import struct
import base64
import sys
import time

def build_icsn_packet(signal_code: str, content_name: str, content: str) -> bytes:
    """CommunicationData 構造体（167バイト）を生成"""
    # struct __attribute__((packed)) CommunicationData {
    #   char signalCode[10];
    #   uint8_t hopCount;
    #   char contentName[100];
    #   char content[20];
    #   uint32_t counter;
    #   uint8_t hmac[32];
    # };
    fmt = '10sB100s20sI32s'
    pkt = struct.pack(
        fmt,
        signal_code.encode().ljust(10, b'\x00')[:10],
        1,  # hopCount
        content_name.encode().ljust(100, b'\x00')[:100],
        content.encode().ljust(20, b'\x00')[:20],
        0,  # counter
        bytes(32),  # hmac (all zero)
    )
    return pkt

device = sys.argv[1] if len(sys.argv) > 1 else '/tmp/virtual-esp32'
mac = 'AA:BB:CC:DD:EE:FF'

with serial.Serial(device, 115200, timeout=1) as ser:
    pkt = build_icsn_packet('DATA', '/icsn/sensor/temp', '25.3')
    encoded = base64.b64encode(pkt).decode()
    line = f'RX:{mac}|{len(pkt)}|{encoded}\n'
    print(f'送信: {line.strip()}')
    ser.write(line.encode())
    time.sleep(0.5)
    # TX 応答確認
    resp = ser.read(256)
    if resp:
        print(f'応答: {resp}')
```

#### ESP32 が接続されている場合

ESP32 ブリッジファームウェアが動作していれば、センサーノードからデータを送信するだけで
ゲートウェイログに以下が出力されます。

```
[INFO] Received DATA from AA:BB:CC:DD:EE:FF: /icsn/sensor/temp = 25.3
[INFO] Published to CEFORE: /icsn/sensor/temp/1713430617
```

---

### 6.2 ICSN → CEFORE データフロー確認

ゲートウェイがセンサーデータを CEFORE に公開していることを確認します。

#### 方法 A: cefgetfile でデータ取得

```bash
# CEFORE の cefgetfile コマンドでデータを取得
# （タイムスタンプ付きの名前で問い合わせる）
cefgetfile ccnx:/icsn/sensor/temp/<タイムスタンプ> -o /tmp/result.txt
cat /tmp/result.txt
```

#### 方法 B: cefpyco（Python バインディング）でデータ取得

```python
#!/usr/bin/env python3
import cefpyco
import time

with cefpyco.create_handle() as handle:
    # Interest を送信
    handle.send_interest("ccnx:/icsn/sensor/temp", 0)
    # Data を受信
    info = handle.receive(timeout_ms=5000)
    if info.is_succeeded:
        print(f"受信コンテンツ名: {info.name}")
        print(f"ペイロード: {info.payload.decode()}")
    else:
        print("タイムアウト: データを受信できませんでした")
```

#### 確認ポイント

- ゲートウェイログに `[INFO] Published to CEFORE: ccnx:/icsn/...` が出力される
- cefnetd の PIT/CS にエントリが追加される

---

### 6.3 CEFORE → ICSN Interest フロー確認

CEFORE から Interest を送信し、ゲートウェイが ESP32 へ転送することを確認します。

#### Interest 送信（別ターミナルから）

```bash
# cefgetfile でゲートウェイが登録しているプレフィックスに Interest を送信
cefgetfile ccnx:/icsn/sensor/temp -o /dev/null
```

または cefpyco を使う場合：

```python
#!/usr/bin/env python3
import cefpyco

with cefpyco.create_handle() as handle:
    handle.send_interest("ccnx:/icsn/sensor/temp", 0)
    print("Interest 送信完了")
```

#### 確認ポイント

ゲートウェイのログに以下が出力されれば正常です。

```
[INFO] Received Interest: ccnx:/icsn/sensor/temp (chunk=0)
[INFO] Forwarded Interest to AA:BB:CC:DD:EE:FF: /icsn/sensor/temp
```

または、FIB にエントリがない場合はブロードキャストになります。

```
[WARN] No FIB entry found for: /icsn/sensor/temp
[INFO] Broadcasting Interest to all nodes
[INFO] Forwarded Interest to FF:FF:FF:FF:FF:FF: /icsn/sensor/temp
```

ESP32 側では UART で以下のフォーマットのデータが受信されます。

```
TX:AA:BB:CC:DD:EE:FF|<Base64エンコードされたICSN-INTERESTパケット>\n
```

---

## 7. 正常動作ログの見方

| ログレベル | プレフィックス | 意味 |
|---|---|---|
| 情報 | `[INFO]` | 正常処理 |
| 警告 | `[WARN]` | 処理は継続するが注意が必要な状態 |
| エラー | `[ERROR]` | 処理に失敗（ゲートウェイは継続動作） |

### 代表的なログメッセージ

| ログメッセージ | 意味 |
|---|---|
| `Gateway initialized successfully` | 初期化成功 |
| `Registered prefix: ccnx:/icsn` | CEFORE プレフィックス登録完了 |
| `Received DATA from <MAC>: <name> = <value>` | センサーデータ受信 |
| `Published to CEFORE: <uri>` | CEFORE へのデータ公開成功 |
| `Received Interest: <uri>` | CEFORE から Interest 受信 |
| `Forwarded Interest to <MAC>: <name>` | ESP32 へ Interest 転送 |
| `Broadcasting Interest to all nodes` | FIB ミスのためブロードキャスト |

---

## 8. トラブルシューティング

### CEFORE 初期化失敗

```
[ERROR] CEFORE initialization failed
```

**原因と対処：**

1. cefnetd が起動していない
   ```bash
   sudo cefnetd &
   sleep 2
   # ゲートウェイを再起動
   ```

2. CEFORE ライブラリが見つからない
   ```bash
   # ライブラリパスを確認
   ldconfig -p | grep cefore
   # 見つからない場合
   sudo ldconfig /usr/local/lib
   ```

3. CEFORE インストールパスが非標準
   ```bash
   cmake -DCEFORE_ROOT=/path/to/cefore ..
   make
   ```

---

### UART デバイスが開けない

```
Error opening /dev/serial0: Permission denied
```

**対処：**

```bash
# dialout グループに追加
sudo usermod -a -G dialout $USER
# ログアウト＆再ログイン
```

または sudo で実行：

```bash
sudo ./gateway /dev/serial0 115200 ../config/test_fib.conf
```

---

### UART デバイスが存在しない

```
Error opening /dev/serial0: No such file or directory
```

**対処：**

1. `raspi-config` で Serial Port Hardware を有効化
2. `/boot/firmware/config.txt` に `enable_uart=1` と `dtoverlay=disable-bt` を追加
3. 再起動

---

### センサーデータが CEFORE に公開されない

**チェックリスト：**

- [ ] ゲートウェイログに `Received DATA from ...` が出力されているか
- [ ] `PacketParser` が解析できているか（ログに `Failed to parse packet` が出ていないか）
- [ ] cefnetd が正常稼働しているか（`ps aux | grep cefnetd`）
- [ ] ICSN パケットの `signalCode` フィールドが `"DATA"` になっているか

---

### Interest がセンサーに届かない

**チェックリスト：**

- [ ] ゲートウェイログに `Received Interest: ...` が出力されているか
- [ ] FIB に対象コンテンツ名のエントリがあるか（事前に DATA を受信しているか）
- [ ] ESP32 の UART 受信処理が正常動作しているか
- [ ] UART の TX コマンドフォーマットが `TX:<MAC>|<Base64>\n` になっているか

---

### ゲートウェイが起動直後にクラッシュする

```
Initialization failed
```

**対処：**

1. cefnetd が起動しているか確認
2. UART デバイスが存在し、アクセス権があるか確認
3. CEFORE ライブラリの依存関係を確認
   ```bash
   ldd ./gateway | grep "not found"
   ```

---

## 9. 動作確認チェックリスト

以下の項目をすべてクリアすることで、ゲートウェイの基本動作が確認されます。

### 環境セットアップ

- [ ] Raspberry Pi 5 の UART が有効化されている（`/dev/serial0` が `ttyAMA0` を指している）
- [ ] `dialout` グループに追加されている、または `sudo` で実行可能
- [ ] CEFORE（libcefore）がインストールされている
- [ ] cefnetd が正常に起動できる

### ビルド

- [ ] `cmake ..` が警告・エラーなしで完了する
- [ ] `make` が成功し `gateway` 実行ファイルが生成される

### 起動確認

- [ ] ゲートウェイが起動し `Gateway initialized successfully` が表示される
- [ ] `Registered prefix: ccnx:/icsn` が表示される

### データフロー確認

- [ ] ESP32（または仮想 UART）からテストパケットを送信し `Received DATA from ...` が表示される
- [ ] `Published to CEFORE: ...` が表示される
- [ ] CEFORE の Interest に応じて `Received Interest: ...` が表示される
- [ ] `Forwarded Interest to ...` が表示され、ESP32 に TX コマンドが送られる

---

## 関連ドキュメント

- [README.md](README.md) — プロジェクト概要
- [BUILD.md](BUILD.md) — ビルド手順の詳細
- [raspi-gateway-design.md](raspi-gateway-design.md) — システム設計書
- [CEFORE 公式サイト](https://cefore.net/)
