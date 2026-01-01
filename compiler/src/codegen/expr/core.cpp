// LLVM IR generator - Core expression generation
// Handles: literals, identifiers, lowlevel blocks, interpolated strings

#include "codegen/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_literal(const parser::LiteralExpr& lit) -> std::string {
    switch (lit.token.kind) {
    case lexer::TokenKind::IntLiteral: {
        // Use the actual numeric value, not the lexeme (handles 0x, 0b, etc.)
        uint64_t val = lit.token.int_value().value;
        // Check if value fits in signed i32 range (-2^31 to 2^31-1)
        // For unsigned values, check if it fits in 0 to 2^31-1 (positive i32)
        constexpr uint64_t MAX_I32 = 2147483647ULL; // 2^31 - 1
        if (val > MAX_I32) {
            // Value too large for i32, use i64 directly
            last_expr_type_ = "i64";
        } else {
            last_expr_type_ = "i32";
        }
        return std::to_string(val);
    }
    case lexer::TokenKind::FloatLiteral:
        last_expr_type_ = "double";
        return std::to_string(lit.token.float_value().value);
    case lexer::TokenKind::BoolLiteral:
        last_expr_type_ = "i1";
        return lit.token.bool_value() ? "1" : "0";
    case lexer::TokenKind::StringLiteral: {
        std::string str_val = std::string(lit.token.string_value().value);
        std::string const_name = add_string_literal(str_val);
        last_expr_type_ = "ptr";
        return const_name;
    }
    case lexer::TokenKind::CharLiteral: {
        // Char literals are stored as i32 (Unicode code point)
        last_expr_type_ = "i32";
        return std::to_string(lit.token.char_value().value);
    }
    case lexer::TokenKind::NullLiteral:
        // null is a pointer type with value null
        last_expr_type_ = "ptr";
        return "null";
    default:
        last_expr_type_ = "i32";
        return "0";
    }
}

