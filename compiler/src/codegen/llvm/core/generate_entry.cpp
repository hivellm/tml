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
        bool slow_test = false;                 // @slow_test: use 10000ms timeout instead of 100ms
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
        bool is_impl_method = false;
        // Original parameter types (before ptr-forcing) for thunk generation.
        // Each entry is (llvm_type, param_name), e.g. ("%struct.IncomingMessage", "req").
        std::vector<std::pair<std::string, std::string>> param_types;
        std::string return_type; // e.g. "ptr" for Str
    };
    std::vector<LegacyRouteEntry> route_functions;

    // Helper: extract route decorator info from a decorator list.
    // Returns true if a route was found, and populates method_str and path.
    auto extract_route_decorator = [](const std::vector<parser::Decorator>& decorators,
                                      std::string& method_str, std::string& path) -> bool {
        for (const auto& decorator : decorators) {
            if (decorator.name == "Get" || decorator.name == "Post" || decorator.name == "Put" ||
                decorator.name == "Delete" || decorator.name == "Patch" ||
                decorator.name == "Head" || decorator.name == "Options") {
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
                        path = lit.token.string_value().value;
                        return true;
                    }
                }
            }
        }
        return false;
    };

    // Helper: extract @Controller prefix from a decorator list (on struct/class types).
    auto extract_controller_prefix =
        [](const std::vector<parser::Decorator>& decorators) -> std::string {
        for (const auto& decorator : decorators) {
            if (decorator.name == "Controller" && !decorator.args.empty()) {
                if (decorator.args[0]->is<parser::LiteralExpr>()) {
                    const auto& lit = decorator.args[0]->as<parser::LiteralExpr>();
                    if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                        std::string prefix = lit.token.string_value().value;
                        // Remove trailing slash to avoid double slashes
                        while (prefix.size() > 1 && prefix.back() == '/') {
                            prefix.pop_back();
                        }
                        return prefix;
                    }
                }
            }
        }
        return "";
    };

    // Helper: prepend controller prefix to a route path.
    auto apply_controller_prefix = [](const std::string& controller_prefix,
                                      const std::string& method_path) -> std::string {
        if (controller_prefix.empty()) {
            return method_path;
        }
        std::string normalized_path = method_path;
        if (normalized_path.empty() || normalized_path[0] != '/') {
            normalized_path = "/" + normalized_path;
        }
        if (normalized_path == "/") {
            return controller_prefix;
        }
        return controller_prefix + normalized_path;
    };

    // Build a map of struct/class name -> @Controller prefix for looking up impl targets.
    std::unordered_map<std::string, std::string> controller_prefixes;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::StructDecl>()) {
            const auto& s = decl->as<parser::StructDecl>();
            std::string prefix = extract_controller_prefix(s.decorators);
            if (!prefix.empty()) {
                controller_prefixes[s.name] = prefix;
            }
        } else if (decl->is<parser::ClassDecl>()) {
            const auto& c = decl->as<parser::ClassDecl>();
            std::string prefix = extract_controller_prefix(c.decorators);
            if (!prefix.empty()) {
                controller_prefixes[c.name] = prefix;
            }
        }
    }

    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            bool is_test = false;
            bool should_panic = false;
            bool slow_test = false;
            std::string expected_panic_message;

            for (const auto& decorator : func.decorators) {
                if (decorator.name == "test") {
                    is_test = true;
                } else if (decorator.name == "slow_test") {
                    slow_test = true;
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
                }
            }

            // Check for route decorators on top-level functions
            std::string method_str, path;
            if (extract_route_decorator(func.decorators, method_str, path)) {
                route_functions.push_back({method_str, path, func.name});
            }

            if (is_test) {
                TestInfo info;
                info.name = func.name;
                info.should_panic = should_panic;
                info.slow_test = slow_test;
                info.expected_panic_message = expected_panic_message;
                // Pre-register the expected message string BEFORE emit_string_constants
                if (!expected_panic_message.empty()) {
                    info.expected_panic_message_str = add_string_literal(expected_panic_message);
                }
                test_functions.push_back(info);
            }
        } else if (decl->is<parser::ImplDecl>()) {
            // Scan impl blocks for methods with @Get/@Post/etc. route decorators.
            // For @Controller types, prepend the controller prefix to route paths.
            const auto& impl_decl = decl->as<parser::ImplDecl>();

            // Extract the type name from self_type (e.g., "UserController")
            std::string type_name;
            if (impl_decl.self_type && impl_decl.self_type->is<parser::NamedType>()) {
                const auto& named = impl_decl.self_type->as<parser::NamedType>();
                if (!named.path.segments.empty()) {
                    type_name = named.path.segments.back();
                }
            }
            if (type_name.empty())
                continue;

            // Look up @Controller prefix for this type
            std::string ctrl_prefix;
            auto cp_it = controller_prefixes.find(type_name);
            if (cp_it != controller_prefixes.end()) {
                ctrl_prefix = cp_it->second;
            }

            for (const auto& method : impl_decl.methods) {
                std::string method_str, path;
                if (extract_route_decorator(method.decorators, method_str, path)) {
                    // Apply controller prefix to the route path
                    std::string full_path = apply_controller_prefix(ctrl_prefix, path);
                    // Compute the mangled function name for the impl method.
                    // mangle_impl_method returns "tml_<prefix>TypeName_method" — strip "tml_"
                    // because the route registration emitter adds "@tml_" itself.
                    std::string mangled = mangle_impl_method(type_name, method.name);
                    std::string func_name = mangled;
                    if (func_name.substr(0, 4) == "tml_") {
                        func_name = func_name.substr(4);
                    }
                    // Collect parameter types for thunk generation.
                    // impl.cpp:350-353 forces the first struct param to ptr, creating
                    // an ABI mismatch with fn-pointer call sites (dispatch.tml:668).
                    LegacyRouteEntry entry;
                    entry.method_str = method_str;
                    entry.path = full_path;
                    entry.func_name = func_name;
                    entry.is_impl_method = true;
                    for (const auto& param : method.params) {
                        std::string ptype = llvm_type_ptr(param.type);
                        if (param.type && param.type->is<parser::FuncType>()) {
                            ptype = "{ ptr, ptr }";
                        }
                        std::string pname = "arg";
                        if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
                            pname = param.pattern->as<parser::IdentPattern>().name;
                        }
                        entry.param_types.push_back({ptype, pname});
                    }
                    // Resolve return type
                    if (method.return_type.has_value()) {
                        entry.return_type = llvm_type_ptr(*method.return_type);
                    } else {
                        entry.return_type = "void";
                    }
                    route_functions.push_back(std::move(entry));
                }
            }
        } else if (decl->is<parser::ClassDecl>()) {
            // Scan class declarations for methods with @Get/@Post/etc. route decorators.
            const auto& class_decl = decl->as<parser::ClassDecl>();
            std::string type_name = class_decl.name;

            // Look up @Controller prefix for this class
            std::string ctrl_prefix;
            auto cp_it = controller_prefixes.find(type_name);
            if (cp_it != controller_prefixes.end()) {
                ctrl_prefix = cp_it->second;
            }

            for (const auto& method : class_decl.methods) {
                std::string method_str, path;
                if (extract_route_decorator(method.decorators, method_str, path)) {
                    std::string full_path = apply_controller_prefix(ctrl_prefix, path);
                    std::string mangled = mangle_impl_method(type_name, method.name);
                    std::string func_name = mangled;
                    if (func_name.substr(0, 4) == "tml_") {
                        func_name = func_name.substr(4);
                    }
                    // Class methods have the same ABI issue as impl methods
                    LegacyRouteEntry entry;
                    entry.method_str = method_str;
                    entry.path = full_path;
                    entry.func_name = func_name;
                    entry.is_impl_method = true;
                    for (const auto& param : method.params) {
                        std::string ptype = llvm_type_ptr(param.type);
                        if (param.type && param.type->is<parser::FuncType>()) {
                            ptype = "{ ptr, ptr }";
                        }
                        std::string pname = "arg";
                        if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
                            pname = param.pattern->as<parser::IdentPattern>().name;
                        }
                        entry.param_types.push_back({ptype, pname});
                    }
                    if (method.return_type.has_value()) {
                        entry.return_type = llvm_type_ptr(*method.return_type);
                    } else {
                        entry.return_type = "void";
                    }
                    route_functions.push_back(std::move(entry));
                }
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

        // Declare tml_set_test_timeout — called per-test below
        require_runtime_decl("tml_set_test_timeout");

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

            // Set timeout per-test: @slow_test gets 10000ms, normal tests get 100ms
            int timeout_ms = test_info.slow_test ? 10000 : 100;
            emit_line("  call void @tml_set_test_timeout(i32 " + std::to_string(timeout_ms) + ")");

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
                // Regular test - wrap with tml_run_test_with_catch for timeout + crash protection
                require_runtime_decl("tml_run_test_with_catch");
                std::string catch_result = "%test_catch_" + idx_str;
                emit_line("  " + catch_result + " = call i32 @tml_run_test_with_catch(ptr " +
                          test_fn + ")");
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

    // Always emit __tml_register_routes — even if no routes exist (empty function).
    // This allows library code (app_listen) to unconditionally call it without
    // worrying about whether the symbol exists.
    {
        // In suite mode (force_internal_linkage), every test .obj in the suite
        // emits its own __tml_register_routes/__tml_route_thunk_N definitions.
        // All references are module-local (func.cpp skips the extern declaration
        // because codegen always defines it in-module), so internal linkage is
        // required to avoid LLD duplicate-symbol errors when linking multiple
        // test objects into one aggregated EXE.
        std::string route_linkage = options_.force_internal_linkage ? "internal " : "";
        emit_line("");
        emit_line(
            "; Route registration from @Get/@Post/@Put/@Delete/@Patch/@Head/@Options decorators");

        // Emit ABI trampoline wrappers for impl/class method routes.
        // impl.cpp:350-353 forces the first struct param of static impl methods to ptr,
        // but dispatch.tml:668 calls handlers as func(IncomingMessage, Response) -> Str
        // passing structs by value. The thunks bridge this ABI gap by receiving params
        // by value and passing the first struct param as a pointer to the real function.
        for (size_t i = 0; i < route_functions.size(); ++i) {
            const auto& route = route_functions[i];
            if (!route.is_impl_method || route.param_types.empty())
                continue;

            std::string idx = std::to_string(i);
            std::string thunk_name = "@__tml_route_thunk_" + idx;
            std::string ret_type = route.return_type.empty() ? "void" : route.return_type;

            // Build thunk parameter list (by-value struct types — matches fn ptr call ABI)
            std::string thunk_params;
            for (size_t j = 0; j < route.param_types.size(); ++j) {
                if (j > 0)
                    thunk_params += ", ";
                thunk_params += route.param_types[j].first + " %" + route.param_types[j].second;
            }

            emit_line("");
            emit_line("; ABI trampoline: receives by-value structs, passes first struct as ptr");
            emit_line("define " + route_linkage + ret_type + " " + thunk_name + "(" + thunk_params +
                      ") {");
            emit_line("entry:");

            // For the first struct parameter, alloca + store to get a pointer.
            // impl.cpp forces only the first struct param to ptr; remaining params pass through.
            std::string call_args;
            for (size_t j = 0; j < route.param_types.size(); ++j) {
                if (j > 0)
                    call_args += ", ";
                const auto& [ptype, pname] = route.param_types[j];
                if (j == 0 && (ptype.find("%struct.") == 0 || ptype.find("%enum.") == 0)) {
                    // Spill first struct param to memory and pass as ptr
                    emit_line("  %spill_" + pname + " = alloca " + ptype + ", align 8");
                    emit_line("  store " + ptype + " %" + pname + ", ptr %spill_" + pname +
                              ", align 8");
                    call_args += "ptr %spill_" + pname;
                } else {
                    call_args += ptype + " %" + pname;
                }
            }

            // Call the real impl method function
            if (ret_type == "void") {
                emit_line("  call void @tml_" + route.func_name + "(" + call_args + ")");
                emit_line("  ret void");
            } else {
                emit_line("  %ret = call " + ret_type + " @tml_" + route.func_name + "(" +
                          call_args + ")");
                emit_line("  ret " + ret_type + " %ret");
            }
            emit_line("}");
        }

        if (!route_functions.empty()) {
            // Emit string constants for route methods and paths
            for (size_t i = 0; i < route_functions.size(); ++i) {
                const auto& route = route_functions[i];
                std::string idx = std::to_string(i);
                size_t method_len = route.method_str.size() + 1;
                size_t path_len = route.path.size() + 1;
                emit_line("@.route.method." + idx + " = private constant [" +
                          std::to_string(method_len) + " x i8] c\"" + route.method_str + "\\00\"");
                emit_line("@.route.path." + idx + " = private constant [" +
                          std::to_string(path_len) + " x i8] c\"" + route.path + "\\00\"");
            }
        }

        emit_line("");
        // Generate inline route registration (no dependency on app_register function)
        // This directly writes to the flat handler table: 24 bytes per entry
        // [method_ptr: i64, path_ptr: i64, handler_ptr: i64]
        emit_line("define " + route_linkage +
                  "void @__tml_register_routes(i64 %table, ptr %count_ptr, i64 %trees) {");
        emit_line("entry:");

        if (route_functions.empty()) {
            // No routes — just return
            emit_line("  ret void");
        } else {
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

                // For impl method routes, register the thunk instead of the real function
                if (route.is_impl_method && !route.param_types.empty()) {
                    emit_line("  %handler_" + idx + " = ptrtoint ptr @__tml_route_thunk_" + idx +
                              " to i64");
                } else {
                    emit_line("  %handler_" + idx + " = ptrtoint ptr @tml_" + route.func_name +
                              " to i64");
                }

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
        }

        emit_line("}");
    }
}

} // namespace tml::codegen
