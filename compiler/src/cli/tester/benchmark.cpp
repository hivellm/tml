//! # Benchmark Framework
//!
//! This file implements the benchmark runner for `tml test --bench`.
//!
//! ## Benchmark Files
//!
//! Benchmarks are defined in `*.bench.tml` files using the `@bench` decorator:
//!
//! ```tml
//! @bench
//! func bench_sorting() {
//!     let data = generate_data(1000)
//!     data.sort()
//! }
//! ```
//!
//! ## Output Format
//!
//! ```text
//!  + bench bubble_sort ... 45,230 ns/iter
//!  + bench quick_sort  ...  1,234 ns/iter (+3645.9% vs baseline)
//! ```
//!
//! ## Baseline Comparison
//!
//! - `--save-baseline=<name>`: Save results to baseline file
//! - `--compare=<name>`: Compare against saved baseline

#include "log/log.hpp"
#include "tester_internal.hpp"

namespace tml::cli::tester {

// ============================================================================
// Parse Benchmark Output
// ============================================================================

/// Parses benchmark output from the test runner format.
std::vector<BenchmarkResult> parse_bench_output(const std::string& output,
                                                const std::string& file_path) {
    std::vector<BenchmarkResult> results;
    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        // Look for lines matching: "  + bench NAME ... X ns/iter"
        size_t bench_pos = line.find("+ bench ");
        if (bench_pos == std::string::npos)
            continue;

        size_t dots_pos = line.find(" ... ");
        if (dots_pos == std::string::npos)
            continue;

        size_t ns_pos = line.find(" ns/iter");
        if (ns_pos == std::string::npos)
            continue;

        // Extract bench name (between "bench " and " ...")
        size_t name_start = bench_pos + 8; // after "+ bench "
        std::string name = line.substr(name_start, dots_pos - name_start);
        // Trim trailing spaces
        while (!name.empty() && name.back() == ' ')
            name.pop_back();

        // Extract ns value (between " ... " and " ns/iter")
        size_t value_start = dots_pos + 5; // after " ... "
        std::string value_str = line.substr(value_start, ns_pos - value_start);
        // Trim leading/trailing spaces
        while (!value_str.empty() && value_str.front() == ' ')
            value_str.erase(0, 1);
        while (!value_str.empty() && value_str.back() == ' ')
            value_str.pop_back();

        int64_t ns_per_iter = 0;
        try {
            ns_per_iter = std::stoll(value_str);
        } catch (...) {
            continue;
        }

        BenchmarkResult result;
        result.file_path = file_path;
        result.bench_name = name;
        result.ns_per_iter = ns_per_iter;
        result.passed = true;
        results.push_back(result);
    }

    return results;
}

// ============================================================================
// Save Benchmark Baseline
// ============================================================================

void save_benchmark_baseline(const std::string& filename,
                             const std::vector<BenchmarkResult>& results) {
    std::ofstream out(filename);
    out << "{\n  \"benchmarks\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"file\": \"" << r.file_path << "\",\n";
        out << "      \"name\": \"" << r.bench_name << "\",\n";
        out << "      \"ns_per_iter\": " << r.ns_per_iter << "\n";
        out << "    }";
        if (i + 1 < results.size())
            out << ",";
        out << "\n";
    }

    out << "  ]\n}\n";
}

// ============================================================================
// Load Benchmark Baseline
// ============================================================================

std::map<std::string, int64_t> load_benchmark_baseline(const std::string& filename) {
    std::map<std::string, int64_t> baseline;
    std::ifstream in(filename);
    if (!in)
        return baseline;

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // Simple JSON parsing for our format
    size_t pos = 0;
    while ((pos = content.find("\"name\":", pos)) != std::string::npos) {
        // Find name value
        size_t name_start = content.find('"', pos + 7);
        if (name_start == std::string::npos)
            break;
        size_t name_end = content.find('"', name_start + 1);
        if (name_end == std::string::npos)
            break;
        std::string name = content.substr(name_start + 1, name_end - name_start - 1);

        // Find ns_per_iter value
        size_t ns_pos = content.find("\"ns_per_iter\":", name_end);
        if (ns_pos == std::string::npos)
            break;
        size_t value_start = ns_pos + 14;
        while (value_start < content.size() &&
               (content[value_start] == ' ' || content[value_start] == ':'))
            value_start++;

        size_t value_end = value_start;
        while (value_end < content.size() &&
               (std::isdigit(content[value_end]) || content[value_end] == '-'))
            value_end++;

        try {
            int64_t ns = std::stoll(content.substr(value_start, value_end - value_start));
            baseline[name] = ns;
        } catch (...) {}

        pos = value_end;
    }

    return baseline;
}

// ============================================================================
// Run Benchmarks
// ============================================================================

