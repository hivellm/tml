// LLVM IR generator - Format print
// Handles: gen_format_print for formatted string output

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

    while (pos < format.size()) {
        // Find next { placeholder
        size_t placeholder = format.find('{', pos);

        if (placeholder == std::string::npos) {
            // No more placeholders, print remaining text
            if (pos < format.size()) {
                std::string remaining = format.substr(pos);
                std::string str_const = add_string_literal(remaining);
                result = fresh_reg();
                emit_line("  " + result +
                          " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr " + str_const +
                          ")");
            }
            break;
        }

        // Print text before placeholder
        if (placeholder > pos) {
            std::string segment = format.substr(pos, placeholder - pos);
            std::string str_const = add_string_literal(segment);
            result = fresh_reg();
            emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr " +
                      str_const + ")");
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

        // Print argument
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

            result = fresh_reg();
            switch (arg_type) {
            case PrintArgType::Str:
                emit_line("  " + result +
                          " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr " + arg_val +
                          ")");
                break;
            case PrintArgType::Bool: {
                std::string label_true = fresh_label("fmt.true");
                std::string label_false = fresh_label("fmt.false");
                std::string label_end = fresh_label("fmt.end");

                emit_line("  br i1 " + arg_val + ", label %" + label_true + ", label %" +
                          label_false);

                emit_line(label_true + ":");
                std::string r1 = fresh_reg();
                emit_line("  " + r1 +
                          " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr @.str.true)");
                emit_line("  br label %" + label_end);

                emit_line(label_false + ":");
                std::string r2 = fresh_reg();
                emit_line("  " + r2 +
                          " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr @.str.false)");
                emit_line("  br label %" + label_end);

                emit_line(label_end + ":");
                block_terminated_ = false;
                break;
            }
            case PrintArgType::I64:
                emit_line("  " + result +
                          " = call i32 (ptr, ...) @printf(ptr @.fmt.i64.no_nl, i64 " + arg_val +
                          ")");
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
                    // Create custom format string for precision
                    std::string fmt_str = "%." + std::to_string(precision) + "f";
                    std::string fmt_const = add_string_literal(fmt_str);
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr " + fmt_const +
                              ", double " + double_val + ")");
                } else {
                    emit_line("  " + result +
                              " = call i32 (ptr, ...) @printf(ptr @.fmt.float.no_nl, double " +
                              double_val + ")");
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
                    std::string fmt_str = "%." + std::to_string(precision) + "f";
                    std::string fmt_const = add_string_literal(fmt_str);
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr " + fmt_const +
                              ", double " + double_val + ")");
                } else {
                    emit_line("  " + result +
                              " = call i32 (ptr, ...) @printf(ptr @.fmt.int.no_nl, i32 " + arg_val +
                              ")");
                }
                break;
            }
            ++arg_idx;
        }

        pos = end_brace + 1; // Skip past }
    }

    // Print newline if println
    if (with_newline) {
        result = fresh_reg();
        emit_line("  " + result + " = call i32 @putchar(i32 10)");
    }

    return result;
}

} // namespace tml::codegen
