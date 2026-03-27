TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Entry Point Generation
//!
//! This file implements `generate_main_and_test_harness()`, which handles all
//! entry point code generation:
//!
//! - Collecting @test / @bench / @fuzz / @Get/@Post/... decorated functions
//! - Emitting string constants accumulated during earlier codegen
//! - Generating the main() entry point (user main, test harness, bench harness,
//!   fuzz target)
//! - Generating the HTTP route registration function (@__tml_register_routes)
//!
//! Related files:
//! - generate.cpp: Main generate() entry point that calls this helper
//! - generate_cache.cpp: GlobalASTCache and GlobalLibraryIRCache implementations
//! - generate_support.cpp: Loop metadata, lifetime intrinsics, namespace support

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

namespace tml::codegen {

void LLVMIRGen::generate_main_and_test_harness(const parser::Module& module) {
    // Collect test, benchmark, and fuzz functions BEFORE emitting string constants
    // so we can pre-register expected panic message strings
    struct TestInfo {
        std::string name;
        bool should_panic = false;
        std::string expected_panic_message;     // Empty means any panic is fine
        std::string expected_panic_message_str; // LLVM string constant reference
    };
    std::vector<TestInfo> test_functions;
    std::vector<std::string> fuzz_functions;
    struct BenchInfo {
        std::string name;
        int64_t iterations = 1000; // Default iterations
    };
    std::vector<BenchInfo> bench_functions;
    struct LegacyRouteEntry {
        std::string method_str;
        std::string path;
        std::string func_name;
    };
    std::vector<LegacyRouteEntry> route_functions;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            bool is_test = false;
            bool should_panic = false;
            std::string expected_panic_message;

            for (const auto& decorator : func.decorators) {
                if (decorator.name == "test") {
                    is_test = true;
                } else if (decorator.name == "should_panic") {
                    should_panic = true;
                    // Check for expected message: @should_panic(expected = "message")
                    for (const auto& arg : decorator.args) {
                        if (arg->is<parser::BinaryExpr>()) {
                            // Handle named argument: expected = "message"
                            const auto& bin = arg->as<parser::BinaryExpr>();
                            if (bin.op == parser::BinaryOp::Assign &&
                                bin.left->is<parser::IdentExpr>() &&
                                bin.right->is<parser::LiteralExpr>()) {
                                const auto& ident = bin.left->as<parser::IdentExpr>();
                                const auto& lit = bin.right->as<parser::LiteralExpr>();
                                if (ident.name == "expected" &&
                                    lit.token.kind == lexer::TokenKind::StringLiteral) {
                                    expected_panic_message = lit.token.string_value().value;
                                }
                            }
                        } else if (arg->is<parser::LiteralExpr>()) {
                            // Also support @should_panic("message") without named argument
                            const auto& lit = arg->as<parser::LiteralExpr>();
                            if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                                expected_panic_message = lit.token.string_value().value;
                            }
                        }
                    }
                } else if (decorator.name == "bench") {
                    BenchInfo info;
                    info.name = func.name;
                    // Check for iterations argument: @bench(1000) or @bench(iterations=1000)
                    if (!decorator.args.empty()) {
                        const auto& arg = *decorator.args[0];
                        if (arg.is<parser::LiteralExpr>()) {
                            const auto& lit = arg.as<parser::LiteralExpr>();
                            if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                                info.iterations = static_cast<int64_t>(lit.token.int_value().value);
                            }
                        }
                    }
                    bench_functions.push_back(info);
                } else if (decorator.name == "fuzz") {
                    fuzz_functions.push_back(func.name);
                } else if (decorator.name == "Get" || decorator.name == "Post" ||
                           decorator.name == "Put" || decorator.name == "Delete" ||
                           decorator.name == "Patch" || decorator.name == "Head" ||
                           decorator.name == "Options") {
                    std::string method_str;
                    if (decorator.name == "Get")
                        method_str = "GET";
                    else if (decorator.name == "Post")
                        method_str = "POST";
                    else if (decorator.name == "Put")
                        method_str = "PUT";
                    else if (decorator.name == "Delete")
                        method_str = "DELETE";
                    else if (decorator.name == "Patch")
                        method_str = "PATCH";
                    else if (decorator.name == "Head")
                        method_str = "HEAD";
                    else
                        method_str = "OPTIONS";
                    if (!decorator.args.empty() && decorator.args[0]->is<parser::LiteralExpr>()) {
                        const auto& lit = decorator.args[0]->as<parser::LiteralExpr>();
                        if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                            route_functions.push_back(
                                {method_str, lit.token.string_value().value, func.name});
                        }
                    }
                }
            }

            if (is_test) {
                TestInfo info;
                info.name = func.name;
                info.should_panic = should_panic;
                info.expected_panic_message = expected_panic_message;
                // Pre-register the expected message string BEFORE emit_string_constants
                if (!expected_panic_message.empty()) {
                    info.expected_panic_message_str = add_string_literal(expected_panic_message);
                }
                test_functions.push_back(info);
            }
        }
    }

    // Emit string constants at the end (they were collected during codegen)
    emit_string_constants();

    // Generate main entry point
    bool has_user_main = false;
    bool main_returns_void = true;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>() && decl->as<parser::FuncDecl>().name == "main") {
            has_user_main = true;
            const auto& func = decl->as<parser::FuncDecl>();
            main_returns_void = !func.return_type.has_value();
            break;
        }
    }

    if (!bench_functions.empty()) {
        // Generate benchmark runner main with proper output
        // Note: time functions are always declared in preamble
        emit_line("; Auto-generated benchmark runner");
        emit_line("");

        // Add format strings for benchmark output
        // String lengths: \0A = 1 byte, \00 = 1 byte (null terminator)
        emit_line(
            "@.bench.header = private constant [23 x i8] c\"\\0A  Running benchmarks\\0A\\00\"");
        emit_line("@.bench.name = private constant [16 x i8] c\"  + bench %-20s\\00\"");
        emit_line("@.bench.time = private constant [19 x i8] c\" ... %lld ns/iter\\0A\\00\"");
        emit_line("@.bench.summary = private constant [30 x i8] c\"\\0A  %d benchmark(s) "
                  "completed\\0A\\00\"");

        // Add string constants for benchmark names
        int idx = 0;
        for (const auto& bench_info : bench_functions) {
            std::string name_const = "@.bench.fn." + std::to_string(idx);
            size_t name_len = bench_info.name.size() + 1;
            emit_line(name_const + " = private constant [" + std::to_string(name_len) +
                      " x i8] c\"" + bench_info.name + "\\00\"");
            idx++;
        }
        emit_line("");

        emit_line("define dso_local i32 @main(i32 %argc, ptr %argv) noinline {");
        emit_line("entry:");

        // Print benchmark header
        emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.header)");
        emit_line("");

        int bench_num = 0;
        std::string prev_block = "entry";
        for (const auto& bench_info : bench_functions) {
            std::string bench_fn = "@tml_" + bench_info.name;
            std::string n = std::to_string(bench_num);
            std::string name_const = "@.bench.fn." + n;
            std::string iterations_str = std::to_string(bench_info.iterations);

            // Print benchmark name
            emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.name, ptr " + name_const + ")");

            // Warmup: Run 10 iterations to warm up caches
            std::string warmup_var = "%warmup_" + n;
            std::string warmup_header = "warmup_header_" + n;
            std::string warmup_body = "warmup_body_" + n;
            std::string warmup_end = "warmup_end_" + n;

            emit_line("  br label %" + warmup_header);
            emit_line("");
            emit_line(warmup_header + ":");
            emit_line("  " + warmup_var + " = phi i64 [ 0, %" + prev_block + " ], [ " + warmup_var +
                      "_next, %" + warmup_body + " ]");
            emit_line("  %warmup_cmp_" + n + " = icmp slt i64 " + warmup_var + ", 10");
            emit_line("  br i1 %warmup_cmp_" + n + ", label %" + warmup_body + ", label %" +
                      warmup_end);
            emit_line("");
            emit_line(warmup_body + ":");
            emit_line("  call void " + bench_fn + "()");
            emit_line("  " + warmup_var + "_next = add i64 " + warmup_var + ", 1");
            emit_line("  br label %" + warmup_header);
            emit_line("");
            emit_line(warmup_end + ":");

            // Get start time (nanoseconds for precision)
            std::string start_time = "%bench_start_" + n;
            emit_line("  " + start_time + " = call i64 @time_ns()");

            // Run benchmark with configured iterations (default 1000)
            std::string iter_var = "%bench_iter_" + n;
            std::string loop_header = "bench_loop_header_" + n;
            std::string loop_body = "bench_loop_body_" + n;
            std::string loop_end = "bench_loop_end_" + n;

            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_header + ":");
            emit_line("  " + iter_var + " = phi i64 [ 0, %" + warmup_end + " ], [ " + iter_var +
                      "_next, %" + loop_body + " ]");
            std::string cmp_var = "%bench_cmp_" + n;
            emit_line("  " + cmp_var + " = icmp slt i64 " + iter_var + ", " + iterations_str);
            emit_line("  br i1 " + cmp_var + ", label %" + loop_body + ", label %" + loop_end);
            emit_line("");
            emit_line(loop_body + ":");
            emit_line("  call void " + bench_fn + "()");
            emit_line("  " + iter_var + "_next = add i64 " + iter_var + ", 1");
            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_end + ":");

            // Get end time and calculate duration
            std::string end_time = "%bench_end_" + n;
            std::string duration = "%bench_duration_" + n;
            emit_line("  " + end_time + " = call i64 @time_ns()");
            emit_line("  " + duration + " = sub i64 " + end_time + ", " + start_time);

            // Calculate average (duration / iterations)
            std::string avg_time = "%bench_avg_" + n;
            emit_line("  " + avg_time + " = sdiv i64 " + duration + ", " + iterations_str);

            // Print benchmark time
            emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.time, i64 " + avg_time + ")");
            emit_line("");

            prev_block = loop_end;
            bench_num++;
        }

        // Print summary
        emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.summary, i32 " +
                  std::to_string(bench_num) + ")");
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (options_.generate_fuzz_entry && !fuzz_functions.empty()) {
        // Generate fuzz target entry point for fuzzing
        // The fuzz target receives (ptr data, i64 len) and calls @fuzz functions
        emit_line("; Auto-generated fuzz target entry point");
        emit_line("");

#ifdef _WIN32
        emit_line("define dllexport i32 @tml_fuzz_target(ptr %data, i64 %len) {");
#else
        emit_line("define i32 @tml_fuzz_target(ptr %data, i64 %len) {");
#endif
        emit_line("entry:");

        // Call each @fuzz function with the input data
        // Fuzz functions should have signature: func fuzz_name(data: Ptr[U8], len: U64)
        for (const auto& fuzz_name : fuzz_functions) {
            std::string fuzz_fn = "@tml_" + fuzz_name;
            // Look up the function's return type from functions_ map
            auto it = functions_.find(fuzz_name);
            if (it != functions_.end()) {
                // Check if function takes (ptr, i64) parameters
                if (it->second.param_types.size() >= 2) {
                    emit_line("  call void " + fuzz_fn + "(ptr %data, i64 %len)");
                } else {
                    // Function doesn't take data parameters, just call it
                    emit_line("  call void " + fuzz_fn + "()");
                }
            } else {
                // Fallback - assume void function
                emit_line("  call void " + fuzz_fn + "()");
            }
        }

        // Return 0 for success (crash will never reach here)
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (!test_functions.empty()) {
        // Generate test runner main (or DLL entry point)
        // @test functions can return I32 (0 for success) or Unit
        // Assertions inside will call panic() on failure which doesn't return
        emit_line("; Auto-generated test runner");

        // Check if any tests need @should_panic support
        bool has_should_panic = false;
        for (const auto& test_info : test_functions) {
            if (test_info.should_panic) {
                has_should_panic = true;
                break;
            }
        }

        // Add error message strings for should_panic tests
        if (has_should_panic) {
            emit_line("");
            emit_line("; Error messages for @should_panic tests");
            // "test did not panic as expected\n\0" = 30 + 1 + 1 = 32 bytes
            emit_line("@.should_panic_no_panic = private constant [32 x i8] c\"test did not "
                      "panic as expected\\0A\\00\"");
            // "panic message did not contain expected string\n\0" = 45 + 1 + 1 = 47 bytes
            emit_line("@.should_panic_wrong_msg = private constant [47 x i8] c\"panic message "
                      "did not contain expected string\\0A\\00\"");
            emit_line("");
        }

        // String constant for coverage file environment variable name
        emit_line("; Environment variable name for coverage file (EXE mode)");
        emit_line("@.tml_cov_file_env = private constant [18 x i8] c\"TML_COVERAGE_FILE\\00\"");

        // For DLL entry, generate exported test entry function instead of main
        if (options_.generate_dll_entry) {
            // Determine entry function name (tml_test_entry or tml_test_N for suites)
            std::string entry_name = "tml_test_entry";
            if (options_.suite_test_index >= 0) {
                entry_name = "tml_test_" + std::to_string(options_.suite_test_index);
            }
#ifdef _WIN32
            emit_line("define dllexport i32 @" + entry_name + "() {");
#else
            emit_line("define i32 @" + entry_name + "() {");
#endif
        } else {
            emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        }
        emit_line("entry:");

        // In suite mode, test functions have a prefix to avoid collisions
        std::string test_suite_prefix = "";
        if (options_.suite_test_index >= 0 && options_.force_internal_linkage) {
            test_suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
        }

        int test_idx = 0;
        std::string prev_block = "entry";
        for (const auto& test_info : test_functions) {
            std::string test_fn = "@tml_" + test_suite_prefix + test_info.name;
            std::string idx_str = std::to_string(test_idx);

            if (test_info.should_panic) {
                // Generate panic-catching call for @should_panic tests
                // Uses callback approach: pass function pointer to tml_run_should_panic()
                // which keeps setjmp on the stack while the test runs

                // Call tml_run_should_panic with function pointer
                // Returns: 1 if panicked (success), 0 if didn't panic (failure)
                std::string result = "%panic_result_" + idx_str;
                emit_line("  " + result + " = call i32 @tml_run_should_panic(ptr " + test_fn + ")");

                // Check if test panicked
                std::string cmp = "%panic_cmp_" + idx_str;
                emit_line("  " + cmp + " = icmp eq i32 " + result + ", 0");

                std::string no_panic_label = "no_panic_" + idx_str;
                std::string panic_ok_label = "panic_ok_" + idx_str;
                std::string test_done_label = "test_done_" + idx_str;

                emit_line("  br i1 " + cmp + ", label %" + no_panic_label + ", label %" +
                          panic_ok_label);
                emit_line("");

                // Test didn't panic - that's an error for @should_panic
                emit_line(no_panic_label + ":");
                emit_line("  call i32 (ptr, ...) @printf(ptr @.should_panic_no_panic)");
                emit_line("  call void @exit(i32 1)");
                emit_line("  unreachable");
                emit_line("");

                // Test panicked - check message if expected
                emit_line(panic_ok_label + ":");
                if (!test_info.expected_panic_message_str.empty()) {
                    // Check if panic message contains expected string
                    std::string msg_check = "%msg_check_" + idx_str;
                    emit_line("  " + msg_check + " = call i32 @tml_panic_message_contains(ptr " +
                              test_info.expected_panic_message_str + ")");

                    std::string msg_ok_label = "msg_ok_" + idx_str;
                    std::string msg_fail_label = "msg_fail_" + idx_str;
                    std::string msg_cmp = "%msg_cmp_" + idx_str;
                    emit_line("  " + msg_cmp + " = icmp ne i32 " + msg_check + ", 0");
                    emit_line("  br i1 " + msg_cmp + ", label %" + msg_ok_label + ", label %" +
                              msg_fail_label);
                    emit_line("");

                    // Message didn't match - fail
                    emit_line(msg_fail_label + ":");
                    emit_line("  call i32 (ptr, ...) @printf(ptr @.should_panic_wrong_msg)");
                    emit_line("  call void @exit(i32 1)");
                    emit_line("  unreachable");
                    emit_line("");

                    // Message matched - continue
                    emit_line(msg_ok_label + ":");
                    emit_line("  br label %" + test_done_label);
                } else {
                    // No expected message - any panic is fine
                    emit_line("  br label %" + test_done_label);
                }
                emit_line("");

                emit_line(test_done_label + ":");
                prev_block = test_done_label;
            } else {
                // Regular test - just call it
                auto it = functions_.find(test_info.name);
                if (it != functions_.end() && it->second.ret_type != "void") {
                    std::string tmp = "%test_result_" + idx_str;
                    emit_line("  " + tmp + " = call " + it->second.ret_type + " " + test_fn + "()");
                } else if (it != functions_.end()) {
                    emit_line("  call void " + test_fn + "()");
                } else {
                    // Test function not found in functions_ map - likely a name collision
                    // with an imported module function (e.g., test function "test_assert_str_empty"
                    // collides with module "test" function "assert_str_empty" -> both mangle to
                    // "tml_test_assert_str_empty"). Emit as i32 call (test convention) with a
                    // stderr warning.
                    emit_line("  ; WARNING: test function '" + test_info.name +
                              "' not found in functions_ map");
                    emit_line(
                        "  ; This may indicate a name collision with an imported module function.");
                    emit_line("  ; Consider renaming the test function to avoid the collision.");
                    std::string tmp = "%test_result_" + idx_str;
                    emit_line("  " + tmp + " = call i32 " + test_fn + "()");
                }
            }

            test_idx++;
        }

        // Write coverage data to file for EXE mode subprocess communication
        // When running under EXE mode, write covered functions to file specified by env var
        emit_line("  %cov_file_env = call ptr @getenv(ptr @.tml_cov_file_env)");
        emit_line("  %cov_file_not_null = icmp ne ptr %cov_file_env, null");
        emit_line("  br i1 %cov_file_not_null, label %write_cov_file, label %cov_file_done");
        emit_line("");
        emit_line("write_cov_file:");
        emit_line("  call void @tml_coverage_write_file(ptr %cov_file_env)");
        emit_line("  br label %cov_file_done");
        emit_line("");
        emit_line("cov_file_done:");

        // All tests passed (if we got here, no assertion failed)
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (has_user_main) {
        // Standard main wrapper for user-defined main
        emit_line("; Entry point");

        // In suite mode, tml_main has a prefix to avoid collisions
        std::string main_suite_prefix = "";
        if (options_.suite_test_index >= 0 && options_.force_internal_linkage) {
            main_suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
        }
        std::string tml_main_fn = "tml_" + main_suite_prefix + "main";

        // For DLL entry, generate exported test entry function instead of main
        if (options_.generate_dll_entry) {
            // Determine entry function name (tml_test_entry or tml_test_N for suites)
            std::string entry_name = "tml_test_entry";
            if (options_.suite_test_index >= 0) {
                entry_name = "tml_test_" + std::to_string(options_.suite_test_index);
            }
#ifdef _WIN32
            emit_line("define dllexport i32 @" + entry_name + "() {");
#else
            emit_line("define i32 @" + entry_name + "() {");
#endif
            emit_line("entry:");
            if (main_returns_void) {
                emit_line("  call void @" + tml_main_fn + "()");
            } else {
                emit_line("  %ret = call i32 @" + tml_main_fn + "()");
            }

            emit_line("  ret i32 " + std::string(main_returns_void ? "0" : "%ret"));
            emit_line("}");
        } else {
            emit_line("define dso_local i32 @main(i32 %argc, ptr %argv) noinline {");
            emit_line("entry:");
            // Enable backtrace on panic if flag is set
            if (CompilerOptions::backtrace) {
                emit_line("  call void @tml_enable_backtrace_on_panic()");
            }
            if (main_returns_void) {
                emit_line("  call void @" + tml_main_fn + "()");
            } else {
                emit_line("  %ret = call i32 @" + tml_main_fn + "()");
            }

            emit_line("  ret i32 " + std::string(main_returns_void ? "0" : "%ret"));
            emit_line("}");
        }
    }

    // Emit HTTP route registration function for @Get/@Post/etc. decorators
    if (!route_functions.empty()) {
        emit_line("");
        emit_line(
            "; Route registration from @Get/@Post/@Put/@Delete/@Patch/@Head/@Options decorators");
        for (size_t i = 0; i < route_functions.size(); ++i) {
            const auto& route = route_functions[i];
            std::string idx = std::to_string(i);
            size_t method_len = route.method_str.size() + 1;
            size_t path_len = route.path.size() + 1;
            emit_line("@.route.method." + idx + " = private constant [" +
                      std::to_string(method_len) + " x i8] c\"" + route.method_str + "\\00\"");
            emit_line("@.route.path." + idx + " = private constant [" + std::to_string(path_len) +
                      " x i8] c\"" + route.path + "\\00\"");
        }
        // Look up the actual mangled LLVM name for app_register
        // Try multiple keys since imported functions use qualified names
        std::string app_register_llvm_name;
        for (const auto& key :
             {"app_register", "app::app_register", "std::http::app::app_register"}) {
            auto it = functions_.find(key);
            if (it != functions_.end()) {
                app_register_llvm_name = it->second.llvm_name;
                break;
            }
        }
        if (app_register_llvm_name.empty()) {
            // Fallback: scan all functions for one containing "app_register"
            for (const auto& [name, info] : functions_) {
                if (name.find("app_register") != std::string::npos) {
                    app_register_llvm_name = info.llvm_name;
                    break;
                }
            }
        }
        (void)app_register_llvm_name; // Not used — inline registration below

        emit_line("");
        // Generate inline route registration (no dependency on app_register function)
        // This directly writes to the flat handler table: 24 bytes per entry
        // [method_ptr: i64, path_ptr: i64, handler_ptr: i64]
        emit_line("define void @__tml_register_routes(i64 %table, ptr %count_ptr, i64 %trees) {");
        emit_line("entry:");
        emit_line("  %count_init = load i64, ptr %count_ptr");

        for (size_t i = 0; i < route_functions.size(); ++i) {
            const auto& route = route_functions[i];
            std::string idx = std::to_string(i);
            size_t method_len = route.method_str.size() + 1;
            size_t path_len = route.path.size() + 1;

            // Compute offset = (count + i) * 24
            emit_line("  %slot_" + idx + " = add i64 %count_init, " + std::to_string(i));
            emit_line("  %offset_" + idx + " = mul i64 %slot_" + idx + ", 24");
            // Base address for this entry
            emit_line("  %base_" + idx + " = add i64 %table, %offset_" + idx);

            // Get method and path string pointers
            emit_line("  %method_" + idx + " = getelementptr [" + std::to_string(method_len) +
                      " x i8], ptr @.route.method." + idx + ", i32 0, i32 0");
            emit_line("  %path_" + idx + " = getelementptr [" + std::to_string(path_len) +
                      " x i8], ptr @.route.path." + idx + ", i32 0, i32 0");
            emit_line("  %handler_" + idx + " = ptrtoint ptr @tml_" + route.func_name + " to i64");

            // Write method ptr at offset + 0
            emit_line("  %method_i64_" + idx + " = ptrtoint ptr %method_" + idx + " to i64");
            emit_line("  %mptr_" + idx + " = inttoptr i64 %base_" + idx + " to ptr");
            emit_line("  store i64 %method_i64_" + idx + ", ptr %mptr_" + idx);

            // Write path ptr at offset + 8
            emit_line("  %path_off_" + idx + " = add i64 %base_" + idx + ", 8");
            emit_line("  %path_i64_" + idx + " = ptrtoint ptr %path_" + idx + " to i64");
            emit_line("  %pptr_" + idx + " = inttoptr i64 %path_off_" + idx + " to ptr");
            emit_line("  store i64 %path_i64_" + idx + ", ptr %pptr_" + idx);

            // Write handler ptr at offset + 16
            emit_line("  %hndl_off_" + idx + " = add i64 %base_" + idx + ", 16");
            emit_line("  %hptr_" + idx + " = inttoptr i64 %hndl_off_" + idx + " to ptr");
            emit_line("  store i64 %handler_" + idx + ", ptr %hptr_" + idx);
        }

        // Update count = count + N
        std::string new_count = std::to_string(route_functions.size());
        emit_line("  %new_count = add i64 %count_init, " + new_count);
        emit_line("  store i64 %new_count, ptr %count_ptr");

        emit_line("  ret void");
        emit_line("}");
    }
}

} // namespace tml::codegen
