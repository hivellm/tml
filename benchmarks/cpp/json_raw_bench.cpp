/**
 * Raw JSON Parser Benchmark
 *
 * Mede a performance real do parser C++ sem nenhuma camada TML.
 */

#include <chrono>
#include <iostream>
#include <string>

// Include the actual parser
#include "../../compiler/include/json/json.hpp"
#include "../../compiler/include/json/json_fast_parser.hpp"

using namespace std::chrono;

const char* SMALL_JSON = R"({"name":"John Doe","age":30,"active":true,"email":"john@example.com","scores":[95,87,92,88,91],"address":{"street":"123 Main St","city":"New York","zip":"10001"}})";
const char* TINY_JSON = R"({"name":"John","age":30})";

void benchmark_parse_only(int iterations) {
    std::cout << "\n=== PARSE ONLY (no allocation) ===" << std::endl;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto result = tml::json::fast::parse_json_fast(SMALL_JSON);
        // Result destructor runs, freeing memory
        (void)result;
    }

    auto end = high_resolution_clock::now();
    auto total_ns = duration_cast<nanoseconds>(end - start).count();

    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Total time: " << total_ns / 1000000 << " ms" << std::endl;
    std::cout << "  Per op:     " << total_ns / iterations << " ns" << std::endl;
    std::cout << "  Ops/sec:    " << (iterations * 1000000000LL) / total_ns << std::endl;
}

void benchmark_with_vector(int iterations) {
    std::cout << "\n=== WITH VECTOR STORAGE (simulates handle system) ===" << std::endl;

    std::vector<tml::json::JsonValue> values;
    values.reserve(iterations);

    auto start = high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto result = tml::json::fast::parse_json_fast(SMALL_JSON);
        if (tml::is_ok(result)) {
            values.push_back(std::move(tml::unwrap(result)));
        }
    }

    auto end = high_resolution_clock::now();
    auto total_ns = duration_cast<nanoseconds>(end - start).count();

    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Total time: " << total_ns / 1000000 << " ms" << std::endl;
    std::cout << "  Per op:     " << total_ns / iterations << " ns" << std::endl;
    std::cout << "  Ops/sec:    " << (iterations * 1000000000LL) / total_ns << std::endl;

    values.clear();
}

void benchmark_ffi_simulation(int iterations) {
    std::cout << "\n=== FFI SIMULATION (string copy + parse + handle) ===" << std::endl;

    std::string json_str(SMALL_JSON);
    std::vector<tml::json::JsonValue> handles;
    std::vector<bool> handles_free;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        // Simulate FFI: copy string (TML passes const char*)
        const char* str = json_str.c_str();

        // Parse
        auto result = tml::json::fast::parse_json_fast(str);

        // Allocate handle (simplified - no linear search)
        if (tml::is_ok(result)) {
            handles.push_back(std::move(tml::unwrap(result)));
            handles_free.push_back(false);
        }
    }

    auto end = high_resolution_clock::now();
    auto total_ns = duration_cast<nanoseconds>(end - start).count();

    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Total time: " << total_ns / 1000000 << " ms" << std::endl;
    std::cout << "  Per op:     " << total_ns / iterations << " ns" << std::endl;
    std::cout << "  Ops/sec:    " << (iterations * 1000000000LL) / total_ns << std::endl;

    handles.clear();
}

void benchmark_standard_parser(int iterations) {
    std::cout << "\n=== STANDARD PARSER (non-SIMD) ===" << std::endl;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto result = tml::json::parse_json(SMALL_JSON);
        (void)result;
    }

    auto end = high_resolution_clock::now();
    auto total_ns = duration_cast<nanoseconds>(end - start).count();

    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Total time: " << total_ns / 1000000 << " ms" << std::endl;
    std::cout << "  Per op:     " << total_ns / iterations << " ns" << std::endl;
    std::cout << "  Ops/sec:    " << (iterations * 1000000000LL) / total_ns << std::endl;
}

void benchmark_tiny(int iterations) {
    std::cout << "\n=== TINY JSON (27 bytes) - Same as TML test ===" << std::endl;
    std::cout << "JSON: " << TINY_JSON << " (" << strlen(TINY_JSON) << " bytes)" << std::endl;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto result = tml::json::fast::parse_json_fast(TINY_JSON);
        (void)result;
    }

    auto end = high_resolution_clock::now();
    auto total_ns = duration_cast<nanoseconds>(end - start).count();

    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Total time: " << total_ns / 1000000 << " ms" << std::endl;
    std::cout << "  Per op:     " << total_ns / iterations << " ns" << std::endl;
    std::cout << "  Ops/sec:    " << (iterations * 1000000000LL) / total_ns << std::endl;
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "   TML JSON Parser Raw C++ Benchmark" << std::endl;
    std::cout << "============================================" << std::endl;

    std::cout << "\nJSON size: " << strlen(SMALL_JSON) << " bytes" << std::endl;
    std::cout << "JSON: " << SMALL_JSON << std::endl;

    const int ITERATIONS = 100000;

    // Warmup
    for (int i = 0; i < 1000; ++i) {
        auto result = tml::json::fast::parse_json_fast(SMALL_JSON);
        (void)result;
    }

    benchmark_parse_only(ITERATIONS);
    benchmark_standard_parser(ITERATIONS);
    benchmark_tiny(ITERATIONS);

    std::cout << "\n============================================" << std::endl;
    std::cout << "               COMPARISON" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "\nC++ tiny JSON:   See above" << std::endl;
    std::cout << "TML tiny JSON:   5847 ns (measured)" << std::endl;
    std::cout << "\nOverhead = TML - C++ = ??? ns" << std::endl;

    return 0;
}
