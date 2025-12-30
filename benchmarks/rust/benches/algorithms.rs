// Algorithm Benchmarks - Rust
//
// Run with: cargo bench

use criterion::{black_box, criterion_group, criterion_main, Criterion};

// ============================================================================
// Factorial
// ============================================================================

fn factorial_recursive(n: i32) -> i32 {
    if n <= 1 {
        1
    } else {
        n * factorial_recursive(n - 1)
    }
}

fn factorial_iterative(n: i32) -> i32 {
    (2..=n).fold(1, |acc, x| acc * x)
}

// ============================================================================
// Fibonacci
// ============================================================================

fn fibonacci_recursive(n: i32) -> i32 {
    if n <= 1 {
        n
    } else {
        fibonacci_recursive(n - 1) + fibonacci_recursive(n - 2)
    }
}

fn fibonacci_iterative(n: i32) -> i32 {
    if n <= 1 {
        return n;
    }
    let mut a = 0;
    let mut b = 1;
    for _ in 2..=n {
        let temp = a + b;
        a = b;
        b = temp;
    }
    b
}

// ============================================================================
// GCD (Greatest Common Divisor)
// ============================================================================

fn gcd_recursive(a: i32, b: i32) -> i32 {
    if b == 0 {
        a
    } else {
        gcd_recursive(b, a % b)
    }
}

fn gcd_iterative(mut a: i32, mut b: i32) -> i32 {
    while b != 0 {
        let temp = b;
        b = a % b;
        a = temp;
    }
    a
}

// ============================================================================
// Power (Fast Exponentiation)
// ============================================================================

fn power_naive(base: i32, exp: i32) -> i32 {
    (0..exp).fold(1, |acc, _| acc * base)
}

fn power_fast(base: i32, exp: i32) -> i32 {
    if exp == 0 {
        return 1;
    }
    if exp == 1 {
        return base;
    }
    let half = power_fast(base, exp / 2);
    if exp % 2 == 0 {
        half * half
    } else {
        base * half * half
    }
}

// ============================================================================
// Prime Check
// ============================================================================

fn is_prime(n: i32) -> bool {
    if n <= 1 {
        return false;
    }
    if n <= 3 {
        return true;
    }
    if n % 2 == 0 || n % 3 == 0 {
        return false;
    }
    let mut i = 5;
    while i * i <= n {
        if n % i == 0 || n % (i + 2) == 0 {
            return false;
        }
        i += 6;
    }
    true
}

fn count_primes(limit: i32) -> i32 {
    (2..=limit).filter(|&n| is_prime(n)).count() as i32
}

// ============================================================================
// Collatz Conjecture
// ============================================================================

fn collatz_steps(mut n: i32) -> i32 {
    let mut steps = 0;
    while n != 1 {
        if n % 2 == 0 {
            n /= 2;
        } else {
            n = 3 * n + 1;
        }
        steps += 1;
    }
    steps
}

// ============================================================================
// Sum Range
// ============================================================================

fn sum_range(start: i32, end: i32) -> i32 {
    (start..=end).sum()
}

// ============================================================================
// Benchmarks
// ============================================================================

fn benchmark_factorial(c: &mut Criterion) {
    c.bench_function("factorial_recursive_10", |b| {
        b.iter(|| factorial_recursive(black_box(10)))
    });
    c.bench_function("factorial_iterative_10", |b| {
        b.iter(|| factorial_iterative(black_box(10)))
    });
}

fn benchmark_fibonacci(c: &mut Criterion) {
    c.bench_function("fibonacci_recursive_20", |b| {
        b.iter(|| fibonacci_recursive(black_box(20)))
    });
    c.bench_function("fibonacci_iterative_20", |b| {
        b.iter(|| fibonacci_iterative(black_box(20)))
    });
}

fn benchmark_gcd(c: &mut Criterion) {
    c.bench_function("gcd_recursive", |b| {
        b.iter(|| gcd_recursive(black_box(48), black_box(18)))
    });
    c.bench_function("gcd_iterative", |b| {
        b.iter(|| gcd_iterative(black_box(48), black_box(18)))
    });
}

fn benchmark_power(c: &mut Criterion) {
    c.bench_function("power_naive_2_10", |b| {
        b.iter(|| power_naive(black_box(2), black_box(10)))
    });
    c.bench_function("power_fast_2_10", |b| {
        b.iter(|| power_fast(black_box(2), black_box(10)))
    });
}

fn benchmark_primes(c: &mut Criterion) {
    c.bench_function("count_primes_100", |b| {
        b.iter(|| count_primes(black_box(100)))
    });
    c.bench_function("count_primes_1000", |b| {
        b.iter(|| count_primes(black_box(1000)))
    });
}

fn benchmark_collatz(c: &mut Criterion) {
    c.bench_function("collatz_27", |b| {
        b.iter(|| collatz_steps(black_box(27)))
    });
}

fn benchmark_sum(c: &mut Criterion) {
    c.bench_function("sum_range_1_100", |b| {
        b.iter(|| sum_range(black_box(1), black_box(100)))
    });
    c.bench_function("sum_range_1_10000", |b| {
        b.iter(|| sum_range(black_box(1), black_box(10000)))
    });
}

criterion_group!(
    benches,
    benchmark_factorial,
    benchmark_fibonacci,
    benchmark_gcd,
    benchmark_power,
    benchmark_primes,
    benchmark_collatz,
    benchmark_sum
);
criterion_main!(benches);