int run_benchmarks(const TestOptions& opts, const ColorOutput& c) {
    std::string cwd = fs::current_path().string();
    std::vector<std::string> bench_files = discover_bench_files(cwd);

    if (bench_files.empty()) {
        if (!opts.quiet) {
            std::cout << c.yellow() << "No benchmark files found" << c.reset()
                      << " (looking for *.bench.tml)\n";
        }
        return 0;
    }

    // Filter by pattern if provided
    if (!opts.patterns.empty()) {
        std::vector<std::string> filtered;
        for (const auto& file : bench_files) {
            for (const auto& pattern : opts.patterns) {
                if (file.find(pattern) != std::string::npos) {
                    filtered.push_back(file);
                    break;
                }
            }
        }
        bench_files = filtered;
    }

    if (bench_files.empty()) {
        if (!opts.quiet) {
            std::cout << c.yellow() << "No benchmarks matched the specified pattern(s)" << c.reset()
                      << "\n";
        }
        return 0;
    }

    // Load baseline for comparison if specified
    std::map<std::string, int64_t> baseline;
    if (!opts.compare_baseline.empty()) {
        baseline = load_benchmark_baseline(opts.compare_baseline);
        if (baseline.empty()) {
            TML_LOG_WARN("test", "Could not load baseline from " << opts.compare_baseline);
        }
    }

    // Print header
    if (!opts.quiet) {
        std::cout << "\n " << c.cyan() << c.bold() << "TML Benchmarks" << c.reset() << " "
                  << c.dim() << "v0.1.0" << c.reset() << "\n";
        std::cout << "\n " << c.dim() << "Running " << bench_files.size() << " benchmark file"
                  << (bench_files.size() != 1 ? "s" : "") << "..." << c.reset() << "\n\n";
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    int failed = 0;
    std::vector<BenchmarkResult> all_results;

    // Run each benchmark file (sequentially for consistent timing)
    for (const auto& file : bench_files) {
        std::string bench_name = fs::path(file).stem().string();

        if (!opts.quiet) {
            std::cout << " " << c.magenta() << "+" << c.reset() << " " << c.bold() << bench_name
                      << c.reset() << "\n";
        }

        std::vector<std::string> empty_args;
        std::string captured_output;

        int exit_code;
        if (opts.nocapture) {
            exit_code = run_run(file, empty_args, opts.release, false, opts.no_cache);
        } else {
            exit_code = run_run_quiet(file, empty_args, opts.release, &captured_output, false,
                                      opts.no_cache);

            // Parse and collect results
            auto results = parse_bench_output(captured_output, file);
            for (auto& r : results) {
                all_results.push_back(r);
            }

            // Print the benchmark output with comparison if available
            if (!baseline.empty()) {
                std::istringstream iss(captured_output);
                std::string line;
                while (std::getline(iss, line)) {
                    std::cout << line;

                    // Check if this is a benchmark result line
                    for (const auto& r : results) {
                        if (line.find("+ bench " + r.bench_name) != std::string::npos &&
                            line.find(" ns/iter") != std::string::npos) {
                            auto it = baseline.find(r.bench_name);
                            if (it != baseline.end()) {
                                int64_t old_ns = it->second;
                                int64_t new_ns = r.ns_per_iter;
                                if (old_ns > 0) {
                                    double change = ((double)(new_ns - old_ns) / old_ns) * 100.0;
                                    if (change < -5.0) {
                                        std::cout << " " << c.green() << "(" << std::fixed
                                                  << std::setprecision(1) << change << "%)"
                                                  << c.reset();
                                    } else if (change > 5.0) {
                                        std::cout << " " << c.red() << "(+" << std::fixed
                                                  << std::setprecision(1) << change << "%)"
                                                  << c.reset();
                                    } else {
                                        std::cout << " " << c.dim() << "(~" << std::fixed
                                                  << std::setprecision(1) << change << "%)"
                                                  << c.reset();
                                    }
                                }
                            }
                            break;
                        }
                    }
                    std::cout << "\n";
                }
            } else if (!captured_output.empty()) {
                std::cout << captured_output;
            }
        }

        if (exit_code != 0) {
            failed++;
            if (!opts.quiet) {
                std::cout << "   " << c.red() << "x" << c.reset() << " Failed with exit code "
                          << exit_code << "\n";
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    int64_t total_duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Save baseline if requested
    if (!opts.save_baseline.empty() && !all_results.empty()) {
        save_benchmark_baseline(opts.save_baseline, all_results);
        if (!opts.quiet) {
            std::cout << "\n " << c.dim() << "Saved baseline to " << opts.save_baseline << c.reset()
                      << "\n";
        }
    }

    // Print summary
    if (!opts.quiet) {
        std::cout << "\n " << c.bold() << "Bench Files " << c.reset();
        if (failed > 0) {
            std::cout << c.red() << c.bold() << failed << " failed" << c.reset() << " | ";
        }
        std::cout << c.green() << c.bold() << (bench_files.size() - failed) << " passed"
                  << c.reset() << " " << c.gray() << "(" << bench_files.size() << ")" << c.reset()
                  << "\n";
        std::cout << " " << c.bold() << "Duration    " << c.reset()
                  << format_duration(total_duration_ms) << "\n\n";
    }

    return failed > 0 ? 1 : 0;
}

} // namespace tml::cli::tester
