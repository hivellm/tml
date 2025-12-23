// LLVM IR text generator for TML - Core utilities
// Generates LLVM IR as text (.ll format) for compilation with clang
//
// This file contains:
// - Constructor and core utilities
// - Type translation helpers
// - Module structure generation (header, runtime decls, string constants)
// - Main generate() function
// - infer_print_type helper
//
// Split files:
// - llvm_ir_gen_decl.cpp: struct, enum, function declarations
// - llvm_ir_gen_stmt.cpp: let statements, expression statements
// - llvm_ir_gen_expr.cpp: literals, identifiers, binary/unary ops
// - llvm_ir_gen_builtins.cpp: builtin function calls (print, memory, atomics, threading)
// - llvm_ir_gen_control.cpp: if, block, loop, while, for, return
// - llvm_ir_gen_types.cpp: struct expressions, fields, arrays, indexing, method calls

#include "tml/codegen/llvm_ir_gen.hpp"
#include <algorithm>
#include <iomanip>

namespace tml::codegen {

LLVMIRGen::LLVMIRGen(const types::TypeEnv& env, LLVMGenOptions options)
    : env_(env), options_(std::move(options)) {}

auto LLVMIRGen::fresh_reg() -> std::string {
    return "%t" + std::to_string(temp_counter_++);
}

auto LLVMIRGen::fresh_label(const std::string& prefix) -> std::string {
    return prefix + std::to_string(label_counter_++);
}

void LLVMIRGen::emit(const std::string& code) {
    output_ << code;
}

void LLVMIRGen::emit_line(const std::string& code) {
    output_ << code << "\n";
}

void LLVMIRGen::report_error(const std::string& msg, const SourceSpan& span) {
    errors_.push_back(LLVMGenError{msg, span, {}});
}

auto LLVMIRGen::llvm_type_name(const std::string& name) -> std::string {
    // Primitive types
    if (name == "I8") return "i8";
    if (name == "I16") return "i16";
    if (name == "I32") return "i32";
    if (name == "I64") return "i64";
    if (name == "I128") return "i128";
    if (name == "U8") return "i8";
    if (name == "U16") return "i16";
    if (name == "U32") return "i32";
    if (name == "U64") return "i64";
    if (name == "U128") return "i128";
    if (name == "F32") return "float";
    if (name == "F64") return "double";
    if (name == "Bool") return "i1";
    if (name == "Char") return "i32";
    if (name == "Str" || name == "String") return "ptr";  // String is a pointer to struct
    if (name == "Unit") return "void";

    // Collection types - all are pointers to runtime structs
    if (name == "List" || name == "Vec" || name == "Array") return "ptr";
    if (name == "HashMap" || name == "Map" || name == "Dict") return "ptr";
    if (name == "Buffer") return "ptr";
    if (name == "Channel") return "ptr";
    if (name == "Mutex") return "ptr";
    if (name == "WaitGroup") return "ptr";

    // User-defined type - return struct type
    return "%struct." + name;
}

auto LLVMIRGen::llvm_type(const parser::Type& type) -> std::string {
    if (type.is<parser::NamedType>()) {
        const auto& named = type.as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            return llvm_type_name(named.path.segments.back());
        }
    } else if (type.is<parser::RefType>()) {
        return "ptr";
    } else if (type.is<parser::PtrType>()) {
        return "ptr";
    } else if (type.is<parser::ArrayType>()) {
        return "ptr";
    } else if (type.is<parser::FuncType>()) {
        // Function types are pointers in LLVM
        return "ptr";
    }
    return "i32";  // Default
}

auto LLVMIRGen::llvm_type_ptr(const parser::TypePtr& type) -> std::string {
    if (!type) return "void";
    return llvm_type(*type);
}

