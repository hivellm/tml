/**
 * Type Conversion Benchmarks (C++)
 *
 * Tests type conversion overhead: int casts, float conversions, pointer casts.
 */

#include "../common/bench.hpp"

#include <cstdint>
#include <cstring>

volatile int64_t sink_i64 = 0;
volatile double sink_f64 = 0.0;
volatile void* sink_ptr = nullptr;

// Integer widening (i32 -> i64)
void bench_int_widen(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        int32_t small = static_cast<int32_t>(i & 0x7FFFFFFF);
        int64_t big = static_cast<int64_t>(small);
        sum += big;
    }
    sink_i64 = sum;
}

// Integer narrowing (i64 -> i32)
void bench_int_narrow(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        int64_t big = i;
        int32_t small = static_cast<int32_t>(big);
        sum += small;
    }
    sink_i64 = sum;
}

// Unsigned to signed
void bench_unsigned_to_signed(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        uint64_t u = static_cast<uint64_t>(i);
        int64_t s = static_cast<int64_t>(u);
        sum += s;
    }
    sink_i64 = sum;
}

// Signed to unsigned
void bench_signed_to_unsigned(int64_t iterations) {
    uint64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        int64_t s = i;
        uint64_t u = static_cast<uint64_t>(s);
        sum += u;
    }
    sink_i64 = static_cast<int64_t>(sum);
}

// Int to float
void bench_int_to_float(int64_t iterations) {
    double sum = 0.0;
    for (int64_t i = 0; i < iterations; ++i) {
        double f = static_cast<double>(i);
        sum += f;
    }
    sink_f64 = sum;
}

// Float to int
void bench_float_to_int(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        double f = static_cast<double>(i) + 0.5;
        int64_t n = static_cast<int64_t>(f);
        sum += n;
    }
    sink_i64 = sum;
}

// Float widening (f32 -> f64)
void bench_float_widen(int64_t iterations) {
    double sum = 0.0;
    for (int64_t i = 0; i < iterations; ++i) {
        float small = static_cast<float>(i % 1000);
        double big = static_cast<double>(small);
        sum += big;
    }
    sink_f64 = sum;
}

// Float narrowing (f64 -> f32)
void bench_float_narrow(int64_t iterations) {
    float sum = 0.0f;
    for (int64_t i = 0; i < iterations; ++i) {
        double big = static_cast<double>(i % 1000);
        float small = static_cast<float>(big);
        sum += small;
    }
    sink_f64 = static_cast<double>(sum);
}

// Byte to int chain (i8 -> i16 -> i32 -> i64)
void bench_byte_chain(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        int8_t b = static_cast<int8_t>(i & 0x7F);
        int16_t s = static_cast<int16_t>(b);
        int32_t m = static_cast<int32_t>(s);
        int64_t l = static_cast<int64_t>(m);
        sum += l;
    }
    sink_i64 = sum;
}

// Mixed type arithmetic
void bench_mixed_arithmetic(int64_t iterations) {
    double sum = 0.0;
    for (int64_t i = 0; i < iterations; ++i) {
        int32_t a = static_cast<int32_t>(i % 100);
        float b = static_cast<float>(i % 50);
        int64_t c = i % 25;
        double d = static_cast<double>(i % 10);

        // Forces multiple conversions
        sum += static_cast<double>(a) + static_cast<double>(b) + static_cast<double>(c) + d;
    }
    sink_f64 = sum;
}

// Pointer cast (via intptr)
void bench_ptr_to_int(int64_t iterations) {
    int64_t arr[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        void* ptr = &arr[i % 10];
        intptr_t addr = reinterpret_cast<intptr_t>(ptr);
        sum += addr & 0xFF;
    }
    sink_i64 = sum;
}

// Int to pointer
void bench_int_to_ptr(int64_t iterations) {
    int64_t base = 0x1000;
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        intptr_t addr = static_cast<intptr_t>(base + (i % 1000) * 8);
        void* ptr = reinterpret_cast<void*>(addr);
        sum += reinterpret_cast<intptr_t>(ptr) & 0xFF;
    }
    sink_i64 = sum;
}

// Bit reinterpret (double <-> i64)
void bench_bit_reinterpret(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        double d = static_cast<double>(i);
        int64_t bits;
        std::memcpy(&bits, &d, sizeof(bits));
        double back;
        std::memcpy(&back, &bits, sizeof(back));
        sum += static_cast<int64_t>(back);
    }
    sink_i64 = sum;
}

int main() {
    bench::Benchmark b("Type Conversions");

    const int64_t ITERATIONS = 10000000; // 10M

    b.run_with_iter("Int Widen (i32->i64)", ITERATIONS, bench_int_widen, 10);
    b.run_with_iter("Int Narrow (i64->i32)", ITERATIONS, bench_int_narrow, 10);
    b.run_with_iter("Unsigned to Signed", ITERATIONS, bench_unsigned_to_signed, 10);
    b.run_with_iter("Signed to Unsigned", ITERATIONS, bench_signed_to_unsigned, 10);
    b.run_with_iter("Int to Float (i64->f64)", ITERATIONS, bench_int_to_float, 10);
    b.run_with_iter("Float to Int (f64->i64)", ITERATIONS, bench_float_to_int, 10);
    b.run_with_iter("Float Widen (f32->f64)", ITERATIONS, bench_float_widen, 10);
    b.run_with_iter("Float Narrow (f64->f32)", ITERATIONS, bench_float_narrow, 10);
    b.run_with_iter("Byte Chain (i8->i64)", ITERATIONS, bench_byte_chain, 10);
    b.run_with_iter("Mixed Type Arithmetic", ITERATIONS, bench_mixed_arithmetic, 10);
    b.run_with_iter("Pointer to Int", ITERATIONS, bench_ptr_to_int, 10);
    b.run_with_iter("Int to Pointer", ITERATIONS, bench_int_to_ptr, 10);
    b.run_with_iter("Bit Reinterpret", ITERATIONS, bench_bit_reinterpret, 10);

    b.print_results();
    b.save_json("../results/type_cpp.json");

    return 0;
}
