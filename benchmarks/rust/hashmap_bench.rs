// HashMap Benchmarks - Rust
//
// Compile: rustc -O hashmap_bench.rs -o hashmap_bench.exe
// Run: ./hashmap_bench.exe
//
// All benchmarks use 1M operations for accurate ns/op measurement.
// black_box() prevents dead code elimination.

use std::collections::HashMap;
use std::time::Instant;
use std::hint::black_box;

fn run_and_print(name: &str, iterations: i64, total_ns: i64) {
    let per_op = if total_ns > 0 { total_ns / iterations } else { 0 };
    let ops_sec = if total_ns > 0 {
        (iterations as u128 * 1_000_000_000) / total_ns as u128
    } else {
        0
    };
    println!("  {}:", name);
    println!("    Iterations: {}", iterations);
    println!("    Total time: {} ms", total_ns / 1_000_000);
    println!("    Per op:     {} ns", per_op);
    println!("    Ops/sec:    {}", ops_sec);
    println!();
}

// HashMap insert (i64 -> i64), grow from small capacity
fn bench_hashmap_insert(n: i64) -> i64 {
    let mut map = HashMap::with_capacity(16);
    for i in 0..n {
        map.insert(i, i * 2);
    }
    black_box(map.len() as i64)
}

// HashMap insert with pre-reserved capacity
fn bench_hashmap_insert_reserved(n: i64) -> i64 {
    let mut map: HashMap<i64, i64> = HashMap::with_capacity(n as usize);
    for i in 0..n {
        map.insert(i, i * 2);
    }
    black_box(map.len() as i64)
}

// HashMap lookup (1M lookups in 10K-entry map)
fn bench_hashmap_lookup(n: i64) -> i64 {
    let mut map = HashMap::with_capacity(10000);
    for i in 0..10000i64 {
        map.insert(i, i * 2);
    }

    let mut sum: i64 = 0;
    for i in 0..n {
        sum += map.get(&(i % 10000)).copied().unwrap_or(0);
    }
    black_box(sum)
}

// HashMap contains_key (half hit, half miss)
fn bench_hashmap_contains(n: i64) -> i64 {
    let mut map = HashMap::with_capacity(10000);
    for i in 0..10000i64 {
        map.insert(i, i);
    }

    let mut found: i64 = 0;
    for i in 0..n {
        if map.contains_key(&(i % 20000)) {
            found += 1;
        }
    }
    black_box(found)
}

// HashMap remove (insert N then remove N)
fn bench_hashmap_remove(n: i64) -> i64 {
    let mut map = HashMap::with_capacity(n as usize);
    for i in 0..n {
        map.insert(i, i);
    }

    let mut removed: i64 = 0;
    for i in 0..n {
        if map.remove(&i).is_some() {
            removed += 1;
        }
    }
    black_box(removed)
}

// HashMap string key insert + lookup
fn bench_hashmap_string_key(n: i64) -> i64 {
    let mut map = HashMap::with_capacity(n as usize);

    for i in 0..n {
        map.insert(format!("key{}", i), i);
    }

    let mut sum: i64 = 0;
    for i in 0..n {
        if let Some(&v) = map.get(&format!("key{}", i)) {
            sum += v;
        }
    }
    black_box(sum)
}

fn main() {
    println!();
    println!("================================================================");
    println!("  HashMap Benchmarks (Rust)");
    println!("================================================================");
    println!();

    let n: i64 = 1_000_000;

    // Warmup
    {
        let mut warmup = HashMap::new();
        for i in 0..1000i64 {
            warmup.insert(i, i);
        }
        black_box(&warmup);
    }

    let start = Instant::now();
    let _ = bench_hashmap_insert(n);
    run_and_print("HashMap Insert", n, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_hashmap_insert_reserved(n);
    run_and_print("HashMap Insert (reserved)", n, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_hashmap_lookup(n);
    run_and_print("HashMap Lookup", n, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_hashmap_contains(n);
    run_and_print("HashMap Contains", n, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_hashmap_remove(n);
    run_and_print("HashMap Remove", n, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_hashmap_string_key(n);
    run_and_print("HashMap String Key", n, start.elapsed().as_nanos() as i64);
}