auto LLVMIRGen::llvm_type_from_semantic(const types::TypePtr& type) -> std::string {
    if (!type) return "void";

    if (type->is<types::PrimitiveType>()) {
        const auto& prim = type->as<types::PrimitiveType>();
        switch (prim.kind) {
            case types::PrimitiveKind::I8: return "i8";
            case types::PrimitiveKind::I16: return "i16";
            case types::PrimitiveKind::I32: return "i32";
            case types::PrimitiveKind::I64: return "i64";
            case types::PrimitiveKind::I128: return "i128";
            case types::PrimitiveKind::U8: return "i8";
            case types::PrimitiveKind::U16: return "i16";
            case types::PrimitiveKind::U32: return "i32";
            case types::PrimitiveKind::U64: return "i64";
            case types::PrimitiveKind::U128: return "i128";
            case types::PrimitiveKind::F32: return "float";
            case types::PrimitiveKind::F64: return "double";
            case types::PrimitiveKind::Bool: return "i1";
            case types::PrimitiveKind::Char: return "i32";
            case types::PrimitiveKind::Str: return "ptr";
            case types::PrimitiveKind::Unit: return "void";
        }
    } else if (type->is<types::NamedType>()) {
        return "%struct." + type->as<types::NamedType>().name;
    } else if (type->is<types::RefType>() || type->is<types::PtrType>()) {
        return "ptr";
    } else if (type->is<types::FuncType>()) {
        // Function types are pointers in LLVM
        return "ptr";
    }

    return "i32";  // Default
}

auto LLVMIRGen::add_string_literal(const std::string& value) -> std::string {
    std::string name = "@.str." + std::to_string(string_literals_.size());
    string_literals_.emplace_back(name, value);
    return name;
}

void LLVMIRGen::emit_header() {
    emit_line("; Generated by TML Compiler");
    emit_line("target triple = \"" + options_.target_triple + "\"");
    emit_line("");
}

