/**
 * Text/StringBuilder Benchmarks (C++)
 *
 * Tests stringstream and string builder patterns.
 * Compares with TML's Text type for efficient string building.
 */

#include "../common/bench.hpp"

#include <cstdint>
#include <sstream>
#include <string>

// Prevent optimization
volatile int64_t sink = 0;

// stringstream append
void bench_stringstream_append(int64_t iterations) {
    std::stringstream ss;
    for (int64_t i = 0; i < iterations; ++i) {
        ss << "item" << i << ",";
    }
    sink = ss.str().size();
}

// string reserve + append (like TML Text)
void bench_string_reserve_append(int64_t iterations) {
    std::string s;
    s.reserve(iterations * 10); // Estimate
    for (int64_t i = 0; i < iterations; ++i) {
        s += "item";
        s += std::to_string(i);
        s += ",";
    }
    sink = s.size();
}

// string naive append (O(n^2) worst case)
void bench_string_naive_append(int64_t iterations) {
    std::string s;
    for (int64_t i = 0; i < iterations; ++i) {
        s += "ab";
    }
    sink = s.size();
}

// Build JSON-like structure
void bench_build_json(int64_t iterations) {
    std::stringstream ss;
    ss << "{\"items\":[";
    for (int64_t i = 0; i < iterations; ++i) {
        if (i > 0)
            ss << ",";
        ss << "{\"id\":" << i << ",\"name\":\"item" << i << "\"}";
    }
    ss << "]}";
    sink = ss.str().size();
}

// Build HTML-like structure
void bench_build_html(int64_t iterations) {
    std::string html;
    html.reserve(iterations * 50);
    html += "<ul>\n";
    for (int64_t i = 0; i < iterations; ++i) {
        html += "  <li>Item ";
        html += std::to_string(i);
        html += "</li>\n";
    }
    html += "</ul>\n";
    sink = html.size();
}

// Build CSV-like data
void bench_build_csv(int64_t iterations) {
    std::string csv;
    csv.reserve(iterations * 30);
    csv += "id,name,value\n";
    for (int64_t i = 0; i < iterations; ++i) {
        csv += std::to_string(i);
        csv += ",item";
        csv += std::to_string(i);
        csv += ",";
        csv += std::to_string(i * 100);
        csv += "\n";
    }
    sink = csv.size();
}

// Repeated small appends (worst case for naive)
void bench_small_appends(int64_t iterations) {
    std::string s;
    s.reserve(iterations);
    for (int64_t i = 0; i < iterations; ++i) {
        s += 'x';
    }
    sink = s.size();
}

// Format numbers into string
void bench_number_formatting(int64_t iterations) {
    std::stringstream ss;
    for (int64_t i = 0; i < iterations; ++i) {
        ss << i << ":" << (i * 3.14159) << "; ";
    }
    sink = ss.str().size();
}

// Log message building
void bench_log_messages(int64_t iterations) {
    std::string log;
    log.reserve(iterations * 64);
    for (int64_t i = 0; i < iterations; ++i) {
        log += "[";
        log += std::to_string(i);
        log += "] INFO: Processing item #";
        log += std::to_string(i);
        log += " with value ";
        log += std::to_string(i * 42);
        log += "\n";
    }
    sink = log.size();
}

// Path building (common in file operations)
void bench_path_building(int64_t iterations) {
    std::string path;
    path.reserve(iterations * 20);
    for (int64_t i = 0; i < iterations; ++i) {
        path.clear();
        path += "/home/user/projects/app/src/module";
        path += std::to_string(i % 100);
        path += "/file";
        path += std::to_string(i);
        path += ".txt";
        bench::do_not_optimize(path.data());
    }
    sink = path.size();
}

int main() {
    bench::Benchmark b("Text/StringBuilder");

    const int64_t BUILD_ITER = 100000;   // 100K for build ops
    const int64_t APPEND_ITER = 1000000; // 1M for simple appends

    // Run benchmarks
    b.run_with_iter("stringstream Append", BUILD_ITER, bench_stringstream_append, 10);
    b.run_with_iter("string Reserve+Append", BUILD_ITER, bench_string_reserve_append, 10);
    b.run_with_iter("string Naive Append", BUILD_ITER, bench_string_naive_append, 10);
    b.run_with_iter("Build JSON", BUILD_ITER / 10, bench_build_json, 10, "10K items");
    b.run_with_iter("Build HTML", BUILD_ITER / 10, bench_build_html, 10, "10K items");
    b.run_with_iter("Build CSV", BUILD_ITER / 10, bench_build_csv, 10, "10K rows");
    b.run_with_iter("Small Appends (1 char)", APPEND_ITER, bench_small_appends, 10);
    b.run_with_iter("Number Formatting", BUILD_ITER, bench_number_formatting, 10);
    b.run_with_iter("Log Messages", BUILD_ITER, bench_log_messages, 10);
    b.run_with_iter("Path Building", BUILD_ITER, bench_path_building, 10);

    // Print results
    b.print_results();

    // Save JSON
    b.save_json("../results/text_cpp.json");

    return 0;
}
