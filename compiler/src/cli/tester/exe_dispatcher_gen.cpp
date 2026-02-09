//! # EXE Test Dispatcher IR Generator
//!
//! Generates LLVM IR for a dispatcher main() function that supports:
//! 1. `--test-index=N` — Run a single test and return its exit code
//! 2. `--run-all` — Run ALL tests sequentially, printing structured results:
//!    `TML_RESULT:<index>:<PASS|FAIL>:<exit_code>`
//!    Returns 0 if all passed, 1 if any failed.
//!
//! The `--run-all` mode is the primary execution mode, reducing subprocess
//! overhead from O(tests) to O(suites) — typically ~454 spawns vs ~3,632.

#include "exe_test_runner.hpp"

#include <sstream>
#include <string>

namespace tml::cli {

std::string generate_dispatcher_ir(int total_tests, const std::string& module_name) {
    std::ostringstream ir;

    // Module header
    ir << "; ModuleID = '" << module_name << "_dispatcher'\n";
    ir << "source_filename = \"" << module_name << "_dispatcher.ll\"\n";
#ifdef _WIN32
    ir << "target triple = \"x86_64-pc-windows-msvc\"\n";
#else
    ir << "target triple = \"x86_64-unknown-linux-gnu\"\n";
#endif
    ir << "\n";

    // String constants
    ir << "@.str.prefix = private unnamed_addr constant [14 x i8] c\"--test-index=\\00\"\n";
    ir << "@.str.run_all = private unnamed_addr constant [10 x i8] c\"--run-all\\00\"\n";
    ir << "@.str.error = private unnamed_addr constant [30 x i8] "
          "c\"ERROR: invalid test index %d\\0A\\00\"\n";
    ir << "@.str.no_index = private unnamed_addr constant [41 x i8] "
          "c\"ERROR: --test-index=N argument required\\0A\\00\"\n";
    ir << "@.str.result_pass = private unnamed_addr constant [24 x i8] "
          "c\"TML_RESULT:%d:PASS:%d\\0A\\00\"\n";
    ir << "@.str.result_fail = private unnamed_addr constant [24 x i8] "
          "c\"TML_RESULT:%d:FAIL:%d\\0A\\00\"\n";
    ir << "\n";

    // External function declarations
    for (int i = 0; i < total_tests; ++i) {
        ir << "declare i32 @tml_test_" << i << "()\n";
    }
    ir << "\n";

    // C library declarations
    ir << "declare i32 @strcmp(i8*, i8*) nounwind\n";
    ir << "declare i32 @strncmp(i8*, i8*, i64) nounwind\n";
    ir << "declare i32 @atoi(i8*) nounwind\n";
    ir << "declare i32 @printf(i8*, ...) nounwind\n";
    ir << "declare i64 @strlen(i8*) nounwind\n";
    ir << "declare void @fflush(i8*) nounwind\n";
    ir << "\n";

    // ===== run_all_tests function =====
    // Runs every test sequentially, prints TML_RESULT lines, returns fail count
    ir << "define i32 @run_all_tests() {\n";
    ir << "entry:\n";
    ir << "  br label %test_0\n";
    ir << "\n";

    for (int i = 0; i < total_tests; ++i) {
        ir << "test_" << i << ":\n";

        // Accumulate fail count from previous tests via phi
        if (i == 0) {
            ir << "  %fails_before_" << i << " = add i32 0, 0\n";
        } else {
            ir << "  %fails_before_" << i << " = phi i32 [ %fails_after_" << (i - 1)
               << ", %result_done_" << (i - 1) << " ]\n";
        }

        ir << "  %rc_" << i << " = call i32 @tml_test_" << i << "()\n";
        ir << "  %ok_" << i << " = icmp eq i32 %rc_" << i << ", 0\n";
        ir << "  br i1 %ok_" << i << ", label %pass_" << i << ", label %fail_" << i << "\n";
        ir << "\n";

        // Pass
        ir << "pass_" << i << ":\n";
        ir << "  %pass_fmt_" << i
           << " = getelementptr [24 x i8], [24 x i8]* @.str.result_pass, i64 0, i64 0\n";
        ir << "  call i32 (i8*, ...) @printf(i8* %pass_fmt_" << i << ", i32 " << i << ", i32 0)\n";
        ir << "  call void @fflush(i8* null)\n";
        ir << "  br label %result_done_" << i << "\n";
        ir << "\n";

        // Fail
        ir << "fail_" << i << ":\n";
        ir << "  %fail_fmt_" << i
           << " = getelementptr [24 x i8], [24 x i8]* @.str.result_fail, i64 0, i64 0\n";
        ir << "  call i32 (i8*, ...) @printf(i8* %fail_fmt_" << i << ", i32 " << i << ", i32 %rc_"
           << i << ")\n";
        ir << "  call void @fflush(i8* null)\n";
        ir << "  br label %result_done_" << i << "\n";
        ir << "\n";

        // Merge
        ir << "result_done_" << i << ":\n";
        ir << "  %did_fail_" << i << " = phi i1 [ false, %pass_" << i << " ], [ true, %fail_" << i
           << " ]\n";
        ir << "  %fail_inc_" << i << " = zext i1 %did_fail_" << i << " to i32\n";
        ir << "  %fails_after_" << i << " = add i32 %fails_before_" << i << ", %fail_inc_" << i
           << "\n";

        if (i < total_tests - 1) {
            ir << "  br label %test_" << (i + 1) << "\n";
        } else {
            ir << "  %any_failed = icmp ne i32 %fails_after_" << i << ", 0\n";
            ir << "  %exit_code = select i1 %any_failed, i32 1, i32 0\n";
            ir << "  ret i32 %exit_code\n";
        }
        ir << "\n";
    }

    // Handle zero tests edge case
    if (total_tests == 0) {
        ir << "  ret i32 0\n";
    }

    ir << "}\n\n";

    // ===== Main function =====
    ir << "define i32 @main(i32 %argc, i8** %argv) {\n";
    ir << "entry:\n";

    // If no arguments beyond program name, print error
    ir << "  %has_args = icmp sgt i32 %argc, 1\n";
    ir << "  br i1 %has_args, label %scan_args, label %no_index\n";
    ir << "\n";

    // Scan arguments for --test-index=N or --run-all
    ir << "scan_args:\n";
    ir << "  %i.start = add i32 0, 1\n";
    ir << "  br label %arg_loop\n";
    ir << "\n";

    ir << "arg_loop:\n";
    ir << "  %i = phi i32 [ %i.start, %scan_args ], [ %i.next, %arg_continue ]\n";
    ir << "  %done = icmp sge i32 %i, %argc\n";
    ir << "  br i1 %done, label %no_index, label %check_arg\n";
    ir << "\n";

    ir << "check_arg:\n";
    ir << "  %i.i64 = sext i32 %i to i64\n";
    ir << "  %arg_ptr = getelementptr i8*, i8** %argv, i64 %i.i64\n";
    ir << "  %arg = load i8*, i8** %arg_ptr\n";

    // Check for --run-all first
    ir << "  %run_all_ptr = getelementptr [10 x i8], [10 x i8]* @.str.run_all, i64 0, i64 0\n";
    ir << "  %cmp_run_all = call i32 @strcmp(i8* %arg, i8* %run_all_ptr)\n";
    ir << "  %is_run_all = icmp eq i32 %cmp_run_all, 0\n";
    ir << "  br i1 %is_run_all, label %do_run_all, label %check_test_index\n";
    ir << "\n";

    // Check for --test-index=N
    ir << "check_test_index:\n";
    ir << "  %prefix_ptr = getelementptr [14 x i8], [14 x i8]* @.str.prefix, i64 0, i64 0\n";
    ir << "  %cmp = call i32 @strncmp(i8* %arg, i8* %prefix_ptr, i64 13)\n";
    ir << "  %is_match = icmp eq i32 %cmp, 0\n";
    ir << "  br i1 %is_match, label %found_index, label %arg_continue\n";
    ir << "\n";

    ir << "arg_continue:\n";
    ir << "  %i.next = add i32 %i, 1\n";
    ir << "  br label %arg_loop\n";
    ir << "\n";

    // --run-all: call run_all_tests()
    ir << "do_run_all:\n";
    ir << "  %run_all_rc = call i32 @run_all_tests()\n";
    ir << "  ret i32 %run_all_rc\n";
    ir << "\n";

    // Parse the index number after the '=' sign
    ir << "found_index:\n";
    ir << "  %num_ptr = getelementptr i8, i8* %arg, i64 13\n";
    ir << "  %test_index = call i32 @atoi(i8* %num_ptr)\n";
    ir << "\n";

    // Switch on test index to call the right function
    ir << "  switch i32 %test_index, label %invalid_index [\n";
    for (int i = 0; i < total_tests; ++i) {
        ir << "    i32 " << i << ", label %call_test_" << i << "\n";
    }
    ir << "  ]\n";
    ir << "\n";

    // Generate call blocks for each test
    for (int i = 0; i < total_tests; ++i) {
        ir << "call_test_" << i << ":\n";
        ir << "  %result_" << i << " = call i32 @tml_test_" << i << "()\n";
        ir << "  ret i32 %result_" << i << "\n";
        ir << "\n";
    }

    // Error: invalid test index
    ir << "invalid_index:\n";
    ir << "  %err_ptr = getelementptr [30 x i8], [30 x i8]* @.str.error, i64 0, i64 0\n";
    ir << "  call i32 (i8*, ...) @printf(i8* %err_ptr, i32 %test_index)\n";
    ir << "  ret i32 99\n";
    ir << "\n";

    // Error: no --test-index argument
    ir << "no_index:\n";
    ir << "  %no_idx_ptr = getelementptr [41 x i8], [41 x i8]* @.str.no_index, i64 0, i64 0\n";
    ir << "  call i32 (i8*, ...) @printf(i8* %no_idx_ptr)\n";
    ir << "  ret i32 98\n";

    ir << "}\n";

    return ir.str();
}

} // namespace tml::cli
