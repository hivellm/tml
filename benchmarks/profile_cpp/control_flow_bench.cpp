/**
 * Control Flow Benchmarks (C++)
 *
 * Tests branching and loop performance.
 */

#include "../common/bench.hpp"

#include <cstdint>

volatile int64_t sink = 0;

// Simple if/else chain
void bench_if_else_chain(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        int64_t x = i % 100;
        if (x < 25) {
            sum += 1;
        } else if (x < 50) {
            sum += 2;
        } else if (x < 75) {
            sum += 3;
        } else {
            sum += 4;
        }
    }
    sink = sum;
}

// Deeply nested if
void bench_nested_if(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        int64_t x = i % 16;
        if (x & 1) {
            if (x & 2) {
                if (x & 4) {
                    if (x & 8) {
                        sum += 15;
                    } else {
                        sum += 7;
                    }
                } else {
                    sum += 3;
                }
            } else {
                sum += 1;
            }
        } else {
            sum += 0;
        }
    }
    sink = sum;
}

// Switch statement (dense)
void bench_switch_dense(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        int x = static_cast<int>(i % 10);
        switch (x) {
        case 0:
            sum += 0;
            break;
        case 1:
            sum += 1;
            break;
        case 2:
            sum += 2;
            break;
        case 3:
            sum += 3;
            break;
        case 4:
            sum += 4;
            break;
        case 5:
            sum += 5;
            break;
        case 6:
            sum += 6;
            break;
        case 7:
            sum += 7;
            break;
        case 8:
            sum += 8;
            break;
        case 9:
            sum += 9;
            break;
        default:
            sum += 10;
            break;
        }
    }
    sink = sum;
}

// Switch statement (sparse)
void bench_switch_sparse(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        int x = static_cast<int>((i * 100) % 1000);
        switch (x) {
        case 0:
            sum += 0;
            break;
        case 100:
            sum += 1;
            break;
        case 200:
            sum += 2;
            break;
        case 300:
            sum += 3;
            break;
        case 400:
            sum += 4;
            break;
        case 500:
            sum += 5;
            break;
        case 600:
            sum += 6;
            break;
        case 700:
            sum += 7;
            break;
        case 800:
            sum += 8;
            break;
        case 900:
            sum += 9;
            break;
        default:
            sum += 10;
            break;
        }
    }
    sink = sum;
}

// For loop (simple)
void bench_for_loop(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += i;
    }
    sink = sum;
}

// While loop with break
void bench_while_break(int64_t iterations) {
    int64_t sum = 0;
    int64_t i = 0;
    while (true) {
        if (i >= iterations)
            break;
        sum += i;
        i++;
    }
    sink = sum;
}

// Nested loops (matrix style)
void bench_nested_loops(int64_t iterations) {
    int64_t sum = 0;
    int64_t n = 1000;
    int64_t rounds = iterations / (n * n);
    if (rounds < 1)
        rounds = 1;

    for (int64_t r = 0; r < rounds; ++r) {
        for (int64_t i = 0; i < n; ++i) {
            for (int64_t j = 0; j < n; ++j) {
                sum += (i * n + j);
            }
        }
    }
    sink = sum;
}

// Loop with continue
void bench_loop_continue(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        if (i % 2 == 0)
            continue;
        sum += i;
    }
    sink = sum;
}

// Ternary operator chain
void bench_ternary_chain(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        int64_t x = i % 100;
        sum += (x < 25) ? 1 : (x < 50) ? 2 : (x < 75) ? 3 : 4;
    }
    sink = sum;
}

// Boolean short-circuit AND
void bench_short_circuit_and(int64_t iterations) {
    int64_t count = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        if ((i % 2 == 0) && (i % 3 == 0) && (i % 5 == 0)) {
            count++;
        }
    }
    sink = count;
}

// Boolean short-circuit OR
void bench_short_circuit_or(int64_t iterations) {
    int64_t count = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        if ((i % 2 == 0) || (i % 3 == 0) || (i % 5 == 0)) {
            count++;
        }
    }
    sink = count;
}

int main() {
    bench::Benchmark b("Control Flow");

    const int64_t ITERATIONS = 10000000; // 10M
    const int64_t NESTED_ITER = 1000000; // 1M for nested loops

    b.run_with_iter("If-Else Chain (4 branches)", ITERATIONS, bench_if_else_chain, 10);
    b.run_with_iter("Nested If (4 levels)", ITERATIONS, bench_nested_if, 10);
    b.run_with_iter("Switch Dense (10 cases)", ITERATIONS, bench_switch_dense, 10);
    b.run_with_iter("Switch Sparse (10 cases)", ITERATIONS, bench_switch_sparse, 10);
    b.run_with_iter("For Loop", ITERATIONS, bench_for_loop, 10);
    b.run_with_iter("While + Break", ITERATIONS, bench_while_break, 10);
    b.run_with_iter("Nested Loops (1000x1000)", NESTED_ITER, bench_nested_loops, 5);
    b.run_with_iter("Loop + Continue", ITERATIONS, bench_loop_continue, 10);
    b.run_with_iter("Ternary Chain", ITERATIONS, bench_ternary_chain, 10);
    b.run_with_iter("Short-Circuit AND", ITERATIONS, bench_short_circuit_and, 10);
    b.run_with_iter("Short-Circuit OR", ITERATIONS, bench_short_circuit_or, 10);

    b.print_results();
    b.save_json("../results/control_flow_cpp.json");

    return 0;
}