void LLVMIRGen::emit_runtime_decls() {
    // String type: { ptr, i64 } (pointer to data, length)
    emit_line("; Runtime type declarations");
    emit_line("%struct.tml_str = type { ptr, i64 }");
    emit_line("");

    // External C functions
    emit_line("; External function declarations");
    emit_line("declare i32 @printf(ptr, ...)");
    emit_line("declare i32 @puts(ptr)");
    emit_line("declare i32 @putchar(i32)");
    emit_line("declare ptr @malloc(i64)");
    emit_line("declare void @free(ptr)");
    emit_line("declare void @exit(i32) noreturn");
    emit_line("");

    // TML runtime functions
    emit_line("; TML runtime functions");
    emit_line("declare void @tml_panic(ptr) noreturn");
    emit_line("");

    // TML test assertion functions
    emit_line("; TML test assertions");
    emit_line("declare void @tml_assert(i1, ptr)");
    emit_line("declare void @tml_assert_eq_i32(i32, i32, ptr)");
    emit_line("declare void @tml_assert_ne_i32(i32, i32, ptr)");
    emit_line("declare void @tml_assert_eq_str(ptr, ptr, ptr)");
    emit_line("declare void @tml_assert_eq_bool(i1, i1, ptr)");
    emit_line("");

    // Threading runtime declarations
    emit_line("; Threading runtime (tml_runtime.c)");
    emit_line("declare ptr @tml_thread_spawn(ptr, ptr)");
    emit_line("declare void @tml_thread_join(ptr)");
    emit_line("declare void @tml_thread_yield()");
    emit_line("declare void @tml_thread_sleep(i32)");
    emit_line("declare i32 @tml_thread_id()");
    emit_line("");

    // Time functions (for benchmarking)
    emit_line("; Time functions");
    emit_line("declare i32 @tml_time_ms()");
    emit_line("declare i64 @tml_time_us()");
    emit_line("declare i64 @tml_time_ns()");
    emit_line("declare ptr @tml_elapsed_secs(i32)");
    emit_line("declare i32 @tml_elapsed_ms(i32)");
    emit_line("; Instant API (like Rust)");
    emit_line("declare i64 @tml_instant_now()");
    emit_line("declare i64 @tml_instant_elapsed(i64)");
    emit_line("declare double @tml_duration_as_secs_f64(i64)");
    emit_line("declare double @tml_duration_as_millis_f64(i64)");
    emit_line("declare i64 @tml_duration_as_millis(i64)");
    emit_line("declare ptr @tml_duration_format_ms(i64)");
    emit_line("declare ptr @tml_duration_format_secs(i64)");
    emit_line("; Black box (prevent optimization)");
    emit_line("declare i32 @tml_black_box_i32(i32)");
    emit_line("declare i64 @tml_black_box_i64(i64)");
    emit_line("; SIMD operations (auto-vectorized)");
    emit_line("declare i64 @tml_simd_sum_i32(ptr, i64)");
    emit_line("declare i64 @tml_simd_sum_i64(ptr, i64)");
    emit_line("declare double @tml_simd_sum_f64(ptr, i64)");
    emit_line("declare double @tml_simd_dot_f64(ptr, ptr, i64)");
    emit_line("declare void @tml_simd_fill_i32(ptr, i32, i64)");
    emit_line("declare void @tml_simd_add_i32(ptr, ptr, ptr, i64)");
    emit_line("declare void @tml_simd_mul_i32(ptr, ptr, ptr, i64)");
    emit_line("");

    // Float functions
    emit_line("; Float functions");
    emit_line("declare ptr @tml_float_to_fixed(double, i32)");
    emit_line("declare ptr @tml_float_to_precision(double, i32)");
    emit_line("declare ptr @tml_float_to_string(double)");
    emit_line("declare double @tml_int_to_float(i32)");
    emit_line("declare double @tml_i64_to_float(i64)");
    emit_line("declare i32 @tml_float_to_int(double)");
    emit_line("declare i64 @tml_float_to_i64(double)");
    emit_line("declare i32 @tml_float_round(double)");
    emit_line("declare i32 @tml_float_floor(double)");
    emit_line("declare i32 @tml_float_ceil(double)");
    emit_line("declare double @tml_float_abs(double)");
    emit_line("declare double @tml_float_sqrt(double)");
    emit_line("declare double @tml_float_pow(double, i32)");
    emit_line("");

    // Channel runtime declarations
    emit_line("; Channel runtime (Go-style)");
    emit_line("declare ptr @tml_channel_create()");
    emit_line("declare i32 @tml_channel_send(ptr, i32)");
    emit_line("declare i32 @tml_channel_recv(ptr, ptr)");
    emit_line("declare i32 @tml_channel_try_send(ptr, i32)");
    emit_line("declare i32 @tml_channel_try_recv(ptr, ptr)");
    emit_line("declare void @tml_channel_close(ptr)");
    emit_line("declare void @tml_channel_destroy(ptr)");
    emit_line("declare i32 @tml_channel_len(ptr)");
    emit_line("");

    // Mutex runtime declarations
    emit_line("; Mutex runtime");
    emit_line("declare ptr @tml_mutex_create()");
    emit_line("declare void @tml_mutex_lock(ptr)");
    emit_line("declare void @tml_mutex_unlock(ptr)");
    emit_line("declare i32 @tml_mutex_try_lock(ptr)");
    emit_line("declare void @tml_mutex_destroy(ptr)");
    emit_line("");

    // WaitGroup runtime declarations
    emit_line("; WaitGroup runtime (Go-style)");
    emit_line("declare ptr @tml_waitgroup_create()");
    emit_line("declare void @tml_waitgroup_add(ptr, i32)");
    emit_line("declare void @tml_waitgroup_done(ptr)");
    emit_line("declare void @tml_waitgroup_wait(ptr)");
    emit_line("declare void @tml_waitgroup_destroy(ptr)");
    emit_line("");

    // Atomic counter runtime declarations
    emit_line("; Atomic counter runtime");
    emit_line("declare ptr @tml_atomic_counter_create(i32)");
    emit_line("declare i32 @tml_atomic_counter_inc(ptr)");
    emit_line("declare i32 @tml_atomic_counter_dec(ptr)");
    emit_line("declare i32 @tml_atomic_counter_get(ptr)");
    emit_line("declare void @tml_atomic_counter_set(ptr, i32)");
    emit_line("declare void @tml_atomic_counter_destroy(ptr)");
    emit_line("");

    // List runtime declarations
    emit_line("; List (dynamic array) runtime");
    emit_line("declare ptr @tml_list_create(i32)");
    emit_line("declare void @tml_list_destroy(ptr)");
    emit_line("declare void @tml_list_push(ptr, i32)");
    emit_line("declare i32 @tml_list_pop(ptr)");
    emit_line("declare i32 @tml_list_get(ptr, i32)");
    emit_line("declare void @tml_list_set(ptr, i32, i32)");
    emit_line("declare i32 @tml_list_len(ptr)");
    emit_line("declare i32 @tml_list_capacity(ptr)");
    emit_line("declare void @tml_list_clear(ptr)");
    emit_line("declare i32 @tml_list_is_empty(ptr)");
    emit_line("");

    // HashMap runtime declarations
    emit_line("; HashMap runtime");
    emit_line("declare ptr @tml_hashmap_create()");
    emit_line("declare void @tml_hashmap_destroy(ptr)");
    emit_line("declare void @tml_hashmap_set(ptr, i32, i32)");
    emit_line("declare i32 @tml_hashmap_get(ptr, i32, ptr)");
    emit_line("declare i32 @tml_hashmap_has(ptr, i32)");
    emit_line("declare i32 @tml_hashmap_remove(ptr, i32)");
    emit_line("declare i32 @tml_hashmap_len(ptr)");
    emit_line("declare void @tml_hashmap_clear(ptr)");
    emit_line("");

    // Buffer runtime declarations
    emit_line("; Buffer runtime");
    emit_line("declare ptr @tml_buffer_create(i32)");
    emit_line("declare void @tml_buffer_destroy(ptr)");
    emit_line("declare void @tml_buffer_write_byte(ptr, i32)");
    emit_line("declare void @tml_buffer_write_i32(ptr, i32)");
    emit_line("declare i32 @tml_buffer_read_byte(ptr)");
    emit_line("declare i32 @tml_buffer_read_i32(ptr)");
    emit_line("declare i32 @tml_buffer_len(ptr)");
    emit_line("declare i32 @tml_buffer_capacity(ptr)");
    emit_line("declare i32 @tml_buffer_remaining(ptr)");
    emit_line("declare void @tml_buffer_clear(ptr)");
    emit_line("declare void @tml_buffer_reset_read(ptr)");
    emit_line("");

    // String utilities
    emit_line("; String utilities");
    emit_line("declare i32 @tml_str_len(ptr)");
    emit_line("declare i32 @tml_str_hash(ptr)");
    emit_line("declare i32 @tml_str_eq(ptr, ptr)");
    emit_line("");

    // Format strings for print/println
    // Size calculation: count actual bytes (each escape like \0A = 1 byte, not 3)
    emit_line("; Format strings");
    emit_line("@.fmt.int = private constant [4 x i8] c\"%d\\0A\\00\"");           // %d\n\0 = 4 bytes
    emit_line("@.fmt.int.no_nl = private constant [3 x i8] c\"%d\\00\"");         // %d\0 = 3 bytes
    emit_line("@.fmt.i64 = private constant [5 x i8] c\"%ld\\0A\\00\"");          // %ld\n\0 = 5 bytes
    emit_line("@.fmt.i64.no_nl = private constant [4 x i8] c\"%ld\\00\"");        // %ld\0 = 4 bytes
    emit_line("@.fmt.float = private constant [4 x i8] c\"%f\\0A\\00\"");         // %f\n\0 = 4 bytes
    emit_line("@.fmt.float.no_nl = private constant [3 x i8] c\"%f\\00\"");       // %f\0 = 3 bytes
    emit_line("@.fmt.float3 = private constant [6 x i8] c\"%.3f\\0A\\00\"");      // %.3f\n\0 = 6 bytes
    emit_line("@.fmt.float3.no_nl = private constant [5 x i8] c\"%.3f\\00\"");    // %.3f\0 = 5 bytes
    emit_line("@.fmt.str.no_nl = private constant [3 x i8] c\"%s\\00\"");         // %s\0 = 3 bytes
    emit_line("@.str.true = private constant [5 x i8] c\"true\\00\"");            // true\0 = 5 bytes
    emit_line("@.str.false = private constant [6 x i8] c\"false\\00\"");          // false\0 = 6 bytes
    emit_line("@.str.space = private constant [2 x i8] c\" \\00\"");              // " "\0 = 2 bytes
    emit_line("@.str.newline = private constant [2 x i8] c\"\\0A\\00\"");         // \n\0 = 2 bytes
    emit_line("");
}

