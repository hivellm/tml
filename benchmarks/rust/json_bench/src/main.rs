//! JSON Benchmark - Rust serde_json Implementation
//!
//! Compares Rust's serde_json performance.
//!
//! Run with: cargo run --release

use serde_json::{json, Value};
use std::time::Instant;

// ============================================================================
// Benchmark Infrastructure
// ============================================================================

struct BenchResult {
    name: String,
    time_us: f64,
    iterations: usize,
    throughput_mb_s: f64,
}

fn benchmark<F>(name: &str, iterations: usize, data_size: usize, mut func: F) -> BenchResult
where
    F: FnMut(),
{
    // Warmup
    let warmup = std::cmp::min(iterations / 10, 10);
    for _ in 0..warmup {
        func();
    }

    let start = Instant::now();
    for _ in 0..iterations {
        func();
    }
    let elapsed = start.elapsed();

    let total_us = elapsed.as_micros() as f64;
    let avg_us = total_us / iterations as f64;
    let throughput = if data_size > 0 {
        (data_size * iterations) as f64 / (total_us / 1e6) / (1024.0 * 1024.0)
    } else {
        0.0
    };

    BenchResult {
        name: name.to_string(),
        time_us: avg_us,
        iterations,
        throughput_mb_s: throughput,
    }
}

fn print_result(r: &BenchResult) {
    print!("{:<40} {:>12.2} us {:>12} iters", r.name, r.time_us, r.iterations);
    if r.throughput_mb_s > 0.0 {
        print!(" {:>12.2} MB/s", r.throughput_mb_s);
    }
    println!();
}

fn print_separator() {
    println!("{}", "-".repeat(80));
}

// ============================================================================
// Test Data Generation
// ============================================================================

fn generate_small_json() -> String {
    json!({
        "name": "John Doe",
        "age": 30,
        "active": true,
        "email": "john@example.com",
        "scores": [95, 87, 92, 88, 91],
        "address": {
            "street": "123 Main St",
            "city": "New York",
            "zip": "10001"
        }
    })
    .to_string()
}

fn generate_medium_json(num_items: usize) -> String {
    let items: Vec<Value> = (0..num_items)
        .map(|i| {
            json!({
                "id": i,
                "name": format!("Item {}", i),
                "price": i as f64 * 1.5,
                "active": i % 2 == 0,
                "tags": ["tag1", "tag2", "tag3"]
            })
        })
        .collect();
    json!({ "items": items }).to_string()
}

fn generate_large_json(num_items: usize) -> String {
    let data: Vec<Value> = (0..num_items)
        .map(|i| {
            json!({
                "id": i,
                "uuid": format!("550e8400-e29b-41d4-a716-446655440{:03}", i % 1000),
                "name": format!("User {}", i),
                "email": format!("user{}@example.com", i),
                "score": i as f64 * 0.1,
                "metadata": {
                    "created": "2024-01-01",
                    "updated": "2024-01-02",
                    "version": i % 10
                },
                "tags": ["alpha", "beta", "gamma", "delta"]
            })
        })
        .collect();
    json!({ "data": data }).to_string()
}

fn generate_deep_json(depth: usize) -> String {
    let mut result = json!(null);
    for i in (0..depth).rev() {
        result = json!({
            "level": i,
            "child": result
        });
    }
    result.to_string()
}

fn generate_wide_array(size: usize) -> String {
    let arr: Vec<usize> = (0..size).collect();
    serde_json::to_string(&arr).unwrap()
}

fn generate_string_heavy_json(num_items: usize) -> String {
    let strings: Vec<String> = (0..num_items)
        .map(|i| {
            format!("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Item {}", i)
        })
        .collect();
    json!({ "strings": strings }).to_string()
}

// ============================================================================
// Benchmarks
// ============================================================================

