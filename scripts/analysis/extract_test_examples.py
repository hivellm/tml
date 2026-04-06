#!/usr/bin/env python3
"""Extract @test functions as @example doc comment blocks.

Scans test files, matches test functions to library functions by name,
and generates example snippets that can be injected into doc comments.

Usage:
    python scripts/extract_test_examples.py [--module core/str] [--inject] [--dry-run]
"""

import os
import re
import sys
import json
from pathlib import Path
from collections import defaultdict

# Map test function names to library functions
# test_str_len -> str::len
# test_split_basic -> str::split
# test_maybe_map -> Maybe::map
def test_name_to_func(test_name: str) -> list[str]:
    """Heuristically map a test function name to library function(s)."""
    # Strip test_ prefix
    name = test_name
    if name.startswith("test_"):
        name = name[5:]

    candidates = []

    # Pattern: test_str_len -> str::len
    if "_" in name:
        parts = name.split("_")
        # Try module_func: str_len -> str::len
        if len(parts) >= 2:
            candidates.append(f"{parts[0]}::{('_'.join(parts[1:]))}")
        # Try type_method: maybe_map -> Maybe::map
        capitalized = parts[0][0].upper() + parts[0][1:] if parts[0] else ""
        if capitalized:
            candidates.append(f"{capitalized}::{'_'.join(parts[1:])}")
    else:
        candidates.append(name)

    return candidates


def extract_tests(test_file: Path) -> list[dict]:
    """Extract @test functions from a test file."""
    tests = []
    lines = test_file.read_text(encoding="utf-8", errors="replace").splitlines()

    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line == "@test":
            # Next non-empty line should be func declaration
            j = i + 1
            while j < len(lines) and lines[j].strip() == "":
                j += 1
            if j < len(lines) and lines[j].strip().startswith("func "):
                func_line = lines[j].strip()
                # Extract function name
                m = re.match(r"func\s+(\w+)\s*\(", func_line)
                if m:
                    func_name = m.group(1)
                    # Collect body until closing }
                    body_lines = []
                    depth = 0
                    k = j
                    while k < len(lines):
                        body_lines.append(lines[k])
                        depth += lines[k].count("{") - lines[k].count("}")
                        if depth <= 0 and k > j:
                            break
                        k += 1

                    # Extract the meaningful body (skip func decl + return 0 + closing brace)
                    inner = []
                    for bl in body_lines[1:]:  # skip func line
                        stripped = bl.strip()
                        if stripped in ("return 0", "return 0;", "}", ""):
                            continue
                        inner.append(bl.rstrip())

                    if inner:
                        tests.append({
                            "name": func_name,
                            "file": str(test_file),
                            "body": inner,
                            "candidates": test_name_to_func(func_name),
                        })
        i += 1

    return tests


def scan_module_tests(module: str, base_dir: Path) -> list[dict]:
    """Scan all test files for a module."""
    # core/str -> lib/core/tests/str/
    parts = module.split("/")
    if len(parts) == 2:
        test_dir = base_dir / "lib" / parts[0] / "tests" / parts[1]
    else:
        test_dir = base_dir / "lib" / module / "tests"

    all_tests = []
    if test_dir.exists():
        for tf in sorted(test_dir.glob("*.test.tml")):
            all_tests.extend(extract_tests(tf))

    return all_tests


def format_example(test: dict, indent: str = "/// ") -> str:
    """Format a test as a doc comment example block."""
    lines = [f"{indent}"]
    lines.append(f"{indent}# Example")
    lines.append(f"{indent}")
    lines.append(f"{indent}```tml")
    for bl in test["body"][:8]:  # Max 8 lines per example
        # Remove leading indentation (typically 4 spaces)
        trimmed = bl
        if trimmed.startswith("    "):
            trimmed = trimmed[4:]
        lines.append(f"{indent}{trimmed}")
    if len(test["body"]) > 8:
        lines.append(f"{indent}// ...")
    lines.append(f"{indent}```")
    return "\n".join(lines)


def main():
    base_dir = Path(__file__).parent.parent
    modules = ["core/str", "core/num", "core/fmt", "core/iter",
               "std/collections", "std/sync", "std/json"]

    if "--module" in sys.argv:
        idx = sys.argv.index("--module")
        if idx + 1 < len(sys.argv):
            modules = [sys.argv[idx + 1]]

    total_tests = 0
    total_mapped = 0
    results = {}

    for module in modules:
        tests = scan_module_tests(module, base_dir)
        total_tests += len(tests)
        results[module] = []
        for t in tests:
            mapped = False
            for candidate in t["candidates"]:
                results[module].append({
                    "test": t["name"],
                    "maps_to": candidate,
                    "body_lines": len(t["body"]),
                })
                mapped = True
            if mapped:
                total_mapped += 1

    # Output summary
    print(f"Total @test functions found: {total_tests}")
    print(f"Mapped to library functions: {total_mapped}")
    print()
    for module, entries in results.items():
        if entries:
            print(f"  {module}: {len(entries)} test->func mappings")
            for e in entries[:5]:
                print(f"    {e['test']} -> {e['maps_to']} ({e['body_lines']} lines)")
            if len(entries) > 5:
                print(f"    ... and {len(entries) - 5} more")

    # JSON output for further processing
    if "--json" in sys.argv:
        print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