void LLVMIRGen::emit_string_constants() {
    if (string_literals_.empty()) return;

    emit_line("; String constants");
    for (const auto& [name, value] : string_literals_) {
        // Escape the string and add null terminator
        std::string escaped;
        for (char c : value) {
            if (c == '\n') escaped += "\\0A";
            else if (c == '\t') escaped += "\\09";
            else if (c == '\\') escaped += "\\5C";
            else if (c == '"') escaped += "\\22";
            else escaped += c;
        }
        escaped += "\\00";

        emit_line(name + " = private constant [" +
                  std::to_string(value.size() + 1) + " x i8] c\"" + escaped + "\"");
    }
    emit_line("");
}

auto LLVMIRGen::generate(const parser::Module& module) -> Result<std::string, std::vector<LLVMGenError>> {
    errors_.clear();
    output_.str("");
    string_literals_.clear();
    temp_counter_ = 0;
    label_counter_ = 0;

    emit_header();
    emit_runtime_decls();

    // First pass: collect const declarations and struct/enum declarations
    for (const auto& decl : module.decls) {
        if (decl->is<parser::ConstDecl>()) {
            const auto& const_decl = decl->as<parser::ConstDecl>();
            // For now, only support literal constants
            if (const_decl.value->is<parser::LiteralExpr>()) {
                const auto& lit = const_decl.value->as<parser::LiteralExpr>();
                std::string value;
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    value = std::to_string(lit.token.int_value().value);
                } else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
                    value = (lit.token.lexeme == "true") ? "1" : "0";
                }
                global_constants_[const_decl.name] = value;
            }
        } else if (decl->is<parser::StructDecl>()) {
            gen_struct_decl(decl->as<parser::StructDecl>());
        } else if (decl->is<parser::EnumDecl>()) {
            gen_enum_decl(decl->as<parser::EnumDecl>());
        }
    }

    // Second pass: generate function declarations
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            gen_func_decl(decl->as<parser::FuncDecl>());
        }
    }

    // Emit generated closure functions
    for (const auto& closure_func : module_functions_) {
        emit(closure_func);
    }

    // Emit string constants at the end (they were collected during codegen)
    emit_string_constants();

    // Collect test and benchmark functions (decorated with @test and @bench)
    std::vector<std::string> test_functions;
    std::vector<std::string> bench_functions;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            for (const auto& decorator : func.decorators) {
                if (decorator.name == "test") {
                    test_functions.push_back(func.name);
                    break;
                } else if (decorator.name == "bench") {
                    bench_functions.push_back(func.name);
                    break;
                }
            }
        }
    }

    // Generate main entry point
    bool has_user_main = false;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>() && decl->as<parser::FuncDecl>().name == "main") {
            has_user_main = true;
            break;
        }
    }

    if (!bench_functions.empty()) {
        // Generate benchmark runner main
        emit_line("; Auto-generated benchmark runner");
        emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        emit_line("entry:");

        int bench_num = 0;
        std::string prev_block = "entry";
        for (const auto& bench_name : bench_functions) {
            std::string bench_fn = "@tml_" + bench_name;

            // Get start time
            std::string start_time = "%bench_start_" + std::to_string(bench_num);
            emit_line("  " + start_time + " = call i64 @tml_time_us()");

            // Run benchmark 1000 iterations
            std::string iter_var = "%bench_iter_" + std::to_string(bench_num);
            std::string loop_header = "bench_loop_header_" + std::to_string(bench_num);
            std::string loop_body = "bench_loop_body_" + std::to_string(bench_num);
            std::string loop_end = "bench_loop_end_" + std::to_string(bench_num);

            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_header + ":");
            emit_line("  " + iter_var + " = phi i32 [ 0, %" + prev_block + " ], [ " + iter_var + "_next, %" + loop_body + " ]");
            std::string cmp_var = "%bench_cmp_" + std::to_string(bench_num);
            emit_line("  " + cmp_var + " = icmp slt i32 " + iter_var + ", 1000");
            emit_line("  br i1 " + cmp_var + ", label %" + loop_body + ", label %" + loop_end);
            emit_line("");
            emit_line(loop_body + ":");
            emit_line("  call void " + bench_fn + "()");
            emit_line("  " + iter_var + "_next = add i32 " + iter_var + ", 1");
            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_end + ":");

            // Get end time and calculate duration
            std::string end_time = "%bench_end_" + std::to_string(bench_num);
            std::string duration = "%bench_duration_" + std::to_string(bench_num);
            emit_line("  " + end_time + " = call i64 @tml_time_us()");
            emit_line("  " + duration + " = sub i64 " + end_time + ", " + start_time);

            // Calculate average (duration / 1000)
            std::string avg_time = "%bench_avg_" + std::to_string(bench_num);
            emit_line("  " + avg_time + " = sdiv i64 " + duration + ", 1000");
            emit_line("");

            prev_block = loop_end;
            bench_num++;
        }

        emit_line("  ret i32 0");
        emit_line("}");
    } else if (!test_functions.empty()) {
        // Generate test runner main
        emit_line("; Auto-generated test runner");
        emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        emit_line("entry:");

        int test_num = 0;
        for (const auto& test_name : test_functions) {
            std::string result_var = "%test_result_" + std::to_string(test_num);
            std::string test_fn = "@tml_" + test_name;

            // Call test function
            emit_line("  " + result_var + " = call i32 " + test_fn + "()");

            // Check if test failed (non-zero return)
            std::string cmp_var = "%test_cmp_" + std::to_string(test_num);
            emit_line("  " + cmp_var + " = icmp ne i32 " + result_var + ", 0");

            // If failed, return the error code
            std::string fail_label = "test_fail_" + std::to_string(test_num);
            std::string next_label = "test_next_" + std::to_string(test_num);
            emit_line("  br i1 " + cmp_var + ", label %" + fail_label + ", label %" + next_label);

            // Failure path
            emit_line(fail_label + ":");
            emit_line("  ret i32 " + result_var);

            // Success path - continue to next test
            emit_line(next_label + ":");

            test_num++;
        }

        // All tests passed
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (has_user_main) {
        // Standard main wrapper for user-defined main
        emit_line("; Entry point");
        emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        emit_line("entry:");
        emit_line("  %ret = call i32 @tml_main()");
        emit_line("  ret i32 %ret");
        emit_line("}");
    }

    // Emit function attributes for optimization
    emit_line("");
    emit_line("; Function attributes for optimization");
    emit_line("attributes #0 = { nounwind mustprogress willreturn }");

    if (!errors_.empty()) {
        return errors_;
    }

    return output_.str();
}

