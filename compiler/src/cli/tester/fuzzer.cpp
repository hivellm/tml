//! # Fuzz Testing Framework
//!
//! This file implements the fuzz testing runner for `tml test --fuzz`.
//!
//! ## Fuzz Test Files
//!
//! Fuzz tests are defined in `*.fuzz.tml` files using the `@fuzz` decorator:
//!
//! ```tml
//! @fuzz
//! func fuzz_parser(data: [U8]) {
//!     let input = String::from_bytes(data)
//!     parse_expression(input)  // Should not crash
//! }
//! ```
//!
//! ## Fuzzing Process
//!
//! 1. Generate random input bytes
//! 2. Call the fuzz function with the input
//! 3. Catch crashes and save crashing inputs
//! 4. Repeat for `--fuzz-duration` seconds
//!
//! ## Corpus Management
//!
//! - `--corpus=<dir>`: Use existing corpus as seed inputs
//! - `--crashes=<dir>`: Save crash-inducing inputs
//! - `--fuzz-max-len=<n>`: Maximum input size in bytes

#include "log/log.hpp"
#include "tester_internal.hpp"

#include <cstdlib>
#include <ctime>
#include <random>

namespace tml::cli::tester {

// ============================================================================
// Discover Fuzz Files
// ============================================================================

/// Discovers fuzz test files (`*.fuzz.tml`) in the project.
std::vector<std::string> discover_fuzz_files(const std::string& root_dir) {
    std::vector<std::string> fuzz_files;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
            if (!entry.is_regular_file())
                continue;

            std::string path_str = entry.path().string();
            std::string filename = entry.path().filename().string();

            // Skip build directories, errors directories, pending directories
            if (path_str.find("build") != std::string::npos ||
                path_str.find("errors") != std::string::npos ||
                path_str.find("pending") != std::string::npos) {
                continue;
            }

            // Include .fuzz.tml files
            if (filename.ends_with(".fuzz.tml")) {
                fuzz_files.push_back(path_str);
            }
        }
    } catch (const fs::filesystem_error& e) {
        TML_LOG_ERROR("test", "Error discovering fuzz files: " << e.what());
    }

    // Sort by name
    std::sort(fuzz_files.begin(), fuzz_files.end());
    return fuzz_files;
}

// ============================================================================
// Fuzz Input Generation
// ============================================================================

// Thread-local random engine for fuzzing
static thread_local std::mt19937 rng(std::random_device{}());

std::vector<uint8_t> generate_fuzz_input(size_t max_len) {
    std::uniform_int_distribution<size_t> len_dist(1, max_len);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    size_t len = len_dist(rng);
    std::vector<uint8_t> input(len);

    for (size_t i = 0; i < len; ++i) {
        input[i] = static_cast<uint8_t>(byte_dist(rng));
    }

    return input;
}

