#!/usr/bin/env python3
"""
TML Benchmark Runner
Compares TML, C++, Go, and Rust performance
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
GO_DIR = BENCH_DIR / "go"
TML_DIR = BENCH_DIR / "tml"
PROJECT_ROOT = BENCH_DIR.parent

# TML compiler paths (try multiple locations)
TML_COMPILER_PATHS = [
    PROJECT_ROOT / "build" / "debug" / "tml.exe",
    PROJECT_ROOT / "build" / "release" / "tml.exe",
    PROJECT_ROOT / "packages" / "compiler" / "build" / "Debug" / "tml.exe",
]

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

def find_tml_compiler() -> Optional[str]:
    """Find TML compiler"""
    for path in TML_COMPILER_PATHS:
        if path.exists():
            return str(path)
    return None

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
    elif lang == "go":
        try:
            subprocess.run(["go", "version"], capture_output=True, check=True)
            return "go"
        except:
            pass
    elif lang == "tml":
        return find_tml_compiler()
    return None

def compile_cpp(source: Path, output: Path) -> tuple[float, bool, str]:
    """Compile C++ source, return (build_time_ms, success, error)"""
    cc = find_compiler("cpp")
    if not cc:
        return 0, False, "C++ compiler not found"

    start = time.perf_counter()
    try:
        # Determine if MSVC (cl.exe) or Clang/GCC
        cc_lower = cc.lower()
        is_msvc = cc_lower.endswith("cl.exe") or cc_lower == "cl"

        if is_msvc:
            # MSVC
            result = subprocess.run(
                [cc, "/O2", "/EHsc", "/Fe:" + str(output), str(source)],
                capture_output=True,
                text=True,
                timeout=60
            )
        else:
            # Clang/GCC
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

def compile_go(source: Path, output: Path) -> tuple[float, bool, str]:
    """Compile Go source, return (build_time_ms, success, error)"""
    start = time.perf_counter()
    try:
        result = subprocess.run(
            ["go", "build", "-o", str(output), str(source)],
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
    """Compile TML source, return (build_time_ms, success, error)"""
    tml_cc = find_tml_compiler()
    if not tml_cc:
        return 0, False, f"TML compiler not found"

    start = time.perf_counter()
    try:
        result = subprocess.run(
            [tml_cc, "build", str(source)],
            capture_output=True,
            text=True,
            timeout=120,
            cwd=PROJECT_ROOT
        )
        elapsed = (time.perf_counter() - start) * 1000
        if result.returncode != 0:
            return elapsed, False, result.stderr + result.stdout

        # TML outputs to build/debug/<name>.exe
        expected_exe = PROJECT_ROOT / "build" / "debug" / (source.stem + ".exe")
        if expected_exe.exists():
            import shutil
            output.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(str(expected_exe), str(output))
            return elapsed, True, ""
        else:
            return elapsed, False, f"Output not found at {expected_exe}"

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

def run_go_benchmark() -> tuple[float, str, bool, str]:
    """Run Go benchmarks using go test"""
    start = time.perf_counter()
    try:
        # First run main.go for correctness
        result = subprocess.run(
            ["go", "run", "."],
            capture_output=True,
            text=True,
            timeout=60,
            cwd=GO_DIR
        )
        output = result.stdout

        # Then run benchmarks
        bench_result = subprocess.run(
            ["go", "test", "-bench=.*", "-benchtime=100ms"],
            capture_output=True,
            text=True,
            timeout=120,
            cwd=GO_DIR
        )
        output += "\n--- Benchmarks ---\n" + bench_result.stdout

        elapsed = (time.perf_counter() - start) * 1000
        return elapsed, output, result.returncode == 0, result.stderr
    except Exception as e:
        return 0, "", False, str(e)

def get_binary_size(path: Path) -> int:
    """Get binary size in bytes"""
    if path.exists():
        return path.stat().st_size
    return 0

def run_all_benchmarks() -> BenchmarkSuite:
    """Run all benchmarks for all languages"""
    suite = BenchmarkSuite(
        timestamp=datetime.now().isoformat(),
        system_info=get_system_info()
    )

    out_dir = RESULTS_DIR / "bin"
    out_dir.mkdir(parents=True, exist_ok=True)

    # TML benchmarks
    print("\n[TML] Running TML benchmarks...")
    print("-" * 50)
    for tml_file in sorted(TML_DIR.glob("*.tml")):
        if tml_file.stem == "tml":  # Skip tml.toml
            continue

        name = tml_file.stem
        print(f"  {name}...", end=" ", flush=True)

        tml_out = out_dir / f"{name}_tml.exe"
        build_time, success, error = compile_tml(tml_file, tml_out)

        result = BenchmarkResult(
            name=name,
            language="TML",
            category="algorithms",
            build_time_ms=build_time,
            success=success,
            error=error
        )

        if success:
            run_time, output, run_success, run_error = run_executable(tml_out)
            result.run_time_ms = run_time
            result.output = output
            result.binary_size_bytes = get_binary_size(tml_out)
            if not run_success:
                result.success = False
                result.error = run_error
            print(f"OK ({build_time:.0f}ms build, {run_time:.0f}ms run)")
        else:
            print(f"FAIL: {error[:50]}")

        suite.results.append(result)

    # C++ benchmark
    print("\n[C++] Running C++ benchmarks...")
    print("-" * 50)
    cpp_source = CPP_DIR / "algorithms.cpp"
    if cpp_source.exists():
        cpp_out = out_dir / "algorithms_cpp.exe"
        print(f"  algorithms...", end=" ", flush=True)
        build_time, success, error = compile_cpp(cpp_source, cpp_out)

        result = BenchmarkResult(
            name="algorithms",
            language="C++",
            category="algorithms",
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
            print(f"OK ({build_time:.0f}ms build, {run_time:.0f}ms run)")
        else:
            print(f"FAIL: {error[:50]}")

        suite.results.append(result)
    else:
        print("  [SKIP] algorithms.cpp not found")

    # Go benchmarks
    print("\n[Go] Running Go benchmarks...")
    print("-" * 50)
    if find_compiler("go"):
        print(f"  algorithms...", end=" ", flush=True)
        run_time, output, success, error = run_go_benchmark()

        result = BenchmarkResult(
            name="algorithms",
            language="Go",
            category="algorithms",
            build_time_ms=0,  # Go compiles on the fly
            run_time_ms=run_time,
            output=output,
            success=success,
            error=error
        )
        if success:
            print(f"OK ({run_time:.0f}ms)")
        else:
            print(f"FAIL: {error[:50]}")

        suite.results.append(result)
    else:
        print("  [SKIP] Go not found")

    # Rust benchmarks
    print("\n[Rust] Running Rust benchmarks...")
    print("-" * 50)
    rust_source = RUST_DIR / "algorithms.rs"
    if rust_source.exists() and find_compiler("rust"):
        rust_out = out_dir / "algorithms_rust.exe"
        print(f"  algorithms...", end=" ", flush=True)
        build_time, success, error = compile_rust(rust_source, rust_out)

        result = BenchmarkResult(
            name="algorithms",
            language="Rust",
            category="algorithms",
            build_time_ms=build_time,
            success=success,
            error=error
        )

        if success:
            run_time, output, run_success, run_error = run_executable(rust_out, timeout=120)
            result.run_time_ms = run_time
            result.output = output
            result.binary_size_bytes = get_binary_size(rust_out)
            if not run_success:
                result.success = False
                result.error = run_error
            print(f"OK ({build_time:.0f}ms build, {run_time:.0f}ms run)")
        else:
            print(f"FAIL: {error[:50]}")

        suite.results.append(result)
    elif not find_compiler("rust"):
        print("  [SKIP] Rust not found")
    else:
        print("  [SKIP] algorithms.rs not found")

    return suite

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

    lines.extend(["", "## Compiler Status", ""])
    lines.append("| Language | Compiler |")
    lines.append("|----------|----------|")
    lines.append(f"| TML | {find_compiler('tml') or 'NOT FOUND'} |")
    lines.append(f"| C++ | {find_compiler('cpp') or 'NOT FOUND'} |")
    lines.append(f"| Go | {find_compiler('go') or 'NOT FOUND'} |")
    lines.append(f"| Rust | {find_compiler('rust') or 'NOT FOUND'} |")

    lines.extend(["", "## Results Summary", ""])

    # Group by language
    by_language = {}
    for r in suite.results:
        if r.language not in by_language:
            by_language[r.language] = []
        by_language[r.language].append(r)

    lines.append("| Language | Benchmark | Build (ms) | Run (ms) | Binary (KB) | Status |")
    lines.append("|----------|-----------|------------|----------|-------------|--------|")

    for lang in ["TML", "C++", "Go", "Rust"]:
        if lang in by_language:
            for r in by_language[lang]:
                status = "✅" if r.success else f"❌ {r.error[:30]}"
                binary_kb = r.binary_size_bytes / 1024 if r.binary_size_bytes else 0
                lines.append(
                    f"| {r.language} | {r.name} | {r.build_time_ms:.1f} | {r.run_time_ms:.1f} | {binary_kb:.1f} | {status} |"
                )

    lines.extend(["", "## Output Comparison", ""])

    for r in suite.results:
        if r.success and r.output.strip():
            lines.append(f"### {r.language} - {r.name}")
            lines.append("```")
            lines.append(r.output.strip()[:1000])  # Limit output
            lines.append("```")
            lines.append("")

    # Performance comparison
    lines.extend(["", "## Performance Comparison", ""])

    # Find common benchmarks
    by_name = {}
    for r in suite.results:
        if r.name not in by_name:
            by_name[r.name] = {}
        by_name[r.name][r.language] = r

    for name, langs in by_name.items():
        if len(langs) < 2:
            continue

        lines.append(f"### {name}")
        lines.append("")

        # Build time comparison
        successful = [(lang, r) for lang, r in langs.items() if r.success]
        if successful:
            lines.append("**Build Time:**")
            max_build = max(r.build_time_ms for _, r in successful) or 1
            for lang, r in sorted(successful):
                bar_len = int(30 * r.build_time_ms / max_build) if max_build > 0 else 0
                bar = "█" * bar_len
                lines.append(f"  {lang:6} {bar} {r.build_time_ms:.1f}ms")
            lines.append("")

            lines.append("**Run Time:**")
            max_run = max(r.run_time_ms for _, r in successful) or 1
            for lang, r in sorted(successful):
                bar_len = int(30 * r.run_time_ms / max_run) if max_run > 0 else 0
                bar = "█" * bar_len
                lines.append(f"  {lang:6} {bar} {r.run_time_ms:.1f}ms")
            lines.append("")

    output_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"\nReport saved to: {output_path}")

def main():
    parser = argparse.ArgumentParser(description="TML Benchmark Runner")
    parser.add_argument("--list", action="store_true", help="List available benchmarks")
    args = parser.parse_args()

    print("=" * 60)
    print("TML Benchmark Suite")
    print("=" * 60)

    # Ensure results directory exists
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)

    if args.list:
        print("\nAvailable benchmarks:")
        print("\nTML:")
        for f in sorted(TML_DIR.glob("*.tml")):
            if f.stem != "tml":
                print(f"  - {f.stem}")
        print("\nC++:")
        for f in sorted(CPP_DIR.glob("*.cpp")):
            print(f"  - {f.stem}")
        print("\nGo:")
        for f in sorted(GO_DIR.glob("*.go")):
            if not f.name.endswith("_test.go"):
                print(f"  - {f.stem}")
        return

    # Check compilers
    print("\nChecking compilers...")
    print(f"  TML:  {find_compiler('tml') or 'NOT FOUND'}")
    print(f"  C++:  {find_compiler('cpp') or 'NOT FOUND'}")
    print(f"  Go:   {find_compiler('go') or 'NOT FOUND'}")
    print(f"  Rust: {find_compiler('rust') or 'NOT FOUND'}")

    # Run benchmarks
    suite = run_all_benchmarks()

    print("\n" + "=" * 60)

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
    (RESULTS_DIR / "latest.json").write_text(json_path.read_text(encoding="utf-8"), encoding="utf-8")
    (RESULTS_DIR / "REPORT.md").write_text(md_path.read_text(encoding="utf-8"), encoding="utf-8")

    print("\nDone!")

if __name__ == "__main__":
    main()
