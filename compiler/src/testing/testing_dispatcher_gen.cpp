TML_MODULE("test")

//! # Dispatcher IR Generator — NDJSON Output
//!
//! Generates LLVM IR for a test dispatcher that emits NDJSON events to stdout.
//! Replaces the old TML_RESULT-based dispatcher with structured JSON output.
//!
//! ## Generated Functions
//!
//! - `@main(i32, i8**)`: Entry point — parses --run-all, --test-index=N, --list
//! - `@run_all_tests()`: Runs all tests with NDJSON events
//! - `@run_single_test(i32)`: Runs one test with NDJSON events
//! - `@list_tests()`: Prints test metadata as JSON array
//!
//! ## Timing
//!
//! Uses `clock()` for portable microsecond-level timing. The resolution
//! varies by platform but is sufficient for test timing purposes.
//!
//! ## Crash Isolation
//!
//! Windows: SEH __try/__except wrapping each test call
//! Unix: Not yet implemented at IR level (requires sigaction in C)
//! Note: Crash isolation at the IR level is limited. Full crash isolation
//! is handled by the coordinator's subprocess management (Process class).

#include "testing/testing_dispatcher_gen.hpp"

#include <set>
#include <sstream>
#include <string>

namespace tml::testing {

namespace {

// Helper: compute the byte length of a C string literal (including null terminator)
size_t cstr_len(const std::string& s) {
    size_t len = 1; // null terminator
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[i + 1];
            if (next == '0' || next == 'n' || next == 'A' || next == 't' || next == '"' ||
                next == '\\') {
                len++; // escape sequence = 1 byte
                i++;   // skip next char
                continue;
            }
        }
        len++;
    }
    return len;
}

// Escape a string for use inside LLVM IR string constants.
// Handles: newline → \0A, double quote → \22, backslash → \5C, null → \00
std::string ir_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        if (c == '\n') {
            out += "\\0A";
        } else if (c == '\r') {
            out += "\\0D";
        } else if (c == '"') {
            out += "\\22";
        } else if (c == '\\') {
            out += "\\5C";
        } else if (c == '\t') {
            out += "\\09";
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\%02X", static_cast<unsigned char>(c));
            out += buf;
        } else {
            out += c;
        }
    }
    return out;
}

// Compute byte length of an IR-escaped string (for LLVM constant type)
size_t ir_byte_len(const std::string& ir_str) {
    size_t len = 0;
    for (size_t i = 0; i < ir_str.size(); ++i) {
        if (ir_str[i] == '\\' && i + 2 < ir_str.size()) {
            // \XX hex escape = 1 byte
            len++;
            i += 2; // skip XX
        } else {
            len++;
        }
    }
    return len;
}

// Emit an LLVM IR string constant declaration
// Returns the global variable name
std::string emit_string_constant(std::ostringstream& ir, const std::string& name,
                                 const std::string& content) {
    std::string escaped = ir_escape(content);
    size_t byte_len = ir_byte_len(escaped) + 1; // +1 for null terminator
    ir << "@" << name << " = private unnamed_addr constant [" << byte_len << " x i8] c\"" << escaped
       << "\\00\"\n";
    return name;
}

} // anonymous namespace

