// LLVM IR generator - Expression generation
// Handles: literals, identifiers, binary ops, unary ops

#include "tml/codegen/llvm_ir_gen.hpp"
#include "tml/lexer/lexer.hpp"
#include <iostream>

namespace tml::codegen {

auto LLVMIRGen::gen_expr(const parser::Expr& expr) -> std::string {
    if (expr.is<parser::LiteralExpr>()) {
        return gen_literal(expr.as<parser::LiteralExpr>());
    } else if (expr.is<parser::IdentExpr>()) {
        return gen_ident(expr.as<parser::IdentExpr>());
    } else if (expr.is<parser::BinaryExpr>()) {
        return gen_binary(expr.as<parser::BinaryExpr>());
    } else if (expr.is<parser::UnaryExpr>()) {
        return gen_unary(expr.as<parser::UnaryExpr>());
    } else if (expr.is<parser::CallExpr>()) {
        return gen_call(expr.as<parser::CallExpr>());
    } else if (expr.is<parser::IfExpr>()) {
        return gen_if(expr.as<parser::IfExpr>());
    } else if (expr.is<parser::TernaryExpr>()) {
        return gen_ternary(expr.as<parser::TernaryExpr>());
    } else if (expr.is<parser::IfLetExpr>()) {
        return gen_if_let(expr.as<parser::IfLetExpr>());
    } else if (expr.is<parser::BlockExpr>()) {
        return gen_block(expr.as<parser::BlockExpr>());
    } else if (expr.is<parser::LoopExpr>()) {
        return gen_loop(expr.as<parser::LoopExpr>());
    } else if (expr.is<parser::WhileExpr>()) {
        return gen_while(expr.as<parser::WhileExpr>());
    } else if (expr.is<parser::ForExpr>()) {
        return gen_for(expr.as<parser::ForExpr>());
    } else if (expr.is<parser::ReturnExpr>()) {
        return gen_return(expr.as<parser::ReturnExpr>());
    } else if (expr.is<parser::WhenExpr>()) {
        return gen_when(expr.as<parser::WhenExpr>());
    } else if (expr.is<parser::StructExpr>()) {
        return gen_struct_expr(expr.as<parser::StructExpr>());
    } else if (expr.is<parser::FieldExpr>()) {
        return gen_field(expr.as<parser::FieldExpr>());
    } else if (expr.is<parser::BreakExpr>()) {
        // Break jumps to end of current loop
        if (!current_loop_end_.empty()) {
            emit_line("  br label %" + current_loop_end_);
            block_terminated_ = true;
        }
        return "void";
    } else if (expr.is<parser::ContinueExpr>()) {
        // Continue jumps to start of current loop
        if (!current_loop_start_.empty()) {
            emit_line("  br label %" + current_loop_start_);
            block_terminated_ = true;
        }
        return "void";
    } else if (expr.is<parser::ArrayExpr>()) {
        return gen_array(expr.as<parser::ArrayExpr>());
    } else if (expr.is<parser::IndexExpr>()) {
        return gen_index(expr.as<parser::IndexExpr>());
    } else if (expr.is<parser::PathExpr>()) {
        return gen_path(expr.as<parser::PathExpr>());
    } else if (expr.is<parser::MethodCallExpr>()) {
        return gen_method_call(expr.as<parser::MethodCallExpr>());
    } else if (expr.is<parser::ClosureExpr>()) {
        return gen_closure(expr.as<parser::ClosureExpr>());
    } else if (expr.is<parser::LowlevelExpr>()) {
        return gen_lowlevel(expr.as<parser::LowlevelExpr>());
    } else if (expr.is<parser::InterpolatedStringExpr>()) {
        return gen_interp_string(expr.as<parser::InterpolatedStringExpr>());
    }

    report_error("Unsupported expression type", expr.span);
    return "0";
}

auto LLVMIRGen::gen_literal(const parser::LiteralExpr& lit) -> std::string {
    switch (lit.token.kind) {
        case lexer::TokenKind::IntLiteral:
            // Use the actual numeric value, not the lexeme (handles 0x, 0b, etc.)
            last_expr_type_ = "i32";
            return std::to_string(lit.token.int_value().value);
        case lexer::TokenKind::FloatLiteral:
            last_expr_type_ = "double";
            return std::to_string(lit.token.float_value().value);
        case lexer::TokenKind::BoolLiteral:
            last_expr_type_ = "i1";
            return lit.token.lexeme == "true" ? "1" : "0";
        case lexer::TokenKind::StringLiteral: {
            std::string str_val = std::string(lit.token.string_value().value);
            std::string const_name = add_string_literal(str_val);
            last_expr_type_ = "ptr";
            return const_name;
        }
        default:
            last_expr_type_ = "i32";
            return "0";
    }
}

auto LLVMIRGen::gen_ident(const parser::IdentExpr& ident) -> std::string {
    // Check global constants first
    auto const_it = global_constants_.find(ident.name);
    if (const_it != global_constants_.end()) {
        last_expr_type_ = "i64";  // Constants are i64 for now
        return const_it->second;
    }

    auto it = locals_.find(ident.name);
    if (it != locals_.end()) {
        const VarInfo& var = it->second;
        last_expr_type_ = var.type;
        // Check if it's an alloca (starts with %t and has digit after) that needs loading
        // But skip %this which is a direct parameter, not an alloca
        // This includes ptr types - we load the pointer value from the alloca
        if (var.reg[0] == '%' && var.reg[1] == 't' && var.reg.length() > 2 && std::isdigit(var.reg[2])) {
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
        last_expr_type_ = "ptr";  // Function pointers are ptr type in LLVM
        return func.llvm_name;    // Return @tml_funcname
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
                emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " + enum_val + ", i32 0, i32 0");
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
                emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " + enum_val + ", i32 0, i32 0");
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
                    emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " + enum_val + ", i32 0, i32 0");
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

auto LLVMIRGen::gen_binary(const parser::BinaryExpr& bin) -> std::string {
    // Handle assignment specially - don't evaluate left for deref assignments
    if (bin.op == parser::BinaryOp::Assign) {
        std::string right = gen_expr(*bin.right);

        if (bin.left->is<parser::IdentExpr>()) {
            auto it = locals_.find(bin.left->as<parser::IdentExpr>().name);
            if (it != locals_.end()) {
                emit_line("  store " + it->second.type + " " + right + ", ptr " + it->second.reg);
            }
        } else if (bin.left->is<parser::UnaryExpr>()) {
            const auto& unary = bin.left->as<parser::UnaryExpr>();
            if (unary.op == parser::UnaryOp::Deref) {
                // Dereferenced pointer assignment: *ptr = value
                // Get the pointer (not the dereferenced value!)
                std::string ptr = gen_expr(*unary.operand);
                emit_line("  store i32 " + right + ", ptr " + ptr);
            }
        } else if (bin.left->is<parser::FieldExpr>()) {
            // Field assignment: obj.field = value or this.field = value
            const auto& field = bin.left->as<parser::FieldExpr>();
            std::string struct_type;
            std::string struct_ptr;

            if (field.object->is<parser::IdentExpr>()) {
                const auto& ident = field.object->as<parser::IdentExpr>();
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    struct_type = it->second.type;
                    struct_ptr = it->second.reg;

                    // Special handling for 'this' in impl methods
                    if (ident.name == "this" && !current_impl_type_.empty()) {
                        struct_type = "%struct." + current_impl_type_;
                        // 'this' is already a pointer parameter, not an alloca - use it directly
                        // struct_ptr is already "%this" which is the direct pointer
                    }
                }
            }

            if (!struct_type.empty() && !struct_ptr.empty()) {
                // Get the struct type name for field lookup
                std::string type_name = struct_type;
                if (type_name.starts_with("%struct.")) {
                    type_name = type_name.substr(8);
                }

                // Get field pointer
                int field_idx = get_field_index(type_name, field.field);
                std::string field_type = get_field_type(type_name, field.field);
                std::string field_ptr = fresh_reg();
                emit_line("  " + field_ptr + " = getelementptr " + struct_type + ", ptr " + struct_ptr + ", i32 0, i32 " + std::to_string(field_idx));

                // Store value to field
                emit_line("  store " + field_type + " " + right + ", ptr " + field_ptr);
            }
        }
        return right;
    }

    // Handle compound assignment operators (+=, -=, *=, /=, %=, etc.)
    if (bin.op >= parser::BinaryOp::AddAssign && bin.op <= parser::BinaryOp::ShrAssign) {
        if (bin.left->is<parser::IdentExpr>()) {
            const auto& ident = bin.left->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                const VarInfo& var = it->second;

                // Load current value
                std::string current = fresh_reg();
                emit_line("  " + current + " = load " + var.type + ", ptr " + var.reg);

                // Generate right operand
                std::string right = gen_expr(*bin.right);

                // Perform the operation
                std::string result = fresh_reg();
                std::string op_type = var.type;
                bool is_float = (op_type == "double" || op_type == "float");

                switch (bin.op) {
                    case parser::BinaryOp::AddAssign:
                        if (is_float)
                            emit_line("  " + result + " = fadd " + op_type + " " + current + ", " + right);
                        else
                            emit_line("  " + result + " = add nsw " + op_type + " " + current + ", " + right);
                        break;
                    case parser::BinaryOp::SubAssign:
                        if (is_float)
                            emit_line("  " + result + " = fsub " + op_type + " " + current + ", " + right);
                        else
                            emit_line("  " + result + " = sub nsw " + op_type + " " + current + ", " + right);
                        break;
                    case parser::BinaryOp::MulAssign:
                        if (is_float)
                            emit_line("  " + result + " = fmul " + op_type + " " + current + ", " + right);
                        else
                            emit_line("  " + result + " = mul nsw " + op_type + " " + current + ", " + right);
                        break;
                    case parser::BinaryOp::DivAssign:
                        if (is_float)
                            emit_line("  " + result + " = fdiv " + op_type + " " + current + ", " + right);
                        else
                            emit_line("  " + result + " = sdiv " + op_type + " " + current + ", " + right);
                        break;
                    case parser::BinaryOp::ModAssign:
                        emit_line("  " + result + " = srem " + op_type + " " + current + ", " + right);
                        break;
                    case parser::BinaryOp::BitAndAssign:
                        emit_line("  " + result + " = and " + op_type + " " + current + ", " + right);
                        break;
                    case parser::BinaryOp::BitOrAssign:
                        emit_line("  " + result + " = or " + op_type + " " + current + ", " + right);
                        break;
                    case parser::BinaryOp::BitXorAssign:
                        emit_line("  " + result + " = xor " + op_type + " " + current + ", " + right);
                        break;
                    case parser::BinaryOp::ShlAssign:
                        emit_line("  " + result + " = shl " + op_type + " " + current + ", " + right);
                        break;
                    case parser::BinaryOp::ShrAssign:
                        emit_line("  " + result + " = ashr " + op_type + " " + current + ", " + right);
                        break;
                    default:
                        result = current;
                        break;
                }

                // Store result back
                emit_line("  store " + var.type + " " + result + ", ptr " + var.reg);
                last_expr_type_ = var.type;
                return result;
            }
        }
        report_error("Compound assignment requires a variable on the left side", bin.span);
        return "0";
    }

    std::string left = gen_expr(*bin.left);
    std::string left_type = last_expr_type_;
    std::string right = gen_expr(*bin.right);
    std::string right_type = last_expr_type_;
    std::string result = fresh_reg();

    // Check if either operand is a struct (enum)
    bool is_enum_struct = left_type.starts_with("%struct.") && right_type.starts_with("%struct.");

    // For enum struct comparisons, extract the tag field (first i32)
    if (is_enum_struct) {
        // Allocate space for left struct
        std::string left_alloca = fresh_reg();
        emit_line("  " + left_alloca + " = alloca " + left_type);
        emit_line("  store " + left_type + " " + left + ", ptr " + left_alloca);

        // Extract tag from left
        std::string left_tag_ptr = fresh_reg();
        emit_line("  " + left_tag_ptr + " = getelementptr " + left_type + ", ptr " + left_alloca + ", i32 0, i32 0");
        std::string left_tag = fresh_reg();
        emit_line("  " + left_tag + " = load i32, ptr " + left_tag_ptr);

        // Allocate space for right struct
        std::string right_alloca = fresh_reg();
        emit_line("  " + right_alloca + " = alloca " + right_type);
        emit_line("  store " + right_type + " " + right + ", ptr " + right_alloca);

        // Extract tag from right
        std::string right_tag_ptr = fresh_reg();
        emit_line("  " + right_tag_ptr + " = getelementptr " + right_type + ", ptr " + right_alloca + ", i32 0, i32 0");
        std::string right_tag = fresh_reg();
        emit_line("  " + right_tag + " = load i32, ptr " + right_tag_ptr);

        // Replace left/right with tag values
        left = left_tag;
        right = right_tag;
        left_type = "i32";
        right_type = "i32";
    }

    // Check if either operand is a float
    bool is_float = (left_type == "double" || left_type == "float" ||
                     right_type == "double" || right_type == "float");

    // Check if either operand is i64
    bool is_i64 = (left_type == "i64" || right_type == "i64");

    // Handle float literals (e.g., 3.0)
    if (!is_float && bin.right->is<parser::LiteralExpr>()) {
        const auto& lit = bin.right->as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
            is_float = true;
        }
    }
    if (!is_float && bin.left->is<parser::LiteralExpr>()) {
        const auto& lit = bin.left->as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
            is_float = true;
        }
    }