auto LLVMIRGen::gen_ident(const parser::IdentExpr& ident) -> std::string {
    // Check global constants first
    auto const_it = global_constants_.find(ident.name);
    if (const_it != global_constants_.end()) {
        last_expr_type_ = "i64"; // Constants are i64 for now
        return const_it->second;
    }

    auto it = locals_.find(ident.name);
    if (it != locals_.end()) {
        const VarInfo& var = it->second;
        last_expr_type_ = var.type;
        // Check if semantic type is unsigned
        last_expr_is_unsigned_ = false;
        if (var.semantic_type) {
            if (auto* prim = std::get_if<types::PrimitiveType>(&var.semantic_type->kind)) {
                last_expr_is_unsigned_ = prim->kind == types::PrimitiveKind::U8 ||
                                         prim->kind == types::PrimitiveKind::U16 ||
                                         prim->kind == types::PrimitiveKind::U32 ||
                                         prim->kind == types::PrimitiveKind::U64 ||
                                         prim->kind == types::PrimitiveKind::U128;
            }
        }
        // Check if it's an alloca (starts with %t and has digit after) that needs loading
        // But skip %this which is a direct parameter, not an alloca
        // This includes ptr types - we load the pointer value from the alloca
        if (var.reg[0] == '%' && var.reg[1] == 't' && var.reg.length() > 2 &&
            std::isdigit(var.reg[2])) {
            std::string reg = fresh_reg();
            emit_line("  " + reg + " = load " + var.type + ", ptr " + var.reg);
            return reg;
        }
        return var.reg;
    }

    // Check if it's a function reference (first-class function)
    auto func_it = functions_.find(ident.name);
    if (func_it != functions_.end()) {
        const FuncInfo& func = func_it->second;
        last_expr_type_ = "ptr"; // Function pointers are ptr type in LLVM
        return func.llvm_name;   // Return @tml_funcname
    }

    // Check if it's an enum unit variant (variant without payload)
    // First check pending generic enums (locally defined generic enums)
    for (const auto& [enum_name, enum_decl] : pending_generic_enums_) {
        for (size_t variant_idx = 0; variant_idx < enum_decl->variants.size(); ++variant_idx) {
            const auto& variant = enum_decl->variants[variant_idx];
            // Unit variant: no tuple_fields or struct_fields
            bool is_unit = (!variant.tuple_fields.has_value() || variant.tuple_fields->empty()) &&
                           (!variant.struct_fields.has_value() || variant.struct_fields->empty());

            if (variant.name == ident.name && is_unit) {
                // Found unit variant - create enum value with just the tag
                std::string enum_type;

                // Use expected_enum_type_ if available (set by caller like generic function call)
                if (!expected_enum_type_.empty()) {
                    enum_type = expected_enum_type_;
                }
                // Or try to infer from function return type
                else if (!current_ret_type_.empty()) {
                    std::string prefix = "%struct." + enum_name + "__";
                    if (current_ret_type_.starts_with(prefix) ||
                        current_ret_type_.find(enum_name + "__") != std::string::npos) {
                        enum_type = current_ret_type_;
                    }
                }

                // If we still don't have a type, we can't properly instantiate
                // Default to I32 as the type parameter
                if (enum_type.empty()) {
                    std::vector<types::TypePtr> default_args = {types::make_i32()};
                    std::string mangled = require_enum_instantiation(enum_name, default_args);
                    enum_type = "%struct." + mangled;
                }

                std::string result = fresh_reg();
                std::string enum_val = fresh_reg();

                // Create enum value on stack
                emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                // Set tag
                std::string tag_ptr = fresh_reg();
                emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " +
                          enum_val + ", i32 0, i32 0");
                emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " + tag_ptr);

                // No payload to set

                // Load the complete enum value
                emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                last_expr_type_ = enum_type;
                return result;
            }
        }
    }

    // Also check local enums (non-generic)
    for (const auto& [enum_name, enum_def] : env_.all_enums()) {
        for (size_t variant_idx = 0; variant_idx < enum_def.variants.size(); ++variant_idx) {
            const auto& [variant_name, payload_types] = enum_def.variants[variant_idx];

            if (variant_name == ident.name && payload_types.empty()) {
                // Found unit variant - create enum value with just the tag
                std::string enum_type = "%struct." + enum_name;

                // For generic enums, try to infer the correct mangled type from context
                // Use expected_enum_type_ first if available
                if (!expected_enum_type_.empty()) {
                    enum_type = expected_enum_type_;
                }
                // Or try to infer from function return type
                else if (!enum_def.type_params.empty() && !current_ret_type_.empty()) {
                    std::string prefix = "%struct." + enum_name + "__";
                    if (current_ret_type_.starts_with(prefix) ||
                        current_ret_type_.find(enum_name + "__") != std::string::npos) {
                        enum_type = current_ret_type_;
                    }
                }
                std::string result = fresh_reg();
                std::string enum_val = fresh_reg();

                // Create enum value on stack
                emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                // Set tag
                std::string tag_ptr = fresh_reg();
                emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " +
                          enum_val + ", i32 0, i32 0");
                emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " + tag_ptr);

                // No payload to set

                // Load the complete enum value
                emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                last_expr_type_ = enum_type;
                return result;
            }
        }
    }

    // Also check module enums for variants
    for (const auto& [mod_path, mod] : env_.get_all_modules()) {
        for (const auto& [enum_name, enum_def] : mod.enums) {
            for (size_t variant_idx = 0; variant_idx < enum_def.variants.size(); ++variant_idx) {
                const auto& [variant_name, payload_types] = enum_def.variants[variant_idx];

                if (variant_name == ident.name && payload_types.empty()) {
                    // Found unit variant in module - create enum value with just the tag
                    std::string enum_type = "%struct." + enum_name;

                    // For generic enums, use expected_enum_type_ first if available
                    if (!expected_enum_type_.empty()) {
                        enum_type = expected_enum_type_;
                    }
                    // Or try to infer from function return type
                    else if (!enum_def.type_params.empty() && !current_ret_type_.empty()) {
                        std::string prefix = "%struct." + enum_name + "__";
                        if (current_ret_type_.starts_with(prefix) ||
                            current_ret_type_.find(enum_name + "__") != std::string::npos) {
                            enum_type = current_ret_type_;
                        }
                    }
                    std::string result = fresh_reg();
                    std::string enum_val = fresh_reg();

                    // Create enum value on stack
                    emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                    // Set tag
                    std::string tag_ptr = fresh_reg();
                    emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " +
                              enum_val + ", i32 0, i32 0");
                    emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " + tag_ptr);

                    // No payload to set

                    // Load the complete enum value
                    emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                    last_expr_type_ = enum_type;
                    return result;
                }
            }
        }
    }

    report_error("Unknown variable: " + ident.name, ident.span);
    last_expr_type_ = "i32";
    return "0";
}

