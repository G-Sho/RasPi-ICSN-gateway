# ビルド手順

## 前提条件

### 必要なライブラリ

1. **CEFORE** (libcefore)
   ```bash
   # CEFOREをソースまたはパッケージマネージャからインストール
   # 参照: https://cefore.net/
   ```

2. **ビルドツール**
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential cmake git
   ```

## ビルド手順

1. リポジトリをクローン
   ```bash
   git clone <repository-url>
   cd RasPi-ICSN-gateway
   ```

2. ビルドディレクトリを作成
   ```bash
   mkdir build
   cd build
   ```

3. CMakeで設定
   ```bash
   cmake ..
   ```

4. ビルド
   ```bash
   make
   ```

5. インストール（オプション）
   ```bash
   sudo make install
   ```

## 実行方法

### 基本的な使い方

```bash
sudo ./gateway /dev/serial0 115200
```

### コマンドライン引数

- `argv[1]`: UARTデバイスパス（デフォルト: `/dev/serial0`）
- `argv[2]`: ボーレート（デフォルト: `115200`）

### 実行例

```bash
# デフォルト設定で実行
sudo ./gateway

# カスタムUARTデバイスとボーレートで実行
sudo ./gateway /dev/ttyUSB0 115200
```

## Raspberry PiのUART設定

1. GPIOピンのUARTを有効化
   ```bash
   sudo raspi-config
   # 移動先: Interface Options -> Serial Port
   # シリアルログインシェルを無効化: No
   # シリアルポートハードウェアを有効化: Yes
   ```

2. `/boot/config.txt`を編集
   ```bash
   sudo nano /boot/config.txt
   ```

   以下を追加または変更:
   ```
   enable_uart=1
   dtoverlay=disable-bt
   ```

3. 再起動
   ```bash
   sudo reboot
   ```

## トラブルシューティング

### CEFOREが見つからない場合

CMakeがCEFOREを見つけられない場合:
```bash
# CEFOREのインストールパスを指定
cmake -DCEFORE_INCLUDE=/path/to/cefore/include -DCEFORE_LIB=/path/to/cefore/lib ..
```

### UART権限エラー

```bash
sudo usermod -a -G dialout $USER
# ログアウトして再度ログイン
```

### cefnetdが起動していない場合

ゲートウェイを起動する前にcefnetdが起動していることを確認:
```bash
sudo cefnetd
```
