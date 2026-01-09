#!/usr/bin/env python3
"""
TML Optimization Benchmark Runner

Compiles the same TML code at different optimization levels and compares:
- LLVM IR instruction counts (same metric for TML and Rust)
- Function counts
- Binary size

Usage:
    python run_optimization_bench.py
    python run_optimization_bench.py --verbose
    python run_optimization_bench.py --output report.md
    python run_optimization_bench.py --with-rust
"""

import subprocess
import os
import sys
import re
import json
from pathlib import Path
from datetime import datetime
import argparse
import shutil

# Paths
SCRIPT_DIR = Path(__file__).parent
BENCHMARKS_DIR = SCRIPT_DIR.parent
PROJECT_ROOT = BENCHMARKS_DIR.parent
BUILD_DIR = PROJECT_ROOT / "build" / "debug"
TML_COMPILER = BUILD_DIR / "tml.exe"
RESULTS_DIR = BENCHMARKS_DIR / "results"
RUST_DIR = BENCHMARKS_DIR / "rust"

# Optimization levels to test
OPT_LEVELS = ["O0", "O1", "O2", "O3"]

# Benchmark files
BENCH_FILES = [
    BENCHMARKS_DIR / "tml" / "optimization_bench.tml",
    BENCHMARKS_DIR / "tml" / "algorithms.tml",
]

# Rust equivalent files (for comparison)
RUST_BENCH_FILES = {
    "optimization_bench.tml": RUST_DIR / "optimization_bench.rs",
}


def count_llvm_ir_instructions(llvm_ir: str) -> dict:
    """Count instructions in LLVM IR output."""
    stats = {
        "functions": 0,
        "blocks": 0,
        "instructions": 0,
        "allocas": 0,
        "loads": 0,
        "stores": 0,
        "calls": 0,
        "branches": 0,
        "rets": 0,
        "arithmetic": 0,
        "comparisons": 0,
        "phis": 0,
    }

    in_function = False
    for line in llvm_ir.split('\n'):
        line = line.strip()

        # Skip comments, metadata, and declarations
        if line.startswith(';') or line.startswith('!') or line.startswith('source_filename'):
            continue
        if line.startswith('target ') or line.startswith('attributes '):
            continue
        if line.startswith('@') and '=' in line:  # Global definitions
            continue

        # Function definitions (not declarations)
        if line.startswith("define "):
            stats["functions"] += 1
            in_function = True
            continue

        # Declarations (external functions)
        if line.startswith("declare "):
            continue

        if line == '}':
            in_function = False
            continue

        if not in_function:
            continue

        # Basic blocks (labels ending with :)
        if line.endswith(':') and not '=' in line:
            stats["blocks"] += 1
            continue

        # Skip preds comments
        if line.startswith('; preds'):
            continue

        # Count instructions
        is_instruction = False

        # Instructions with assignment
        if ' = ' in line:
            is_instruction = True
            if ' = alloca ' in line:
                stats["allocas"] += 1
            elif ' = load ' in line:
                stats["loads"] += 1
            elif ' = call ' in line:
                stats["calls"] += 1
            elif ' = phi ' in line:
                stats["phis"] += 1
            elif ' = add ' in line or ' = sub ' in line or ' = mul ' in line or ' = sdiv ' in line or ' = udiv ' in line:
                stats["arithmetic"] += 1
            elif ' = fadd ' in line or ' = fsub ' in line or ' = fmul ' in line or ' = fdiv ' in line:
                stats["arithmetic"] += 1
            elif ' = and ' in line or ' = or ' in line or ' = xor ' in line:
                stats["arithmetic"] += 1
            elif ' = shl ' in line or ' = lshr ' in line or ' = ashr ' in line:
                stats["arithmetic"] += 1
            elif ' = icmp ' in line or ' = fcmp ' in line:
                stats["comparisons"] += 1

        # Void instructions (no assignment)
        if line.startswith('store '):
            is_instruction = True
            stats["stores"] += 1
        elif line.startswith('call void') or (line.startswith('call ') and ' = call' not in line):
            is_instruction = True
            stats["calls"] += 1
        elif line.startswith('br '):
            is_instruction = True
            stats["branches"] += 1
        elif line.startswith('ret '):
            is_instruction = True
            stats["rets"] += 1
        elif line.startswith('switch '):
            is_instruction = True
            stats["branches"] += 1
        elif line.startswith('unreachable'):
            is_instruction = True

        if is_instruction:
            stats["instructions"] += 1

    return stats


