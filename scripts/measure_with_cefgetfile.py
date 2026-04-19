#!/usr/bin/env python3

import subprocess
import json
import time
import re

class CEFOReMeasurement:
    def __init__(self, gateway_host="localhost"):
        self.gateway_host = gateway_host
        self.results = {
            "1hop": [],
            "2hop": [],
            "3hop": []
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

    def measure_pattern(self, content_path, hop_label, num_iterations=50):
        """パターン1回分を測定"""

        print(f"\n[MEASURE] {hop_label}: {num_iterations} iterations")

        # ノードのメモリバッファをリセット（手動）
        print("  → Resetting sensor buffers...")
        print("    [MANUAL] On each ESP32 node, send: reset_perf")
        print("    - Bridge:   echo 'reset_perf' | sudo tee /dev/ttyAMA0")
        print("    - Sensor A: echo 'reset_perf' | tee /dev/ttyUSB0")
        print("    - Sensor B: echo 'reset_perf' | tee /dev/ttyUSB1")
        print("    - Sensor C: echo 'reset_perf' | tee /dev/ttyUSB2")
        input("  → Press ENTER when all buffers are reset...")

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

        print("\n  === Measurement Complete ===")
        print("  [MANUAL] Collect sensor data from nodes:")
        print("    1. Bridge:   echo 'dump_perf' > /dev/ttyAMA0  (then: cat /dev/ttyAMA0 | tee bridge_measurements.json)")
        print("    2. Sensor A: echo 'dump_perf' > /dev/ttyUSB0  (then: cat /dev/ttyUSB0 | tee sensor_a_measurements.json)")
        print("    3. Sensor B: echo 'dump_perf' > /dev/ttyUSB1  (then: cat /dev/ttyUSB1 | tee sensor_b_measurements.json)")
        print("    4. Sensor C: echo 'dump_perf' > /dev/ttyUSB2  (then: cat /dev/ttyUSB2 | tee sensor_c_measurements.json)")
        print("  Save outputs to respective JSON files for analysis.")

        # 結果をまとめる
        result = {
            "hop_label": hop_label,
            "num_iterations": num_iterations,
            "cefore_times_us": cefore_times
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
