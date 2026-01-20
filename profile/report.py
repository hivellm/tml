#!/usr/bin/env python3
"""
TML Profile Report Generator

Parses benchmark JSON outputs and generates a comparison report.
"""

import json
import os
import sys
from datetime import datetime
from pathlib import Path

def load_json_results(filepath):
    """Load benchmark results from JSON file."""
    try:
        with open(filepath, 'r') as f:
            return json.load(f)
    except Exception as e:
        print(f"Warning: Could not load {filepath}: {e}")
        return None

def format_number(n):
    """Format large numbers with commas."""
    return f"{n:,}"

def calculate_ratio(cpp_val, tml_val):
    """Calculate TML/C++ ratio and format as percentage."""
    if cpp_val == 0:
        return "N/A"
    ratio = tml_val / cpp_val
    if ratio > 1:
        return f"{ratio:.2f}x slower"
    elif ratio < 1:
        return f"{1/ratio:.2f}x faster"
    else:
        return "1.00x (same)"

def generate_report():
    """Generate the comparison report."""
    results_dir = Path(__file__).parent / "results"

    categories = ["math", "string", "json", "collections", "memory", "text"]

    print("=" * 70)
    print("  TML vs C++ Performance Comparison Report")
    print(f"  Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 70)
    print()

    summary = []

    for category in categories:
        cpp_file = results_dir / f"{category}_cpp.json"
        tml_file = results_dir / f"{category}_tml.json"

        cpp_data = load_json_results(cpp_file)
        tml_data = load_json_results(tml_file)

        if not cpp_data or not tml_data:
            print(f"\n--- {category.upper()} ---")
            print("  [Missing JSON results - run benchmarks first]")
            continue

        print(f"\n{'=' * 70}")
        print(f"  {category.upper()} BENCHMARKS")
        print(f"{'=' * 70}")
        print()

        # Header
        print(f"{'Benchmark':<35} {'C++ (ns/op)':<15} {'TML (ns/op)':<15} {'Ratio':<20}")
        print("-" * 85)

        # Match results by name
        cpp_results = {r['name']: r for r in cpp_data.get('results', [])}
        tml_results = {r['name']: r for r in tml_data.get('results', [])}

        all_names = set(cpp_results.keys()) | set(tml_results.keys())

        for name in sorted(all_names):
            cpp_r = cpp_results.get(name)
            tml_r = tml_results.get(name)

            cpp_ns = cpp_r['per_op_ns'] if cpp_r else 0
            tml_ns = tml_r['per_op_ns'] if tml_r else 0

            ratio = calculate_ratio(cpp_ns, tml_ns) if cpp_ns and tml_ns else "N/A"

            cpp_str = format_number(cpp_ns) if cpp_r else "N/A"
            tml_str = format_number(tml_ns) if tml_r else "N/A"

            print(f"{name:<35} {cpp_str:<15} {tml_str:<15} {ratio:<20}")

            if cpp_r and tml_r:
                summary.append({
                    'category': category,
                    'name': name,
                    'cpp_ns': cpp_ns,
                    'tml_ns': tml_ns,
                    'ratio': tml_ns / cpp_ns if cpp_ns > 0 else 0
                })

    # Summary statistics
    if summary:
        print()
        print("=" * 70)
        print("  SUMMARY")
        print("=" * 70)
        print()

        # Calculate averages by category
        by_category = {}
        for s in summary:
            cat = s['category']
            if cat not in by_category:
                by_category[cat] = []
            by_category[cat].append(s['ratio'])

        print(f"{'Category':<20} {'Avg Ratio':<15} {'Status':<30}")
        print("-" * 65)

        total_ratio = 0
        total_count = 0

        for cat, ratios in sorted(by_category.items()):
            avg = sum(ratios) / len(ratios)
            total_ratio += sum(ratios)
            total_count += len(ratios)

            if avg <= 1.1:
                status = "✓ Excellent (within 10%)"
            elif avg <= 1.5:
                status = "~ Good (within 50%)"
            elif avg <= 2.0:
                status = "! Needs attention (2x slower)"
            else:
                status = "!! Critical (>2x slower)"

            print(f"{cat:<20} {avg:.2f}x{'':<10} {status:<30}")

        if total_count > 0:
            overall_avg = total_ratio / total_count
            print()
            print(f"Overall Average: {overall_avg:.2f}x")

            if overall_avg <= 1.2:
                print("Status: TML is performing well relative to C++")
            elif overall_avg <= 1.5:
                print("Status: TML has acceptable overhead")
            else:
                print("Status: TML needs performance optimization")

    print()
    print("=" * 70)
    print("  Note: Ratios > 1.0 mean TML is slower than C++")
    print("        Ratios < 1.0 mean TML is faster than C++")
    print("=" * 70)

if __name__ == "__main__":
    generate_report()