def compile_tml_to_llvm_ir(file_path: Path, opt_level: str, verbose: bool = False) -> dict:
    """Compile a TML file at a specific optimization level and get LLVM IR stats."""
    result = {
        "file": file_path.name,
        "opt_level": opt_level,
        "success": False,
        "llvm_stats": {},
        "error": None,
    }

    try:
        # Compile with --emit-ir to get LLVM IR output
        cmd = [
            str(TML_COMPILER),
            "build",
            str(file_path),
            f"-{opt_level}",
            "--emit-ir",
        ]

        if verbose:
            print(f"    Running: {' '.join(cmd)}")

        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60,
            cwd=str(PROJECT_ROOT),
        )

        # Check for LLVM IR output
        if proc.returncode != 0:
            result["error"] = proc.stderr or proc.stdout
            return result

        # Find the .ll file - compiler outputs it to build/debug/<basename>.ll
        ll_file = BUILD_DIR / f"{file_path.stem}.ll"

        if not ll_file.exists():
            # Try next to source file
            ll_file = file_path.with_suffix(".ll")

        if ll_file.exists():
            llvm_content = ll_file.read_text(encoding="utf-8", errors="replace")
            result["llvm_stats"] = count_llvm_ir_instructions(llvm_content)
            result["llvm_size"] = len(llvm_content)
            result["success"] = True

            # Keep the .ll file for comparison if verbose
            if not verbose:
                try:
                    ll_file.unlink()
                except:
                    pass
        else:
            result["error"] = f"LLVM IR file not found: {ll_file}"

    except subprocess.TimeoutExpired:
        result["error"] = "Compilation timed out"
    except Exception as e:
        result["error"] = str(e)

    return result


def compile_rust_to_llvm_ir(file_path: Path, opt_level: str, verbose: bool = False) -> dict:
    """Compile a Rust file at a specific optimization level and get LLVM IR stats."""
    result = {
        "file": file_path.name,
        "opt_level": opt_level,
        "success": False,
        "llvm_stats": {},
        "binary_size": 0,
        "error": None,
    }

    # Check if rustc is available
    if not shutil.which("rustc"):
        result["error"] = "rustc not found in PATH"
        return result

    try:
        # Map optimization levels
        rust_opt = {
            "O0": "",
            "O1": "-C opt-level=1",
            "O2": "-O",
            "O3": "-C opt-level=3",
        }.get(opt_level, "-O")

        # Output path
        output_dir = RESULTS_DIR / "bin"
        output_dir.mkdir(exist_ok=True)
        output_exe = output_dir / f"{file_path.stem}_rust_{opt_level}.exe"
        output_ll = output_dir / f"{file_path.stem}_rust_{opt_level}.ll"

        # Compile to LLVM IR
        cmd_parts = ["rustc", "--emit=llvm-ir", "-o", str(output_ll)]
        if rust_opt:
            cmd_parts.extend(rust_opt.split())
        cmd_parts.append(str(file_path))

        if verbose:
            print(f"    Running: {' '.join(cmd_parts)}")

        proc = subprocess.run(
            cmd_parts,
            capture_output=True,
            text=True,
            timeout=60,
        )

        if proc.returncode != 0:
            result["error"] = proc.stderr or "Compilation failed"
            return result

        # Also compile to binary for size comparison
        cmd_exe = ["rustc", "-o", str(output_exe)]
        if rust_opt:
            cmd_exe.extend(rust_opt.split())
        cmd_exe.append(str(file_path))

        subprocess.run(cmd_exe, capture_output=True, timeout=60)

        if output_exe.exists():
            result["binary_size"] = output_exe.stat().st_size

        # Parse LLVM IR
        if output_ll.exists():
            llvm_content = output_ll.read_text(encoding="utf-8", errors="replace")
            result["llvm_stats"] = count_llvm_ir_instructions(llvm_content)
            result["llvm_size"] = len(llvm_content)
            result["success"] = True

            # Clean up
            if not verbose:
                try:
                    output_ll.unlink()
                except:
                    pass

    except subprocess.TimeoutExpired:
        result["error"] = "Compilation timed out"
    except Exception as e:
        result["error"] = str(e)

    return result