auto LLVMIRGen::gen_lowlevel(const parser::LowlevelExpr& lowlevel) -> std::string {
    // Lowlevel blocks are generated like regular blocks
    // but without borrow checking (which is handled at type check level)
    std::string result = "void";

    // Generate each statement
    for (const auto& stmt : lowlevel.stmts) {
        gen_stmt(*stmt);
    }

    // Generate trailing expression if present
    if (lowlevel.expr) {
        result = gen_expr(**lowlevel.expr);
    }

    return result;
}

auto LLVMIRGen::gen_interp_string(const parser::InterpolatedStringExpr& interp) -> std::string {
    // Generate code for interpolated string: "Hello {name}!"
    // Strategy: Convert each segment to a string, then concatenate them all
    // using str_concat

    if (interp.segments.empty()) {
        // Empty string
        std::string const_name = add_string_literal("");
        last_expr_type_ = "ptr";
        return const_name;
    }

    std::vector<std::string> segment_strs;

    for (const auto& segment : interp.segments) {
        if (std::holds_alternative<std::string>(segment.content)) {
            // Literal text segment - add as string constant
            const std::string& text = std::get<std::string>(segment.content);
            std::string const_name = add_string_literal(text);
            segment_strs.push_back(const_name);
        } else {
            // Expression segment - evaluate and convert to string if needed
            const auto& expr_ptr = std::get<parser::ExprPtr>(segment.content);
            std::string expr_val = gen_expr(*expr_ptr);
            std::string expr_type = last_expr_type_;

            // If the expression is already a string (ptr), use it directly
            // Otherwise, convert it to string using appropriate runtime function
            if (expr_type == "ptr") {
                segment_strs.push_back(expr_val);
            } else if (expr_type == "i32" || expr_type == "i64") {
                // Convert integer to string using i64_to_str
                std::string int_val = expr_val;
                if (expr_type == "i32") {
                    // Sign extend i32 to i64
                    std::string ext_reg = fresh_reg();
                    emit_line("  " + ext_reg + " = sext i32 " + expr_val + " to i64");
                    int_val = ext_reg;
                }
                std::string str_result = fresh_reg();
                emit_line("  " + str_result + " = call ptr @i64_to_str(i64 " + int_val + ")");
                segment_strs.push_back(str_result);
            } else if (expr_type == "double" || expr_type == "float") {
                // Convert float to string using f64_to_str
                std::string float_val = expr_val;
                if (expr_type == "float") {
                    // Extend float to double
                    std::string ext_reg = fresh_reg();
                    emit_line("  " + ext_reg + " = fpext float " + expr_val + " to double");
                    float_val = ext_reg;
                }
                std::string str_result = fresh_reg();
                emit_line("  " + str_result + " = call ptr @f64_to_str(double " + float_val + ")");
                segment_strs.push_back(str_result);
            } else if (expr_type == "i1") {
                // Convert bool to string
                std::string str_result = fresh_reg();
                emit_line("  " + str_result + " = select i1 " + expr_val +
                          ", ptr @.str.true, ptr @.str.false");
                segment_strs.push_back(str_result);
            } else {
                // For unknown types, use the value as-is (assume it's a string ptr)
                segment_strs.push_back(expr_val);
            }
        }
    }

    // Concatenate all segments using str_concat
    if (segment_strs.size() == 1) {
        last_expr_type_ = "ptr";
        return segment_strs[0];
    }

    std::string result = segment_strs[0];
    for (size_t i = 1; i < segment_strs.size(); ++i) {
        std::string new_result = fresh_reg();
        emit_line("  " + new_result + " = call ptr @str_concat(ptr " + result + ", ptr " +
                  segment_strs[i] + ")");
        result = new_result;
    }

    last_expr_type_ = "ptr";
    return result;
}

} // namespace tml::codegen
