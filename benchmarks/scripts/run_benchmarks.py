#!/usr/bin/env python3
"""
TML Benchmark Runner
Compares TML, C++, and Rust performance
"""

import subprocess
import json
import time
import os
import sys
import re
import argparse
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Optional
from datetime import datetime

# Paths
SCRIPT_DIR = Path(__file__).parent
BENCH_DIR = SCRIPT_DIR.parent
RESULTS_DIR = BENCH_DIR / "results"
CPP_DIR = BENCH_DIR / "cpp"
RUST_DIR = BENCH_DIR / "rust"
TML_DIR = BENCH_DIR / "tml"
TML_COMPILER = BENCH_DIR.parent / "packages" / "compiler" / "build" / "Debug" / "tml.exe"

@dataclass
class BenchmarkResult:
    """Result of a single benchmark run"""
    name: str
    language: str
    category: str
    build_time_ms: float = 0.0
    run_time_ms: float = 0.0
    binary_size_bytes: int = 0
    output: str = ""
    success: bool = True
    error: str = ""

@dataclass
class BenchmarkSuite:
    """Collection of benchmark results"""
    timestamp: str = ""
    system_info: dict = field(default_factory=dict)
    results: list = field(default_factory=list)

def get_system_info() -> dict:
    """Get system information"""
    import platform
    return {
        "os": platform.system(),
        "os_version": platform.version(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "python_version": platform.python_version(),
    }

def find_compiler(lang: str) -> Optional[str]:
    """Find compiler for a language"""
    if lang == "cpp":
        # Check common paths first
        cpp_paths = [
            "F:/LLVM/bin/clang++.exe",
            "C:/LLVM/bin/clang++.exe",
            "C:/Program Files/LLVM/bin/clang++.exe",
        ]
        for cc_path in cpp_paths:
            if Path(cc_path).exists():
                return cc_path
        # Then try PATH
        for cc in ["clang++", "g++", "cl"]:
            try:
                subprocess.run([cc, "--version"], capture_output=True, check=True)
                return cc
            except:
                pass
    elif lang == "rust":
        try:
            subprocess.run(["rustc", "--version"], capture_output=True, check=True)
            return "rustc"
        except:
            pass
    elif lang == "tml":
        if TML_COMPILER.exists():
            return str(TML_COMPILER)
    return None

def compile_cpp(source: Path, output: Path) -> tuple[float, bool, str]:
    """Compile C++ source, return (build_time_ms, success, error)"""
    cc = find_compiler("cpp")
    if not cc:
        return 0, False, "C++ compiler not found"

    start = time.perf_counter()
    try:
        result = subprocess.run(
            [cc, "-O2", "-std=c++20", str(source), "-o", str(output)],
            capture_output=True,
            text=True,
            timeout=60
        )
        elapsed = (time.perf_counter() - start) * 1000
        if result.returncode != 0:
            return elapsed, False, result.stderr
        return elapsed, True, ""
    except Exception as e:
        return 0, False, str(e)

def compile_rust(source: Path, output: Path) -> tuple[float, bool, str]:
    """Compile Rust source, return (build_time_ms, success, error)"""
    start = time.perf_counter()
    try:
        result = subprocess.run(
            ["rustc", "-O", str(source), "-o", str(output)],
            capture_output=True,
            text=True,
            timeout=60
        )
        elapsed = (time.perf_counter() - start) * 1000
        if result.returncode != 0:
            return elapsed, False, result.stderr
        return elapsed, True, ""
    except Exception as e:
        return 0, False, str(e)

def compile_tml(source: Path, output: Path) -> tuple[float, bool, str]:
    """Compile TML source, return (build_time_ms, success, error)

    Note: TML compiler outputs to source directory, so we build there
    and then move the exe to the desired output path.
    """
    if not TML_COMPILER.exists():
        return 0, False, f"TML compiler not found at {TML_COMPILER}"

    start = time.perf_counter()
    try:
        result = subprocess.run(
            [str(TML_COMPILER), "build", str(source)],
            capture_output=True,
            text=True,
            timeout=120
        )
        elapsed = (time.perf_counter() - start) * 1000
        if result.returncode != 0:
            return elapsed, False, result.stderr

        # TML outputs the exe next to the source file
        source_exe = source.parent / (source.stem + ".exe")
        if source_exe.exists():
            # Move to desired output location
            import shutil
            output.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(source_exe), str(output))

        return elapsed, True, ""
    except Exception as e:
        return 0, False, str(e)

