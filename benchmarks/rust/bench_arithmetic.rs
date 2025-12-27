// Benchmark: Basic Arithmetic Operations (No Allocation)
// Category: arithmetic
// Description: Tests pure computation speed

use std::time::Instant;
use std::hint::black_box;

// Benchmark 1: Integer addition loop
fn bench_int_add(iterations: i64) -> i64 {
    let mut sum: i64 = 0;
    for i in 0..iterations {
        sum += i;
    }
    sum
}

// Benchmark 2: Integer multiplication
fn bench_int_mul(iterations: i64) -> i64 {
    let mut product: i64 = 1;
    for i in 1..=iterations {
        product = (product * i) % 1000000007;
    }
    product
}

// Benchmark 3: Mixed arithmetic
fn bench_mixed_ops(iterations: i64) -> i64 {
    let mut a: i64 = 1;
    let mut b: i64 = 2;
    let mut c: i64 = 3;
    for _ in 0..iterations {
        a = (a + b) * c % 1000000007;
        b = (b * c + a) % 1000000007;
        c = (c + a - b) % 1000000007;
        if c < 0 { c += 1000000007; }
    }
    a + b + c
}

// Benchmark 4: Fibonacci iterative
fn bench_fibonacci(n: i64) -> i64 {
    if n <= 1 { return n; }
    let mut a: i64 = 0;
    let mut b: i64 = 1;
    for _ in 2..=n {
        let temp = a + b;
        a = b;
        b = temp;
    }
    b
}

// Benchmark 5: Prime counting
fn bench_count_primes(limit: i64) -> i64 {
    let mut count: i64 = 0;
    for n in 2..=limit {
        let mut is_prime = true;
        let mut i = 2;
        while i * i <= n {
            if n % i == 0 {
                is_prime = false;
                break;
            }
            i += 1;
        }
        if is_prime { count += 1; }
    }
    count
}

fn run_benchmark<F>(name: &str, func: F, arg: i64, runs: i32)
where
    F: Fn(i64) -> i64,
{
    // Warmup
    black_box(func(arg));

    // Timed runs
    let mut total_ms = 0.0f64;
    for _ in 0..runs {
        let start = Instant::now();
        black_box(func(arg));
        let elapsed = start.elapsed();
        total_ms += elapsed.as_secs_f64() * 1000.0;
    }

    let avg_ms = total_ms / runs as f64;
    println!("{}: {:.3} ms (avg of {} runs)", name, avg_ms, runs);
}

fn main() {
    println!("=== Rust Arithmetic Benchmarks ===\n");

    const RUNS: i32 = 3;

    run_benchmark("int_add_1M", bench_int_add, 1000000, RUNS);
    run_benchmark("int_mul_100K", bench_int_mul, 100000, RUNS);
    run_benchmark("mixed_ops_100K", bench_mixed_ops, 100000, RUNS);
    run_benchmark("fibonacci_10K", bench_fibonacci, 10000, RUNS);
    run_benchmark("count_primes_1K", bench_count_primes, 1000, RUNS);

    println!("\nDone.");
}