// Infer print argument type from expression
auto LLVMIRGen::infer_print_type(const parser::Expr& expr) -> PrintArgType {
    if (expr.is<parser::LiteralExpr>()) {
        const auto& lit = expr.as<parser::LiteralExpr>();
        switch (lit.token.kind) {
            case lexer::TokenKind::IntLiteral: return PrintArgType::Int;
            case lexer::TokenKind::FloatLiteral: return PrintArgType::Float;
            case lexer::TokenKind::BoolLiteral: return PrintArgType::Bool;
            case lexer::TokenKind::StringLiteral: return PrintArgType::Str;
            default: return PrintArgType::Unknown;
        }
    }
    if (expr.is<parser::BinaryExpr>()) {
        const auto& bin = expr.as<parser::BinaryExpr>();
        switch (bin.op) {
            case parser::BinaryOp::Add:
            case parser::BinaryOp::Sub:
            case parser::BinaryOp::Mul:
            case parser::BinaryOp::Div:
            case parser::BinaryOp::Mod:
                // Check if operands are float
                if (infer_print_type(*bin.left) == PrintArgType::Float ||
                    infer_print_type(*bin.right) == PrintArgType::Float) {
                    return PrintArgType::Float;
                }
                return PrintArgType::Int;
            case parser::BinaryOp::Eq:
            case parser::BinaryOp::Ne:
            case parser::BinaryOp::Lt:
            case parser::BinaryOp::Gt:
            case parser::BinaryOp::Le:
            case parser::BinaryOp::Ge:
            case parser::BinaryOp::And:
            case parser::BinaryOp::Or:
                return PrintArgType::Bool;
            default:
                return PrintArgType::Int;
        }
    }
    if (expr.is<parser::UnaryExpr>()) {
        const auto& un = expr.as<parser::UnaryExpr>();
        if (un.op == parser::UnaryOp::Not) return PrintArgType::Bool;
        if (un.op == parser::UnaryOp::Neg) {
            // Check if operand is float
            if (infer_print_type(*un.operand) == PrintArgType::Float) {
                return PrintArgType::Float;
            }
            return PrintArgType::Int;
        }
    }
    if (expr.is<parser::IdentExpr>()) {
        // For identifiers, we need to check the variable type
        // For now, default to Unknown (will be checked by caller)
        return PrintArgType::Unknown;
    }
    if (expr.is<parser::CallExpr>()) {
        const auto& call = expr.as<parser::CallExpr>();
        // Check for known I64-returning functions
        if (call.callee->is<parser::IdentExpr>()) {
            const auto& fn_name = call.callee->as<parser::IdentExpr>().name;
            if (fn_name == "time_us" || fn_name == "time_ns") {
                return PrintArgType::I64;
            }
        }
        return PrintArgType::Int; // Assume functions return int
    }
    return PrintArgType::Unknown;
}

} // namespace tml::codegen