def calculate_reduction(before: int, after: int) -> float:
    """Calculate percentage reduction."""
    if before == 0:
        return 0.0
    return 100.0 * (1.0 - after / before)


def run_benchmarks(verbose: bool = False, with_rust: bool = False) -> tuple:
    """Run all benchmarks at all optimization levels."""
    all_results = []
    rust_results = {}

    for bench_file in BENCH_FILES:
        if not bench_file.exists():
            print(f"Warning: Benchmark file not found: {bench_file}")
            continue

        print(f"\nBenchmarking: {bench_file.name}")
        print("-" * 50)

        file_results = {"file": bench_file.name, "levels": {}}

        for opt_level in OPT_LEVELS:
            print(f"  {opt_level}...", end=" ", flush=True)
            result = compile_tml_to_llvm_ir(bench_file, opt_level, verbose)

            if result["success"]:
                stats = result["llvm_stats"]
                print(f"OK (funcs={stats.get('functions', '?')}, "
                      f"instrs={stats.get('instructions', '?')})")
            else:
                print(f"FAILED: {result.get('error', 'Unknown error')[:50]}")

            file_results["levels"][opt_level] = result

        # Calculate reductions
        if "O0" in file_results["levels"] and "O2" in file_results["levels"]:
            o0 = file_results["levels"]["O0"]
            o2 = file_results["levels"]["O2"]

            if o0["success"] and o2["success"]:
                o0_instrs = o0["llvm_stats"].get("instructions", 0)
                o2_instrs = o2["llvm_stats"].get("instructions", 0)
                reduction = calculate_reduction(o0_instrs, o2_instrs)
                file_results["instruction_reduction_O0_O2"] = reduction

                print(f"  Reduction (O0 -> O2): {reduction:.1f}%")

        all_results.append(file_results)

    # Run Rust benchmarks if requested
    if with_rust:
        for tml_name, rust_file in RUST_BENCH_FILES.items():
            if not rust_file.exists():
                print(f"  Skipping Rust comparison for {tml_name} (file not found)")
                continue

            print(f"\nRust comparison: {rust_file.name}")
            print("-" * 50)

            file_results = {"file": rust_file.name, "levels": {}}

            for opt_level in OPT_LEVELS:
                print(f"  {opt_level}...", end=" ", flush=True)
                result = compile_rust_to_llvm_ir(rust_file, opt_level, verbose)

                if result["success"]:
                    stats = result["llvm_stats"]
                    print(f"OK (funcs={stats.get('functions', '?')}, "
                          f"instrs={stats.get('instructions', '?')}, "
                          f"size={result.get('binary_size', 0)//1024}KB)")
                else:
                    print(f"FAILED: {result.get('error', 'Unknown')[:40]}")

                file_results["levels"][opt_level] = result

            # Calculate reductions
            if "O0" in file_results["levels"] and "O2" in file_results["levels"]:
                o0 = file_results["levels"]["O0"]
                o2 = file_results["levels"]["O2"]

                if o0["success"] and o2["success"]:
                    o0_instrs = o0["llvm_stats"].get("instructions", 0)
                    o2_instrs = o2["llvm_stats"].get("instructions", 0)
                    reduction = calculate_reduction(o0_instrs, o2_instrs)
                    file_results["instruction_reduction_O0_O2"] = reduction

                    print(f"  Reduction (O0 -> O2): {reduction:.1f}%")

            rust_results[tml_name] = file_results

    return all_results, rust_results