fn run_benchmarks() {
    println!("\n=== Rust serde_json ===\n");
    print_separator();

    let mut results = Vec::new();

    // Small JSON parsing
    let json_str = generate_small_json();
    let r = benchmark("Rust: Parse small JSON", 100000, json_str.len(), || {
        let _: Value = serde_json::from_str(&json_str).unwrap();
    });
    results.push(r);
    print_result(results.last().unwrap());

    // Medium JSON parsing
    let json_str = generate_medium_json(1000);
    let r = benchmark("Rust: Parse medium JSON (100KB)", 1000, json_str.len(), || {
        let _: Value = serde_json::from_str(&json_str).unwrap();
    });
    results.push(r);
    print_result(results.last().unwrap());

    // Large JSON parsing
    let json_str = generate_large_json(10000);
    let r = benchmark("Rust: Parse large JSON (1MB)", 100, json_str.len(), || {
        let _: Value = serde_json::from_str(&json_str).unwrap();
    });
    results.push(r);
    print_result(results.last().unwrap());

    // Deep nesting
    let json_str = generate_deep_json(100);
    let r = benchmark(
        "Rust: Parse deep nesting (100 levels)",
        10000,
        json_str.len(),
        || {
            let _: Value = serde_json::from_str(&json_str).unwrap();
        },
    );
    results.push(r);
    print_result(results.last().unwrap());

    // Wide array
    let json_str = generate_wide_array(10000);
    let r = benchmark(
        "Rust: Parse wide array (10K ints)",
        1000,
        json_str.len(),
        || {
            let _: Value = serde_json::from_str(&json_str).unwrap();
        },
    );
    results.push(r);
    print_result(results.last().unwrap());

    // String-heavy JSON
    let json_str = generate_string_heavy_json(1000);
    let r = benchmark("Rust: Parse string-heavy JSON", 500, json_str.len(), || {
        let _: Value = serde_json::from_str(&json_str).unwrap();
    });
    results.push(r);
    print_result(results.last().unwrap());

    print_separator();

    // Serialization benchmarks
    let json_str = generate_medium_json(1000);
    let obj: Value = serde_json::from_str(&json_str).unwrap();
    let r = benchmark("Rust: Serialize medium JSON", 1000, json_str.len(), || {
        let _ = serde_json::to_string(&obj).unwrap();
    });
    results.push(r);
    print_result(results.last().unwrap());

    let json_str = generate_large_json(10000);
    let obj: Value = serde_json::from_str(&json_str).unwrap();
    let r = benchmark("Rust: Serialize large JSON", 100, json_str.len(), || {
        let _ = serde_json::to_string(&obj).unwrap();
    });
    results.push(r);
    print_result(results.last().unwrap());

    let json_str = generate_medium_json(1000);
    let obj: Value = serde_json::from_str(&json_str).unwrap();
    let r = benchmark("Rust: Pretty print medium JSON", 500, json_str.len(), || {
        let _ = serde_json::to_string_pretty(&obj).unwrap();
    });
    results.push(r);
    print_result(results.last().unwrap());

    print_separator();

    // Build benchmark
    let r = benchmark("Rust: Build object (1000 fields)", 10000, 0, || {
        let mut obj = serde_json::Map::new();
        for i in 0..1000 {
            obj.insert(format!("field{}", i), json!(i));
        }
        let _ = Value::Object(obj);
    });
    results.push(r);
    print_result(results.last().unwrap());

    let r = benchmark("Rust: Build array (10000 items)", 1000, 0, || {
        let arr: Vec<Value> = (0..10000).map(|i| json!(i)).collect();
        let _ = Value::Array(arr);
    });
    results.push(r);
    print_result(results.last().unwrap());

    print_separator();

    // Access patterns
    let json_str = generate_medium_json(1000);
    let obj: Value = serde_json::from_str(&json_str).unwrap();
    let items = obj.get("items").unwrap().as_array().unwrap();

    let r = benchmark("Rust: Random access (1000 items)", 10000, 0, || {
        let mut total: i64 = 0;
        for item in items {
            if let Some(id) = item.get("id").and_then(|v| v.as_i64()) {
                total += id;
            }
        }
        std::hint::black_box(total);
    });
    results.push(r);
    print_result(results.last().unwrap());

    print_separator();

    // Summary
    println!("\n=== Summary ===\n");
    let total_time: f64 = results.iter().map(|r| r.time_us).sum();
    println!("Total benchmark time: {:.2} ms", total_time / 1000.0);
}

fn main() {
    println!("JSON Benchmark Suite - Rust serde_json Implementation");
    println!("{}", "=".repeat(56));

    // Show test data sizes
    println!("\nTest data sizes:");
    println!("  Small JSON:  {} bytes", generate_small_json().len());
    println!("  Medium JSON: {} bytes", generate_medium_json(1000).len());
    println!("  Large JSON:  {} bytes", generate_large_json(10000).len());
    println!("  Deep JSON:   {} bytes", generate_deep_json(100).len());
    println!("  Wide Array:  {} bytes", generate_wide_array(10000).len());
    println!(
        "  String-heavy:{} bytes",
        generate_string_heavy_json(1000).len()
    );

    run_benchmarks();

    println!("\nBenchmark complete.");
}
