#!/usr/bin/env python3
"""
TML vs C++ Profile Benchmark Runner

Runs all profile benchmarks (TML + C++), parses per-op timing,
and generates a comprehensive markdown comparison report.

Usage:
    python benchmarks/scripts/run_profile_bench.py
    python benchmarks/scripts/run_profile_bench.py --tml-only
    python benchmarks/scripts/run_profile_bench.py --cpp-only
    python benchmarks/scripts/run_profile_bench.py --skip-build
"""

import subprocess
import sys
import os
import re
import platform
import argparse
from pathlib import Path
from datetime import datetime
from dataclasses import dataclass

# ── Paths ──────────────────────────────────────────────────────────
SCRIPT_DIR = Path(__file__).parent
BENCH_DIR = SCRIPT_DIR.parent
PROJECT_ROOT = BENCH_DIR.parent
TML_DIR = BENCH_DIR / "profile_tml"
CPP_DIR = BENCH_DIR / "profile_cpp"
CPP_BUILD_DIR = CPP_DIR / "build"
CPP_BIN_DIR = BENCH_DIR / "results" / "bin"
RESULTS_DIR = BENCH_DIR / "profile_results"
TML_EXE = PROJECT_ROOT / "build" / "debug" / "tml.exe"

# ── Benchmark suites (matched pairs) ──────────────────────────────
BENCH_SUITES = [
    "math",
    "string",
    "collections",
    "hashmap",
    "list",
    "control_flow",
    "closure",
    "function",
    "memory",
    "text",
    "oop",
    "type",
    "encoding",
    "crypto",
    "json",
]

# ── Data structures ───────────────────────────────────────────────
@dataclass
class Metric:
    name: str
    iterations: int
    total_ms: int
    per_op_ns: int
    ops_sec: int
    notes: str = ""

@dataclass
class SuiteResult:
    suite: str
    language: str
    metrics: list
    success: bool = True
    error: str = ""


# ── Output parser ─────────────────────────────────────────────────
def parse_bench_output(text: str) -> list[Metric]:
    """Parse benchmark output lines into Metric objects.

    Expected format per benchmark:
      Name:
        Iterations: 100000
        Total time: 38 ms
        Per op:     384 ns
        Ops/sec:    2600016
        Notes:      optional
    """
    metrics = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].strip()

        # Detect a benchmark name line: "  Name:" (not a key: value)
        # It ends with ":" but doesn't match "Key: Value" pattern
        if line.endswith(":") and not any(
            line.startswith(k) for k in ("Iterations:", "Total time:", "Per op:", "Ops/sec:", "Notes:")
        ):
            name = line.rstrip(":")
            iterations = 0
            total_ms = 0
            per_op_ns = 0
            ops_sec = 0
            notes = ""

            # Read subsequent key-value lines
            j = i + 1
            while j < len(lines):
                kv = lines[j].strip()
                if not kv or (
                    kv.endswith(":")
                    and not any(kv.startswith(k) for k in ("Iterations:", "Total time:", "Per op:", "Ops/sec:", "Notes:"))
                ):
                    break  # Next benchmark or blank

                m_iter = re.match(r"Iterations:\s*(\d+)", kv)
                m_total = re.match(r"Total time:\s*(\d+)\s*ms", kv)
                m_perop = re.match(r"Per op:\s*(\d+)\s*ns", kv)
                m_ops = re.match(r"Ops/sec:\s*(\d+)", kv)
                m_notes = re.match(r"Notes:\s*(.+)", kv)
                # Alternate formats: "N ns/op", "N ops/sec"
                m_perop2 = re.match(r"(\d+)\s*ns/op", kv)
                m_ops2 = re.match(r"(\d+)\s*ops/sec", kv)

                if m_iter:
                    iterations = int(m_iter.group(1))
                elif m_total:
                    total_ms = int(m_total.group(1))
                elif m_perop:
                    per_op_ns = int(m_perop.group(1))
                elif m_perop2:
                    per_op_ns = int(m_perop2.group(1))
                elif m_ops:
                    ops_sec = int(m_ops.group(1))
                elif m_ops2:
                    ops_sec = int(m_ops2.group(1))
                elif m_notes:
                    notes = m_notes.group(1).strip()
                elif kv.startswith("best_ns=") or kv.startswith("[DEBUG]"):
                    pass  # Skip debug lines
                j += 1

            metrics.append(Metric(name, iterations, total_ms, per_op_ns, ops_sec, notes))
            i = j
            continue
        i += 1

    return metrics


