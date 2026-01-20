/**
 * TML Profile Benchmark Framework (C++)
 *
 * Common utilities for consistent benchmarking across all C++ tests.
 * Each benchmark should output results in a standardized JSON format
 * for easy comparison with TML benchmarks.
 */

#pragma once

#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace bench {

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::nanoseconds;

struct BenchResult {
    std::string name;
    std::string category;
    int64_t iterations;
    int64_t total_ns;
    int64_t per_op_ns;
    int64_t ops_per_sec;
    std::string notes;
};

class Benchmark {
public:
    explicit Benchmark(const std::string& category) : category_(category) {}

    // Run a benchmark with warmup
    template <typename Func>
    BenchResult run(const std::string& name, int64_t iterations, Func&& func, int warmup = 1000,
                    const std::string& notes = "") {
        // Warmup
        for (int i = 0; i < warmup; ++i) {
            func();
        }

        // Actual benchmark
        auto start = Clock::now();
        for (int64_t i = 0; i < iterations; ++i) {
            func();
        }
        auto end = Clock::now();

        auto total_ns = std::chrono::duration_cast<Duration>(end - start).count();
        auto per_op_ns = total_ns / iterations;
        auto ops_per_sec =
            iterations > 0 && total_ns > 0 ? (iterations * 1000000000LL) / total_ns : 0;

        BenchResult result{name, category_, iterations, total_ns, per_op_ns, ops_per_sec, notes};
        results_.push_back(result);
        return result;
    }

    // Run benchmark that takes iteration count
    template <typename Func>
    BenchResult run_with_iter(const std::string& name, int64_t iterations, Func&& func,
                              int warmup = 100, const std::string& notes = "") {
        // Warmup
        for (int i = 0; i < warmup; ++i) {
            func(100);
        }

        // Actual benchmark
        auto start = Clock::now();
        func(iterations);
        auto end = Clock::now();

        auto total_ns = std::chrono::duration_cast<Duration>(end - start).count();
        auto per_op_ns = total_ns / iterations;
        auto ops_per_sec =
            iterations > 0 && total_ns > 0 ? (iterations * 1000000000LL) / total_ns : 0;

        BenchResult result{name, category_, iterations, total_ns, per_op_ns, ops_per_sec, notes};
        results_.push_back(result);
        return result;
    }

    // Print results to stdout
    void print_results() const {
        std::cout << "\n";
        std::cout << "================================================================\n";
        std::cout << "  " << category_ << " Benchmarks (C++)\n";
        std::cout << "================================================================\n\n";

        for (const auto& r : results_) {
            std::cout << "  " << r.name << ":\n";
            std::cout << "    Iterations: " << r.iterations << "\n";
            std::cout << "    Total time: " << r.total_ns / 1000000 << " ms\n";
            std::cout << "    Per op:     " << r.per_op_ns << " ns\n";
            std::cout << "    Ops/sec:    " << r.ops_per_sec << "\n";
            if (!r.notes.empty()) {
                std::cout << "    Notes:      " << r.notes << "\n";
            }
            std::cout << "\n";
        }
    }

    // Output JSON format for comparison
    void output_json(std::ostream& out) const {
        out << "{\n";
        out << "  \"category\": \"" << category_ << "\",\n";
        out << "  \"language\": \"cpp\",\n";
        out << "  \"results\": [\n";

        for (size_t i = 0; i < results_.size(); ++i) {
            const auto& r = results_[i];
            out << "    {\n";
            out << "      \"name\": \"" << r.name << "\",\n";
            out << "      \"iterations\": " << r.iterations << ",\n";
            out << "      \"total_ns\": " << r.total_ns << ",\n";
            out << "      \"per_op_ns\": " << r.per_op_ns << ",\n";
            out << "      \"ops_per_sec\": " << r.ops_per_sec;
            if (!r.notes.empty()) {
                out << ",\n      \"notes\": \"" << r.notes << "\"";
            }
            out << "\n    }";
            if (i < results_.size() - 1)
                out << ",";
            out << "\n";
        }

        out << "  ]\n";
        out << "}\n";
    }

    // Save JSON to file
    void save_json(const std::string& filename) const {
        std::ofstream file(filename);
        if (file.is_open()) {
            output_json(file);
            file.close();
        }
    }

    const std::vector<BenchResult>& results() const {
        return results_;
    }

private:
    std::string category_;
    std::vector<BenchResult> results_;
};

// Helper to prevent compiler optimization of dead code
template <typename T> inline void do_not_optimize(T&& value) {
    asm volatile("" : : "g"(value) : "memory");
}

// Get current time in nanoseconds
inline int64_t time_ns() {
    return std::chrono::duration_cast<Duration>(Clock::now().time_since_epoch()).count();
}

} // namespace bench