def run_executable(exe: Path, timeout: int = 60) -> tuple[float, str, bool, str]:
    """Run executable, return (run_time_ms, output, success, error)"""
    start = time.perf_counter()
    try:
        result = subprocess.run(
            [str(exe)],
            capture_output=True,
            text=True,
            timeout=timeout
        )
        elapsed = (time.perf_counter() - start) * 1000
        return elapsed, result.stdout, result.returncode == 0, result.stderr
    except subprocess.TimeoutExpired:
        return timeout * 1000, "", False, "Timeout"
    except Exception as e:
        return 0, "", False, str(e)

def get_binary_size(path: Path) -> int:
    """Get binary size in bytes"""
    if path.exists():
        return path.stat().st_size
    return 0

def run_benchmark(name: str, category: str) -> list[BenchmarkResult]:
    """Run a benchmark for all languages"""
    results = []

    # Prepare output directory
    out_dir = RESULTS_DIR / "bin"
    out_dir.mkdir(parents=True, exist_ok=True)

    # C++
    cpp_source = CPP_DIR / f"bench_{name}.cpp"
    if cpp_source.exists():
        cpp_out = out_dir / f"bench_{name}_cpp.exe"
        build_time, success, error = compile_cpp(cpp_source, cpp_out)

        result = BenchmarkResult(
            name=name,
            language="C++",
            category=category,
            build_time_ms=build_time,
            success=success,
            error=error
        )

        if success:
            run_time, output, run_success, run_error = run_executable(cpp_out)
            result.run_time_ms = run_time
            result.output = output
            result.binary_size_bytes = get_binary_size(cpp_out)
            if not run_success:
                result.success = False
                result.error = run_error

        results.append(result)

    # Rust
    rust_source = RUST_DIR / f"bench_{name}.rs"
    if rust_source.exists():
        rust_out = out_dir / f"bench_{name}_rust.exe"
        build_time, success, error = compile_rust(rust_source, rust_out)

        result = BenchmarkResult(
            name=name,
            language="Rust",
            category=category,
            build_time_ms=build_time,
            success=success,
            error=error
        )

        if success:
            run_time, output, run_success, run_error = run_executable(rust_out)
            result.run_time_ms = run_time
            result.output = output
            result.binary_size_bytes = get_binary_size(rust_out)
            if not run_success:
                result.success = False
                result.error = run_error

        results.append(result)

    # TML
    tml_source = TML_DIR / f"bench_{name}.tml"
    if tml_source.exists():
        tml_out = out_dir / f"bench_{name}_tml.exe"
        build_time, success, error = compile_tml(tml_source, tml_out)

        result = BenchmarkResult(
            name=name,
            language="TML",
            category=category,
            build_time_ms=build_time,
            success=success,
            error=error
        )

        if success:
            run_time, output, run_success, run_error = run_executable(tml_out, timeout=120)
            result.run_time_ms = run_time
            result.output = output
            result.binary_size_bytes = get_binary_size(tml_out)
            if not run_success:
                result.success = False
                result.error = run_error

        results.append(result)

    return results

def discover_benchmarks() -> dict[str, list[str]]:
    """Discover available benchmarks by category"""
    benchmarks = {}

    # Scan TML directory for bench_*.tml files
    for f in TML_DIR.glob("bench_*.tml"):
        name = f.stem.replace("bench_", "")
        # Read category from file
        content = f.read_text()
        category = "other"
        for line in content.split("\n"):
            if "Category:" in line:
                category = line.split("Category:")[1].strip()
                break

        if category not in benchmarks:
            benchmarks[category] = []
        benchmarks[category].append(name)

    return benchmarks

