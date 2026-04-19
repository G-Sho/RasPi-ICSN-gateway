#!/usr/bin/env python3

import subprocess
import json
import time
import re

# hop パターンごとに readsensor を送るノードの対応表
HOP_SENSOR_NODES = {
    "1hop": ["sensor_a"],
    "2hop": ["sensor_a", "sensor_b"],
    "3hop": ["sensor_a", "sensor_b", "sensor_c"],
}

# hop パターンごとにキャッシュをクリアすべき中間ノードの対応表
# Data が終端センサーから戻る経路上で中間ノードが Data をキャッシュするため，
# 各イテレーション前にこれらのノードのキャッシュを削除しないと
# 2回目以降の Interest が中間ノードで返されて事実上 hop 数が短縮されてしまう
# 1hop: 中間ノードなし
# 2hop: A が B からの Data をキャッシュする
# 3hop: B が C からの Data を，A が B/C からの Data をキャッシュする
HOP_INTERMEDIATE_NODES = {
    "1hop": [],
    "2hop": ["sensor_a"],
    "3hop": ["sensor_a", "sensor_b"],
}

class CEFOReMeasurement:
    def __init__(self, gateway_host="localhost"):
        self.gateway_host = gateway_host
        self.results = {
            "1hop": [],
            "2hop": [],
            "3hop": []
        }
        self.serial_ports = {
            "bridge":   "/dev/ttyAMA0",
            "sensor_a": "/dev/ttyUSB0",
            "sensor_b": "/dev/ttyUSB1",
            "sensor_c": "/dev/ttyUSB2"
        }

    def run_cefgetfile(self, content_path, hop_label):
        """1回のcefgetfileを実行"""

        uri = f"ccnx:{content_path}"
        cmd = ["cefgetfile", uri]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=10
            )

            # CEFOREの出力からDurationを抽出
            match = re.search(r'Duration\s*=\s*([\d.]+)\s*sec', result.stdout)
            if match:
                duration_sec = float(match.group(1))
                duration_us = int(duration_sec * 1_000_000)

                return {
                    "status": "ok",
                    "cefore_duration_us": duration_us,
                    "stdout": result.stdout
                }
            else:
                return {"status": "parse_error", "stdout": result.stdout}

        except subprocess.TimeoutExpired:
            return {"status": "timeout"}
        except Exception as e:
            return {"status": "error", "message": str(e)}

    def reset_all_buffers(self):
        """全ノードのバッファをリセット"""
        print("  → Resetting all sensor buffers...")
        for node_name, port in self.serial_ports.items():
            try:
                subprocess.run(
                    ["sudo", "tee", port],
                    input=b"reset_perf\n",
                    capture_output=True,
                    timeout=3
                )
                time.sleep(0.3)
            except Exception as e:
                print(f"    [WARN] Failed to reset {node_name}: {e}")

    def send_readsensor(self, node_names):
        """指定ノードへ readsensor コマンドを送信してデータを生成させる"""
        for node_name in node_names:
            port = self.serial_ports.get(node_name)
            if port is None:
                print(f"    [WARN] Unknown node for readsensor: {node_name}")
                continue
            try:
                with open(port, "wb") as dev:
                    dev.write(b"read_sensor\n")
            except Exception as e:
                print(f"    [WARN] readsensor to {node_name} ({port}) failed: {e}")

    def flush_cefore_cache(self, content_path):
        """cefnetd の CS から対象コンテンツのキャッシュを削除する"""
        uri = f"ccnx:{content_path}"
        try:
            subprocess.run(
                ["cefctrl", "cs", "del", uri],
                capture_output=True,
                timeout=5
            )
        except FileNotFoundError:
            pass  # cefctrl が無い環境ではスキップ
        except Exception as e:
            print(f"    [WARN] flush_cefore_cache failed: {e}")

    def flush_node_caches(self, node_names):
        """中間 ESP32 ノードのコンテンツキャッシュをクリアする

        Data パケットが終端センサーから戻る経路で中間ノードにキャッシュされるため，
        2 回目以降の Interest が中間ノードで返されて hop 数が短縮されるのを防ぐ．
        """
        for node_name in node_names:
            port = self.serial_ports.get(node_name)
            if port is None:
                print(f"    [WARN] Unknown node for cache flush: {node_name}")
                continue
            try:
                with open(port, "wb") as dev:
                    dev.write(b"clear_cache\n")
            except Exception as e:
                print(f"    [WARN] flush_node_caches to {node_name} ({port}) failed: {e}")

    def clear_esp32_cache(self, node_names):
        """ESP32ノードのキャッシュをクリア"""
        print("  → Clearing ESP32 cache...")
        for node_name in node_names:
            if node_name not in self.serial_ports:
                continue
            port = self.serial_ports[node_name]
            print(f"    [{node_name}]", end=" ", flush=True)
            try:
                echo_proc = subprocess.Popen(
                    ["echo", "clear_cache"],
                    stdout=subprocess.PIPE
                )
                picocom_proc = subprocess.Popen(
                    ["timeout", "2", "picocom", "-b", "115200", port],
                    stdin=echo_proc.stdout,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE
                )
                echo_proc.stdout.close()
                stdout, _ = picocom_proc.communicate()
                result_stdout = stdout.decode("utf-8", errors="replace")

                if "cleared" in result_stdout.lower() or "ok" in result_stdout.lower():
                    print("✓")
                else:
                    print("⚠ (unclear response)")
            except Exception as e:
                print(f"✗ ({e})")
            time.sleep(0.3)

    def collect_sensor_data(self, node_name):
        """指定ノードからメモリバッファを取得"""

        if node_name not in self.serial_ports:
            return {"status": "unknown_node"}

        port = self.serial_ports[node_name]

        try:
            echo_proc = subprocess.Popen(
                ["echo", "dump_perf"],
                stdout=subprocess.PIPE
            )
            picocom_proc = subprocess.Popen(
                ["timeout", "2", "picocom", "-b", "115200", port],
                stdin=echo_proc.stdout,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            echo_proc.stdout.close()
            stdout, _ = picocom_proc.communicate(timeout=3)
            result_stdout = stdout.decode("utf-8", errors="replace")

            json_match = re.search(r'\{.*\}', result_stdout, re.DOTALL)
            if json_match:
                return json.loads(json_match.group(0))
            else:
                return {"status": "parse_error"}

        except subprocess.TimeoutExpired:
            return {"status": "timeout"}
        except Exception as e:
            return {"status": "error", "message": str(e)}

    def measure_pattern(self, content_path, hop_label, num_iterations=50, sensor_nodes=None, intermediate_nodes=None,
                        cache_clear_nodes=None):
        """パターン1回分を測定"""

        if sensor_nodes is None:
            sensor_nodes = HOP_SENSOR_NODES.get(hop_label, [])
        if intermediate_nodes is None:
            intermediate_nodes = HOP_INTERMEDIATE_NODES.get(hop_label, [])

        print(f"\n[MEASURE] {hop_label}: {num_iterations} iterations")
        print(f"  readsensor nodes:    {sensor_nodes}")
        print(f"  intermediate nodes:  {intermediate_nodes}")

        # キャッシュクリア（2hop/3hop時）
        if cache_clear_nodes:
            self.clear_esp32_cache(cache_clear_nodes)

        # 全ノードのメモリバッファをリセット
        self.reset_all_buffers()

        cefore_times = []

        for i in range(num_iterations):
            print(f"    [{i+1}/{num_iterations}]", end="", flush=True)

            # 1) ゲートウェイ (cefnetd) の CS から前回の Data を削除
            self.flush_cefore_cache(content_path)

            # 2) 中間 ESP32 ノードのキャッシュをクリア
            #    （これをしないと 2 回目以降の Interest が中間ノードで返されて hop 数が短縮される）
            self.flush_node_caches(intermediate_nodes)

            # 3) 対象センサーノードに readsensor を送信してデータを生成
            self.send_readsensor(sensor_nodes)

            # センサーが応答できるまで少し待つ
            time.sleep(0.1)

            # 4) cefgetfile 実行
            measurement = self.run_cefgetfile(content_path, hop_label)

            if measurement["status"] == "ok":
                cefore_times.append(measurement["cefore_duration_us"])
                print(" ✓", end="")
            else:
                print(" ✗", end="")

            if (i + 1) % 10 == 0:
                print(f"  ({i+1}/{num_iterations})")
            else:
                print("", end="")

            time.sleep(0.3)  # 連続実行によるオーバーロード回避

        print()

        # 全ノードからセンサー計測データを自動収集
        print("  → Collecting sensor measurements...")
        sensor_measurements = {}
        for node_name in self.serial_ports:
            print(f"    [{node_name}]", end=" ", flush=True)
            data = self.collect_sensor_data(node_name)
            sensor_measurements[node_name] = data
            status = data.get("status", "ok")
            if status != "ok":
                print(f"⚠ {status}")
            else:
                print("✓")

        # 結果をまとめる
        result = {
            "hop_label": hop_label,
            "num_iterations": num_iterations,
            "cefore_times_us": cefore_times,
            "sensor_measurements": sensor_measurements
        }

        self.results[hop_label] = result
        self._print_statistics(result)

        return result

    def _print_statistics(self, result):
        """統計情報を表示"""

        times = result["cefore_times_us"]
        if not times:
            print("  [WARN] No valid measurements")
            return

        times.sort()
        n = len(times)
        mean = sum(times) / n
        variance = sum((t - mean) ** 2 for t in times) / n
        std = variance ** 0.5

        print(f"\n  === {result['hop_label']} Statistics ===")
        print(f"  CEFORE Duration (µs):")
        print(f"    Mean:   {mean:8.1f}")
        print(f"    Median: {times[n//2]:8.1f}")
        print(f"    Std:    {std:8.1f}")
        print(f"    Min:    {min(times):8}")
        print(f"    Max:    {max(times):8}")
        print(f"    P95:    {times[int(n*0.95)]:8}")

        sensor_measurements = result.get("sensor_measurements", {})
        if sensor_measurements:
            print(f"\n  OTA Statistics per Node:")
            for node_name, data in sensor_measurements.items():
                status = data.get("status")
                if status and status != "ok":
                    print(f"    [{node_name}] status={status}")
                    continue
                ota_us = data.get("ota_us")
                roundtrip_us = data.get("roundtrip_us")
                fib_us = data.get("fib_us")
                total_us = data.get("total_us")
                parts = []
                if ota_us is not None:
                    parts.append(f"ota={ota_us} µs")
                if roundtrip_us is not None:
                    parts.append(f"roundtrip={roundtrip_us} µs")
                if fib_us is not None:
                    parts.append(f"fib={fib_us} µs")
                if total_us is not None:
                    parts.append(f"total={total_us} µs")
                if parts:
                    print(f"    [{node_name}] " + ", ".join(parts))
                else:
                    print(f"    [{node_name}] (no timing fields)")

    def export_results(self, filename="measurements.json"):
        """結果をJSONファイルに保存"""

        with open(filename, "w") as f:
            json.dump(self.results, f, indent=2)

        print(f"\n[INFO] Results saved to {filename}")

    def run_all_patterns(self, num_iterations=50):
        """全パターンを順次測定"""

        print("="*60)
        print("ICSN Performance Measurement with CEFORE")
        print("="*60)

        # Pattern 1: 1hop (bridge → A)
        self.measure_pattern(
            "/iot/buildingA/room101/1hop",
            "1hop",
            num_iterations,
            sensor_nodes=HOP_SENSOR_NODES["1hop"],
            intermediate_nodes=HOP_INTERMEDIATE_NODES["1hop"],
            cache_clear_nodes=None
        )

        time.sleep(2)

        # Pattern 2: 2hop (bridge → A → B)
        self.measure_pattern(
            "/iot/buildingA/room101/2hop",
            "2hop",
            num_iterations,
            sensor_nodes=HOP_SENSOR_NODES["2hop"],
            intermediate_nodes=HOP_INTERMEDIATE_NODES["2hop"],
            cache_clear_nodes=["sensor_a"]
        )

        time.sleep(2)

        # Pattern 3: 3hop (bridge → A → B → C)
        self.measure_pattern(
            "/iot/buildingA/room101/3hop",
            "3hop",
            num_iterations,
            sensor_nodes=HOP_SENSOR_NODES["3hop"],
            intermediate_nodes=HOP_INTERMEDIATE_NODES["3hop"],
            cache_clear_nodes=["sensor_a", "sensor_b"]
        )

        # 結果保存
        self.export_results()
        self._print_summary()

    def _print_summary(self):
        """全体サマリー表示"""

        print("\n" + "="*60)
        print("SUMMARY")
        print("="*60)

        for hop_label in ["1hop", "2hop", "3hop"]:
            if self.results[hop_label]:
                data = self.results[hop_label]
                times = data["cefore_times_us"]
                mean = sum(times) / len(times) if times else 0
                print(f"{hop_label:6}: {mean:8.1f} µs (mean)")


if __name__ == "__main__":
    measurement = CEFOReMeasurement()
    measurement.run_all_patterns(num_iterations=50)  # 50回測定