    if (is_float) {
        // Convert integer operands to double if needed
        if (left_type == "i32" || left_type == "i64") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = sitofp " + left_type + " " + left + " to double");
            left = conv;
        }
        if (right_type == "i32" || right_type == "i64") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = sitofp " + right_type + " " + right + " to double");
            right = conv;
        }
        // Also handle float literals that need double representation
        if (bin.right->is<parser::LiteralExpr>()) {
            const auto& lit = bin.right->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
                right = std::to_string(lit.token.float_value().value);
            }
        }
    } else if (is_i64) {
        // Promote i32 to i64 if needed
        if (left_type == "i32") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = sext i32 " + left + " to i64");
            left = conv;
        }
        if (right_type == "i32") {
            std::string conv = fresh_reg();
            emit_line("  " + conv + " = sext i32 " + right + " to i64");
            right = conv;
        }
    }

    // Check if either operand is Bool (i1)
    bool is_bool = (left_type == "i1" || right_type == "i1");

    // Determine the integer type to use
    std::string int_type = is_bool ? "i1" : (is_i64 ? "i64" : "i32");

    switch (bin.op) {
        case parser::BinaryOp::Add:
            if (is_float) {
                emit_line("  " + result + " = fadd double " + left + ", " + right);
                last_expr_type_ = "double";
            } else {
                emit_line("  " + result + " = add nsw " + int_type + " " + left + ", " + right);
                last_expr_type_ = int_type;
            }
            break;
        case parser::BinaryOp::Sub:
            if (is_float) {
                emit_line("  " + result + " = fsub double " + left + ", " + right);
                last_expr_type_ = "double";
            } else {
                emit_line("  " + result + " = sub nsw " + int_type + " " + left + ", " + right);
                last_expr_type_ = int_type;
            }
            break;
        case parser::BinaryOp::Mul:
            if (is_float) {
                emit_line("  " + result + " = fmul double " + left + ", " + right);
                last_expr_type_ = "double";
            } else {
                emit_line("  " + result + " = mul nsw " + int_type + " " + left + ", " + right);
                last_expr_type_ = int_type;
            }
            break;
        case parser::BinaryOp::Div:
            if (is_float) {
                emit_line("  " + result + " = fdiv double " + left + ", " + right);
                last_expr_type_ = "double";
            } else {
                emit_line("  " + result + " = sdiv " + int_type + " " + left + ", " + right);
                last_expr_type_ = int_type;
            }
            break;
        case parser::BinaryOp::Mod:
            emit_line("  " + result + " = srem " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
            break;
        // Comparisons return i1 (use fcmp for floats, icmp for integers)
        case parser::BinaryOp::Eq:
            if (is_float) {
                emit_line("  " + result + " = fcmp oeq double " + left + ", " + right);
            } else {
                emit_line("  " + result + " = icmp eq " + int_type + " " + left + ", " + right);
            }
            last_expr_type_ = "i1";
            break;
        case parser::BinaryOp::Ne:
            if (is_float) {
                emit_line("  " + result + " = fcmp one double " + left + ", " + right);
            } else {
                emit_line("  " + result + " = icmp ne " + int_type + " " + left + ", " + right);
            }
            last_expr_type_ = "i1";
            break;
        case parser::BinaryOp::Lt:
            if (is_float) {
                emit_line("  " + result + " = fcmp olt double " + left + ", " + right);
            } else {
                emit_line("  " + result + " = icmp slt " + int_type + " " + left + ", " + right);
            }
            last_expr_type_ = "i1";
            break;
        case parser::BinaryOp::Gt:
            if (is_float) {
                emit_line("  " + result + " = fcmp ogt double " + left + ", " + right);
            } else {
                emit_line("  " + result + " = icmp sgt " + int_type + " " + left + ", " + right);
            }
            last_expr_type_ = "i1";
            break;
        case parser::BinaryOp::Le:
            if (is_float) {
                emit_line("  " + result + " = fcmp ole double " + left + ", " + right);
            } else {
                emit_line("  " + result + " = icmp sle " + int_type + " " + left + ", " + right);
            }
            last_expr_type_ = "i1";
            break;
        case parser::BinaryOp::Ge:
            if (is_float) {
                emit_line("  " + result + " = fcmp oge double " + left + ", " + right);
            } else {
                emit_line("  " + result + " = icmp sge " + int_type + " " + left + ", " + right);
            }
            last_expr_type_ = "i1";
            break;
        // Logical operators work on i1
        case parser::BinaryOp::And:
            emit_line("  " + result + " = and i1 " + left + ", " + right);
            last_expr_type_ = "i1";
            break;
        case parser::BinaryOp::Or:
            emit_line("  " + result + " = or i1 " + left + ", " + right);
            last_expr_type_ = "i1";
            break;
        // Bitwise operators work on same type
        case parser::BinaryOp::BitAnd:
            emit_line("  " + result + " = and " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
            break;
        case parser::BinaryOp::BitOr:
            emit_line("  " + result + " = or " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
            break;
        case parser::BinaryOp::BitXor:
            emit_line("  " + result + " = xor " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
            break;
        case parser::BinaryOp::Shl:
            // nuw = no unsigned wrap for shift
            emit_line("  " + result + " = shl nuw " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
            break;
        case parser::BinaryOp::Shr:
            emit_line("  " + result + " = ashr " + int_type + " " + left + ", " + right);
            last_expr_type_ = int_type;
            break;
        // Note: Assign is handled above before evaluating left/right
        default:
            emit_line("  " + result + " = add nsw i32 " + left + ", " + right);
            break;
    }

    return result;
}

auto LLVMIRGen::gen_unary(const parser::UnaryExpr& unary) -> std::string {
    // Handle ref operations specially - we need the address, not the value
    if (unary.op == parser::UnaryOp::Ref || unary.op == parser::UnaryOp::RefMut) {
        // Get pointer to the operand
        if (unary.operand->is<parser::IdentExpr>()) {
            const auto& ident = unary.operand->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                // Return the alloca pointer directly (don't load)
                return it->second.reg;
            }
        }
        report_error("Can only take reference of variables", unary.span);
        return "null";
    }

    // Handle deref - load from pointer
    if (unary.op == parser::UnaryOp::Deref) {
        std::string ptr = gen_expr(*unary.operand);
        std::string result = fresh_reg();
        // Assume dereferencing i32* for now
        emit_line("  " + result + " = load i32, ptr " + ptr);
        return result;
    }

    // Handle postfix increment (i++)
    if (unary.op == parser::UnaryOp::Inc) {
        if (unary.operand->is<parser::IdentExpr>()) {
            const auto& ident = unary.operand->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                const VarInfo& var = it->second;
                // Load current value
                std::string old_val = fresh_reg();
                emit_line("  " + old_val + " = load " + var.type + ", ptr " + var.reg);
                // Add 1
                std::string new_val = fresh_reg();
                emit_line("  " + new_val + " = add " + var.type + " " + old_val + ", 1");
                // Store new value
                emit_line("  store " + var.type + " " + new_val + ", ptr " + var.reg);
                // Return old value (postfix semantics)
                return old_val;
            }
        }
        report_error("Can only increment variables", unary.span);
        return "0";
    }

    // Handle postfix decrement (i--)
    if (unary.op == parser::UnaryOp::Dec) {
        if (unary.operand->is<parser::IdentExpr>()) {
            const auto& ident = unary.operand->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                const VarInfo& var = it->second;
                // Load current value
                std::string old_val = fresh_reg();
                emit_line("  " + old_val + " = load " + var.type + ", ptr " + var.reg);
                // Subtract 1
                std::string new_val = fresh_reg();
                emit_line("  " + new_val + " = sub " + var.type + " " + old_val + ", 1");
                // Store new value
                emit_line("  store " + var.type + " " + new_val + ", ptr " + var.reg);
                // Return old value (postfix semantics)
                return old_val;
            }
        }
        report_error("Can only decrement variables", unary.span);
        return "0";
    }

    std::string operand = gen_expr(*unary.operand);
    std::string operand_type = last_expr_type_;
    std::string result = fresh_reg();

    switch (unary.op) {
        case parser::UnaryOp::Neg:
            if (operand_type == "double" || operand_type == "float") {
                emit_line("  " + result + " = fsub double 0.0, " + operand);
                last_expr_type_ = "double";
            } else {
                emit_line("  " + result + " = sub " + operand_type + " 0, " + operand);
                last_expr_type_ = operand_type;
            }
            break;
        case parser::UnaryOp::Not:
            emit_line("  " + result + " = xor i1 " + operand + ", 1");
            last_expr_type_ = "i1";
            break;
        case parser::UnaryOp::BitNot:
            emit_line("  " + result + " = xor i32 " + operand + ", -1");
            last_expr_type_ = "i32";
            break;
        default:
            return operand;
    }

    return result;
}

auto LLVMIRGen::gen_closure(const parser::ClosureExpr& closure) -> std::string {
    // Clear previous closure capture info
    last_closure_captures_ = std::nullopt;

    // Generate a unique function name
    std::string closure_name = "tml_closure_" + std::to_string(closure_counter_++);

    // Collect capture info if there are captured variables
    if (!closure.captured_vars.empty()) {
        ClosureCaptureInfo capture_info;
        for (const auto& captured_name : closure.captured_vars) {
            auto it = locals_.find(captured_name);
            std::string captured_type = (it != locals_.end()) ? it->second.type : "i32";
            capture_info.captured_names.push_back(captured_name);
            capture_info.captured_types.push_back(captured_type);
        }
        last_closure_captures_ = capture_info;
    }

    // Build parameter types string, including captured variables as first parameters
    std::string param_types_str;
    std::vector<std::string> param_names;

    // Add captured variables as parameters
    for (const auto& captured_name : closure.captured_vars) {
        if (!param_types_str.empty()) param_types_str += ", ";

        // Look up the type from locals
        auto it = locals_.find(captured_name);
        std::string captured_type = (it != locals_.end()) ? it->second.type : "i32";

        param_types_str += captured_type + " %" + captured_name + "_captured";
        param_names.push_back(captured_name);
    }

    // Add closure parameters
    for (size_t i = 0; i < closure.params.size(); ++i) {
        if (!param_types_str.empty()) param_types_str += ", ";

        // For now, assume i32 parameters (simplified)
        param_types_str += "i32";

        // Get parameter name from pattern
        if (closure.params[i].first->is<parser::IdentPattern>()) {
            const auto& ident = closure.params[i].first->as<parser::IdentPattern>();
            param_names.push_back(ident.name);
        } else {
            param_names.push_back("_p" + std::to_string(i));
        }

        param_types_str += " %" + param_names.back();
    }

    // Determine return type from closure annotation or infer from body
    std::string ret_type = "i32";
    if (closure.return_type.has_value()) {
        ret_type = llvm_type(*closure.return_type.value());
    } else if (closure.body) {
        // Infer return type from closure body expression
        types::TypePtr inferred = infer_expr_type(*closure.body);
        if (inferred) {
            ret_type = llvm_type_from_semantic(inferred);
        }
    }

    // Save information about captured variables before clearing locals
    std::vector<std::pair<std::string, std::string>> captured_info;  // (name, type)
    for (const auto& captured_name : closure.captured_vars) {
        auto it = locals_.find(captured_name);
        std::string captured_type = (it != locals_.end()) ? it->second.type : "i32";
        captured_info.push_back({captured_name, captured_type});
    }

    // Save current function state
    std::stringstream saved_output;
    saved_output << output_.str();
    output_.str("");  // Clear for closure generation
    auto saved_locals = locals_;
    auto saved_ret_type = current_ret_type_;
    bool saved_terminated = block_terminated_;

    // Start new function
    locals_.clear();
    current_ret_type_ = ret_type;
    block_terminated_ = false;

    // Emit function header
    emit_line("define internal " + ret_type + " @" + closure_name + "(" + param_types_str + ") #0 {");
    emit_line("entry:");

    // Bind captured variables to local scope
    for (size_t i = 0; i < captured_info.size(); ++i) {
        const auto& [captured_name, captured_type] = captured_info[i];

        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + captured_type);
        emit_line("  store " + captured_type + " %" + captured_name + "_captured, ptr " + alloca_reg);
        locals_[captured_name] = VarInfo{alloca_reg, captured_type};
    }

    // Bind closure parameters to local scope
    for (size_t i = closure.captured_vars.size(); i < param_names.size(); ++i) {
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca i32");
        emit_line("  store i32 %" + param_names[i] + ", ptr " + alloca_reg);
        locals_[param_names[i]] = VarInfo{alloca_reg, "i32", nullptr};
    }

    // Generate body
    std::string body_val = gen_expr(*closure.body);

    // Return the result
    if (!block_terminated_) {
        emit_line("  ret " + ret_type + " " + body_val);
    }

    emit_line("}");
    emit_line("");

    // Store generated closure function
    std::string closure_code = output_.str();

    // Restore original function state
    output_.str(saved_output.str());
    output_.seekp(0, std::ios_base::end);  // Restore position to end
    locals_ = saved_locals;
    current_ret_type_ = saved_ret_type;
    block_terminated_ = saved_terminated;

    // Add closure function to module-level code
    module_functions_.push_back(closure_code);

    // Return function pointer
    // For now, return the function name as a "function pointer"
    // This is simplified - full support would need actual function pointers
    last_expr_type_ = "ptr";  // Closures are function pointers
    return "@" + closure_name;
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
                std::string str_result = fresh_reg();
                emit_line("  " + str_result + " = call ptr @i64_to_str(i64 " + expr_val + ")");
                segment_strs.push_back(str_result);
            } else if (expr_type == "double" || expr_type == "float") {
                // Convert float to string using f64_to_str
                std::string str_result = fresh_reg();
                emit_line("  " + str_result + " = call ptr @f64_to_str(double " + expr_val + ")");
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
        emit_line("  " + new_result + " = call ptr @str_concat(ptr " + result + ", ptr " + segment_strs[i] + ")");
        result = new_result;
    }

    last_expr_type_ = "ptr";
    return result;
}

} // namespace tml::codegen
