/**
 * Memory Benchmarks (C++)
 *
 * Tests heap allocation, struct operations, and memory access patterns.
 * Establishes baseline for TML memory comparison.
 */

#include "../common/bench.hpp"

#include <cstdint>
#include <cstring>
#include <memory>

// Test structures
struct SmallStruct {
    int64_t a;
    int64_t b;
};

struct MediumStruct {
    int64_t a, b, c, d;
    double x, y, z, w;
};

struct LargeStruct {
    int64_t data[16];
    double coords[8];
};

// Prevent optimization
volatile int64_t sink = 0;
volatile void* ptr_sink = nullptr;

// malloc/free cycles
void bench_malloc_free(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        void* ptr = malloc(64);
        bench::do_not_optimize(ptr);
        free(ptr);
    }
}

// new/delete cycles (small struct)
void bench_new_delete_small(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        auto* ptr = new SmallStruct{i, i + 1};
        bench::do_not_optimize(ptr);
        delete ptr;
    }
}

// new/delete cycles (medium struct)
void bench_new_delete_medium(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        auto* ptr = new MediumStruct{i, i + 1, i + 2, i + 3, 1.0, 2.0, 3.0, 4.0};
        bench::do_not_optimize(ptr);
        delete ptr;
    }
}

// new/delete cycles (large struct)
void bench_new_delete_large(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        auto* ptr = new LargeStruct{};
        ptr->data[0] = i;
        bench::do_not_optimize(ptr);
        delete ptr;
    }
}

// Stack struct creation
void bench_stack_struct(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        SmallStruct s{i, i + 1};
        sum += s.a + s.b;
    }
    sink = sum;
}

// unique_ptr (RAII)
void bench_unique_ptr(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        auto ptr = std::make_unique<SmallStruct>(SmallStruct{i, i + 1});
        bench::do_not_optimize(ptr.get());
    }
}

// Struct copy
void bench_struct_copy(int64_t iterations) {
    MediumStruct src{1, 2, 3, 4, 1.0, 2.0, 3.0, 4.0};
    MediumStruct dst;
    for (int64_t i = 0; i < iterations; ++i) {
        dst = src;
        bench::do_not_optimize(&dst);
    }
    sink = dst.a;
}

// memcpy
void bench_memcpy(int64_t iterations) {
    char src[1024];
    char dst[1024];
    memset(src, 'x', 1024);

    for (int64_t i = 0; i < iterations; ++i) {
        memcpy(dst, src, 1024);
        bench::do_not_optimize(dst);
    }
    sink = dst[0];
}

// Array of structs allocation
void bench_array_alloc(int64_t iterations) {
    const int ARRAY_SIZE = 1000;
    for (int64_t i = 0; i < iterations; ++i) {
        auto* arr = new SmallStruct[ARRAY_SIZE];
        arr[0].a = i;
        bench::do_not_optimize(arr);
        delete[] arr;
    }
}

// Sequential memory access
void bench_sequential_access(int64_t iterations) {
    std::vector<int64_t> data(10000);
    for (int i = 0; i < 10000; ++i) {
        data[i] = i;
    }

    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += data[i % 10000];
    }
    sink = sum;
}

// Random memory access (cache unfriendly)
void bench_random_access(int64_t iterations) {
    std::vector<int64_t> data(10000);
    std::vector<int64_t> indices(10000);

    // Create pseudo-random indices
    for (int i = 0; i < 10000; ++i) {
        data[i] = i;
        indices[i] = (i * 7919 + 1) % 10000; // Simple PRNG
    }

    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += data[indices[i % 10000]];
    }
    sink = sum;
}

// Pointer indirection
void bench_pointer_indirection(int64_t iterations) {
    std::vector<std::unique_ptr<int64_t>> ptrs(1000);
    for (int i = 0; i < 1000; ++i) {
        ptrs[i] = std::make_unique<int64_t>(i);
    }

    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += *ptrs[i % 1000];
    }
    sink = sum;
}

int main() {
    bench::Benchmark b("Memory");

    const int64_t ALLOC_ITER = 1000000;   // 1M alloc cycles
    const int64_t ACCESS_ITER = 10000000; // 10M access ops
    const int64_t COPY_ITER = 1000000;    // 1M copies

    // Run benchmarks
    b.run_with_iter("malloc/free (64 bytes)", ALLOC_ITER, bench_malloc_free, 100);
    b.run_with_iter("new/delete Small (16 bytes)", ALLOC_ITER, bench_new_delete_small, 100);
    b.run_with_iter("new/delete Medium (64 bytes)", ALLOC_ITER, bench_new_delete_medium, 100);
    b.run_with_iter("new/delete Large (192 bytes)", ALLOC_ITER, bench_new_delete_large, 100);
    b.run_with_iter("Stack Struct Creation", ACCESS_ITER, bench_stack_struct, 100);
    b.run_with_iter("unique_ptr RAII", ALLOC_ITER, bench_unique_ptr, 100);
    b.run_with_iter("Struct Copy (64 bytes)", COPY_ITER, bench_struct_copy, 100);
    b.run_with_iter("memcpy (1KB)", COPY_ITER, bench_memcpy, 100);
    b.run_with_iter("Array Alloc (1000 structs)", ALLOC_ITER / 100, bench_array_alloc, 10);
    b.run_with_iter("Sequential Access", ACCESS_ITER, bench_sequential_access, 100);
    b.run_with_iter("Random Access", ACCESS_ITER, bench_random_access, 100);
    b.run_with_iter("Pointer Indirection", ACCESS_ITER, bench_pointer_indirection, 100);

    // Print results
    b.print_results();

    // Save JSON
    b.save_json("../results/memory_cpp.json");

    return 0;
}
