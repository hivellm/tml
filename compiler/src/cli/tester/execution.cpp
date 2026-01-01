// Test command - test execution

#include "tester_internal.hpp"

namespace tml::cli::tester {

// ============================================================================
// In-Process Test Execution
// ============================================================================

TestResult compile_and_run_test_inprocess(const std::string& test_file, const TestOptions& opts) {
    TestResult result;
    result.file_path = test_file;
    result.test_name = fs::path(test_file).stem().string();
    result.group = extract_group_name(test_file);
    result.test_count = count_tests_in_file(test_file);

    auto start_time = std::chrono::high_resolution_clock::now();

    auto inproc_result = compile_and_run_test_in_process(test_file, opts.verbose, opts.no_cache);

    auto end_time = std::chrono::high_resolution_clock::now();
    result.duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (result.duration_ms > opts.timeout_seconds * 1000) {
        result.timeout = true;
    }

    result.exit_code = inproc_result.exit_code;
    result.passed = inproc_result.success;

    // Check for compilation error
    if (!inproc_result.error.empty() &&
        (inproc_result.error.find("Lexer") != std::string::npos ||
         inproc_result.error.find("Parser") != std::string::npos ||
         inproc_result.error.find("Type") != std::string::npos ||
         inproc_result.error.find("Codegen") != std::string::npos ||
         inproc_result.error.find("Compilation") != std::string::npos ||
         inproc_result.error.find("Linking") != std::string::npos)) {
        result.compilation_error = true;
        result.exit_code = EXIT_COMPILATION_ERROR;
    }

    if (!result.passed) {
        if (result.compilation_error) {
            result.error_message = "COMPILATION FAILED";
        } else {
            result.error_message = "Exit code: " + std::to_string(result.exit_code);
        }
        if (!inproc_result.error.empty()) {
            result.error_message += "\n" + inproc_result.error;
        }
        if (!inproc_result.output.empty()) {
            result.error_message += "\n" + inproc_result.output;
        }
    }

    return result;
}

// ============================================================================
// Process-Based Test Execution
// ============================================================================

TestResult compile_and_run_test_with_result(const std::string& test_file, const TestOptions& opts) {
    // Use in-process execution for faster test runs (unless nocapture is set)
    // nocapture requires process-based execution to properly display output
    if (!opts.nocapture && !opts.coverage) {
        return compile_and_run_test_inprocess(test_file, opts);
    }

    // Fall back to process-based execution for nocapture or coverage mode
    TestResult result;
    result.file_path = test_file;
    result.test_name = fs::path(test_file).stem().string();
    result.group = extract_group_name(test_file);
    result.test_count = count_tests_in_file(test_file);

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::string> empty_args;
    std::string captured_output;

    // Run test directly (parallelism is handled at the outer level)
    if (opts.nocapture) {
        result.exit_code =
            run_run(test_file, empty_args, opts.release, opts.coverage, opts.no_cache);
    } else {
        result.exit_code = run_run_quiet(test_file, empty_args, opts.release, &captured_output,
                                         opts.coverage, opts.no_cache);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Check for soft timeout (just flag it, test already completed)
    if (result.duration_ms > opts.timeout_seconds * 1000) {
        result.timeout = true;
    }

    result.passed = (result.exit_code == 0);

    // Check if this was a compilation error (special exit code)
    result.compilation_error = (result.exit_code == EXIT_COMPILATION_ERROR);

    if (!result.passed) {
        if (result.compilation_error) {
            result.error_message = "COMPILATION FAILED";
        } else {
            result.error_message = "Exit code: " + std::to_string(result.exit_code);
        }
        if (!captured_output.empty()) {
            result.error_message += "\n" + captured_output;
        }
    }

    return result;
}

// ============================================================================
// Profiled Test Execution
// ============================================================================

TestResult compile_and_run_test_profiled(const std::string& test_file, const TestOptions& opts,
                                         PhaseTimings* timings) {
    TestResult result;
    result.file_path = test_file;
    result.test_name = fs::path(test_file).stem().string();
    result.group = extract_group_name(test_file);
    result.test_count = count_tests_in_file(test_file);

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::string> empty_args;
    std::string captured_output;

    result.exit_code = run_run_profiled(test_file, empty_args, opts.release, &captured_output,
                                        timings, opts.coverage, opts.no_cache);

    auto end_time = std::chrono::high_resolution_clock::now();
    result.duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (result.duration_ms > opts.timeout_seconds * 1000) {
        result.timeout = true;
    }

    result.passed = (result.exit_code == 0);
    result.compilation_error = (result.exit_code == EXIT_COMPILATION_ERROR);

    if (!result.passed) {
        if (result.compilation_error) {
            result.error_message = "COMPILATION FAILED";
        } else {
            result.error_message = "Exit code: " + std::to_string(result.exit_code);
        }
        if (!captured_output.empty()) {
            result.error_message += "\n" + captured_output;
        }
    }

    return result;
}

// ============================================================================
// Thread Worker
// ============================================================================

void test_worker(const std::vector<std::string>& test_files, std::atomic<size_t>& current_index,
                 TestResultCollector& collector, const TestOptions& opts) {
    while (true) {
        // Stop if a compilation error has occurred in another thread
        if (collector.has_compilation_error()) {
            break;
        }

        size_t index = current_index.fetch_add(1);
        if (index >= test_files.size()) {
            break;
        }

        const auto& file = test_files[index];
        TestResult result = compile_and_run_test_with_result(file, opts);
        collector.add(std::move(result));

        // Stop immediately if this was a compilation error
        if (result.compilation_error) {
            break;
        }
    }
}

} // namespace tml::cli::tester
