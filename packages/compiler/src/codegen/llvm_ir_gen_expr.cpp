// LLVM IR generator - Expression generation
// Handles: literals, identifiers, binary ops, unary ops

#include "tml/codegen/llvm_ir_gen.hpp"
#include "tml/lexer/lexer.hpp"

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
        // Check if it's an alloca (starts with %t) that needs loading
        // This includes ptr types - we load the pointer value from the alloca
        if (var.reg[0] == '%' && var.reg[1] == 't') {
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
    for (const auto& [enum_name, enum_def] : env_.all_enums()) {
        for (size_t variant_idx = 0; variant_idx < enum_def.variants.size(); ++variant_idx) {
            const auto& [variant_name, payload_types] = enum_def.variants[variant_idx];

            if (variant_name == ident.name && payload_types.empty()) {
                // Found unit variant - create enum value with just the tag
                std::string enum_type = "%struct." + enum_name;
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

    // Determine the integer type to use
    std::string int_type = is_i64 ? "i64" : "i32";

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
        // Comparisons return i1
        case parser::BinaryOp::Eq:
            emit_line("  " + result + " = icmp eq " + int_type + " " + left + ", " + right);
            last_expr_type_ = "i1";
            break;
        case parser::BinaryOp::Ne:
            emit_line("  " + result + " = icmp ne " + int_type + " " + left + ", " + right);
            last_expr_type_ = "i1";
            break;
        case parser::BinaryOp::Lt:
            emit_line("  " + result + " = icmp slt " + int_type + " " + left + ", " + right);
            last_expr_type_ = "i1";
            break;
        case parser::BinaryOp::Gt:
            emit_line("  " + result + " = icmp sgt " + int_type + " " + left + ", " + right);
            last_expr_type_ = "i1";
            break;
        case parser::BinaryOp::Le:
            emit_line("  " + result + " = icmp sle " + int_type + " " + left + ", " + right);
            last_expr_type_ = "i1";
            break;
        case parser::BinaryOp::Ge:
            emit_line("  " + result + " = icmp sge " + int_type + " " + left + ", " + right);
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
    std::string result = fresh_reg();

    switch (unary.op) {
        case parser::UnaryOp::Neg:
            emit_line("  " + result + " = sub i32 0, " + operand);
            break;
        case parser::UnaryOp::Not:
            emit_line("  " + result + " = xor i1 " + operand + ", 1");
            break;
        case parser::UnaryOp::BitNot:
            emit_line("  " + result + " = xor i32 " + operand + ", -1");
            break;
        default:
            return operand;
    }

    return result;
}

auto LLVMIRGen::gen_closure(const parser::ClosureExpr& closure) -> std::string {
    // For now, generate a simple lambda function as an inline helper
    // Full closure support would require capturing environment variables

    // Generate a unique function name
    std::string closure_name = "tml_closure_" + std::to_string(closure_counter_++);

    // Build parameter types string
    std::string param_types_str;
    std::vector<std::string> param_names;
    for (size_t i = 0; i < closure.params.size(); ++i) {
        if (i > 0) param_types_str += ", ";

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

    // Determine return type (simplified to i32 for now)
    std::string ret_type = "i32";

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

    // Bind parameters to local scope
    for (size_t i = 0; i < param_names.size(); ++i) {
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca i32");
        emit_line("  store i32 %" + param_names[i] + ", ptr " + alloca_reg);
        locals_[param_names[i]] = VarInfo{alloca_reg, "i32"};
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
    return "@" + closure_name;
}

} // namespace tml::codegen
