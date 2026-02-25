#!/usr/bin/env python3
"""
TCP Async Benchmark Runner
Compiles and runs async TCP socket bind benchmarks across TML, Rust, Go, Node.js, Python
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
TML_EXE = PROJECT_ROOT / "build" / "debug" / "tml.exe"

@dataclass
class BenchResult:
    language: str
    mode: str
    per_op_ns: int = 0
    ops_sec: int = 0
    success: bool = False
    error: str = ""

def parse_bench_output(text: str) -> dict:
    """Parse benchmark output for per_op_ns and ops_sec"""
    result = {
        "per_op_ns": 0,
        "ops_sec": 0,
    }

    # Pattern: "Per op:     12345 ns"
    m = re.search(r"Per op:\s*(\d+)\s*ns", text)
    if m:
        result["per_op_ns"] = int(m.group(1))

    # Pattern: "Ops/sec:    12345"
    m = re.search(r"Ops/sec:\s*(\d+)", text)
    if m:
        result["ops_sec"] = int(m.group(1))

    return result

def run_tml_async() -> BenchResult:
    """Run TML async benchmark"""
    print("  [TML] Running async benchmark...")
    try:
        tml_file = PROFILE_TML_DIR / "tcp_bench.tml"
        if not tml_file.exists():
            return BenchResult("TML", "Async", success=False, error="tcp_bench.tml not found")

        r = subprocess.run(
            [str(TML_EXE), "run", str(tml_file), "--release"],
            capture_output=True, text=True, timeout=30, cwd=str(PROJECT_ROOT)
        )

        if r.returncode != 0:
            return BenchResult("TML", "Async", success=False, error=r.stderr[:100])

        data = parse_bench_output(r.stdout)
        return BenchResult("TML", "Async", data["per_op_ns"], data["ops_sec"], success=True)
    except Exception as e:
        return BenchResult("TML", "Async", success=False, error=str(e))

def run_rust_async() -> BenchResult:
    """Run Rust async benchmark"""
    print("  [Rust] Building and running async benchmark (requires tokio)...")
    try:
        # Check if Cargo.toml exists in rust dir
        cargo_file = RUST_DIR / "Cargo.toml"
        if not cargo_file.exists():
            return BenchResult("Rust", "Async", success=False, error="Cargo.toml not found")

        # Note: Would need to add tokio dependency. For now, skip if not available
        rs_file = RUST_DIR / "tcp_async_bench.rs"
        if not rs_file.exists():
            return BenchResult("Rust", "Async", success=False, error="tcp_async_bench.rs not found")

        return BenchResult("Rust", "Async", success=False, error="Requires tokio (not installed)")
    except Exception as e:
        return BenchResult("Rust", "Async", success=False, error=str(e))

def run_go_async() -> BenchResult:
    """Run Go async benchmark"""
    print("  [Go] Running async benchmark...")
    try:
        go_file = GO_DIR / "tcp_async_bench.go"
        if not go_file.exists():
            return BenchResult("Go", "Async", success=False, error="tcp_async_bench.go not found")

        r = subprocess.run(
            ["go", "run", str(go_file)],
            capture_output=True, text=True, timeout=30, cwd=str(GO_DIR)
        )

        if r.returncode != 0:
            return BenchResult("Go", "Async", success=False, error=r.stderr[:100])

        data = parse_bench_output(r.stdout)
        return BenchResult("Go", "Async", data["per_op_ns"], data["ops_sec"], success=True)
    except Exception as e:
        return BenchResult("Go", "Async", success=False, error=str(e))

def run_nodejs_async() -> BenchResult:
    """Run Node.js async benchmark"""
    print("  [Node.js] Running async benchmark...")
    try:
        js_file = NODE_DIR / "tcp_async_bench.js"
        if not js_file.exists():
            return BenchResult("Node.js", "Async", success=False, error="tcp_async_bench.js not found")

        r = subprocess.run(
            ["node", str(js_file)],
            capture_output=True, text=True, timeout=30
        )

        if r.returncode != 0:
            return BenchResult("Node.js", "Async", success=False, error=r.stderr[:100])

        data = parse_bench_output(r.stdout)
        return BenchResult("Node.js", "Async", data["per_op_ns"], data["ops_sec"], success=True)
    except Exception as e:
        return BenchResult("Node.js", "Async", success=False, error=str(e))

def run_python_async() -> BenchResult:
    """Run Python async benchmark"""
    print("  [Python] Running async benchmark...")
    try:
        py_file = PYTHON_DIR / "tcp_async_bench.py"
        if not py_file.exists():
            return BenchResult("Python", "Async", success=False, error="tcp_async_bench.py not found")

        r = subprocess.run(
            ["python3", str(py_file)],
            capture_output=True, text=True, timeout=30
        )

        if r.returncode != 0:
            return BenchResult("Python", "Async", success=False, error=r.stderr[:100])

        data = parse_bench_output(r.stdout)
        return BenchResult("Python", "Async", data["per_op_ns"], data["ops_sec"], success=True)
    except Exception as e:
        return BenchResult("Python", "Async", success=False, error=str(e))

def main():
    print("\n" + "="*70)
    print("  TCP Async Benchmark Suite - Cross-Language Comparison")
    print("="*70 + "\n")

    print(f"Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Platform: Windows 10 (AMD64)")
    print(f"Test: 50 iterations of async TCP bind(127.0.0.1:0)\n")

    print("Running benchmarks...\n")

    results = []
    results.append(run_tml_async())
    results.append(run_python_async())
    results.append(run_go_async())
    results.append(run_nodejs_async())
    results.append(run_rust_async())

    # Print results table
    print("\n" + "="*70)
    print("  RESULTS")
    print("="*70 + "\n")

    print("| Language | Mode | Per Op (ns) | Ops/sec | Status |")
    print("|---|---|---:|---:|---|")

    successful = [r for r in results if r.success]
    successful.sort(key=lambda x: x.per_op_ns)

    for i, r in enumerate(successful):
        rank = "[FASTEST]" if i == 0 else ""
        print(f"| {r.language:10} | {r.mode:7} | {r.per_op_ns:11,} | {r.ops_sec:7,} | {rank} |")

    print("\nFailed:")
    for r in results:
        if not r.success:
            print(f"| {r.language:10} | {r.mode:7} | ERROR: {r.error} |")

    print("\n" + "="*70)

    # Summary
    if successful:
        fastest = successful[0]
        slowest = successful[-1]
        ratio = slowest.per_op_ns / fastest.per_op_ns
        print(f"\nFastest:  {fastest.language} ({fastest.per_op_ns:,} ns)")
        print(f"Slowest:  {slowest.language} ({slowest.per_op_ns:,} ns)")
        print(f"Ratio:    {ratio:.1f}x")

    print("\n")

if __name__ == "__main__":
    main()
