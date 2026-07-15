TML_MODULE("codegen_x86")

//! # LLVM IR Generator - I/O Builtins
//!
//! This file implements builtin I/O function code generation.
//!
//! ## Functions
//!
//! | Function  | Generated Code                      |
//! |-----------|-------------------------------------|
//! | `print`   | `printf` with format specifier      |
//! | `println` | `printf` with newline               |
//! | `panic`   | `@panic(ptr)` then unreachable      |
//!
//! ## Print Type Detection
//!
//! Print infers the format specifier from argument type:
//! - I32 → `%d`
//! - I64 → `%lld`
//! - F64 → `%f`
//! - Bool → `true`/`false`
//! - Str → `%s`

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_io(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // Handle builtin print/println - unified for all types
    if (fn_name == "print" || fn_name == "println") {
        bool with_newline = (fn_name == "println");

        if (call.args.empty()) {
            if (with_newline) {
                // Use runtime println(null) for just newline - respects suppression
                emit_line("  call void @println(ptr null)");
                last_expr_type_ = "void";
                return "0";
            }
            last_expr_type_ = "void";
            return "0";
        }

        // Check if first arg is a format string with {} placeholders
        bool is_format_string = false;
        std::string format_str;
        if (call.args[0]->is<parser::LiteralExpr>()) {
            const auto& lit = call.args[0]->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                format_str = std::string(lit.token.string_value().value);
                if (format_str.find("{}") != std::string::npos && call.args.size() > 1) {
                    is_format_string = true;
                }
            }
        }

        if (is_format_string) {
            // Handle format string: "text {} more {}" with args
            return gen_format_print(format_str, call.args, 1, with_newline);
        }

        // Single value print - auto-detect type
        const auto& arg_expr = *call.args[0];
        std::string arg_val = gen_expr(arg_expr);
        std::string gen_type = last_expr_type_; // Capture type from gen_expr

        // Try to infer type from expression
        auto arg_type = infer_print_type(arg_expr);

        // For identifiers, check if it's a known variable with type info
        if (arg_type == PrintArgType::Unknown && arg_expr.is<parser::IdentExpr>()) {
            const auto& ident = arg_expr.as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                if (it->second.type == "i1")
                    arg_type = PrintArgType::Bool;
                else if (it->second.type == "i32")
                    arg_type = PrintArgType::Int;
                else if (it->second.type == "i64")
                    arg_type = PrintArgType::I64;
                else if (it->second.type == "float" || it->second.type == "double")
                    arg_type = PrintArgType::Float;
                else if (it->second.type == "ptr")
                    arg_type = PrintArgType::Str;
            }
        }

        // For string constants (@.str.X), treat as string
        if (arg_val.starts_with("@.str.")) {
            arg_type = PrintArgType::Str;
        }

        // Use gen_type as fallback for type inference from gen_expr result
        if (arg_type == PrintArgType::Unknown || arg_type == PrintArgType::Int) {
            if (gen_type == "i64")
                arg_type = PrintArgType::I64;
            else if (gen_type == "ptr")
                arg_type = PrintArgType::Str;
            else if (gen_type == "i1")
                arg_type = PrintArgType::Bool;
            else if (gen_type == "float" || gen_type == "double")
                arg_type = PrintArgType::Float;
            else if (gen_type == "i32")
                arg_type = PrintArgType::Int;
            else if (gen_type == "%struct.Text") {
                // SSO Text { _w0: i64, _w1: i64, _w2: i64 }
                // _w2 < 0 (inline): bytes 0-22 = data, byte 23 = 0x80|len
                // _w2 >= 0 (heap): _w0 = data_ptr (as i64)
                std::string w0 = fresh_reg();
                std::string w2 = fresh_reg();
                emit_line("  " + w0 + " = extractvalue %struct.Text " + arg_val + ", 0");
                emit_line("  " + w2 + " = extractvalue %struct.Text " + arg_val + ", 2");

                std::string is_inline = fresh_reg();
                emit_line("  " + is_inline + " = icmp slt i64 " + w2 + ", 0");

                std::string lbl_heap = fresh_label("text.heap");
                std::string lbl_inline = fresh_label("text.inline");
                std::string lbl_done = fresh_label("text.done");
                emit_line("  br i1 " + is_inline + ", label %" + lbl_inline +
                          ", label %" + lbl_heap);

                // Heap path: _w0 is the data pointer (as i64)
                emit_line(lbl_heap + ":");
                std::string heap_ptr = fresh_reg();
                emit_line("  " + heap_ptr + " = inttoptr i64 " + w0 + " to ptr");
                emit_line("  br label %" + lbl_done);

                // Inline path: store struct to stack, null-terminate, use as Str
                emit_line(lbl_inline + ":");
                std::string buf = fresh_reg();
                emit_line("  " + buf + " = alloca %struct.Text");
                emit_line("  store %struct.Text " + arg_val + ", ptr " + buf);
                // length = (w2 >> 56) & 0x7F
                std::string len_raw = fresh_reg();
                emit_line("  " + len_raw + " = ashr i64 " + w2 + ", 56");
                std::string len_val = fresh_reg();
                emit_line("  " + len_val + " = and i64 " + len_raw + ", 127");
                // Null-terminate at buf + len
                std::string end_ptr = fresh_reg();
                emit_line("  " + end_ptr + " = getelementptr i8, ptr " + buf +
                          ", i64 " + len_val);
                emit_line("  store i8 0, ptr " + end_ptr);
                emit_line("  br label %" + lbl_done);

                // Merge
                emit_line(lbl_done + ":");
                std::string data_ptr = fresh_reg();
                emit_line("  " + data_ptr + " = phi ptr [ " + heap_ptr + ", %" +
                          lbl_heap + " ], [ " + buf + ", %" + lbl_inline + " ]");

                arg_val = data_ptr;
                arg_type = PrintArgType::Str;
            }
        }

        // Use runtime print functions that respect output suppression flag
        switch (arg_type) {
        case PrintArgType::Str: {
            if (with_newline) {
                // Use runtime println() which checks suppression flag
                emit_line("  call void @println(ptr " + arg_val + ")");
            } else {
                // Use runtime print() which checks suppression flag
                emit_line("  call void @print(ptr " + arg_val + ")");
            }
            break;
        }
        case PrintArgType::Bool: {
            // Use runtime print_bool() which checks suppression flag
            std::string bool_val = fresh_reg();
            emit_line("  " + bool_val + " = zext i1 " + arg_val + " to i32");
            emit_line("  call void @print_bool(i32 " + bool_val + ")");
            if (with_newline) {
                // Use runtime println(null) for newline with suppression check
                emit_line("  call void @println(ptr null)");
            }
            break;
        }
        case PrintArgType::I64: {
            // Use runtime print_i64() which checks suppression flag
            emit_line("  call void @print_i64(i64 " + arg_val + ")");
            if (with_newline) {
                emit_line("  call void @println(ptr null)");
            }
            break;
        }
        case PrintArgType::Float: {
            // For printf, floats are promoted to double - convert if needed
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = fpext float " + arg_val + " to double");
            // Use runtime print_f64() which checks suppression flag
            emit_line("  call void @print_f64(double " + double_val + ")");
            if (with_newline) {
                emit_line("  call void @println(ptr null)");
            }
            break;
        }
        case PrintArgType::Int:
        case PrintArgType::Unknown:
        default: {
            // Use runtime print_i32() which checks suppression flag
            emit_line("  call void @print_i32(i32 " + arg_val + ")");
            if (with_newline) {
                emit_line("  call void @println(ptr null)");
            }
            break;
        }
        }
        // Print functions return void/Unit in TML - return dummy 0
        last_expr_type_ = "void";
        return "0";
    }

    // panic(msg: Str) -> Never
    // Prints error message to stderr and exits
    if (fn_name == "panic") {
        if (!call.args.empty()) {
            std::string msg = gen_expr(*call.args[0]);
            emit_line("  call void @panic(ptr " + msg + ")");
            emit_line("  unreachable");
            block_terminated_ = true;
            return "0";
        }
        return "0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