def generate_report(suite: BenchmarkSuite, output_path: Path):
    """Generate markdown report"""
    lines = [
        "# TML Benchmark Report",
        "",
        f"**Generated:** {suite.timestamp}",
        "",
        "## System Information",
        "",
        "| Property | Value |",
        "|----------|-------|",
    ]

    for k, v in suite.system_info.items():
        lines.append(f"| {k} | {v} |")

    lines.extend(["", "## Results Summary", ""])

    # Group by category
    by_category = {}
    for r in suite.results:
        if r.category not in by_category:
            by_category[r.category] = []
        by_category[r.category].append(r)

    for category, results in by_category.items():
        lines.extend([f"### {category.title()}", ""])
        lines.append("| Benchmark | Language | Build (ms) | Run (ms) | Binary (KB) | Status |")
        lines.append("|-----------|----------|------------|----------|-------------|--------|")

        for r in results:
            status = "OK" if r.success else f"FAIL: {r.error[:30]}"
            binary_kb = r.binary_size_bytes / 1024 if r.binary_size_bytes else 0
            lines.append(
                f"| {r.name} | {r.language} | {r.build_time_ms:.1f} | {r.run_time_ms:.1f} | {binary_kb:.1f} | {status} |"
            )
        lines.append("")

    # Comparison charts (text-based)
    lines.extend(["## Performance Comparison", ""])

    # Group by benchmark name
    by_name = {}
    for r in suite.results:
        if r.name not in by_name:
            by_name[r.name] = {}
        by_name[r.name][r.language] = r

    for name, langs in by_name.items():
        lines.append(f"### {name}")
        lines.append("")

        # Build time comparison
        lines.append("**Build Time:**")
        max_build = max(r.build_time_ms for r in langs.values() if r.success) or 1
        for lang, r in sorted(langs.items()):
            if r.success:
                bar_len = int(40 * r.build_time_ms / max_build)
                bar = "#" * bar_len
                lines.append(f"  {lang:6} {bar} {r.build_time_ms:.1f}ms")
        lines.append("")

        # Run time comparison
        lines.append("**Run Time:**")
        max_run = max(r.run_time_ms for r in langs.values() if r.success) or 1
        for lang, r in sorted(langs.items()):
            if r.success:
                bar_len = int(40 * r.run_time_ms / max_run)
                bar = "#" * bar_len
                lines.append(f"  {lang:6} {bar} {r.run_time_ms:.1f}ms")
        lines.append("")

    output_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"Report saved to: {output_path}")

def main():
    parser = argparse.ArgumentParser(description="TML Benchmark Runner")
    parser.add_argument("--category", help="Run specific category only")
    parser.add_argument("--benchmark", help="Run specific benchmark only")
    parser.add_argument("--list", action="store_true", help="List available benchmarks")
    args = parser.parse_args()

    # Ensure results directory exists
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)

    # Discover benchmarks
    benchmarks = discover_benchmarks()

    if args.list:
        print("Available benchmarks:")
        for category, names in benchmarks.items():
            print(f"\n  {category}:")
            for name in names:
                print(f"    - {name}")
        return

    print("=" * 60)
    print("TML Benchmark Suite")
    print("=" * 60)
    print()

    # Check compilers
    print("Checking compilers...")
    cpp_cc = find_compiler("cpp")
    rust_cc = find_compiler("rust")
    tml_cc = find_compiler("tml")

    print(f"  C++:  {cpp_cc or 'NOT FOUND'}")
    print(f"  Rust: {rust_cc or 'NOT FOUND'}")
    print(f"  TML:  {tml_cc or 'NOT FOUND'}")
    print()

    # Build compiler if needed
    if not tml_cc:
        print("Building TML compiler...")
        build_dir = BENCH_DIR.parent / "packages" / "compiler" / "build"
        result = subprocess.run(
            ["cmake", "--build", ".", "--config", "Debug"],
            cwd=build_dir,
            capture_output=True
        )
        if result.returncode == 0:
            tml_cc = find_compiler("tml")
            print(f"  TML:  {tml_cc or 'BUILD FAILED'}")
        print()

    # Run benchmarks
    suite = BenchmarkSuite(
        timestamp=datetime.now().isoformat(),
        system_info=get_system_info()
    )

    for category, names in benchmarks.items():
        if args.category and category != args.category:
            continue

        print(f"Running {category} benchmarks...")
        for name in names:
            if args.benchmark and name != args.benchmark:
                continue

            print(f"  {name}...", end=" ", flush=True)
            results = run_benchmark(name, category)
            suite.results.extend(results)

            # Quick summary
            statuses = [f"{r.language}:{'OK' if r.success else 'FAIL'}" for r in results]
            print(", ".join(statuses))

    print()

    # Save results
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

    # JSON
    json_path = RESULTS_DIR / f"results_{timestamp}.json"
    with open(json_path, "w") as f:
        json.dump(asdict(suite), f, indent=2)
    print(f"JSON saved to: {json_path}")

    # Markdown report
    md_path = RESULTS_DIR / f"report_{timestamp}.md"
    generate_report(suite, md_path)

    # Also save as latest
    (RESULTS_DIR / "latest.json").write_text(json_path.read_text())
    (RESULTS_DIR / "REPORT.md").write_text(md_path.read_text())

    print()
    print("Done!")

if __name__ == "__main__":
    main()
