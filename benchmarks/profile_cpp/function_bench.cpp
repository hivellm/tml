/**
 * Function Call Benchmarks (C++)
 *
 * Tests function call overhead: direct, indirect, recursive, inlined.
 */

#include "../common/bench.hpp"

#include <cstdint>
#include <functional>

volatile int64_t sink = 0;

// Simple function (should be inlined)
inline int64_t add_inline(int64_t a, int64_t b) {
    return a + b;
}

// Non-inline function
#ifdef _MSC_VER
__declspec(noinline)
#else
__attribute__((noinline))
#endif
int64_t add_noinline(int64_t a, int64_t b) {
    return a + b;
}

// Function with more parameters
#ifdef _MSC_VER
__declspec(noinline)
#else
__attribute__((noinline))
#endif
int64_t add_many_params(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f) {
    return a + b + c + d + e + f;
}

// Recursive function
int64_t fib_recursive(int n) {
    if (n <= 1)
        return n;
    return fib_recursive(n - 1) + fib_recursive(n - 2);
}

// Tail recursive (optimizable)
int64_t fib_tail_helper(int n, int64_t a, int64_t b) {
    if (n == 0)
        return a;
    if (n == 1)
        return b;
    return fib_tail_helper(n - 1, b, a + b);
}

int64_t fib_tail(int n) {
    return fib_tail_helper(n, 0, 1);
}

// Mutual recursion
int64_t is_even(int64_t n);
int64_t is_odd(int64_t n);

int64_t is_even(int64_t n) {
    if (n == 0)
        return 1;
    return is_odd(n - 1);
}

int64_t is_odd(int64_t n) {
    if (n == 0)
        return 0;
    return is_even(n - 1);
}

// Function pointer
typedef int64_t (*BinaryOp)(int64_t, int64_t);

int64_t apply_op(BinaryOp op, int64_t a, int64_t b) {
    return op(a, b);
}

int64_t mul_func(int64_t a, int64_t b) {
    return a * b;
}

// std::function (type-erased)
int64_t apply_std_func(std::function<int64_t(int64_t, int64_t)> f, int64_t a, int64_t b) {
    return f(a, b);
}

// Virtual function call
class Calculator {
public:
    virtual int64_t compute(int64_t a, int64_t b) = 0;
    virtual ~Calculator() = default;
};

class Adder : public Calculator {
public:
    int64_t compute(int64_t a, int64_t b) override {
        return a + b;
    }
};

class Multiplier : public Calculator {
public:
    int64_t compute(int64_t a, int64_t b) override {
        return a * b;
    }
};

// Benchmarks
void bench_inline_call(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum = add_inline(sum, i);
    }
    sink = sum;
}

void bench_direct_call(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum = add_noinline(sum, i);
    }
    sink = sum;
}

void bench_many_params(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum = add_many_params(i, i + 1, i + 2, i + 3, i + 4, i + 5);
    }
    sink = sum;
}

void bench_fib_recursive(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += fib_recursive(20);
    }
    sink = sum;
}

void bench_fib_tail(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += fib_tail(50);
    }
    sink = sum;
}

void bench_mutual_recursion(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += is_even(100);
    }
    sink = sum;
}

void bench_function_pointer(int64_t iterations) {
    int64_t sum = 0;
    BinaryOp op = mul_func;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += apply_op(op, i % 100, (i + 1) % 100);
    }
    sink = sum;
}

void bench_std_function(int64_t iterations) {
    int64_t sum = 0;
    std::function<int64_t(int64_t, int64_t)> f = [](int64_t a, int64_t b) { return a * b; };
    for (int64_t i = 0; i < iterations; ++i) {
        sum += apply_std_func(f, i % 100, (i + 1) % 100);
    }
    sink = sum;
}

void bench_virtual_call(int64_t iterations) {
    Adder adder;
    Calculator* calc = &adder;
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum = calc->compute(sum, i);
    }
    sink = sum;
}

void bench_devirtualized_call(int64_t iterations) {
    Adder adder;
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum = adder.compute(sum, i);
    }
    sink = sum;
}

int main() {
    bench::Benchmark b("Function Calls");

    const int64_t ITERATIONS = 10000000; // 10M
    const int64_t FIB_ITER = 1000;       // Fewer for recursive

    b.run_with_iter("Inline Call", ITERATIONS, bench_inline_call, 10);
    b.run_with_iter("Direct Call (noinline)", ITERATIONS, bench_direct_call, 10);
    b.run_with_iter("Many Parameters (6 args)", ITERATIONS, bench_many_params, 10);
    b.run_with_iter("Fibonacci Recursive (n=20)", FIB_ITER, bench_fib_recursive, 5);
    b.run_with_iter("Fibonacci Tail (n=50)", ITERATIONS, bench_fib_tail, 10);
    b.run_with_iter("Mutual Recursion (n=100)", ITERATIONS, bench_mutual_recursion, 10);
    b.run_with_iter("Function Pointer", ITERATIONS, bench_function_pointer, 10);
    b.run_with_iter("std::function", ITERATIONS, bench_std_function, 10);
    b.run_with_iter("Virtual Call", ITERATIONS, bench_virtual_call, 10);
    b.run_with_iter("Devirtualized Call", ITERATIONS, bench_devirtualized_call, 10);

    b.print_results();
    b.save_json("../results/function_cpp.json");

    return 0;
}
