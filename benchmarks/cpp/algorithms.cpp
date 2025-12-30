// Algorithm Benchmarks - C++
//
// Compile: g++ -O3 -o algorithms algorithms.cpp
// Run: ./algorithms

#include <chrono>
#include <cstdint>
#include <iostream>

// ============================================================================
// Factorial
// ============================================================================

int32_t factorial_recursive(int32_t n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial_recursive(n - 1);
}

int32_t factorial_iterative(int32_t n) {
    int32_t result = 1;
    for (int32_t i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}

// ============================================================================
// Fibonacci
// ============================================================================

int32_t fibonacci_recursive(int32_t n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci_recursive(n - 1) + fibonacci_recursive(n - 2);
}

int32_t fibonacci_iterative(int32_t n) {
    if (n <= 1) {
        return n;
    }
    int32_t a = 0, b = 1;
    for (int32_t i = 2; i <= n; ++i) {
        int32_t temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

// ============================================================================
// GCD (Greatest Common Divisor)
// ============================================================================

int32_t gcd_recursive(int32_t a, int32_t b) {
    if (b == 0) {
        return a;
    }
    return gcd_recursive(b, a % b);
}

int32_t gcd_iterative(int32_t a, int32_t b) {
    while (b != 0) {
        int32_t temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

// ============================================================================
// Power (Fast Exponentiation)
// ============================================================================

int32_t power_naive(int32_t base, int32_t exp) {
    int32_t result = 1;
    for (int32_t i = 0; i < exp; ++i) {
        result *= base;
    }
    return result;
}

int32_t power_fast(int32_t base, int32_t exp) {
    if (exp == 0) {
        return 1;
    }
    if (exp == 1) {
        return base;
    }
    int32_t half = power_fast(base, exp / 2);
    if (exp % 2 == 0) {
        return half * half;
    }
    return base * half * half;
}

// ============================================================================
// Prime Check
// ============================================================================

bool is_prime(int32_t n) {
    if (n <= 1) {
        return false;
    }
    if (n <= 3) {
        return true;
    }
    if (n % 2 == 0 || n % 3 == 0) {
        return false;
    }
    for (int32_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return false;
        }
    }
    return true;
}

int32_t count_primes(int32_t limit) {
    int32_t count = 0;
    for (int32_t n = 2; n <= limit; ++n) {
        if (is_prime(n)) {
            ++count;
        }
    }
    return count;
}

// ============================================================================
// Collatz Conjecture
// ============================================================================

int32_t collatz_steps(int32_t n) {
    int32_t steps = 0;
    while (n != 1) {
        if (n % 2 == 0) {
            n /= 2;
        } else {
            n = 3 * n + 1;
        }
        ++steps;
    }
    return steps;
}

// ============================================================================
// Sum Range
// ============================================================================

int32_t sum_range(int32_t start, int32_t end) {
    int32_t sum = 0;
    for (int32_t i = start; i <= end; ++i) {
        sum += i;
    }
    return sum;
}

// ============================================================================
// Timing Helper
// ============================================================================

template <typename Func> double benchmark(Func func, int iterations = 1000000) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        volatile auto result = func();
        (void)result;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return static_cast<double>(duration.count()) / iterations;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== C++ Algorithm Benchmarks ===\n\n";

    // Correctness tests
    std::cout << "Factorial(10): " << factorial_iterative(10) << "\n";
    std::cout << "Fibonacci(20): " << fibonacci_iterative(20) << "\n";
    std::cout << "GCD(48, 18): " << gcd_iterative(48, 18) << "\n";
    std::cout << "Power(2, 10): " << power_fast(2, 10) << "\n";
    std::cout << "Primes up to 100: " << count_primes(100) << "\n";
    std::cout << "Sum(1..100): " << sum_range(1, 100) << "\n";
    std::cout << "Collatz steps(27): " << collatz_steps(27) << "\n";

    std::cout << "\n=== Timing (ns per call) ===\n\n";

    // Benchmarks
    std::cout << "factorial_recursive(10): " << benchmark([]() { return factorial_recursive(10); })
              << " ns\n";
    std::cout << "factorial_iterative(10): " << benchmark([]() { return factorial_iterative(10); })
              << " ns\n";

    std::cout << "fibonacci_recursive(20): "
              << benchmark([]() { return fibonacci_recursive(20); }, 10000) << " ns\n";
    std::cout << "fibonacci_iterative(20): " << benchmark([]() { return fibonacci_iterative(20); })
              << " ns\n";

    std::cout << "gcd_recursive(48, 18): " << benchmark([]() { return gcd_recursive(48, 18); })
              << " ns\n";
    std::cout << "gcd_iterative(48, 18): " << benchmark([]() { return gcd_iterative(48, 18); })
              << " ns\n";

    std::cout << "power_naive(2, 10): " << benchmark([]() { return power_naive(2, 10); })
              << " ns\n";
    std::cout << "power_fast(2, 10): " << benchmark([]() { return power_fast(2, 10); }) << " ns\n";

    std::cout << "count_primes(100): " << benchmark([]() { return count_primes(100); }, 100000)
              << " ns\n";
    std::cout << "count_primes(1000): " << benchmark([]() { return count_primes(1000); }, 10000)
              << " ns\n";

    std::cout << "collatz_steps(27): " << benchmark([]() { return collatz_steps(27); }) << " ns\n";

    std::cout << "sum_range(1, 100): " << benchmark([]() { return sum_range(1, 100); }) << " ns\n";
    std::cout << "sum_range(1, 10000): " << benchmark([]() { return sum_range(1, 10000); }, 100000)
              << " ns\n";

    return 0;
}
