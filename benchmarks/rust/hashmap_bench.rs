// HashMap Benchmarks - Rust
//
// Compile: rustc -O hashmap_bench.rs -o hashmap_bench.exe
// Run: ./hashmap_bench.exe
//
// Matches TML's hashmap_bench.tml and C++ collections_bench.cpp
// for direct performance comparison.

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

// HashMap insert (i64 -> i64)
fn bench_hashmap_insert(iterations: i64) -> i64 {
    let mut map = HashMap::with_capacity(16);
    for i in 0..iterations {
        map.insert(i, i * 2);
    }
    black_box(map.len() as i64)
}

// HashMap lookup
fn bench_hashmap_lookup(iterations: i64) -> i64 {
    let mut map = HashMap::with_capacity(10000);
    for i in 0..10000i64 {
        map.insert(i, i * 2);
    }

    let mut sum: i64 = 0;
    for i in 0..iterations {
        sum += map.get(&(i % 10000)).copied().unwrap_or(0);
    }
    black_box(sum)
}

// HashMap contains_key
fn bench_hashmap_contains(iterations: i64) -> i64 {
    let mut map = HashMap::with_capacity(10000);
    for i in 0..10000i64 {
        map.insert(i, i);
    }

    let mut found: i64 = 0;
    for i in 0..iterations {
        if map.contains_key(&(i % 20000)) {
            found += 1;
        }
    }
    black_box(found)
}

// HashMap remove
fn bench_hashmap_remove(iterations: i64) -> i64 {
    let mut map = HashMap::with_capacity(iterations as usize);
    for i in 0..iterations {
        map.insert(i, i);
    }

    let mut removed: i64 = 0;
    for i in 0..iterations {
        if map.remove(&i).is_some() {
            removed += 1;
        }
    }
    black_box(removed)
}

// HashMap string key insert + lookup
fn bench_hashmap_string_key(iterations: i64) -> i64 {
    let mut map = HashMap::with_capacity(iterations as usize);

    // Insert
    for i in 0..iterations {
        map.insert(format!("key{}", i), i);
    }

    // Lookup
    let mut sum: i64 = 0;
    for i in 0..iterations {
        if let Some(&v) = map.get(&format!("key{}", i)) {
            sum += v;
        }
    }
    black_box(sum)
}

// HashMap insert with reserve
fn bench_hashmap_insert_reserved(iterations: i64) -> i64 {
    let mut map: HashMap<i64, i64> = HashMap::with_capacity(iterations as usize);
    for i in 0..iterations {
        map.insert(i, i * 2);
    }
    black_box(map.len() as i64)
}

fn main() {
    println!();
    println!("================================================================");
    println!("  HashMap Benchmarks (Rust)");
    println!("================================================================");
    println!();

    let map_iter: i64 = 100_000;
    let lookup_iter: i64 = 1_000_000;

    // Warmup
    {
        let mut warmup = HashMap::new();
        for i in 0..100i64 {
            warmup.insert(i, i);
        }
        black_box(&warmup);
    }

    let start = Instant::now();
    let _ = bench_hashmap_insert(map_iter);
    run_and_print("HashMap Insert", map_iter, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_hashmap_insert_reserved(map_iter);
    run_and_print("HashMap Insert (reserved)", map_iter, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_hashmap_lookup(lookup_iter);
    run_and_print("HashMap Lookup", lookup_iter, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_hashmap_contains(lookup_iter);
    run_and_print("HashMap Contains", lookup_iter, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_hashmap_remove(map_iter);
    run_and_print("HashMap Remove", map_iter, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_hashmap_string_key(map_iter);
    run_and_print("HashMap String Key", map_iter, start.elapsed().as_nanos() as i64);
}
