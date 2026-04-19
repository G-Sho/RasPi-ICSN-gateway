#!/usr/bin/env python3
"""
ICSN Performance Measurement Script using cefgetfile
測定フロー:
  [1hop]  → cefgetfile × 50 → dump_perf
  [2hop]  → clear_cache (sensor_a) → reset_all_buffers → cefgetfile × 50 → dump_perf
  [3hop]  → clear_cache (sensor_a, sensor_b) → reset_all_buffers → cefgetfile × 50 → dump_perf
"""

import subprocess
import json
import time
import statistics
import os
import sys


class ICSNMeasurement:
    """ICSN パフォーマンス測定クラス"""

    def __init__(self):
        # シリアルポート設定
        self.serial_ports = {
            "bridge":   "/dev/ttyAMA0",
            "sensor_a": "/dev/ttyUSB0",
            "sensor_b": "/dev/ttyUSB1",
            "sensor_c": "/dev/ttyUSB2",
        }
        self.baud_rate = 115200
        self.results = {}

    # ------------------------------------------------------------------
    # キャッシュクリア
    # ------------------------------------------------------------------

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
                    ["timeout", "2", "picocom", "-b", str(self.baud_rate), port],
                    stdin=echo_proc.stdout,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE
                )
                echo_proc.stdout.close()
                stdout, _ = picocom_proc.communicate(timeout=3)
                result_stdout = stdout.decode("utf-8", errors="replace")

                if "cleared" in result_stdout.lower() or "ok" in result_stdout.lower():
                    print("✓")
                else:
                    print("⚠ (unclear response)")
            except Exception as e:
                print(f"✗ ({e})")
            time.sleep(0.3)

    # ------------------------------------------------------------------
    # バッファリセット
    # ------------------------------------------------------------------

    def reset_node_buffer(self, node_name):
        """単一ノードの計測バッファをリセット"""
        if node_name not in self.serial_ports:
            return False
        port = self.serial_ports[node_name]
        try:
            echo_proc = subprocess.Popen(
                ["echo", "reset_perf"],
                stdout=subprocess.PIPE
            )
            picocom_proc = subprocess.Popen(
                ["timeout", "2", "picocom", "-b", str(self.baud_rate), port],
                stdin=echo_proc.stdout,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            echo_proc.stdout.close()
            picocom_proc.communicate(timeout=3)
            return True
        except Exception:
            return False

    def reset_all_buffers(self):
        """全ノードのメモリバッファをリセット"""
        print("  → Resetting all sensor buffers...")
        for node_name in self.serial_ports:
            ok = self.reset_node_buffer(node_name)
            status = "✓" if ok else "✗"
            print(f"    [{node_name}] {status}")
            time.sleep(0.2)

    # ------------------------------------------------------------------
    # cefgetfile 実行
    # ------------------------------------------------------------------

    def run_cefgetfile(self, content_path, iteration, total):
        """cefgetfile を1回実行し所要時間（µs）を返す。失敗時は None"""
        print(f"    [{iteration}/{total}]", end=" ", flush=True)
        try:
            start = time.perf_counter()
            result = subprocess.run(
                ["cefgetfile", content_path, "-o", "/dev/null"],
                capture_output=True,
                timeout=10
            )
            elapsed_us = int((time.perf_counter() - start) * 1_000_000)
            if result.returncode == 0:
                print("✓", end=" ", flush=True)
                return elapsed_us
            else:
                print("✗", end=" ", flush=True)
                return None
        except subprocess.TimeoutExpired:
            print("T", end=" ", flush=True)
            return None
        except Exception:
            print("E", end=" ", flush=True)
            return None

    # ------------------------------------------------------------------
    # dump_perf 収集
    # ------------------------------------------------------------------

    def dump_perf_node(self, node_name):
        """単一ノードから dump_perf データを取得して辞書で返す"""
        if node_name not in self.serial_ports:
            return None
        port = self.serial_ports[node_name]
        try:
            echo_proc = subprocess.Popen(
                ["echo", "dump_perf"],
                stdout=subprocess.PIPE
            )
            picocom_proc = subprocess.Popen(
                ["timeout", "2", "picocom", "-b", str(self.baud_rate), port],
                stdin=echo_proc.stdout,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            echo_proc.stdout.close()
            stdout, _ = picocom_proc.communicate(timeout=3)
            raw = stdout.decode("utf-8", errors="replace")
            # JSON 部分を抽出
            start = raw.find("{")
            end = raw.rfind("}") + 1
            if start >= 0 and end > start:
                try:
                    return json.loads(raw[start:end])
                except json.JSONDecodeError:
                    pass
        except Exception:
            pass
        return None

    def collect_perf_all(self):
        """全ノードから dump_perf を収集"""
        print("  → Collecting sensor measurements...")
        perf_data = {}
        for node_name in self.serial_ports:
            data = self.dump_perf_node(node_name)
            if data is not None:
                perf_data[node_name] = data
                print(f"    [{node_name}] ✓")
            else:
                print(f"    [{node_name}] ✗")
            time.sleep(0.2)
        return perf_data

    # ------------------------------------------------------------------
    # 統計出力
    # ------------------------------------------------------------------

    def print_statistics(self, hop_label, durations_us, perf_data):
        """測定結果の統計を表示"""
        valid = [d for d in durations_us if d is not None]
        print(f"\n  === {hop_label} Statistics ===")
        if valid:
            print("  CEFORE Duration (µs):")
            print(f"    Mean:   {statistics.mean(valid):.1f}")
            print(f"    Median: {statistics.median(valid):.1f}")
            if len(valid) >= 2:
                print(f"    Std:    {statistics.stdev(valid):.1f}")
            print(f"    Min:    {min(valid)}")
            print(f"    Max:    {max(valid)}")
            sorted_v = sorted(valid)
            p95_idx = min(int(len(sorted_v) * 0.95), len(sorted_v) - 1)
            print(f"    P95:    {sorted_v[p95_idx]}")
        else:
            print("  CEFORE Duration: no valid samples")

        if perf_data:
            print("\n  OTA Statistics per Node:")
            for node_name, data in perf_data.items():
                parts = [f"{k}={v}" for k, v in data.items()]
                print(f"    [{node_name}] {', '.join(parts)}")

    # ------------------------------------------------------------------
    # パターン測定（メイン）
    # ------------------------------------------------------------------

    def measure_pattern(self, content_path, hop_label, num_iterations=50,
                        cache_clear_nodes=None):
        """パターン1回分を測定"""
        print(f"\n[MEASURE] {hop_label}: {num_iterations} iterations")

        # キャッシュクリア（2hop/3hop時）
        if cache_clear_nodes:
            self.clear_esp32_cache(cache_clear_nodes)

        # 全ノードのメモリバッファをリセット
        self.reset_all_buffers()

        # cefgetfile × num_iterations
        print("  → Running cefgetfile...")
        durations_us = []
        for i in range(1, num_iterations + 1):
            d = self.run_cefgetfile(content_path, i, num_iterations)
            durations_us.append(d)
            time.sleep(0.1)
        print()  # 改行

        # dump_perf 収集
        perf_data = self.collect_perf_all()

        # 統計表示
        self.print_statistics(hop_label, durations_us, perf_data)

        # 結果を保存
        self.results[hop_label] = {
            "content_path": content_path,
            "durations_us": durations_us,
            "perf_data": perf_data,
        }

    # ------------------------------------------------------------------
    # 全パターン実行
    # ------------------------------------------------------------------

    def run_all_patterns(self, num_iterations=50):
        """全パターンを順次測定"""

        # Pattern 1: 1hop（キャッシュクリアなし）
        self.measure_pattern(
            "/iot/buildingA/room101/1hop",
            "1hop",
            num_iterations,
            cache_clear_nodes=None
        )

        time.sleep(2)

        # Pattern 2: 2hop（Sensor A のキャッシュクリア）
        self.measure_pattern(
            "/iot/buildingA/room101/2hop",
            "2hop",
            num_iterations,
            cache_clear_nodes=["sensor_a"]
        )

        time.sleep(2)

        # Pattern 3: 3hop（Sensor A/B のキャッシュクリア）
        self.measure_pattern(
            "/iot/buildingA/room101/3hop",
            "3hop",
            num_iterations,
            cache_clear_nodes=["sensor_a", "sensor_b"]
        )

    # ------------------------------------------------------------------
    # 結果保存
    # ------------------------------------------------------------------

    def save_results(self, output_path="measurements.json"):
        """測定結果を JSON ファイルに保存"""
        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(self.results, f, ensure_ascii=False, indent=2)
        print(f"\n[INFO] Results saved to {output_path}")


# ----------------------------------------------------------------------
# エントリポイント
# ----------------------------------------------------------------------

def main():
    num_iterations = 50
    if len(sys.argv) > 1:
        try:
            num_iterations = int(sys.argv[1])
        except ValueError:
            print(f"[WARN] Invalid iteration count '{sys.argv[1]}', using default 50")

    print("=" * 60)
    print("ICSN Performance Measurement with CEFORE")
    print("=" * 60)

    measurement = ICSNMeasurement()
    measurement.run_all_patterns(num_iterations)
    measurement.save_results()


if __name__ == "__main__":
    main()
