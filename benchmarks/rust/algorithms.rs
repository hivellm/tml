// Algorithm Benchmarks - Rust
//
// Compile: rustc -O algorithms.rs -o algorithms.exe
// Run: ./algorithms.exe

use std::time::Instant;

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
// Benchmark Helper
// ============================================================================

fn benchmark<F, R>(name: &str, iterations: u32, f: F)
where
    F: Fn() -> R,
{
    // Warmup
    let _ = f();

    let start = Instant::now();
    for _ in 0..iterations {
        std::hint::black_box(f());
    }
    let elapsed = start.elapsed();
    let ns_per_op = elapsed.as_nanos() as f64 / iterations as f64;
    println!("{}: {:.4} ns/op", name, ns_per_op);
}

// ============================================================================
// Main
// ============================================================================

fn main() {
    println!("=== Rust Algorithm Benchmarks ===");
    println!();

    // Correctness tests
    println!("Factorial(10): {}", factorial_iterative(10));
    println!("Fibonacci(20): {}", fibonacci_iterative(20));
    println!("GCD(48, 18): {}", gcd_iterative(48, 18));
    println!("Power(2, 10): {}", power_fast(2, 10));
    println!("Primes up to 100: {}", count_primes(100));
    println!("Sum(1..100): {}", sum_range(1, 100));
    println!("Collatz steps(27): {}", collatz_steps(27));

    println!();
    println!("=== Timing (ns per call) ===");
    println!();

    let iterations = 1_000_000u32;
    let small_iterations = 10_000u32;

    benchmark("factorial_recursive(10)", iterations, || factorial_recursive(10));
    benchmark("factorial_iterative(10)", iterations, || factorial_iterative(10));
    benchmark("fibonacci_recursive(20)", small_iterations, || fibonacci_recursive(20));
    benchmark("fibonacci_iterative(20)", iterations, || fibonacci_iterative(20));
    benchmark("gcd_recursive(48, 18)", iterations, || gcd_recursive(48, 18));
    benchmark("gcd_iterative(48, 18)", iterations, || gcd_iterative(48, 18));
    benchmark("power_naive(2, 10)", iterations, || power_naive(2, 10));
    benchmark("power_fast(2, 10)", iterations, || power_fast(2, 10));
    benchmark("count_primes(100)", 100_000, || count_primes(100));
    benchmark("count_primes(1000)", 10_000, || count_primes(1000));
    benchmark("collatz_steps(27)", iterations, || collatz_steps(27));
    benchmark("sum_range(1, 100)", iterations, || sum_range(1, 100));
    benchmark("sum_range(1, 10000)", 100_000, || sum_range(1, 10000));
}