def generate_report(results: list, rust_results: dict = None) -> str:
    """Generate a markdown report."""
    lines = [
        "# TML Optimization Benchmark Report",
        "",
        f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        "",
        "## Measurement Method",
        "",
        "**Both TML and Rust are measured using LLVM IR instruction counts.**",
        "",
        "This ensures a fair comparison since both compilers target LLVM.",
        "",
        "## Optimization Levels",
        "",
        "| Level | TML | Rust |",
        "|-------|-----|------|",
        "| O0 | No optimizations | No optimizations |",
        "| O1 | Constant folding/propagation | Basic optimizations |",
        "| O2 | O1 + CSE, copy prop, DCE, inlining | Release mode (-O) |",
        "| O3 | O2 + aggressive inlining | Maximum optimization |",
        "",
        "## TML Results (LLVM IR)",
        "",
    ]

    for file_result in results:
        lines.append(f"### {file_result['file']}")
        lines.append("")
        lines.append("| Opt Level | Functions | Instructions | Reduction |")
        lines.append("|-----------|-----------|--------------|-----------|")

        baseline_instrs = None
        for opt_level in OPT_LEVELS:
            if opt_level not in file_result["levels"]:
                continue

            result = file_result["levels"][opt_level]
            if not result["success"]:
                lines.append(f"| {opt_level} | - | - | FAILED |")
                continue

            stats = result["llvm_stats"]
            funcs = stats.get("functions", 0)
            instrs = stats.get("instructions", 0)

            if baseline_instrs is None:
                baseline_instrs = instrs
                reduction = "-"
            else:
                reduction = f"{calculate_reduction(baseline_instrs, instrs):.1f}%"

            lines.append(f"| {opt_level} | {funcs} | {instrs} | {reduction} |")

        lines.append("")

        if "instruction_reduction_O0_O2" in file_result:
            lines.append(f"**Total instruction reduction (O0 → O2): "
                        f"{file_result['instruction_reduction_O0_O2']:.1f}%**")
            lines.append("")

    # Add Rust comparison if available
    if rust_results:
        lines.extend([
            "## Rust Results (LLVM IR)",
            "",
        ])

        for tml_name, rust_result in rust_results.items():
            lines.append(f"### {rust_result['file']} (equivalent to {tml_name})")
            lines.append("")
            lines.append("| Opt Level | Functions | Instructions | Reduction |")
            lines.append("|-----------|-----------|--------------|-----------|")

            baseline_instrs = None
            for opt_level in OPT_LEVELS:
                if opt_level not in rust_result["levels"]:
                    continue

                result = rust_result["levels"][opt_level]
                if not result["success"]:
                    lines.append(f"| {opt_level} | - | - | FAILED |")
                    continue

                stats = result["llvm_stats"]
                funcs = stats.get("functions", 0)
                instrs = stats.get("instructions", 0)

                if baseline_instrs is None:
                    baseline_instrs = instrs
                    reduction = "-"
                else:
                    reduction = f"{calculate_reduction(baseline_instrs, instrs):.1f}%"

                lines.append(f"| {opt_level} | {funcs} | {instrs} | {reduction} |")

            lines.append("")

            if "instruction_reduction_O0_O2" in rust_result:
                lines.append(f"**Rust instruction reduction (O0 → O2): "
                            f"{rust_result['instruction_reduction_O0_O2']:.1f}%**")
                lines.append("")

        # TML vs Rust comparison summary
        lines.extend([
            "## TML vs Rust Comparison",
            "",
            "Direct comparison using LLVM IR instruction counts:",
            "",
        ])

        for tml_name in rust_results.keys():
            tml_result = next((r for r in results if r["file"] == tml_name), None)
            rust_result = rust_results.get(tml_name)

            if tml_result and rust_result:
                lines.append(f"### {tml_name}")
                lines.append("")
                lines.append("| Metric | TML | Rust |")
                lines.append("|--------|-----|------|")

                # Compare each optimization level
                for opt_level in OPT_LEVELS:
                    tml_level = tml_result["levels"].get(opt_level, {})
                    rust_level = rust_result["levels"].get(opt_level, {})

                    tml_instrs = tml_level.get("llvm_stats", {}).get("instructions", "N/A") if tml_level.get("success") else "N/A"
                    rust_instrs = rust_level.get("llvm_stats", {}).get("instructions", "N/A") if rust_level.get("success") else "N/A"
                    tml_funcs = tml_level.get("llvm_stats", {}).get("functions", "N/A") if tml_level.get("success") else "N/A"
                    rust_funcs = rust_level.get("llvm_stats", {}).get("functions", "N/A") if rust_level.get("success") else "N/A"

                    lines.append(f"| {opt_level} Instructions | {tml_instrs} | {rust_instrs} |")
                    lines.append(f"| {opt_level} Functions | {tml_funcs} | {rust_funcs} |")

                # Reductions
                tml_reduction = tml_result.get("instruction_reduction_O0_O2", 0)
                rust_reduction = rust_result.get("instruction_reduction_O0_O2", 0)
                lines.append(f"| **Reduction (O0→O2)** | **{tml_reduction:.1f}%** | **{rust_reduction:.1f}%** |")

                lines.append("")

    lines.extend([
        "## Notes",
        "",
        "- Both TML and Rust generate LLVM IR which is then compiled by LLVM/Clang",
        "- TML applies optimizations at the MIR level before LLVM IR generation",
        "- Rust applies optimizations at the MIR level and then LLVM applies more",
        "- The LLVM IR shown here is BEFORE LLVM's own optimization passes",
        "",
    ])

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="TML Optimization Benchmark Runner")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("-o", "--output", type=str, help="Output file for report")
    parser.add_argument("--with-rust", action="store_true", help="Include Rust comparison")
    args = parser.parse_args()

    # Check compiler exists
    if not TML_COMPILER.exists():
        print(f"Error: TML compiler not found at {TML_COMPILER}")
        print("Run 'scripts\\build.bat' first.")
        sys.exit(1)

    print("=" * 60)
    print("TML Optimization Benchmark (LLVM IR Comparison)")
    print("=" * 60)
    print(f"Compiler: {TML_COMPILER}")
    print(f"Optimization levels: {', '.join(OPT_LEVELS)}")
    print(f"Metric: LLVM IR instruction count")
    if args.with_rust:
        print("Rust comparison: ENABLED (same LLVM IR metric)")

    results, rust_results = run_benchmarks(args.verbose, args.with_rust)

    # Generate report
    report = generate_report(results, rust_results if args.with_rust else None)

    # Ensure results directory exists
    RESULTS_DIR.mkdir(exist_ok=True)

    # Save report
    if args.output:
        output_path = Path(args.output)
    else:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_path = RESULTS_DIR / f"optimization_report_{timestamp}.md"

    output_path.write_text(report, encoding="utf-8")
    print(f"\nReport saved to: {output_path}")

    # Also save JSON results
    json_path = output_path.with_suffix(".json")
    all_data = {"tml": results}
    if rust_results:
        all_data["rust"] = rust_results
    with open(json_path, "w") as f:
        json.dump(all_data, f, indent=2, default=str)
    print(f"JSON data saved to: {json_path}")

    print("\n" + "=" * 60)
    print("Done!")


if __name__ == "__main__":
    main()