std::string generate_ndjson_dispatcher_ir(const std::vector<DispatcherTestInfo>& tests,
                                          const std::string& suite_name) {
    std::ostringstream ir;
    int total_tests = static_cast<int>(tests.size());

    // ========================================================================
    // Module header
    // ========================================================================
    ir << "; ModuleID = '" << suite_name << "_ndjson_dispatcher'\n";
    ir << "source_filename = \"" << suite_name << "_ndjson_dispatcher.ll\"\n";
#ifdef _WIN32
    ir << "target triple = \"x86_64-pc-windows-msvc\"\n";
#else
    ir << "target triple = \"x86_64-unknown-linux-gnu\"\n";
#endif
    ir << "\n";

    // ========================================================================
    // String constants for JSON event templates
    // ========================================================================

    // Command-line argument strings
    emit_string_constant(ir, ".str.run_all", "--run-all");
    emit_string_constant(ir, ".str.prefix", "--test-index=");
    emit_string_constant(ir, ".str.list", "--list");
    emit_string_constant(ir, ".str.error", "ERROR: invalid test index %d\n");
    emit_string_constant(ir, ".str.no_args",
                         "ERROR: --run-all, --test-index=N, or --list required\n");

    // Suite name
    emit_string_constant(ir, ".str.suite_name", suite_name);

    // NDJSON event format strings (each is a printf format)
    // suite_start: {"event":"suite_start","name":"<suite>","test_count":<N>,"file_count":<M>}
    {
        std::string fmt = R"({"event":"suite_start","name":"%s","test_count":%d,"file_count":%d})";
        fmt += '\n';
        emit_string_constant(ir, ".fmt.suite_start", fmt);
    }
    // suite_end:
    // {"event":"suite_end","passed":<P>,"failed":<F>,"crashed":0,"timed_out":0,"skipped":0,"duration_us":<D>}
    {
        std::string fmt =
            R"({"event":"suite_end","passed":%d,"failed":%d,"crashed":%d,"timed_out":0,"skipped":0,"duration_us":%lld})";
        fmt += '\n';
        emit_string_constant(ir, ".fmt.suite_end", fmt);
    }
    // test_start: {"event":"test_start","index":<I>,"name":"<name>","file":"<file>"}
    {
        std::string fmt = R"({"event":"test_start","index":%d,"name":"%s","file":"%s"})";
        fmt += '\n';
        emit_string_constant(ir, ".fmt.test_start", fmt);
    }
    // test_pass: {"event":"test_pass","index":<I>,"duration_us":<D>}
    {
        std::string fmt = R"({"event":"test_pass","index":%d,"duration_us":%lld})";
        fmt += '\n';
        emit_string_constant(ir, ".fmt.test_pass", fmt);
    }
    // test_fail: {"event":"test_fail","index":<I>,"name":"<name>","error":"non-zero
    // exit","file":"<file>","line":0,"exit_code":<E>,"duration_us":<D>}
    {
        std::string fmt =
            R"({"event":"test_fail","index":%d,"name":"%s","error":"non-zero exit","file":"%s","line":0,"exit_code":%d,"duration_us":%lld})";
        fmt += '\n';
        emit_string_constant(ir, ".fmt.test_fail", fmt);
    }

    // Coverage: TML_COVERAGE_FILE env var name
    emit_string_constant(ir, ".str.cov_env", "TML_COVERAGE_FILE");

    // Per-test name and file string constants
    for (int i = 0; i < total_tests; ++i) {
        emit_string_constant(ir, ".str.test_name_" + std::to_string(i), tests[i].name);
        emit_string_constant(ir, ".str.test_file_" + std::to_string(i), tests[i].file);
    }

    // List mode string constants (must be global, not inside function bodies)
    emit_string_constant(ir, ".str.empty_arr", "[]\n");
    std::string open_bracket = "[";
    std::string close_bracket = "]\n";
    std::string entry_fmt_first = R"({"name":"%s","file":"%s","index":%d})";
    std::string entry_fmt_rest = R"(,{"name":"%s","file":"%s","index":%d})";
    emit_string_constant(ir, ".str.list_open", open_bracket);
    emit_string_constant(ir, ".str.list_close", close_bracket);
    emit_string_constant(ir, ".fmt.list_first", entry_fmt_first);
    emit_string_constant(ir, ".fmt.list_rest", entry_fmt_rest);
    ir << "\n";

    // ========================================================================
    // External function declarations
    // ========================================================================

    // Test functions. F-023: the entry symbol is `tml_test_<symbol_id>` where
    // symbol_id is the file-stable id (falls back to the suite index when unset,
    // e.g. legacy callers). Declared once, deduplicated defensively.
    {
        std::set<uint32_t> declared;
        for (int i = 0; i < total_tests; ++i) {
            uint32_t sym = tests[i].symbol_id != 0 ? tests[i].symbol_id
                                                   : static_cast<uint32_t>(tests[i].index);
            if (declared.insert(sym).second) {
                ir << "declare i32 @tml_test_" << sym << "()\n";
            }
        }
    }
    ir << "\n";

    // C library functions
    ir << "declare i32 @strcmp(i8*, i8*) nounwind\n";
    ir << "declare i32 @strncmp(i8*, i8*, i64) nounwind\n";
    ir << "declare i32 @atoi(i8*) nounwind\n";
    ir << "declare i32 @printf(i8*, ...) nounwind\n";
    ir << "declare void @fflush(i8*) nounwind\n";
    ir << "declare i64 @clock() nounwind\n";
    ir << "declare i8* @getenv(i8*) nounwind\n";
    ir << "declare void @tml_coverage_write_file(i8*) nounwind\n";
    ir << "\n";

    // ========================================================================
    // Helper: get_str_ptr — GEP into a string constant
    // ========================================================================
    // Instead of emitting GEP inline everywhere, we use a macro-like pattern
    // in the IR generation.

    // Lambda to emit GEP for a string constant
    auto gep_str = [&](const std::string& var_name, const std::string& content) -> std::string {
        std::string escaped = ir_escape(content);
        size_t byte_len = ir_byte_len(escaped) + 1;
        std::ostringstream gep;
        gep << "getelementptr [" << byte_len << " x i8], [" << byte_len << " x i8]* @" << var_name
            << ", i64 0, i64 0";
        return gep.str();
    };

    // Pre-compute GEP strings for format constants
    auto gep_suite_start = gep_str(
        ".fmt.suite_start",
        std::string(R"({"event":"suite_start","name":"%s","test_count":%d,"file_count":%d})") +
            '\n');
    auto gep_suite_end = gep_str(
        ".fmt.suite_end",
        std::string(
            R"({"event":"suite_end","passed":%d,"failed":%d,"crashed":%d,"timed_out":0,"skipped":0,"duration_us":%lld})") +
            '\n');
    auto gep_test_start =
        gep_str(".fmt.test_start",
                std::string(R"({"event":"test_start","index":%d,"name":"%s","file":"%s"})") + '\n');
    auto gep_test_pass =
        gep_str(".fmt.test_pass",
                std::string(R"({"event":"test_pass","index":%d,"duration_us":%lld})") + '\n');
    auto gep_test_fail = gep_str(
        ".fmt.test_fail",
        std::string(
            R"({"event":"test_fail","index":%d,"name":"%s","error":"non-zero exit","file":"%s","line":0,"exit_code":%d,"duration_us":%lld})") +
            '\n');
    auto gep_suite_name = gep_str(".str.suite_name", suite_name);

    // ========================================================================
    // list_tests() — Emit test metadata as JSON array
    // ========================================================================
    ir << "define i32 @list_tests() {\n";
    ir << "entry:\n";

    if (total_tests == 0) {
        // Empty array
        auto gep_empty = gep_str(".str.empty_arr", "[]\n");
        ir << "  %empty = " << gep_empty << "\n";
        ir << "  call i32 (i8*, ...) @printf(i8* %empty)\n";
    } else {
        // Print opening bracket
        auto gep_open = gep_str(".str.list_open", open_bracket);
        ir << "  %open = " << gep_open << "\n";
        ir << "  call i32 (i8*, ...) @printf(i8* %open)\n";

        // Each test entry
        auto gep_first = gep_str(".fmt.list_first", entry_fmt_first);
        auto gep_rest = gep_str(".fmt.list_rest", entry_fmt_rest);

        for (int i = 0; i < total_tests; ++i) {
            auto gep_name = gep_str(".str.test_name_" + std::to_string(i), tests[i].name);
            auto gep_file = gep_str(".str.test_file_" + std::to_string(i), tests[i].file);

            if (i == 0) {
                ir << "  %lfmt_" << i << " = " << gep_first << "\n";
            } else {
                ir << "  %lfmt_" << i << " = " << gep_rest << "\n";
            }
            ir << "  %lname_" << i << " = " << gep_name << "\n";
            ir << "  %lfile_" << i << " = " << gep_file << "\n";
            ir << "  call i32 (i8*, ...) @printf(i8* %lfmt_" << i << ", i8* %lname_" << i
               << ", i8* %lfile_" << i << ", i32 " << i << ")\n";
        }

        // Close bracket + newline
        auto gep_close = gep_str(".str.list_close", close_bracket);
        ir << "  %close = " << gep_close << "\n";
        ir << "  call i32 (i8*, ...) @printf(i8* %close)\n";
    }

    ir << "  call void @fflush(i8* null)\n";
    ir << "  ret i32 0\n";
    ir << "}\n\n";

    // ========================================================================
    // run_single_test(i32 %index) — Run one test with NDJSON events
    // ========================================================================
    ir << "define i32 @run_single_test(i32 %index) {\n";
    ir << "entry:\n";

    if (total_tests == 0) {
        ir << "  ret i32 99\n";
    } else {
        ir << "  switch i32 %index, label %invalid [\n";
        for (int i = 0; i < total_tests; ++i) {
            ir << "    i32 " << tests[i].index << ", label %test_" << i << "\n";
        }
        ir << "  ]\n\n";

        for (int i = 0; i < total_tests; ++i) {
            int tidx = tests[i].index; // suite position — NDJSON event index
            uint32_t sym = tests[i].symbol_id != 0 ? tests[i].symbol_id
                                                   : static_cast<uint32_t>(tests[i].index);
            auto gep_name = gep_str(".str.test_name_" + std::to_string(i), tests[i].name);
            auto gep_file = gep_str(".str.test_file_" + std::to_string(i), tests[i].file);

            ir << "test_" << i << ":\n";

            // Emit test_start event
            ir << "  %ts_fmt_" << i << " = " << gep_test_start << "\n";
            ir << "  %ts_name_" << i << " = " << gep_name << "\n";
            ir << "  %ts_file_" << i << " = " << gep_file << "\n";
            ir << "  call i32 (i8*, ...) @printf(i8* %ts_fmt_" << i << ", i32 " << tidx
               << ", i8* %ts_name_" << i << ", i8* %ts_file_" << i << ")\n";
            ir << "  call void @fflush(i8* null)\n";

            // Record start time
            ir << "  %t0_" << i << " = call i64 @clock()\n";

            // Call test function (F-023: stable per-file symbol id)
            ir << "  %rc_" << i << " = call i32 @tml_test_" << sym << "()\n";

            // Record end time
            ir << "  %t1_" << i << " = call i64 @clock()\n";

            // Calculate duration in microseconds
            // clock() returns clock_t (ticks), CLOCKS_PER_SEC = 1000000 on POSIX, 1000 on Windows
            // We compute (t1 - t0) * 1000000 / CLOCKS_PER_SEC
            // On Windows CLOCKS_PER_SEC = 1000, so multiply by 1000
            // On POSIX CLOCKS_PER_SEC = 1000000, so multiply by 1
            ir << "  %dt_" << i << " = sub i64 %t1_" << i << ", %t0_" << i << "\n";
#ifdef _WIN32
            ir << "  %us_" << i << " = mul i64 %dt_" << i << ", 1000\n";
#else
            ir << "  %us_" << i << " = add i64 %dt_" << i << ", 0\n"; // already in microseconds
#endif

            // Check pass/fail
            ir << "  %ok_" << i << " = icmp eq i32 %rc_" << i << ", 0\n";
            ir << "  br i1 %ok_" << i << ", label %pass_" << i << ", label %fail_" << i << "\n\n";

            // Pass — event index MUST be the original test index (tidx), matching
            // test_start: when files are skipped (compile failure) the dispatcher
            // list has gaps, and the coordinator maps event indices to suite slots.
            ir << "pass_" << i << ":\n";
            ir << "  %tp_fmt_" << i << " = " << gep_test_pass << "\n";
            ir << "  call i32 (i8*, ...) @printf(i8* %tp_fmt_" << i << ", i32 " << tidx
               << ", i64 %us_" << i << ")\n";
            ir << "  call void @fflush(i8* null)\n";
            ir << "  br label %exit_pass\n\n";

            // Fail — same original-index rule as pass
            ir << "fail_" << i << ":\n";
            ir << "  %tf_fmt_" << i << " = " << gep_test_fail << "\n";
            ir << "  %tf_name_" << i << " = " << gep_name << "\n";
            ir << "  %tf_file_" << i << " = " << gep_file << "\n";
            ir << "  call i32 (i8*, ...) @printf(i8* %tf_fmt_" << i << ", i32 " << tidx
               << ", i8* %tf_name_" << i << ", i8* %tf_file_" << i << ", i32 %rc_" << i
               << ", i64 %us_" << i << ")\n";
            ir << "  call void @fflush(i8* null)\n";
            ir << "  br label %exit_fail\n\n";
        }

        ir << "invalid:\n";
        auto gep_error = gep_str(".str.error", "ERROR: invalid test index %d\n");
        ir << "  %err = " << gep_error << "\n";
        ir << "  call i32 (i8*, ...) @printf(i8* %err, i32 %index)\n";
        ir << "  ret i32 99\n\n";

        // Common exit blocks with coverage write epilogue
        auto gep_cov_env_single = gep_str(".str.cov_env", "TML_COVERAGE_FILE");

        ir << "exit_pass:\n";
        ir << "  %sp_cov_ptr = " << gep_cov_env_single << "\n";
        ir << "  %sp_cov_file = call i8* @getenv(i8* %sp_cov_ptr)\n";
        ir << "  %sp_cov_nn = icmp ne i8* %sp_cov_file, null\n";
        ir << "  br i1 %sp_cov_nn, label %sp_write_cov, label %sp_ret\n\n";
        ir << "sp_write_cov:\n";
        ir << "  call void @tml_coverage_write_file(i8* %sp_cov_file)\n";
        ir << "  br label %sp_ret\n\n";
        ir << "sp_ret:\n";
        ir << "  ret i32 0\n\n";

        ir << "exit_fail:\n";
        ir << "  %sf_cov_ptr = " << gep_cov_env_single << "\n";
        ir << "  %sf_cov_file = call i8* @getenv(i8* %sf_cov_ptr)\n";
        ir << "  %sf_cov_nn = icmp ne i8* %sf_cov_file, null\n";
        ir << "  br i1 %sf_cov_nn, label %sf_write_cov, label %sf_ret\n\n";
        ir << "sf_write_cov:\n";
        ir << "  call void @tml_coverage_write_file(i8* %sf_cov_file)\n";
        ir << "  br label %sf_ret\n\n";
        ir << "sf_ret:\n";
        ir << "  ret i32 1\n";
    }
    ir << "}\n\n";

    // ========================================================================
    // run_all_tests() — Run all tests with NDJSON events
    // ========================================================================
    ir << "define i32 @run_all_tests() {\n";
    ir << "entry:\n";

    // Emit suite_start
    ir << "  %ss_fmt = " << gep_suite_start << "\n";
    ir << "  %ss_name = " << gep_suite_name << "\n";

    // Count unique files
    std::vector<std::string> unique_files;
    for (auto& t : tests) {
        bool found = false;
        for (auto& f : unique_files) {
            if (f == t.file) {
                found = true;
                break;
            }
        }
        if (!found)
            unique_files.push_back(t.file);
    }
    int file_count = static_cast<int>(unique_files.size());

    ir << "  call i32 (i8*, ...) @printf(i8* %ss_fmt, i8* %ss_name, i32 " << total_tests << ", i32 "
       << file_count << ")\n";
    ir << "  call void @fflush(i8* null)\n\n";

    // Record suite start time
    ir << "  %suite_t0 = call i64 @clock()\n\n";

    if (total_tests == 0) {
        ir << "  %suite_t1_empty = call i64 @clock()\n";
        ir << "  %suite_dt_empty = sub i64 %suite_t1_empty, %suite_t0\n";
#ifdef _WIN32
        ir << "  %suite_us_empty = mul i64 %suite_dt_empty, 1000\n";
#else
        ir << "  %suite_us_empty = add i64 %suite_dt_empty, 0\n";
#endif
        ir << "  %se_fmt_empty = " << gep_suite_end << "\n";
        ir << "  call i32 (i8*, ...) @printf(i8* %se_fmt_empty, i32 0, i32 0, i32 0, i64 "
              "%suite_us_empty)\n";
        ir << "  call void @fflush(i8* null)\n";
        ir << "  ret i32 0\n";
    } else {
        // For each test: emit test_start, call test, emit test_pass/fail
        ir << "  br label %test_0\n\n";

        for (int i = 0; i < total_tests; ++i) {
            int tidx = tests[i].index; // suite position — NDJSON event index
            uint32_t sym = tests[i].symbol_id != 0 ? tests[i].symbol_id
                                                   : static_cast<uint32_t>(tests[i].index);
            auto gep_name = gep_str(".str.test_name_" + std::to_string(i), tests[i].name);
            auto gep_file = gep_str(".str.test_file_" + std::to_string(i), tests[i].file);

            ir << "test_" << i << ":\n";

            // Phi for pass/fail/crash counters
            if (i == 0) {
                ir << "  %pass_cnt_" << i << " = add i32 0, 0\n";
                ir << "  %fail_cnt_" << i << " = add i32 0, 0\n";
                ir << "  %crash_cnt_" << i << " = add i32 0, 0\n";
            } else {
                ir << "  %pass_cnt_" << i << " = phi i32 [ %pass_after_" << (i - 1) << ", %done_"
                   << (i - 1) << " ]\n";
                ir << "  %fail_cnt_" << i << " = phi i32 [ %fail_after_" << (i - 1) << ", %done_"
                   << (i - 1) << " ]\n";
                ir << "  %crash_cnt_" << i << " = phi i32 [ %crash_after_" << (i - 1) << ", %done_"
                   << (i - 1) << " ]\n";
            }

            // Emit test_start event
            ir << "  %at_ts_fmt_" << i << " = " << gep_test_start << "\n";
            ir << "  %at_ts_name_" << i << " = " << gep_name << "\n";
            ir << "  %at_ts_file_" << i << " = " << gep_file << "\n";
            ir << "  call i32 (i8*, ...) @printf(i8* %at_ts_fmt_" << i << ", i32 " << tidx
               << ", i8* %at_ts_name_" << i << ", i8* %at_ts_file_" << i << ")\n";
            ir << "  call void @fflush(i8* null)\n";

            // Record test start time
            ir << "  %at_t0_" << i << " = call i64 @clock()\n";

            // Call test function (F-023: stable per-file symbol id)
            ir << "  %at_rc_" << i << " = call i32 @tml_test_" << sym << "()\n";

            // Record test end time
            ir << "  %at_t1_" << i << " = call i64 @clock()\n";
            ir << "  %at_dt_" << i << " = sub i64 %at_t1_" << i << ", %at_t0_" << i << "\n";
#ifdef _WIN32
            ir << "  %at_us_" << i << " = mul i64 %at_dt_" << i << ", 1000\n";
#else
            ir << "  %at_us_" << i << " = add i64 %at_dt_" << i << ", 0\n";
#endif

            // Check pass/fail
            ir << "  %at_ok_" << i << " = icmp eq i32 %at_rc_" << i << ", 0\n";
            ir << "  br i1 %at_ok_" << i << ", label %at_pass_" << i << ", label %at_fail_" << i
               << "\n\n";

            // Pass branch — event index MUST be the original test index (tidx),
            // matching test_start: when files are skipped (compile failure) the
            // dispatcher list has gaps and the coordinator maps event indices to
            // suite slots. Using the sequential position here misattributed
            // results to the wrong tests.
            ir << "at_pass_" << i << ":\n";
            ir << "  %at_tp_fmt_" << i << " = " << gep_test_pass << "\n";
            ir << "  call i32 (i8*, ...) @printf(i8* %at_tp_fmt_" << i << ", i32 " << tidx
               << ", i64 %at_us_" << i << ")\n";
            ir << "  call void @fflush(i8* null)\n";
            ir << "  br label %done_" << i << "\n\n";

            // Fail branch — same original-index rule as pass
            ir << "at_fail_" << i << ":\n";
            ir << "  %at_tf_fmt_" << i << " = " << gep_test_fail << "\n";
            ir << "  %at_tf_name_" << i << " = " << gep_name << "\n";
            ir << "  %at_tf_file_" << i << " = " << gep_file << "\n";
            ir << "  call i32 (i8*, ...) @printf(i8* %at_tf_fmt_" << i << ", i32 " << tidx
               << ", i8* %at_tf_name_" << i << ", i8* %at_tf_file_" << i << ", i32 %at_rc_" << i
               << ", i64 %at_us_" << i << ")\n";
            ir << "  call void @fflush(i8* null)\n";
            ir << "  br label %done_" << i << "\n\n";

            // Merge — update counters
            ir << "done_" << i << ":\n";
            ir << "  %did_pass_" << i << " = phi i1 [ true, %at_pass_" << i
               << " ], [ false, %at_fail_" << i << " ]\n";
            ir << "  %pass_inc_" << i << " = zext i1 %did_pass_" << i << " to i32\n";
            ir << "  %pass_after_" << i << " = add i32 %pass_cnt_" << i << ", %pass_inc_" << i
               << "\n";
            ir << "  %did_fail_" << i << " = xor i1 %did_pass_" << i << ", true\n";
            ir << "  %fail_inc_" << i << " = zext i1 %did_fail_" << i << " to i32\n";
            ir << "  %fail_after_" << i << " = add i32 %fail_cnt_" << i << ", %fail_inc_" << i
               << "\n";
            ir << "  %crash_after_" << i << " = add i32 %crash_cnt_" << i << ", 0\n";

            if (i < total_tests - 1) {
                ir << "  br label %test_" << (i + 1) << "\n\n";
            } else {
                ir << "  br label %suite_done\n\n";
            }
        }

        // Suite done — emit suite_end
        ir << "suite_done:\n";
        int last = total_tests - 1;
        ir << "  %final_pass = phi i32 [ %pass_after_" << last << ", %done_" << last << " ]\n";
        ir << "  %final_fail = phi i32 [ %fail_after_" << last << ", %done_" << last << " ]\n";
        ir << "  %final_crash = phi i32 [ %crash_after_" << last << ", %done_" << last << " ]\n";

        // Suite end time
        ir << "  %suite_t1 = call i64 @clock()\n";
        ir << "  %suite_dt = sub i64 %suite_t1, %suite_t0\n";
#ifdef _WIN32
        ir << "  %suite_us = mul i64 %suite_dt, 1000\n";
#else
        ir << "  %suite_us = add i64 %suite_dt, 0\n";
#endif

        ir << "  %se_fmt = " << gep_suite_end << "\n";
        ir << "  call i32 (i8*, ...) @printf(i8* %se_fmt, i32 %final_pass, i32 %final_fail, i32 "
              "%final_crash, i64 %suite_us)\n";
        ir << "  call void @fflush(i8* null)\n";

        // Coverage: write covered function names to TML_COVERAGE_FILE if set
        auto gep_cov_env = gep_str(".str.cov_env", "TML_COVERAGE_FILE");
        ir << "\n  ; Coverage write epilogue\n";
        ir << "  %cov_env_ptr = " << gep_cov_env << "\n";
        ir << "  %cov_file = call i8* @getenv(i8* %cov_env_ptr)\n";
        ir << "  %cov_not_null = icmp ne i8* %cov_file, null\n";
        ir << "  br i1 %cov_not_null, label %write_cov, label %cov_done\n\n";
        ir << "write_cov:\n";
        ir << "  call void @tml_coverage_write_file(i8* %cov_file)\n";
        ir << "  br label %cov_done\n\n";
        ir << "cov_done:\n";

        // Return 0 if all passed, 1 if any failed
        ir << "  %any_failed = icmp ne i32 %final_fail, 0\n";
        ir << "  %any_crashed = icmp ne i32 %final_crash, 0\n";
        ir << "  %any_bad = or i1 %any_failed, %any_crashed\n";
        ir << "  %exit = select i1 %any_bad, i32 1, i32 0\n";
        ir << "  ret i32 %exit\n";
    }
    ir << "}\n\n";

    // ========================================================================
    // main(argc, argv) — Entry point
    // ========================================================================
    ir << "define i32 @main(i32 %argc, i8** %argv) {\n";
    ir << "entry:\n";

    // Check if we have arguments
    ir << "  %has_args = icmp sgt i32 %argc, 1\n";
    ir << "  br i1 %has_args, label %scan_args, label %no_args\n\n";

    // Scan arguments
    ir << "scan_args:\n";
    ir << "  %i.start = add i32 0, 1\n";
    ir << "  br label %arg_loop\n\n";

    ir << "arg_loop:\n";
    ir << "  %i = phi i32 [ %i.start, %scan_args ], [ %i.next, %arg_continue ]\n";
    ir << "  %done_scanning = icmp sge i32 %i, %argc\n";
    ir << "  br i1 %done_scanning, label %no_args, label %check_arg\n\n";

    ir << "check_arg:\n";
    ir << "  %i.i64 = sext i32 %i to i64\n";
    ir << "  %arg_ptr = getelementptr i8*, i8** %argv, i64 %i.i64\n";
    ir << "  %arg = load i8*, i8** %arg_ptr\n\n";

    // Check --run-all
    {
        auto gep_ra = gep_str(".str.run_all", "--run-all");
        ir << "  %ra_ptr = " << gep_ra << "\n";
        ir << "  %cmp_ra = call i32 @strcmp(i8* %arg, i8* %ra_ptr)\n";
        ir << "  %is_ra = icmp eq i32 %cmp_ra, 0\n";
        ir << "  br i1 %is_ra, label %do_run_all, label %check_list\n\n";
    }

    // Check --list
    ir << "check_list:\n";
    {
        auto gep_list = gep_str(".str.list", "--list");
        ir << "  %list_ptr = " << gep_list << "\n";
        ir << "  %cmp_list = call i32 @strcmp(i8* %arg, i8* %list_ptr)\n";
        ir << "  %is_list = icmp eq i32 %cmp_list, 0\n";
        ir << "  br i1 %is_list, label %do_list, label %check_index\n\n";
    }

    // Check --test-index=N
    ir << "check_index:\n";
    {
        auto gep_prefix = gep_str(".str.prefix", "--test-index=");
        ir << "  %prefix_ptr = " << gep_prefix << "\n";
        ir << "  %cmp_prefix = call i32 @strncmp(i8* %arg, i8* %prefix_ptr, i64 13)\n";
        ir << "  %is_index = icmp eq i32 %cmp_prefix, 0\n";
        ir << "  br i1 %is_index, label %do_index, label %arg_continue\n\n";
    }

    // Continue to next argument
    ir << "arg_continue:\n";
    ir << "  %i.next = add i32 %i, 1\n";
    ir << "  br label %arg_loop\n\n";

    // --run-all
    ir << "do_run_all:\n";
    ir << "  %ra_rc = call i32 @run_all_tests()\n";
    ir << "  ret i32 %ra_rc\n\n";

    // --list
    ir << "do_list:\n";
    ir << "  %list_rc = call i32 @list_tests()\n";
    ir << "  ret i32 %list_rc\n\n";

    // --test-index=N
    ir << "do_index:\n";
    ir << "  %num_ptr = getelementptr i8, i8* %arg, i64 13\n";
    ir << "  %test_index = call i32 @atoi(i8* %num_ptr)\n";
    ir << "  %idx_rc = call i32 @run_single_test(i32 %test_index)\n";
    ir << "  ret i32 %idx_rc\n\n";

    // No valid arguments found
    ir << "no_args:\n";
    {
        auto gep_no_args =
            gep_str(".str.no_args", "ERROR: --run-all, --test-index=N, or --list required\n");
        ir << "  %na_ptr = " << gep_no_args << "\n";
        ir << "  call i32 (i8*, ...) @printf(i8* %na_ptr)\n";
        ir << "  ret i32 98\n";
    }

    ir << "}\n";

    return ir.str();
}

} // namespace tml::testing