# ── Runners ───────────────────────────────────────────────────────
def run_cpp_bench(suite: str) -> SuiteResult:
    exe = CPP_BIN_DIR / f"{suite}_bench.exe"
    if not exe.exists():
        return SuiteResult(suite, "C++", [], False, f"{exe.name} not found")

    try:
        r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=300)
        if r.returncode != 0:
            return SuiteResult(suite, "C++", [], False, r.stderr[:200])
        metrics = parse_bench_output(r.stdout)
        return SuiteResult(suite, "C++", metrics)
    except subprocess.TimeoutExpired:
        return SuiteResult(suite, "C++", [], False, "Timeout (300s)")
    except Exception as e:
        return SuiteResult(suite, "C++", [], False, str(e))


def run_tml_bench(suite: str) -> SuiteResult:
    tml_file = TML_DIR / f"{suite}_bench.tml"
    if not tml_file.exists():
        return SuiteResult(suite, "TML", [], False, f"{tml_file.name} not found")

    if not TML_EXE.exists():
        return SuiteResult(suite, "TML", [], False, "tml.exe not found")

    try:
        r = subprocess.run(
            [str(TML_EXE), "run", str(tml_file), "--release"],
            capture_output=True, text=True, timeout=300,
            cwd=str(PROJECT_ROOT),
        )
        if r.returncode != 0:
            return SuiteResult(suite, "TML", [], False, (r.stderr + r.stdout)[:200])
        metrics = parse_bench_output(r.stdout)
        return SuiteResult(suite, "TML", metrics)
    except subprocess.TimeoutExpired:
        return SuiteResult(suite, "TML", [], False, "Timeout (300s)")
    except Exception as e:
        return SuiteResult(suite, "TML", [], False, str(e))


# ── C++ build ─────────────────────────────────────────────────────
def build_cpp_benchmarks() -> bool:
    print("  Building C++ benchmarks (MSVC /O2)...")
    CPP_BUILD_DIR.mkdir(parents=True, exist_ok=True)

    # Configure
    r = subprocess.run(
        ["cmake", "-G", "Visual Studio 17 2022", "-A", "x64", ".."],
        capture_output=True, text=True, cwd=str(CPP_BUILD_DIR), timeout=60,
    )
    if r.returncode != 0:
        print(f"    CMake configure FAILED: {r.stderr[:200]}")
        return False

    # Build all targets except json_bench (broken)
    targets = [
        "math_bench", "string_bench", "control_flow_bench", "closure_bench",
        "function_bench", "collections_bench", "memory_bench", "text_bench",
        "oop_bench", "encoding_bench", "type_bench", "crypto_bench",
    ]
    target_args = []
    for t in targets:
        target_args.extend(["--target", t])

    r = subprocess.run(
        ["cmake", "--build", ".", "--config", "Release"] + target_args,
        capture_output=True, text=True, cwd=str(CPP_BUILD_DIR), timeout=300,
    )
    if r.returncode != 0:
        # Partial failure is OK — some targets might have issues
        print(f"    CMake build warnings (partial): {r.stderr[:200]}")
    print("  C++ build complete.")
    return True


# ── Report generator ──────────────────────────────────────────────
def verdict(cpp_ns: int, tml_ns: int) -> str:
    if cpp_ns == 0 and tml_ns == 0:
        return "~tied"
    if cpp_ns == 0:
        cpp_ns = 1  # avoid div/0, means "sub-nanosecond"
    if tml_ns == 0:
        tml_ns = 1

    ratio = tml_ns / cpp_ns
    if ratio <= 0.80:
        return f"**TML {1/ratio:.1f}x faster**"
    elif ratio <= 1.20:
        return "~tied"
    elif ratio <= 2.0:
        return f"C++ {ratio:.1f}x"
    else:
        return f"C++ {ratio:.0f}x"


