# 計測スクリプト

## 計測手順（手動方式）

### Phase 1: Gatewayで自動測定

```bash
cd ~/RasPi-ICSN-gateway
python3 scripts/measure_with_cefgetfile.py
```

測定中に以下のプロンプトが表示されます：

1. 各ノードのバッファをリセットするよう指示
2. ENTER キーで測定開始
3. 全パターン測定完了後、データ収集方法をガイド

### Phase 2: 各ノードから手動でデータ収集

各ホップパターンの測定完了後、スクリプトが表示する手順に従って各ノードから計測データを収集します。

シリアルポートに接続し、`dump_perf` コマンドを送信してレスポンス（JSON）を保存します。

**Bridge（Raspberry Pi から）:**

```bash
# コマンドを送信
echo "dump_perf" > /dev/ttyAMA0

# レスポンスを読み取り（Ctrl+C で終了）
cat /dev/ttyAMA0 | tee bridge_measurements.json
```

または picocom を使う場合:

```bash
picocom -b 115200 /dev/ttyAMA0
# 接続後 dump_perf と入力し、出力を別途保存
```

**Sensor A（PC から）:**

```bash
echo "dump_perf" > /dev/ttyUSB0
cat /dev/ttyUSB0 | tee sensor_a_measurements.json
```

**Sensor B:**

```bash
echo "dump_perf" > /dev/ttyUSB1
cat /dev/ttyUSB1 | tee sensor_b_measurements.json
```

**Sensor C:**

```bash
echo "dump_perf" > /dev/ttyUSB2
cat /dev/ttyUSB2 | tee sensor_c_measurements.json
```

### Phase 3: 結果の分析

各ノードの measurements.json ファイルを別途 Python スクリプトで統合分析します。

Gateway の `measurements.json` には CEFORE の Duration データが格納されます。
各ノードの JSON ファイルと組み合わせることで、エンドツーエンドの性能分析が可能です。
