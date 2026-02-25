#!/usr/bin/env python3
"""
JSON Benchmark Runner - Cross-Language Comparison
Compiles and runs JSON parsing benchmarks across TML, Rust, Go, Node.js, Python, C++
Reports results in comparison table
"""

import subprocess
import sys
import os
import re
from pathlib import Path
from datetime import datetime
from dataclasses import dataclass

# ── Paths ──────────────────────────────────────────────────────────
SCRIPT_DIR = Path(__file__).parent
BENCH_DIR = SCRIPT_DIR.parent
PROJECT_ROOT = BENCH_DIR.parent
PROFILE_TML_DIR = BENCH_DIR / "profile_tml"
RUST_DIR = BENCH_DIR / "rust"
GO_DIR = BENCH_DIR / "go"
NODE_DIR = BENCH_DIR / "node"
PYTHON_DIR = BENCH_DIR / "python"
CPP_DIR = BENCH_DIR / "cpp"
TML_EXE = PROJECT_ROOT / "build" / "debug" / "tml.exe"

@dataclass
class BenchResult:
    language: str
    test: str
    time_us: float = 0.0
    throughput_mbps: float = 0.0
    success: bool = False
    error: str = ""

def parse_bench_output(text: str, language: str) -> list:
    """Parse benchmark output for timing and throughput data"""
    results = []

    if language == "TML":
        # TML format: "Per op:     XXX ns" and "Ops/sec:    XXXXX"
        lines = text.split('\n')
        current_test = None
        for line in lines:
            if line.startswith('  '):
                # Test name line
                if ':' in line:
                    current_test = line.strip().rstrip(':').strip()
            elif 'Per op:' in line and current_test:
                m = re.search(r'Per op:\s*(\d+)\s*ns', line)
                if m:
                    time_ns = int(m.group(1))
                    results.append(BenchResult(
                        language="TML",
                        test=current_test,
                        time_us=time_ns / 1000.0,
                        success=True
                    ))

    elif language in ["Python", "Node.js", "Go"]:
        # Format: "Test Name"  XXXX.XX us  XXXX iters  XXXX.XX MB/s
        for line in text.split('\n'):
            # Skip separator and header lines
            if not line.strip() or '---' in line or 'Test' in line:
                continue

            # Try to parse data line
            parts = line.split()
            if len(parts) >= 3:
                try:
                    # Find the time value (ends with "us")
                    time_idx = None
                    for i, part in enumerate(parts):
                        if 'us' in part:
                            time_idx = i
                            break

                    if time_idx:
                        # Extract test name (everything before the time)
                        test_name = ' '.join(parts[:time_idx]).strip()
                        time_us = float(parts[time_idx].replace('us', '').strip())

                        # Try to find throughput
                        throughput = 0.0
                        if len(parts) > time_idx + 2:
                            for j in range(time_idx + 1, len(parts)):
                                if 'MB/s' in parts[j]:
                                    throughput = float(parts[j].replace('MB/s', '').strip())
                                    break

                        results.append(BenchResult(
                            language=language,
                            test=test_name,
                            time_us=time_us,
                            throughput_mbps=throughput,
                            success=True
                        ))
                except (ValueError, IndexError):
                    continue

    elif language == "Rust":
        # Rust format varies, look for timing patterns
        for line in text.split('\n'):
            if 'ns' in line or 'us' in line or 'ms' in line:
                # Try to extract test name and time
                parts = re.split(r'\s+', line.strip())
                if len(parts) >= 2:
                    try:
                        # Find time value
                        for i, part in enumerate(parts):
                            if 'ns' in part or 'us' in part or 'ms' in part:
                                test_name = ' '.join(parts[:i]).strip()
                                time_str = part.replace('ns', '').replace('us', '').replace('ms', '')
                                time_val = float(time_str)

                                # Convert to microseconds
                                if 'ns' in part:
                                    time_us = time_val / 1000
                                elif 'ms' in part:
                                    time_us = time_val * 1000
                                else:
                                    time_us = time_val

                                results.append(BenchResult(
                                    language="Rust",
                                    test=test_name,
                                    time_us=time_us,
                                    success=True
                                ))
                                break
                    except (ValueError, IndexError):
                        continue

    return results

def run_tml_json() -> list:
    """Run TML JSON benchmark"""
    print("  [TML] Running JSON benchmark...")
    try:
        tml_file = PROFILE_TML_DIR / "json_bench.tml"
        if not tml_file.exists():
            return [BenchResult("TML", "Error", success=False, error="json_bench.tml not found")]

        r = subprocess.run(
            [str(TML_EXE), "run", str(tml_file), "--release"],
            capture_output=True, text=True, timeout=60, cwd=str(PROJECT_ROOT)
        )

        if r.returncode != 0:
            return [BenchResult("TML", "Error", success=False, error=r.stderr[:100])]

        return parse_bench_output(r.stdout, "TML")
    except Exception as e:
        return [BenchResult("TML", "Error", success=False, error=str(e))]