def generate_report(
    cpp_results: dict[str, SuiteResult],
    tml_results: dict[str, SuiteResult],
    elapsed_s: float,
) -> str:
    now = datetime.now()
    lines = []

    lines.append("# TML vs C++ Profile Benchmark Report")
    lines.append("")
    lines.append(f"**Date:** {now.strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append(f"**Platform:** {platform.system()} {platform.release()} ({platform.machine()})")
    lines.append(f"**C++ Compiler:** MSVC /O2 (Visual Studio 2022)")
    lines.append(f"**TML Compiler:** tml.exe --release (LLVM -O3)")
    lines.append(f"**Total runtime:** {elapsed_s:.1f}s")
    lines.append("")
    lines.append("---")
    lines.append("")

    # ── Per-suite tables ──────────────────────────────────────────
    total_wins = {"TML": 0, "C++": 0, "tied": 0}

    for suite in BENCH_SUITES:
        cpp = cpp_results.get(suite)
        tml = tml_results.get(suite)

        if not cpp or not tml:
            continue
        if not cpp.success and not tml.success:
            continue

        suite_title = suite.replace("_", " ").title()
        lines.append(f"## {suite_title}")
        lines.append("")

        if not cpp.success:
            lines.append(f"> C++ benchmark failed: {cpp.error}")
            lines.append("")
            # Show TML-only results
            if tml.success and tml.metrics:
                lines.append("| Operation | TML (ns/op) | TML Ops/sec |")
                lines.append("|---|---|---|")
                for m in tml.metrics:
                    lines.append(f"| {m.name} | {m.per_op_ns} | {m.ops_sec:,} |")
                lines.append("")
            continue

        if not tml.success:
            lines.append(f"> TML benchmark failed: {tml.error}")
            lines.append("")
            if cpp.success and cpp.metrics:
                lines.append("| Operation | C++ (ns/op) | C++ Ops/sec |")
                lines.append("|---|---|---|")
                for m in cpp.metrics:
                    lines.append(f"| {m.name} | {m.per_op_ns} | {m.ops_sec:,} |")
                lines.append("")
            continue

        # Both succeeded — build comparison table
        # Try to match metrics by name (fuzzy: lowercase, strip whitespace)
        def norm(s):
            return re.sub(r"\s+", " ", s.strip().lower())

        cpp_by_name = {norm(m.name): m for m in cpp.metrics}
        tml_by_name = {norm(m.name): m for m in tml.metrics}

        # Pair up: exact match first, then positional fallback
        all_names = list(dict.fromkeys(
            [norm(m.name) for m in cpp.metrics] + [norm(m.name) for m in tml.metrics]
        ))

        has_pairs = False
        table_rows = []
        for n in all_names:
            cm = cpp_by_name.get(n)
            tm = tml_by_name.get(n)
            display_name = cm.name if cm else (tm.name if tm else n)

            c_ns = cm.per_op_ns if cm else "-"
            t_ns = tm.per_op_ns if tm else "-"

            if cm and tm:
                has_pairs = True
                v = verdict(cm.per_op_ns, tm.per_op_ns)
                ratio = tm.per_op_ns / cm.per_op_ns if cm.per_op_ns > 0 else 0
                if "TML" in v and "faster" in v:
                    total_wins["TML"] += 1
                elif "tied" in v:
                    total_wins["tied"] += 1
                else:
                    total_wins["C++"] += 1
                table_rows.append(f"| {display_name} | {c_ns} | {t_ns} | {v} |")
            elif cm:
                table_rows.append(f"| {display_name} | {c_ns} | - | C++ only |")
            else:
                table_rows.append(f"| {display_name} | - | {t_ns} | TML only |")

        # If there are no matched pairs, show side by side anyway
        if not has_pairs and len(cpp.metrics) == len(tml.metrics):
            table_rows = []
            for cm, tm in zip(cpp.metrics, tml.metrics):
                display_name = cm.name
                v = verdict(cm.per_op_ns, tm.per_op_ns)
                ratio_val = tm.per_op_ns / cm.per_op_ns if cm.per_op_ns > 0 else 0
                if "TML" in v and "faster" in v:
                    total_wins["TML"] += 1
                elif "tied" in v:
                    total_wins["tied"] += 1
                else:
                    total_wins["C++"] += 1
                table_rows.append(
                    f"| {cm.name} / {tm.name} | {cm.per_op_ns} | {tm.per_op_ns} | {v} |"
                )

        lines.append("| Operation | C++ (ns/op) | TML (ns/op) | Verdict |")
        lines.append("|---|---:|---:|---|")
        lines.extend(table_rows)
        lines.append("")

    # ── Summary ───────────────────────────────────────────────────
    lines.append("---")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    total = total_wins["TML"] + total_wins["C++"] + total_wins["tied"]
    lines.append(f"| | Count | % |")
    lines.append(f"|---|---:|---:|")
    if total > 0:
        lines.append(f"| **TML faster** | {total_wins['TML']} | {100*total_wins['TML']/total:.0f}% |")
        lines.append(f"| **Tied** (< 1.2x) | {total_wins['tied']} | {100*total_wins['tied']/total:.0f}% |")
        lines.append(f"| **C++ faster** | {total_wins['C++']} | {100*total_wins['C++']/total:.0f}% |")
        lines.append(f"| **Total comparisons** | {total} | 100% |")
    lines.append("")

    # ── Errors section ────────────────────────────────────────────
    errors = []
    for suite in BENCH_SUITES:
        for lang, results in [("C++", cpp_results), ("TML", tml_results)]:
            r = results.get(suite)
            if r and not r.success:
                errors.append(f"- **{lang}/{suite}**: {r.error}")
    if errors:
        lines.append("## Errors")
        lines.append("")
        lines.extend(errors)
        lines.append("")

    return "\n".join(lines)


# ── Main ──────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="TML vs C++ Profile Benchmark Runner")
    parser.add_argument("--tml-only", action="store_true", help="Run TML benchmarks only")
    parser.add_argument("--cpp-only", action="store_true", help="Run C++ benchmarks only")
    parser.add_argument("--skip-build", action="store_true", help="Skip C++ cmake build step")
    parser.add_argument("--suites", nargs="+", help="Run specific suites only", choices=BENCH_SUITES)
    args = parser.parse_args()

    suites = args.suites or BENCH_SUITES
    run_cpp = not args.tml_only
    run_tml = not args.cpp_only

    start_time = datetime.now()

    print("=" * 64)
    print("  TML vs C++ Profile Benchmark Runner")
    print(f"  {start_time.strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 64)
    print()

    # ── Build C++ ─────────────────────────────────────────────────
    if run_cpp and not args.skip_build:
        build_cpp_benchmarks()
        print()

    # ── Run benchmarks ────────────────────────────────────────────
    cpp_results: dict[str, SuiteResult] = {}
    tml_results: dict[str, SuiteResult] = {}

    for suite in suites:
        suite_title = suite.replace("_", " ").title()

        if run_cpp:
            print(f"  [{suite_title}] C++ ...", end=" ", flush=True)
            cpp_results[suite] = run_cpp_bench(suite)
            r = cpp_results[suite]
            if r.success:
                print(f"OK ({len(r.metrics)} metrics)")
            else:
                print(f"FAIL: {r.error[:60]}")

        if run_tml:
            print(f"  [{suite_title}] TML ...", end=" ", flush=True)
            tml_results[suite] = run_tml_bench(suite)
            r = tml_results[suite]
            if r.success:
                print(f"OK ({len(r.metrics)} metrics)")
            else:
                print(f"FAIL: {r.error[:60]}")

    elapsed = (datetime.now() - start_time).total_seconds()
    print()
    print(f"  All benchmarks completed in {elapsed:.1f}s")
    print()

    # ── Generate report ───────────────────────────────────────────
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    timestamp = start_time.strftime("%Y-%m-%d_%H%M%S")
    report_file = RESULTS_DIR / f"benchmark_{timestamp}.md"

    report = generate_report(cpp_results, tml_results, elapsed)
    report_file.write_text(report, encoding="utf-8")

    # Also save as latest
    latest = RESULTS_DIR / "latest.md"
    latest.write_text(report, encoding="utf-8")

    print(f"  Report saved: {report_file.name}")
    print(f"  Latest link:  {latest.name}")
    print()

    # ── Print quick summary to stdout ─────────────────────────────
    # Find summary section and print it
    for line in report.splitlines():
        if line.startswith("## Summary") or line.startswith("| **"):
            print(f"  {line}")
    print()


if __name__ == "__main__":
    main()
