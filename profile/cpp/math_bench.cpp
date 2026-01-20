/**
 * Math Benchmarks (C++)
 *
 * Tests basic arithmetic operations and loop performance.
 * This establishes the baseline for TML comparison.
 */

#include "../common/bench.hpp"

#include <cmath>
#include <cstdint>

// Prevent optimization
volatile int64_t sink = 0;

// Integer addition benchmark
void bench_int_add(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += i;
    }
    sink = sum;
}

// Integer multiplication benchmark
void bench_int_mul(int64_t iterations) {
    int64_t prod = 1;
    for (int64_t i = 1; i <= iterations; ++i) {
        prod = (prod * i) % 1000000007; // Prevent overflow with modulo
    }
    sink = prod;
}

// Float addition benchmark
void bench_float_add(int64_t iterations) {
    double sum = 0.0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += static_cast<double>(i) * 0.001;
    }
    bench::do_not_optimize(sum);
}

// Float multiplication benchmark
void bench_float_mul(int64_t iterations) {
    double prod = 1.0;
    for (int64_t i = 1; i <= iterations; ++i) {
        prod *= 1.0000001; // Small multiplier to avoid infinity
    }
    bench::do_not_optimize(prod);
}

// Fibonacci (recursive) - tests function call overhead
int64_t fib(int n) {
    if (n <= 1)
        return n;
    return fib(n - 1) + fib(n - 2);
}

void bench_fib_recursive(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += fib(20); // fib(20) = 6765
    }
    sink = sum;
}

// Fibonacci (iterative) - tests loop performance
int64_t fib_iter(int n) {
    if (n <= 1)
        return n;
    int64_t a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        int64_t temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

void bench_fib_iterative(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += fib_iter(50);
    }
    sink = sum;
}

// Empty loop - measures loop overhead
void bench_empty_loop(int64_t iterations) {
    volatile int64_t counter = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        counter = i;
    }
}

// Square root benchmark
void bench_sqrt(int64_t iterations) {
    double sum = 0.0;
    for (int64_t i = 1; i <= iterations; ++i) {
        sum += std::sqrt(static_cast<double>(i));
    }
    bench::do_not_optimize(sum);
}

// Division benchmark (integer)
void bench_int_div(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 1; i <= iterations; ++i) {
        sum += (i * 1000000) / (i + 1);
    }
    sink = sum;
}

// Modulo benchmark
void bench_int_mod(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 1; i <= iterations; ++i) {
        sum += i % 17;
    }
    sink = sum;
}

// Bitwise operations
void bench_bitwise(int64_t iterations) {
    int64_t result = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        result = (result ^ i) | (i & 0xFF) | ((i << 3) >> 1);
    }
    sink = result;
}

int main() {
    bench::Benchmark b("Math");

    const int64_t ITERATIONS = 10000000; // 10M for most tests
    const int64_t FIB_ITERATIONS = 1000; // Fewer for recursive fib

    // Run benchmarks
    b.run_with_iter("Integer Addition", ITERATIONS, bench_int_add, 10, "sum of 0..N");
    b.run_with_iter("Integer Multiplication", ITERATIONS, bench_int_mul, 10, "product mod 1e9+7");
    b.run_with_iter("Integer Division", ITERATIONS, bench_int_div, 10);
    b.run_with_iter("Integer Modulo", ITERATIONS, bench_int_mod, 10);
    b.run_with_iter("Bitwise Operations", ITERATIONS, bench_bitwise, 10);
    b.run_with_iter("Float Addition", ITERATIONS, bench_float_add, 10);
    b.run_with_iter("Float Multiplication", ITERATIONS, bench_float_mul, 10);
    b.run_with_iter("Square Root", ITERATIONS, bench_sqrt, 10);
    b.run_with_iter("Fibonacci Recursive (n=20)", FIB_ITERATIONS, bench_fib_recursive, 10);
    b.run_with_iter("Fibonacci Iterative (n=50)", ITERATIONS, bench_fib_iterative, 10);
    b.run_with_iter("Empty Loop", ITERATIONS, bench_empty_loop, 10, "baseline overhead");

    // Print results
    b.print_results();

    // Save JSON
    b.save_json("../results/math_cpp.json");

    return 0;
}
