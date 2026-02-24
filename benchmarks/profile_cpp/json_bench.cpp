/**
 * JSON Benchmarks (C++)
 *
 * Tests JSON parsing and access performance.
 * Uses the TML compiler's internal JSON parser for fair comparison.
 */

#include "../../compiler/include/json/json.hpp"
#include "../../compiler/include/json/json_fast_parser.hpp"
#include "../common/bench.hpp"

// Test JSON strings
const char* TINY_JSON = R"({"name":"John","age":30})";

const char* SMALL_JSON = R"({
    "name": "John Doe",
    "age": 30,
    "active": true,
    "email": "john@example.com",
    "scores": [95, 87, 92, 88, 91],
    "address": {
        "street": "123 Main St",
        "city": "New York",
        "zip": "10001"
    }
})";

const char* MEDIUM_JSON = R"({
    "users": [
        {"id": 1, "name": "Alice", "email": "alice@example.com", "active": true},
        {"id": 2, "name": "Bob", "email": "bob@example.com", "active": false},
        {"id": 3, "name": "Charlie", "email": "charlie@example.com", "active": true},
        {"id": 4, "name": "Diana", "email": "diana@example.com", "active": true},
        {"id": 5, "name": "Eve", "email": "eve@example.com", "active": false}
    ],
    "metadata": {
        "total": 5,
        "page": 1,
        "per_page": 10,
        "has_more": false
    }
})";

// Prevent optimization
volatile int64_t sink = 0;

// Parse tiny JSON (27 bytes)
void bench_parse_tiny(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        auto result = tml::json::fast::parse_json_fast(TINY_JSON);
        bench::do_not_optimize(result);
    }
}

// Parse small JSON (~200 bytes)
void bench_parse_small(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        auto result = tml::json::fast::parse_json_fast(SMALL_JSON);
        bench::do_not_optimize(result);
    }
}

// Parse medium JSON (~500 bytes)
void bench_parse_medium(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        auto result = tml::json::fast::parse_json_fast(MEDIUM_JSON);
        bench::do_not_optimize(result);
    }
}

// Parse with standard parser (non-SIMD)
void bench_parse_standard(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        auto result = tml::json::parse_json(SMALL_JSON);
        bench::do_not_optimize(result);
    }
}

// Field access (parse + access field)
void bench_field_access(int64_t iterations) {
    int64_t total_age = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        auto result = tml::json::fast::parse_json_fast(SMALL_JSON);
        if (tml::is_ok(result)) {
            auto& val = tml::unwrap(result);
            if (val.is_object()) {
                auto* age = val.get("age");
                if (age && age->is_number()) {
                    total_age += static_cast<int64_t>(age->as_number());
                }
            }
        }
    }
    sink = total_age;
}

// Array iteration
void bench_array_iterate(int64_t iterations) {
    int64_t total = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        auto result = tml::json::fast::parse_json_fast(SMALL_JSON);
        if (tml::is_ok(result)) {
            auto& val = tml::unwrap(result);
            if (val.is_object()) {
                auto* scores = val.get("scores");
                if (scores && scores->is_array()) {
                    for (size_t j = 0; j < scores->array_size(); ++j) {
                        auto* elem = scores->at(j);
                        if (elem && elem->is_number()) {
                            total += static_cast<int64_t>(elem->as_number());
                        }
                    }
                }
            }
        }
    }
    sink = total;
}

// Nested object access
void bench_nested_access(int64_t iterations) {
    int64_t count = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        auto result = tml::json::fast::parse_json_fast(SMALL_JSON);
        if (tml::is_ok(result)) {
            auto& val = tml::unwrap(result);
            if (val.is_object()) {
                auto* addr = val.get("address");
                if (addr && addr->is_object()) {
                    auto* city = addr->get("city");
                    if (city && city->is_string()) {
                        count++;
                    }
                }
            }
        }
    }
    sink = count;
}

// Parse and validate (type checks on all fields)
void bench_parse_validate(int64_t iterations) {
    int64_t valid = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        auto result = tml::json::fast::parse_json_fast(SMALL_JSON);
        if (tml::is_ok(result)) {
            auto& val = tml::unwrap(result);
            if (val.is_object()) {
                bool ok = true;
                ok = ok && val.get("name") && val.get("name")->is_string();
                ok = ok && val.get("age") && val.get("age")->is_number();
                ok = ok && val.get("active") && val.get("active")->is_bool();
                ok = ok && val.get("scores") && val.get("scores")->is_array();
                ok = ok && val.get("address") && val.get("address")->is_object();
                if (ok)
                    valid++;
            }
        }
    }
    sink = valid;
}

// Object traversal (iterate all keys)
void bench_object_traverse(int64_t iterations) {
    int64_t keys = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        auto result = tml::json::fast::parse_json_fast(SMALL_JSON);
        if (tml::is_ok(result)) {
            auto& val = tml::unwrap(result);
            if (val.is_object()) {
                keys += val.object_size();
            }
        }
    }
    sink = keys;
}

int main() {
    bench::Benchmark b("JSON");

    const int64_t PARSE_ITER = 100000;  // 100K parses
    const int64_t ACCESS_ITER = 100000; // 100K access ops

    // Run benchmarks
    b.run_with_iter("Parse Tiny (27 bytes)", PARSE_ITER, bench_parse_tiny, 100);
    b.run_with_iter("Parse Small (200 bytes)", PARSE_ITER, bench_parse_small, 100);
    b.run_with_iter("Parse Medium (500 bytes)", PARSE_ITER, bench_parse_medium, 100);
    b.run_with_iter("Parse Standard (non-SIMD)", PARSE_ITER, bench_parse_standard, 100);
    b.run_with_iter("Field Access", ACCESS_ITER, bench_field_access, 100);
    b.run_with_iter("Array Iteration", ACCESS_ITER, bench_array_iterate, 100);
    b.run_with_iter("Nested Object Access", ACCESS_ITER, bench_nested_access, 100);
    b.run_with_iter("Parse + Validate", ACCESS_ITER, bench_parse_validate, 100);
    b.run_with_iter("Object Traversal", ACCESS_ITER, bench_object_traverse, 100);

    // Print results
    b.print_results();

    // Save JSON
    b.save_json("../results/json_cpp.json");

    return 0;
}
