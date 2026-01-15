//! # LLVM IR Generator - Formatted Print
//!
//! This file implements format string printing.
//!
//! ## Format Syntax
//!
//! | Format   | Description                    |
//! |----------|--------------------------------|
//! | `{}`     | Default format for value       |
//! | `{:.N}`  | Float with N decimal places    |
//!
//! ## Example
//!
//! ```tml
//! println("x = {}, y = {:.2}", x, y)
//! ```
//!
//! Generates printf calls with appropriate format specifiers.

#include "codegen/llvm_ir_gen.hpp"

#include <algorithm>
#include <cctype>

namespace tml::codegen {

// Generate formatted print: "hello {} world {}" with args
// Supports {}, {:.N} for floats with N decimal places
auto LLVMIRGen::gen_format_print(const std::string& format,
                                 const std::vector<parser::ExprPtr>& args, size_t start_idx,
                                 bool with_newline) -> std::string {
    // Parse format string and print segments with arguments
    size_t arg_idx = start_idx;
    size_t pos = 0;
    std::string result = "0";

    // All print calls use runtime functions that respect output suppression flag
    while (pos < format.size()) {
        // Find next { placeholder
        size_t placeholder = format.find('{', pos);

        if (placeholder == std::string::npos) {
            // No more placeholders, print remaining text
            if (pos < format.size()) {
                std::string remaining = format.substr(pos);
                std::string str_const = add_string_literal(remaining);
                // Use runtime print() which checks suppression flag
                emit_line("  call void @print(ptr " + str_const + ")");
            }
            break;
        }

        // Print text before placeholder
        if (placeholder > pos) {
            std::string segment = format.substr(pos, placeholder - pos);
            std::string str_const = add_string_literal(segment);
            // Use runtime print() which checks suppression flag
            emit_line("  call void @print(ptr " + str_const + ")");
        }

        // Parse placeholder: {} or {:.N}
        size_t end_brace = format.find('}', placeholder);
        if (end_brace == std::string::npos) {
            pos = placeholder + 1;
            continue;
        }

        std::string placeholder_content =
            format.substr(placeholder + 1, end_brace - placeholder - 1);
        int precision = -1; // -1 means no precision specified

        // Parse {:.N} format
        if (placeholder_content.starts_with(":.")) {
            std::string prec_str = placeholder_content.substr(2);
            if (!prec_str.empty() && std::all_of(prec_str.begin(), prec_str.end(), ::isdigit)) {
                precision = std::stoi(prec_str);
            }
        }

        // Print argument using runtime functions that check suppression
        if (arg_idx < args.size()) {
            const auto& arg_expr = *args[arg_idx];
            std::string arg_val = gen_expr(arg_expr);
            auto arg_type = infer_print_type(arg_expr);

            // For identifiers, check variable type
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

            // For string constants
            if (arg_val.starts_with("@.str.")) {
                arg_type = PrintArgType::Str;
            }

            switch (arg_type) {
            case PrintArgType::Str:
                // Use runtime print() which checks suppression flag
                emit_line("  call void @print(ptr " + arg_val + ")");
                break;
            case PrintArgType::Bool: {
                // Use runtime print_bool() which checks suppression flag
                std::string bool_val = fresh_reg();
                emit_line("  " + bool_val + " = zext i1 " + arg_val + " to i32");
                emit_line("  call void @print_bool(i32 " + bool_val + ")");
                break;
            }
            case PrintArgType::I64:
                // Use runtime print_i64() which checks suppression flag
                emit_line("  call void @print_i64(i64 " + arg_val + ")");
                break;
            case PrintArgType::Float: {
                // Check if already double (from variable type or last_expr_type_)
                bool is_double = (last_expr_type_ == "double");
                if (!is_double && arg_expr.is<parser::IdentExpr>()) {
                    const auto& ident = arg_expr.as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    if (it != locals_.end() && it->second.type == "double") {
                        is_double = true;
                    }
                }

                std::string double_val;
                if (is_double) {
                    // Already a double, no conversion needed
                    double_val = arg_val;
                } else {
                    // For printf, floats are promoted to double
                    double_val = fresh_reg();
                    emit_line("  " + double_val + " = fpext float " + arg_val + " to double");
                }
                if (precision >= 0) {
                    // Use float_to_precision then print() for formatted output
                    result = fresh_reg();
                    emit_line("  " + result + " = call ptr @float_to_precision(double " +
                              double_val + ", i32 " + std::to_string(precision) + ")");
                    emit_line("  call void @print(ptr " + result + ")");
                } else {
                    // Use runtime print_f64() which checks suppression flag
                    emit_line("  call void @print_f64(double " + double_val + ")");
                }
                break;
            }
            case PrintArgType::Int:
            case PrintArgType::Unknown:
            default:
                // If precision is specified for int, treat as float for fractional display
                if (precision >= 0) {
                    // Convert i32 to double for fractional display (e.g., us to ms)
                    std::string double_val = fresh_reg();
                    emit_line("  " + double_val + " = sitofp i32 " + arg_val + " to double");
                    // Use float_to_precision then print() for formatted output
                    result = fresh_reg();
                    emit_line("  " + result + " = call ptr @float_to_precision(double " +
                              double_val + ", i32 " + std::to_string(precision) + ")");
                    emit_line("  call void @print(ptr " + result + ")");
                } else {
                    // Use runtime print_i32() which checks suppression flag
                    emit_line("  call void @print_i32(i32 " + arg_val + ")");
                }
                break;
            }
            ++arg_idx;
        }

        pos = end_brace + 1; // Skip past }
    }

    // Print newline if println - use runtime function that checks suppression
    if (with_newline) {
        emit_line("  call void @println(ptr null)");
    }

    return result;
}

} // namespace tml::codegen