def run_python_json() -> list:
    """Run Python JSON benchmark"""
    print("  [Python] Running JSON benchmark...")
    try:
        py_file = PYTHON_DIR / "json_bench.py"
        if not py_file.exists():
            return [BenchResult("Python", "Error", success=False, error="json_bench.py not found")]

        r = subprocess.run(
            ["python3", str(py_file)],
            capture_output=True, text=True, timeout=60, cwd=str(PYTHON_DIR)
        )

        if r.returncode != 0:
            return [BenchResult("Python", "Error", success=False, error=r.stderr[:100])]

        return parse_bench_output(r.stdout, "Python")
    except Exception as e:
        return [BenchResult("Python", "Error", success=False, error=str(e))]

def run_nodejs_json() -> list:
    """Run Node.js JSON benchmark"""
    print("  [Node.js] Running JSON benchmark...")
    try:
        js_file = NODE_DIR / "json_bench.js"
        if not js_file.exists():
            return [BenchResult("Node.js", "Error", success=False, error="json_bench.js not found")]

        r = subprocess.run(
            ["node", str(js_file)],
            capture_output=True, text=True, timeout=60, cwd=str(NODE_DIR)
        )

        if r.returncode != 0:
            return [BenchResult("Node.js", "Error", success=False, error=r.stderr[:100])]

        return parse_bench_output(r.stdout, "Node.js")
    except Exception as e:
        return [BenchResult("Node.js", "Error", success=False, error=str(e))]

def run_go_json() -> list:
    """Run Go JSON benchmark"""
    print("  [Go] Running JSON benchmark...")
    try:
        go_file = GO_DIR / "json_bench.go"
        if not go_file.exists():
            return [BenchResult("Go", "Error", success=False, error="json_bench.go not found")]

        r = subprocess.run(
            ["go", "run", str(go_file)],
            capture_output=True, text=True, timeout=60, cwd=str(GO_DIR)
        )

        if r.returncode != 0:
            return [BenchResult("Go", "Error", success=False, error=r.stderr[:100])]

        return parse_bench_output(r.stdout, "Go")
    except Exception as e:
        return [BenchResult("Go", "Error", success=False, error=str(e))]

def run_rust_json() -> list:
    """Run Rust JSON benchmark"""
    print("  [Rust] Running JSON benchmark...")
    try:
        # Check for cargo project
        cargo_file = RUST_DIR / "json_bench" / "Cargo.toml"
        if not cargo_file.exists():
            return [BenchResult("Rust", "Error", success=False, error="Cargo.toml not found")]

        r = subprocess.run(
            ["cargo", "run", "--release"],
            capture_output=True, text=True, timeout=120, cwd=str(RUST_DIR / "json_bench")
        )

        if r.returncode != 0:
            return [BenchResult("Rust", "Error", success=False, error=r.stderr[:100])]

        return parse_bench_output(r.stdout, "Rust")
    except Exception as e:
        return [BenchResult("Rust", "Error", success=False, error=str(e))]

def run_cpp_json() -> list:
    """Run C++ JSON benchmark"""
    print("  [C++] Running JSON benchmark...")
    try:
        cpp_exe = CPP_DIR / "json_bench.exe"
        if not cpp_exe.exists():
            return [BenchResult("C++", "Error", success=False, error="json_bench.exe not found")]

        r = subprocess.run(
            [str(cpp_exe)],
            capture_output=True, text=True, timeout=60
        )

        if r.returncode != 0:
            return [BenchResult("C++", "Error", success=False, error=r.stderr[:100])]

        return parse_bench_output(r.stdout, "C++")
    except Exception as e:
        return [BenchResult("C++", "Error", success=False, error=str(e))]

def main():
    print("\n" + "="*80)
    print("  JSON Benchmark Suite - Cross-Language Comparison")
    print("="*80 + "\n")

    print(f"Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Platform: Windows 10 (AMD64)")
    print(f"Test: JSON parsing and serialization across 6 languages\n")

    print("Running benchmarks...\n")

    results = []
    results.extend(run_tml_json())
    results.extend(run_python_json())
    results.extend(run_go_json())
    results.extend(run_nodejs_json())
    results.extend(run_rust_json())
    results.extend(run_cpp_json())

    # Print results table
    print("\n" + "="*80)
    print("  RESULTS")
    print("="*80 + "\n")

    # Group by test
    tests_dict = {}
    for r in results:
        if r.test not in tests_dict:
            tests_dict[r.test] = []
        tests_dict[r.test].append(r)

    for test_name in sorted(tests_dict.keys()):
        test_results = tests_dict[test_name]
        successful = [r for r in test_results if r.success]

        if not successful:
            print(f"\n{test_name}:")
            for r in test_results:
                if not r.success:
                    print(f"  {r.language}: ERROR - {r.error}")
            continue

        # Sort by time
        successful.sort(key=lambda x: x.time_us)

        print(f"\n{test_name}:")
        print(f"{'Language':<15} {'Time (us)':<15} {'Throughput (MB/s)':<20} {'Status':<15}")
        print("-" * 70)

        for i, r in enumerate(successful):
            rank = " [FASTEST]" if i == 0 else ""
            throughput_str = f"{r.throughput_mbps:.2f}" if r.throughput_mbps > 0 else "N/A"
            print(f"{r.language:<15} {r.time_us:>12.2f} us {throughput_str:>17} {rank}")

    print("\n" + "="*80)
    print(f"\nTotal benchmarks: {len(results)}")
    print(f"Successful: {len([r for r in results if r.success])}")
    print(f"Failed: {len([r for r in results if not r.success])}")

    print("\n")

if __name__ == "__main__":
    main()
