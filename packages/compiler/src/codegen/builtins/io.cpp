// LLVM IR generator - IO builtin functions
// Handles: print, println, panic

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_io(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // Handle builtin print/println - unified for all types
    if (fn_name == "print" || fn_name == "println") {
        bool with_newline = (fn_name == "println");

        if (call.args.empty()) {
            if (with_newline) {
                std::string result = fresh_reg();
                emit_line("  " + result + " = call i32 @putchar(i32 10)");
                last_expr_type_ = "i32";
                return result;
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

        // Try to infer type from expression
        auto arg_type = infer_print_type(arg_expr);

        // For identifiers, check if it's a known variable with type info
        if (arg_type == PrintArgType::Unknown && arg_expr.is<parser::IdentExpr>()) {
            const auto& ident = arg_expr.as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                if (it->second.type == "i1") arg_type = PrintArgType::Bool;
                else if (it->second.type == "i32") arg_type = PrintArgType::Int;
                else if (it->second.type == "i64") arg_type = PrintArgType::I64;
                else if (it->second.type == "float" || it->second.type == "double") arg_type = PrintArgType::Float;
                else if (it->second.type == "ptr") arg_type = PrintArgType::Str;
            }
        }

        // For string constants (@.str.X), treat as string
        if (arg_val.starts_with("@.str.")) {
            arg_type = PrintArgType::Str;
        }

        std::string result = fresh_reg();

        switch (arg_type) {
            case PrintArgType::Str: {
                if (with_newline) {
                    emit_line("  " + result + " = call i32 @puts(ptr " + arg_val + ")");
                } else {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr " + arg_val + ")");
                }
                break;
            }
            case PrintArgType::Bool: {
                std::string label_true = fresh_label("print.true");
                std::string label_false = fresh_label("print.false");
                std::string label_end = fresh_label("print.end");

                emit_line("  br i1 " + arg_val + ", label %" + label_true + ", label %" + label_false);

                emit_line(label_true + ":");
                std::string r1 = fresh_reg();
                if (with_newline) {
                    emit_line("  " + r1 + " = call i32 @puts(ptr @.str.true)");
                } else {
                    emit_line("  " + r1 + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr @.str.true)");
                }
                emit_line("  br label %" + label_end);

                emit_line(label_false + ":");
                std::string r2 = fresh_reg();
                if (with_newline) {
                    emit_line("  " + r2 + " = call i32 @puts(ptr @.str.false)");
                } else {
                    emit_line("  " + r2 + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr @.str.false)");
                }
                emit_line("  br label %" + label_end);

                emit_line(label_end + ":");
                block_terminated_ = false;
                last_expr_type_ = "i32";  // Bool print returns i32
                return "0";
            }
            case PrintArgType::I64: {
                if (with_newline) {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.i64, i64 " + arg_val + ")");
                } else {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.i64.no_nl, i64 " + arg_val + ")");
                }
                break;
            }
            case PrintArgType::Float: {
                // For printf, floats are promoted to double - convert if needed
                std::string double_val = fresh_reg();
                emit_line("  " + double_val + " = fpext float " + arg_val + " to double");
                if (with_newline) {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.float, double " + double_val + ")");
                } else {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.float.no_nl, double " + double_val + ")");
                }
                break;
            }
            case PrintArgType::Int:
            case PrintArgType::Unknown:
            default: {
                if (with_newline) {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.int, i32 " + arg_val + ")");
                } else {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.int.no_nl, i32 " + arg_val + ")");
                }
                break;
            }
        }
        last_expr_type_ = "i32";  // print/println return i32
        return result;
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