std::vector<uint8_t> mutate_fuzz_input(const std::vector<uint8_t>& input, size_t max_len) {
    if (input.empty()) {
        return generate_fuzz_input(max_len);
    }

    std::vector<uint8_t> mutated = input;
    std::uniform_int_distribution<int> mutation_type(0, 5);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    switch (mutation_type(rng)) {
    case 0: // Flip a random bit
        if (!mutated.empty()) {
            std::uniform_int_distribution<size_t> pos_dist(0, mutated.size() - 1);
            std::uniform_int_distribution<int> bit_dist(0, 7);
            size_t pos = pos_dist(rng);
            mutated[pos] ^= (1 << bit_dist(rng));
        }
        break;

    case 1: // Replace a random byte
        if (!mutated.empty()) {
            std::uniform_int_distribution<size_t> pos_dist(0, mutated.size() - 1);
            mutated[pos_dist(rng)] = static_cast<uint8_t>(byte_dist(rng));
        }
        break;

    case 2: // Insert a random byte
        if (mutated.size() < max_len) {
            std::uniform_int_distribution<size_t> pos_dist(0, mutated.size());
            size_t pos = pos_dist(rng);
            mutated.insert(mutated.begin() + pos, static_cast<uint8_t>(byte_dist(rng)));
        }
        break;

    case 3: // Delete a random byte
        if (mutated.size() > 1) {
            std::uniform_int_distribution<size_t> pos_dist(0, mutated.size() - 1);
            mutated.erase(mutated.begin() + pos_dist(rng));
        }
        break;

    case 4: // Swap two random bytes
        if (mutated.size() > 1) {
            std::uniform_int_distribution<size_t> pos_dist(0, mutated.size() - 1);
            size_t pos1 = pos_dist(rng);
            size_t pos2 = pos_dist(rng);
            std::swap(mutated[pos1], mutated[pos2]);
        }
        break;

    case 5: // Duplicate a section
        if (mutated.size() > 1 && mutated.size() < max_len / 2) {
            std::uniform_int_distribution<size_t> pos_dist(0, mutated.size() - 1);
            std::uniform_int_distribution<size_t> len_dist(
                1, std::min(mutated.size(), max_len - mutated.size()));
            size_t pos = pos_dist(rng);
            size_t len = len_dist(rng);
            if (pos + len <= mutated.size()) {
                std::vector<uint8_t> section(mutated.begin() + pos, mutated.begin() + pos + len);
                mutated.insert(mutated.end(), section.begin(), section.end());
            }
        }
        break;
    }

    // Trim to max length
    if (mutated.size() > max_len) {
        mutated.resize(max_len);
    }

    return mutated;
}

// ============================================================================
// Hex Conversion
// ============================================================================

std::string bytes_to_hex(const std::vector<uint8_t>& bytes) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);

    for (uint8_t byte : bytes) {
        result += hex_chars[(byte >> 4) & 0xF];
        result += hex_chars[byte & 0xF];
    }

    return result;
}

std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);

    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        uint8_t high = (hex[i] >= 'a') ? (hex[i] - 'a' + 10) : (hex[i] - '0');
        uint8_t low = (hex[i + 1] >= 'a') ? (hex[i + 1] - 'a' + 10) : (hex[i + 1] - '0');
        bytes.push_back((high << 4) | low);
    }

    return bytes;
}

// ============================================================================
// Save Crash Input
// ============================================================================

static void save_crash_input(const std::string& crashes_dir, const std::string& fuzz_name,
                             const std::vector<uint8_t>& input) {
    // Create crashes directory if it doesn't exist
    fs::create_directories(crashes_dir);

    // Generate filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << crashes_dir << "/" << fuzz_name << "_" << time << ".crash";

    std::ofstream out(ss.str(), std::ios::binary);
    if (out) {
        out.write(reinterpret_cast<const char*>(input.data()), input.size());
        TML_LOG_INFO("test", "Crash input saved to: " << ss.str());
    }
}

// ============================================================================
// Load Corpus
// ============================================================================

