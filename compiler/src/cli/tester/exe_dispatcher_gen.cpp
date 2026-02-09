//! # EXE Test Dispatcher IR Generator
//!
//! Generates LLVM IR for a dispatcher main() function that:
//! 1. Parses --test-index=N from command-line arguments
//! 2. Calls the corresponding tml_test_N() function
//! 3. Returns the test's exit code
//!
//! The generated IR is compiled to an object file and linked with
//! the test objects to produce a suite executable.

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
    ir << "@.str.error = private unnamed_addr constant [30 x i8] "
          "c\"ERROR: invalid test index %d\\0A\\00\"\n";
    ir << "@.str.no_index = private unnamed_addr constant [41 x i8] "
          "c\"ERROR: --test-index=N argument required\\0A\\00\"\n";
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
    ir << "\n";

    // Main function
    ir << "define i32 @main(i32 %argc, i8** %argv) {\n";
    ir << "entry:\n";

    // If no arguments beyond program name, print error
    ir << "  %has_args = icmp sgt i32 %argc, 1\n";
    ir << "  br i1 %has_args, label %scan_args, label %no_index\n";
    ir << "\n";

    // Scan arguments for --test-index=N
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
    ir << "  %prefix_ptr = getelementptr [14 x i8], [14 x i8]* @.str.prefix, i64 0, i64 0\n";
    // Compare first 13 chars (\"--test-index=\")
    ir << "  %cmp = call i32 @strncmp(i8* %arg, i8* %prefix_ptr, i64 13)\n";
    ir << "  %is_match = icmp eq i32 %cmp, 0\n";
    ir << "  br i1 %is_match, label %found_index, label %arg_continue\n";
    ir << "\n";

    ir << "arg_continue:\n";
    ir << "  %i.next = add i32 %i, 1\n";
    ir << "  br label %arg_loop\n";
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
