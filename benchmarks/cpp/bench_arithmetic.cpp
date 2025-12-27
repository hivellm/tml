// Benchmark: Basic Arithmetic Operations (No Allocation)
// Category: arithmetic
// Description: Tests pure computation speed

#include <chrono>
#include <cstdint>
#include <cstdio>

using namespace std::chrono;

// Prevent compiler from optimizing away the result
volatile int64_t sink = 0;

// Benchmark 1: Integer addition loop
int64_t bench_int_add(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += i;
    }
    return sum;
}

// Benchmark 2: Integer multiplication
int64_t bench_int_mul(int64_t iterations) {
    int64_t product = 1;
    for (int64_t i = 1; i <= iterations; ++i) {
        product = (product * i) % 10000007; // Prevent overflow
    }
    return product;
}

// Benchmark 3: Mixed arithmetic
int64_t bench_mixed_ops(int64_t iterations) {
    int64_t a = 1, b = 2, c = 3;
    for (int64_t i = 0; i < iterations; ++i) {
        a = (a + b) * c % 10000007;
        b = (b * c + a) % 10000007;
        c = (c + a - b) % 10000007;
        if (c < 0)
            c += 10000007;
    }
    return a + b + c;
}

// Benchmark 4: Fibonacci iterative
int64_t bench_fibonacci(int64_t n) {
    if (n <= 1)
        return n;
    int64_t a = 0, b = 1;
    for (int64_t i = 2; i <= n; ++i) {
        int64_t temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

// Benchmark 5: Prime counting (sieve-like)
int64_t bench_count_primes(int64_t limit) {
    int64_t count = 0;
    for (int64_t n = 2; n <= limit; ++n) {
        bool is_prime = true;
        for (int64_t i = 2; i * i <= n; ++i) {
            if (n % i == 0) {
                is_prime = false;
                break;
            }
        }
        if (is_prime)
            ++count;
    }
    return count;
}

void run_benchmark(const char* name, int64_t (*func)(int64_t), int64_t arg, int runs) {
    // Warmup
    sink = func(arg);

    // Timed runs
    double total_ms = 0;
    for (int r = 0; r < runs; ++r) {
        auto start = high_resolution_clock::now();
        sink = func(arg);
        auto end = high_resolution_clock::now();
        total_ms += duration<double, std::milli>(end - start).count();
    }

    double avg_ms = total_ms / runs;
    printf("%s: %.3f ms (avg of %d runs)\n", name, avg_ms, runs);
}

int main() {
    printf("=== C++ Arithmetic Benchmarks ===\n\n");

    const int RUNS = 3;

    run_benchmark("int_add_1M", bench_int_add, 1000000, RUNS);
    run_benchmark("int_mul_100K", bench_int_mul, 100000, RUNS);
    run_benchmark("mixed_ops_100K", bench_mixed_ops, 100000, RUNS);
    run_benchmark("fibonacci_10K", bench_fibonacci, 10000, RUNS);
    run_benchmark("count_primes_1K", bench_count_primes, 1000, RUNS);

    printf("\nDone.\n");
    return 0;
}
