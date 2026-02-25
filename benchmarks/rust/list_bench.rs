// List/Vec Benchmarks - Rust
//
// Compile: rustc -O list_bench.rs -o list_bench.exe
// Run: ./list_bench.exe
//
// All benchmarks use 1M operations for accurate ns/op measurement.
// black_box() prevents dead code elimination.

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

// Vec push (grow from empty)
fn bench_vec_push(n: i64) -> i64 {
    let mut vec = Vec::new();
    for i in 0..n {
        vec.push(i);
    }
    black_box(vec.len() as i64)
}

// Vec push with pre-reserved capacity
fn bench_vec_push_reserved(n: i64) -> i64 {
    let mut vec = Vec::with_capacity(n as usize);
    for i in 0..n {
        vec.push(i);
    }
    black_box(vec.len() as i64)
}

// Vec random access (1M accesses in 10K-element vec)
fn bench_vec_access(n: i64) -> i64 {
    let mut vec = Vec::with_capacity(10000);
    for i in 0..10000i64 {
        vec.push(i * 2);
    }

    let mut sum: i64 = 0;
    for i in 0..n {
        sum += vec[(i % 10000) as usize];
    }
    black_box(sum)
}

// Vec iteration (sequential, 100 rounds of 10K)
fn bench_vec_iterate(n: i64) -> i64 {
    let mut vec = Vec::with_capacity(10000);
    for i in 0..10000i64 {
        vec.push(i);
    }

    let mut sum: i64 = 0;
    for _ in 0..(n / 10000) {
        for i in 0..10000 {
            sum += vec[i as usize];
        }
    }
    black_box(sum)
}

// Vec pop (push N then pop all)
fn bench_vec_pop(n: i64) -> i64 {
    let mut vec = Vec::with_capacity(n as usize);
    for i in 0..n {
        vec.push(i);
    }

    let mut sum: i64 = 0;
    while let Some(v) = vec.pop() {
        sum += v;
    }
    black_box(sum)
}

// Vec set (modify 1M elements in 10K-element vec)
fn bench_vec_set(n: i64) -> i64 {
    let mut vec = vec![0i64; 10000];

    for i in 0..n {
        vec[(i % 10000) as usize] = i;
    }
    black_box(vec[0] + vec[9999])
}

fn main() {
    println!();
    println!("================================================================");
    println!("  List/Vec Benchmarks (Rust)");
    println!("================================================================");
    println!();

    let n: i64 = 10_000_000;

    // Warmup
    {
        let mut warmup = Vec::new();
        for i in 0..1000i64 {
            warmup.push(i);
        }
        black_box(&warmup);
    }

    let start = Instant::now();
    let _ = bench_vec_push(n);
    run_and_print("List Push (grow)", n, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_vec_push_reserved(n);
    run_and_print("List Push (reserved)", n, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_vec_access(n);
    run_and_print("List Random Access", n, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_vec_iterate(n);
    run_and_print("List Iteration", n, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_vec_pop(n);
    run_and_print("List Pop", n, start.elapsed().as_nanos() as i64);

    let start = Instant::now();
    let _ = bench_vec_set(n);
    run_and_print("List Set", n, start.elapsed().as_nanos() as i64);
}