static std::vector<std::vector<uint8_t>> load_corpus(const std::string& corpus_dir) {
    std::vector<std::vector<uint8_t>> corpus;

    if (corpus_dir.empty() || !fs::exists(corpus_dir)) {
        return corpus;
    }

    try {
        for (const auto& entry : fs::directory_iterator(corpus_dir)) {
            if (!entry.is_regular_file())
                continue;

            std::ifstream in(entry.path(), std::ios::binary);
            if (in) {
                std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
                if (!data.empty()) {
                    corpus.push_back(std::move(data));
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // Ignore errors
    }

    return corpus;
}

// ============================================================================
// Run Fuzz Tests
// ============================================================================

int run_fuzz_tests(const TestOptions& opts, const ColorOutput& c) {
    std::string cwd = fs::current_path().string();
    std::vector<std::string> fuzz_files = discover_fuzz_files(cwd);

    if (fuzz_files.empty()) {
        if (!opts.quiet) {
            TML_LOG_INFO("test",
                         c.yellow() << "No fuzz files found" << c.reset()
                                    << " (looking for *.fuzz.tml)");
        }
        return 0;
    }

    // Filter by pattern if provided
    if (!opts.patterns.empty()) {
        std::vector<std::string> filtered;
        for (const auto& file : fuzz_files) {
            for (const auto& pattern : opts.patterns) {
                if (file.find(pattern) != std::string::npos) {
                    filtered.push_back(file);
                    break;
                }
            }
        }
        fuzz_files = filtered;
    }

    if (fuzz_files.empty()) {
        if (!opts.quiet) {
            TML_LOG_INFO("test",
                         c.yellow() << "No fuzz tests matched the specified pattern(s)" << c.reset());
        }
        return 0;
    }

    // Print header
    if (!opts.quiet) {
        TML_LOG_INFO("test", c.cyan() << c.bold() << "TML Fuzzer" << c.reset() << " " << c.dim()
                                      << "v0.1.0" << c.reset());
        TML_LOG_INFO("test", c.dim() << "Running " << fuzz_files.size() << " fuzz target"
                                     << (fuzz_files.size() != 1 ? "s" : "") << " for "
                                     << opts.fuzz_duration << "s each..." << c.reset());
    }

    auto overall_start = std::chrono::high_resolution_clock::now();
    int crashes_found = 0;
    std::vector<FuzzResult> all_results;

    // Determine crashes directory
    std::string crashes_dir = opts.crashes_dir;
    if (crashes_dir.empty()) {
        crashes_dir = cwd + "/fuzz_crashes";
    }

    // Run each fuzz target
    for (const auto& file : fuzz_files) {
        std::string fuzz_name = fs::path(file).stem().string();
        // Remove .fuzz suffix if present
        if (fuzz_name.ends_with(".fuzz")) {
            fuzz_name = fuzz_name.substr(0, fuzz_name.size() - 5);
        }

        if (!opts.quiet) {
            TML_LOG_INFO("test", c.magenta() << "~" << c.reset() << " " << c.bold() << fuzz_name
                                             << c.reset());
        }

        FuzzResult result;
        result.file_path = file;
        result.fuzz_name = fuzz_name;

        // Load corpus if available
        std::string corpus_dir = opts.corpus_dir;
        if (corpus_dir.empty()) {
            corpus_dir = cwd + "/fuzz_corpus/" + fuzz_name;
        }
        std::vector<std::vector<uint8_t>> corpus = load_corpus(corpus_dir);

        // Compile the fuzz target to a shared library
        auto compile_result = compile_fuzz_to_shared_lib(file, opts.verbose, opts.no_cache);
        if (!compile_result.success) {
            result.passed = false;
            result.crash_message = "Compilation failed: " + compile_result.error_message;
            all_results.push_back(result);

            if (!opts.quiet) {
                TML_LOG_ERROR("test", c.red() << "[COMPILE ERROR]" << c.reset());
                if (opts.verbose) {
                    TML_LOG_ERROR("test", compile_result.error_message);
                }
            }
            crashes_found++;
            continue;
        }

        // Load the shared library
        DynamicLibrary lib;
        if (!lib.load(compile_result.lib_path)) {
            result.passed = false;
            result.crash_message = "Failed to load library: " + lib.get_error();
            all_results.push_back(result);

            if (!opts.quiet) {
                TML_LOG_ERROR("test", c.red() << "[LOAD ERROR]" << c.reset());
            }
            crashes_found++;
            continue;
        }

        // Get the fuzz target function
        auto fuzz_target = lib.get_function<FuzzTargetFunc>("tml_fuzz_target");
        if (!fuzz_target) {
            result.passed = false;
            result.crash_message = "No tml_fuzz_target function found (add @fuzz decorator)";
            all_results.push_back(result);

            if (!opts.quiet) {
                TML_LOG_WARN("test", c.yellow() << "[NO FUZZ TARGET]" << c.reset());
            }
            continue;
        }

        // Run fuzzing loop
        auto fuzz_start = std::chrono::high_resolution_clock::now();
        auto fuzz_end = fuzz_start + std::chrono::seconds(opts.fuzz_duration);
        int64_t iterations = 0;
        bool found_crash = false;
        std::vector<uint8_t> crash_input;

        while (std::chrono::high_resolution_clock::now() < fuzz_end) {
            // Generate or mutate input
            std::vector<uint8_t> input;
            if (!corpus.empty() && (iterations % 10) < 7) {
                // 70% of the time, mutate from corpus
                std::uniform_int_distribution<size_t> corpus_dist(0, corpus.size() - 1);
                input = mutate_fuzz_input(corpus[corpus_dist(rng)], opts.fuzz_max_len);
            } else {
                // 30% of the time, generate new random input
                input = generate_fuzz_input(opts.fuzz_max_len);
            }

            iterations++;

            // Call the fuzz target with the generated input
            try {
                int fuzz_result = fuzz_target(input.data(), input.size());
                if (fuzz_result != 0) {
                    // Non-zero return indicates a crash/failure
                    found_crash = true;
                    crash_input = input;
                    break;
                }
            } catch (...) {
                // Exception indicates a crash
                found_crash = true;
                crash_input = input;
                break;
            }
        }

        // Clean up the shared library file
        try {
            fs::remove(compile_result.lib_path);
#ifdef _WIN32
            fs::path lib_file = compile_result.lib_path;
            lib_file.replace_extension(".lib");
            if (fs::exists(lib_file)) {
                fs::remove(lib_file);
            }
#endif
        } catch (...) {
            // Ignore cleanup errors
        }

        auto fuzz_actual_end = std::chrono::high_resolution_clock::now();
        result.duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(fuzz_actual_end - fuzz_start)
                .count();
        result.iterations = iterations;
        result.found_crash = found_crash;
        result.passed = !found_crash;

        if (found_crash) {
            result.crash_input = bytes_to_hex(crash_input);
            save_crash_input(crashes_dir, fuzz_name, crash_input);
            crashes_found++;
        }

        all_results.push_back(result);

        // Print result
        if (!opts.quiet) {
            if (found_crash) {
                TML_LOG_INFO("test", c.red() << "[CRASH]" << c.reset() << " " << c.dim()
                                             << iterations << " iterations in "
                                             << format_duration(result.duration_ms) << c.reset());
            } else {
                TML_LOG_INFO("test", c.green() << "[OK]" << c.reset() << " " << c.dim()
                                               << iterations << " iterations in "
                                               << format_duration(result.duration_ms) << c.reset());
            }
        }
    }

    auto overall_end = std::chrono::high_resolution_clock::now();
    int64_t total_duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(overall_end - overall_start).count();

    // Print summary
    if (!opts.quiet) {
        std::ostringstream summary;
        summary << c.bold() << "Fuzz Targets  " << c.reset();
        if (crashes_found > 0) {
            summary << c.red() << c.bold() << crashes_found << " crashed" << c.reset() << " | ";
        }
        summary << c.green() << c.bold() << (fuzz_files.size() - crashes_found) << " ok"
                << c.reset() << " " << c.gray() << "(" << fuzz_files.size() << ")" << c.reset();
        TML_LOG_INFO("test", summary.str());
        TML_LOG_INFO("test",
                     c.bold() << "Duration      " << c.reset() << format_duration(total_duration_ms));

        // Print total iterations
        int64_t total_iterations = 0;
        for (const auto& r : all_results) {
            total_iterations += r.iterations;
        }
        TML_LOG_INFO("test", c.bold() << "Iterations    " << c.reset() << total_iterations);

        if (crashes_found > 0) {
            TML_LOG_INFO("test",
                         c.red() << c.bold() << "Crashes saved to: " << crashes_dir << c.reset());
        } else {
            TML_LOG_INFO("test", c.green() << c.bold() << "No crashes found!" << c.reset());
        }
    }

    return crashes_found > 0 ? 1 : 0;
}

} // namespace tml::cli::tester
