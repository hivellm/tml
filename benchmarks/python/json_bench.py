#!/usr/bin/env python3
"""
JSON Benchmark - Python Implementation

Compares Python's built-in json module performance.
Run with: python json_bench.py
"""

import json
import time
import sys
from typing import Any, Callable


def benchmark(name: str, iterations: int, data_size: int, func: Callable[[], Any]) -> dict:
    """Run a benchmark and return results."""
    # Warmup
    for _ in range(min(iterations // 10, 10)):
        func()

    start = time.perf_counter()
    for _ in range(iterations):
        func()
    end = time.perf_counter()

    total_us = (end - start) * 1_000_000
    avg_us = total_us / iterations
    throughput = (data_size * iterations) / (total_us / 1_000_000) / (1024 * 1024) if data_size > 0 else 0

    return {
        "name": name,
        "time_us": avg_us,
        "iterations": iterations,
        "throughput_mb_s": throughput
    }


def print_result(r: dict):
    """Print a benchmark result."""
    print(f"{r['name']:<40} {r['time_us']:>12.2f} us {r['iterations']:>12} iters", end="")
    if r['throughput_mb_s'] > 0:
        print(f" {r['throughput_mb_s']:>12.2f} MB/s", end="")
    print()


def print_separator():
    print("-" * 80)


# Test Data Generation

def generate_small_json() -> str:
    return json.dumps({
        "name": "John Doe",
        "age": 30,
        "active": True,
        "email": "john@example.com",
        "scores": [95, 87, 92, 88, 91],
        "address": {
            "street": "123 Main St",
            "city": "New York",
            "zip": "10001"
        }
    })


def generate_medium_json(num_items: int = 1000) -> str:
    items = [
        {
            "id": i,
            "name": f"Item {i}",
            "price": i * 1.5,
            "active": i % 2 == 0,
            "tags": ["tag1", "tag2", "tag3"]
        }
        for i in range(num_items)
    ]
    return json.dumps({"items": items})


def generate_large_json(num_items: int = 10000) -> str:
    data = [
        {
            "id": i,
            "uuid": f"550e8400-e29b-41d4-a716-446655440{i % 1000:03d}",
            "name": f"User {i}",
            "email": f"user{i}@example.com",
            "score": i * 0.1,
            "metadata": {
                "created": "2024-01-01",
                "updated": "2024-01-02",
                "version": i % 10
            },
            "tags": ["alpha", "beta", "gamma", "delta"]
        }
        for i in range(num_items)
    ]
    return json.dumps({"data": data})


def generate_deep_json(depth: int = 100) -> str:
    result: dict = {"level": depth - 1, "child": None}
    for i in range(depth - 2, -1, -1):
        result = {"level": i, "child": result}
    return json.dumps(result)


def generate_wide_array(size: int = 10000) -> str:
    return json.dumps(list(range(size)))


def generate_string_heavy_json(num_items: int = 1000) -> str:
    strings = [
        f"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Item {i}"
        for i in range(num_items)
    ]
    return json.dumps({"strings": strings})


def run_benchmarks():
    print("\n=== Python json Module ===\n")
    print_separator()

    results = []

    # Small JSON parsing
    json_str = generate_small_json()
    r = benchmark("Python: Parse small JSON", 100000, len(json_str), lambda: json.loads(json_str))
    results.append(r)
    print_result(r)

    # Medium JSON parsing
    json_str = generate_medium_json(1000)
    r = benchmark("Python: Parse medium JSON (100KB)", 1000, len(json_str), lambda: json.loads(json_str))
    results.append(r)
    print_result(r)

    # Large JSON parsing
    json_str = generate_large_json(10000)
    r = benchmark("Python: Parse large JSON (1MB)", 100, len(json_str), lambda: json.loads(json_str))
    results.append(r)
    print_result(r)

    # Deep nesting
    json_str = generate_deep_json(100)
    r = benchmark("Python: Parse deep nesting (100 levels)", 10000, len(json_str), lambda: json.loads(json_str))
    results.append(r)
    print_result(r)

    # Wide array
    json_str = generate_wide_array(10000)
    r = benchmark("Python: Parse wide array (10K ints)", 1000, len(json_str), lambda: json.loads(json_str))
    results.append(r)
    print_result(r)

    # String-heavy JSON
    json_str = generate_string_heavy_json(1000)
    r = benchmark("Python: Parse string-heavy JSON", 500, len(json_str), lambda: json.loads(json_str))
    results.append(r)
    print_result(r)

    print_separator()

    # Serialization benchmarks
    json_str = generate_medium_json(1000)
    obj = json.loads(json_str)
    r = benchmark("Python: Serialize medium JSON", 1000, len(json_str), lambda: json.dumps(obj))
    results.append(r)
    print_result(r)

    json_str = generate_large_json(10000)
    obj = json.loads(json_str)
    r = benchmark("Python: Serialize large JSON", 100, len(json_str), lambda: json.dumps(obj))
    results.append(r)
    print_result(r)

    json_str = generate_medium_json(1000)
    obj = json.loads(json_str)
    r = benchmark("Python: Pretty print medium JSON", 500, len(json_str), lambda: json.dumps(obj, indent=2))
    results.append(r)
    print_result(r)

    print_separator()

    # Build benchmark
    def build_object():
        return {f"field{i}": i for i in range(1000)}
    r = benchmark("Python: Build dict (1000 fields)", 10000, 0, build_object)
    results.append(r)
    print_result(r)

    def build_array():
        return list(range(10000))
    r = benchmark("Python: Build list (10000 items)", 1000, 0, build_array)
    results.append(r)
    print_result(r)

    print_separator()

    # Access patterns
    json_str = generate_medium_json(1000)
    obj = json.loads(json_str)
    items = obj["items"]

    def random_access():
        total = 0
        for item in items:
            total += item["id"]
        return total

    r = benchmark("Python: Random access (1000 items)", 10000, 0, random_access)
    results.append(r)
    print_result(r)

    print_separator()

    # Summary
    print("\n=== Summary ===\n")
    total_time = sum(r["time_us"] for r in results)
    print(f"Total benchmark time: {total_time / 1000:.2f} ms")


def main():
    print("JSON Benchmark Suite - Python Implementation")
    print("=" * 48)
    print(f"Python version: {sys.version}")

    # Show test data sizes
    print("\nTest data sizes:")
    print(f"  Small JSON:  {len(generate_small_json())} bytes")
    print(f"  Medium JSON: {len(generate_medium_json(1000))} bytes")
    print(f"  Large JSON:  {len(generate_large_json(10000))} bytes")
    print(f"  Deep JSON:   {len(generate_deep_json(100))} bytes")
    print(f"  Wide Array:  {len(generate_wide_array(10000))} bytes")
    print(f"  String-heavy:{len(generate_string_heavy_json(1000))} bytes")

    run_benchmarks()

    print("\nBenchmark complete.")


if __name__ == "__main__":
    main()
