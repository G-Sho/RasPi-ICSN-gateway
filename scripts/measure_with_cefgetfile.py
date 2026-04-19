#!/usr/bin/env python3

import subprocess
import json
import time
import re
from pathlib import Path

class CEFOReMeasurement:
    def __init__(self, gateway_host="localhost"):
        self.gateway_host = gateway_host
        self.results = {
            "1hop": [],
            "2hop": [],
            "3hop": []
        }
        # Raspberry Pi に接続された全ノード
        self.serial_ports = {
            "bridge": "/dev/ttyAMA0",
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
                    f"echo 'reset_perf' | sudo tee {port}",
                    shell=True,
                    capture_output=True,
                    timeout=3
                )
                time.sleep(0.3)
            except Exception as e:
                print(f"    [WARN] Failed to reset {node_name}: {e}")

    def collect_sensor_data(self, node_name):
        """指定ノードからメモリバッファを取得"""

        if node_name not in self.serial_ports:
            return {"status": "unknown_node"}

        port = self.serial_ports[node_name]

        try:
            result = subprocess.run(
                f"echo 'dump_perf' | timeout 2 picocom -b 115200 {port}",
                shell=True,
                capture_output=True,
                text=True,
                timeout=3
            )

            # JSON抽出
            json_match = re.search(r'\{.*\}', result.stdout, re.DOTALL)
            if json_match:
                return json.loads(json_match.group(0))
            else:
                return {"status": "parse_error"}

        except subprocess.TimeoutExpired:
            return {"status": "timeout"}
        except Exception as e:
            return {"status": "error", "message": str(e)}

    def measure_pattern(self, content_path, hop_label, num_iterations=50):
        """パターン1回分を測定"""

        print(f"\n[MEASURE] {hop_label}: {num_iterations} iterations")

        # 全ノードのバッファをリセット
        self.reset_all_buffers()
        time.sleep(1)

        cefore_times = []

        for i in range(num_iterations):
            print(f"    [{i+1}/{num_iterations}]", end="", flush=True)

            # cefgetfile実行
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

        # 測定完了後、各ノードのデータを収集
        print("  → Collecting sensor measurements...")
        sensor_measurements = {}
        for node_name in ["bridge", "sensor_a", "sensor_b", "sensor_c"]:
            print(f"    [{node_name}]", end=" ", flush=True)
            data = self.collect_sensor_data(node_name)
            sensor_measurements[node_name] = data
            if data.get("status") in ("error", "timeout", "parse_error", "unknown_node"):
                print("✗ (skipped)")
            else:
                print("✓")
            time.sleep(0.3)

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

        # ノード別OTA統計表示
        sensors = result.get("sensor_measurements", {})
        for node_name in ["bridge", "sensor_a", "sensor_b", "sensor_c"]:
            node_data = sensors.get(node_name, {})
            if node_data.get("measurements"):
                ota_times = [m.get("ota_us", 0) for m in node_data["measurements"]]
                if ota_times:
                    print(f"\n  {node_name.replace('_', ' ').upper()} OTA (µs):")
                    print(f"    Mean: {sum(ota_times) / len(ota_times):8.1f}")
                    print(f"    Min:  {min(ota_times):8}")
                    print(f"    Max:  {max(ota_times):8}")

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
            num_iterations
        )

        time.sleep(2)

        # Pattern 2: 2hop (bridge → A → B)
        self.measure_pattern(
            "/iot/buildingA/room101/2hop",
            "2hop",
            num_iterations
        )

        time.sleep(2)

        # Pattern 3: 3hop (bridge → A → B → C)
        self.measure_pattern(
            "/iot/buildingA/room101/3hop",
            "3hop",
            num_iterations
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
