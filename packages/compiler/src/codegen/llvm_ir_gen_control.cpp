// LLVM IR generator - Control flow
// Handles: if, block, loop, while, for, return

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

// Forward declaration for is_bool_expr (defined in llvm_ir_gen_stmt.cpp)
bool is_bool_expr(const parser::Expr& expr, const std::unordered_map<std::string, LLVMIRGen::VarInfo>& locals);

auto LLVMIRGen::gen_if(const parser::IfExpr& if_expr) -> std::string {
    std::string cond = gen_expr(*if_expr.condition);

    // If condition is not already i1 (bool), convert it
    // This handles user-defined functions that return Bool but are generated as i32
    if (!is_bool_expr(*if_expr.condition, locals_)) {
        // Condition might be i32, convert to i1 by comparing with 0
        std::string bool_cond = fresh_reg();
        emit_line("  " + bool_cond + " = icmp ne i32 " + cond + ", 0");
        cond = bool_cond;
    }

    std::string label_then = fresh_label("if.then");
    std::string label_else = fresh_label("if.else");
    std::string label_end = fresh_label("if.end");

    // Branch
    if (if_expr.else_branch.has_value()) {
        emit_line("  br i1 " + cond + ", label %" + label_then + ", label %" + label_else);
    } else {
        emit_line("  br i1 " + cond + ", label %" + label_then + ", label %" + label_end);
    }

    // Then block
    emit_line(label_then + ":");
    block_terminated_ = false;
    std::string then_val = gen_expr(*if_expr.then_branch);
    std::string then_type = last_expr_type_;
    bool then_terminated = block_terminated_;
    std::string then_block_label = label_then;
    if (!block_terminated_) {
        emit_line("  br label %" + label_end);
        then_block_label = fresh_label("if.then.end");
        // Track the actual block label where the then value was produced
        then_block_label = label_then;
    }

    // Else block
    std::string else_val = "0";
    std::string else_type = "i32";
    bool else_terminated = false;
    std::string else_block_label = label_else;
    if (if_expr.else_branch.has_value()) {
        emit_line(label_else + ":");
        block_terminated_ = false;
        else_val = gen_expr(*if_expr.else_branch.value());
        else_type = last_expr_type_;
        else_terminated = block_terminated_;
        else_block_label = label_else;
        if (!block_terminated_) {
            emit_line("  br label %" + label_end);
        }
    }

    // End block
    emit_line(label_end + ":");
    block_terminated_ = false;

    // If both branches return values and neither is terminated, create phi node
    if (if_expr.else_branch.has_value() && !then_terminated && !else_terminated) {
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + then_type + " [ " + then_val + ", %" + label_then + " ], [ " + else_val + ", %" + label_else + " ]");
        last_expr_type_ = then_type;
        return result;
    }

    return "0";
}

auto LLVMIRGen::gen_if_let(const parser::IfLetExpr& if_let) -> std::string {
    // Evaluate scrutinee
    std::string scrutinee = gen_expr(*if_let.scrutinee);
    std::string scrutinee_type = last_expr_type_;

    std::string label_then = fresh_label("iflet.then");
    std::string label_else = fresh_label("iflet.else");
    std::string label_end = fresh_label("iflet.end");

    // Check if pattern matches and bind variables
    // For enum patterns, check the tag
    if (if_let.pattern->is<parser::EnumPattern>()) {
        const auto& enum_pat = if_let.pattern->as<parser::EnumPattern>();
        std::string variant_name = enum_pat.path.segments.back();

        // Allocate space for scrutinee
        std::string scrutinee_ptr = fresh_reg();
        emit_line("  " + scrutinee_ptr + " = alloca " + scrutinee_type);
        emit_line("  store " + scrutinee_type + " " + scrutinee + ", ptr " + scrutinee_ptr);

        // Extract tag
        std::string tag_ptr = fresh_reg();
        emit_line("  " + tag_ptr + " = getelementptr inbounds " + scrutinee_type + ", ptr " + scrutinee_ptr + ", i32 0, i32 0");
        std::string tag = fresh_reg();
        emit_line("  " + tag + " = load i32, ptr " + tag_ptr);

        // Find variant index
        int variant_tag = -1;
        for (const auto& [enum_name, enum_def] : env_.all_enums()) {
            for (size_t v_idx = 0; v_idx < enum_def.variants.size(); ++v_idx) {
                if (enum_def.variants[v_idx].first == variant_name) {
                    variant_tag = static_cast<int>(v_idx);
                    break;
                }
            }
            if (variant_tag >= 0) break;
        }

        // Compare tag
        if (variant_tag >= 0) {
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq i32 " + tag + ", " + std::to_string(variant_tag));
            emit_line("  br i1 " + cmp + ", label %" + label_then + ", label %" + label_else);
        } else {
            // Unknown variant, go to else
            emit_line("  br label %" + label_else);
        }

        // Then block - pattern matched
        emit_line(label_then + ":");
        block_terminated_ = false;

        // Bind pattern variables
        if (enum_pat.payload.has_value() && !enum_pat.payload->empty()) {
            // Extract payload value
            std::string payload_ptr = fresh_reg();
            emit_line("  " + payload_ptr + " = getelementptr inbounds " + scrutinee_type + ", ptr " + scrutinee_ptr + ", i32 0, i32 1");
            std::string payload = fresh_reg();
            emit_line("  " + payload + " = load i64, ptr " + payload_ptr);

            // Bind to first pattern variable
            if (enum_pat.payload->at(0)->is<parser::IdentPattern>()) {
                const auto& ident = enum_pat.payload->at(0)->as<parser::IdentPattern>();

                // Convert i64 to i32 if needed
                std::string bound_val = payload;
                std::string bound_type = "i64";

                std::string converted = fresh_reg();
                emit_line("  " + converted + " = trunc i64 " + payload + " to i32");
                bound_val = converted;
                bound_type = "i32";

                // Allocate and store bound variable
                std::string var_alloca = fresh_reg();
                emit_line("  " + var_alloca + " = alloca " + bound_type);
                emit_line("  store " + bound_type + " " + bound_val + ", ptr " + var_alloca);
                locals_[ident.name] = VarInfo{var_alloca, bound_type};
            }
        }
    } else {
        // For other patterns (wildcard, ident), always match
        emit_line("  br label %" + label_then);
        emit_line(label_then + ":");
        block_terminated_ = false;
    }

    // Execute then branch
    std::string then_val = gen_expr(*if_let.then_branch);
    if (!block_terminated_) {
        emit_line("  br label %" + label_end);
    }

    // Else block
    if (if_let.else_branch.has_value()) {
        emit_line(label_else + ":");
        block_terminated_ = false;
        std::string else_val = gen_expr(*if_let.else_branch.value());
        if (!block_terminated_) {
            emit_line("  br label %" + label_end);
        }
    }

    // End block
    emit_line(label_end + ":");
    block_terminated_ = false;

    return "0";
}

auto LLVMIRGen::gen_block(const parser::BlockExpr& block) -> std::string {
    std::string result = "0";

    for (const auto& stmt : block.stmts) {
        if (block_terminated_) {
            // Block already terminated, skip remaining statements
            break;
        }
        gen_stmt(*stmt);
    }

    if (block.expr.has_value() && !block_terminated_) {
        result = gen_expr(*block.expr.value());
    }

    return result;
}

auto LLVMIRGen::gen_loop(const parser::LoopExpr& loop) -> std::string {
    std::string label_start = fresh_label("loop.start");
    std::string label_body = fresh_label("loop.body");
    std::string label_end = fresh_label("loop.end");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    current_loop_start_ = label_start;
    current_loop_end_ = label_end;

    emit_line("  br label %" + label_start);
    emit_line(label_start + ":");
    block_terminated_ = false;

    gen_expr(*loop.body);

    if (!block_terminated_) {
        emit_line("  br label %" + label_start);
    }

    emit_line(label_end + ":");
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;

    return "0";
}

auto LLVMIRGen::gen_while(const parser::WhileExpr& while_expr) -> std::string {
    std::string label_cond = fresh_label("while.cond");
    std::string label_body = fresh_label("while.body");
    std::string label_end = fresh_label("while.end");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    current_loop_start_ = label_cond;
    current_loop_end_ = label_end;

    // Jump to condition
    emit_line("  br label %" + label_cond);

    // Condition block
    emit_line(label_cond + ":");
    block_terminated_ = false;
    std::string cond = gen_expr(*while_expr.condition);

    // If condition is not already i1 (bool), convert it
    if (!is_bool_expr(*while_expr.condition, locals_)) {
        std::string bool_cond = fresh_reg();
        emit_line("  " + bool_cond + " = icmp ne i32 " + cond + ", 0");
        cond = bool_cond;
    }

    emit_line("  br i1 " + cond + ", label %" + label_body + ", label %" + label_end);

    // Body block
    emit_line(label_body + ":");
    block_terminated_ = false;
    gen_expr(*while_expr.body);
    if (!block_terminated_) {
        emit_line("  br label %" + label_cond);
    }

    // End block
    emit_line(label_end + ":");
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;

    return "0";
}

auto LLVMIRGen::gen_for(const parser::ForExpr& for_expr) -> std::string {
    // For loops: for pattern in iter { body }
    // We support range expressions: for i in 0 to 10 { ... }
    std::string label_init = fresh_label("for.init");
    std::string label_cond = fresh_label("for.cond");
    std::string label_body = fresh_label("for.body");
    std::string label_incr = fresh_label("for.incr");
    std::string label_end = fresh_label("for.end");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    current_loop_start_ = label_incr;  // continue goes to increment
    current_loop_end_ = label_end;

    // Get loop variable name from pattern
    std::string var_name = "_for_idx";
    if (for_expr.pattern->is<parser::IdentPattern>()) {
        var_name = for_expr.pattern->as<parser::IdentPattern>().name;
    }

    // Check if iter is a range expression
    std::string range_start = "0";
    std::string range_end = "0";
    bool inclusive = false;

    std::string range_type = "i32";  // Default type for range

    if (for_expr.iter->is<parser::RangeExpr>()) {
        const auto& range = for_expr.iter->as<parser::RangeExpr>();
        inclusive = range.inclusive;
        if (range.start.has_value()) {
            range_start = gen_expr(*range.start.value());
        }
        if (range.end.has_value()) {
            range_end = gen_expr(*range.end.value());
            range_type = last_expr_type_;  // Use type of end value
        }
    } else {
        // Fallback: treat as simple range 0 to iter
        range_end = gen_expr(*for_expr.iter);
        range_type = last_expr_type_;
    }

    // Allocate loop variable with correct type
    std::string var_alloca = fresh_reg();
    emit_line("  " + var_alloca + " = alloca " + range_type);
    emit_line("  store " + range_type + " " + range_start + ", ptr " + var_alloca);
    locals_[var_name] = VarInfo{var_alloca, range_type};

    // Jump to condition
    emit_line("  br label %" + label_cond);

    // Condition block
    emit_line(label_cond + ":");
    block_terminated_ = false;
    std::string current = fresh_reg();
    emit_line("  " + current + " = load " + range_type + ", ptr " + var_alloca);
    std::string cmp_result = fresh_reg();
    if (inclusive) {
        emit_line("  " + cmp_result + " = icmp sle " + range_type + " " + current + ", " + range_end);
    } else {
        emit_line("  " + cmp_result + " = icmp slt " + range_type + " " + current + ", " + range_end);
    }
    emit_line("  br i1 " + cmp_result + ", label %" + label_body + ", label %" + label_end);

    // Body block
    emit_line(label_body + ":");
    block_terminated_ = false;
    gen_expr(*for_expr.body);
    if (!block_terminated_) {
        emit_line("  br label %" + label_incr);
    }

    // Increment block
    emit_line(label_incr + ":");
    block_terminated_ = false;
    std::string next_val = fresh_reg();
    std::string current2 = fresh_reg();
    emit_line("  " + current2 + " = load " + range_type + ", ptr " + var_alloca);
    emit_line("  " + next_val + " = add nsw " + range_type + " " + current2 + ", 1");
    emit_line("  store " + range_type + " " + next_val + ", ptr " + var_alloca);
    emit_line("  br label %" + label_cond);

    // End block
    emit_line(label_end + ":");
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;

    return "0";
}

auto LLVMIRGen::gen_return(const parser::ReturnExpr& ret) -> std::string {
    if (ret.value.has_value()) {
        std::string val = gen_expr(*ret.value.value());
        emit_line("  ret " + current_ret_type_ + " " + val);
    } else {
        emit_line("  ret void");
    }
    block_terminated_ = true;
    return "void";
}

auto LLVMIRGen::gen_when(const parser::WhenExpr& when) -> std::string {
    // Evaluate scrutinee
    std::string scrutinee = gen_expr(*when.scrutinee);
    std::string scrutinee_type = last_expr_type_;

    // Allocate space for scrutinee if needed
    std::string scrutinee_ptr = fresh_reg();
    emit_line("  " + scrutinee_ptr + " = alloca " + scrutinee_type);
    emit_line("  store " + scrutinee_type + " " + scrutinee + ", ptr " + scrutinee_ptr);

    // Extract tag (assumes enum is { i32, i64 })
    std::string tag_ptr = fresh_reg();
    emit_line("  " + tag_ptr + " = getelementptr inbounds " + scrutinee_type + ", ptr " + scrutinee_ptr + ", i32 0, i32 0");
    std::string tag = fresh_reg();
    emit_line("  " + tag + " = load i32, ptr " + tag_ptr);

    // Generate labels for each arm + end
    std::vector<std::string> arm_labels;
    for (size_t i = 0; i < when.arms.size(); ++i) {
        arm_labels.push_back(fresh_label("when.arm" + std::to_string(i)));
    }
    std::string label_end = fresh_label("when.end");

    // Generate switch based on pattern
    // For now, simplified: each arm is checked sequentially
    for (size_t arm_idx = 0; arm_idx < when.arms.size(); ++arm_idx) {
        const auto& arm = when.arms[arm_idx];
        std::string next_label = (arm_idx + 1 < when.arms.size())
            ? fresh_label("when.check" + std::to_string(arm_idx + 1))
            : label_end;

        // Check if pattern matches
        if (arm.pattern->is<parser::EnumPattern>()) {
            const auto& enum_pat = arm.pattern->as<parser::EnumPattern>();
            std::string variant_name = enum_pat.path.segments.back();

            // Find variant index in enum definition
            int variant_tag = -1;
            for (const auto& [enum_name, enum_def] : env_.all_enums()) {
                for (size_t v_idx = 0; v_idx < enum_def.variants.size(); ++v_idx) {
                    if (enum_def.variants[v_idx].first == variant_name) {
                        variant_tag = static_cast<int>(v_idx);
                        break;
                    }
                }
                if (variant_tag >= 0) break;
            }

            if (variant_tag >= 0) {
                // Compare tag
                std::string cmp = fresh_reg();
                emit_line("  " + cmp + " = icmp eq i32 " + tag + ", " + std::to_string(variant_tag));
                emit_line("  br i1 " + cmp + ", label %" + arm_labels[arm_idx] + ", label %" + next_label);
            } else {
                // Unknown variant, skip to next
                emit_line("  br label %" + next_label);
            }
        } else {
            // Wildcard or other pattern - always matches
            emit_line("  br label %" + arm_labels[arm_idx]);
        }

        // Generate arm body
        emit_line(arm_labels[arm_idx] + ":");
        block_terminated_ = false;

        // Bind pattern variables
        if (arm.pattern->is<parser::EnumPattern>()) {
            const auto& enum_pat = arm.pattern->as<parser::EnumPattern>();

            if (enum_pat.payload.has_value() && !enum_pat.payload->empty()) {
                // Extract payload value
                std::string payload_ptr = fresh_reg();
                emit_line("  " + payload_ptr + " = getelementptr inbounds " + scrutinee_type + ", ptr " + scrutinee_ptr + ", i32 0, i32 1");
                std::string payload = fresh_reg();
                emit_line("  " + payload + " = load i64, ptr " + payload_ptr);

                // Bind to first pattern variable (simplified - only handles single payload)
                if (enum_pat.payload->at(0)->is<parser::IdentPattern>()) {
                    const auto& ident = enum_pat.payload->at(0)->as<parser::IdentPattern>();

                    // Convert i64 back to i32 if needed
                    std::string bound_val = payload;
                    std::string bound_type = "i64";

                    // For now, assume payloads are i32 and convert
                    std::string converted = fresh_reg();
                    emit_line("  " + converted + " = trunc i64 " + payload + " to i32");
                    bound_val = converted;
                    bound_type = "i32";

                    // Allocate and store bound variable
                    std::string var_alloca = fresh_reg();
                    emit_line("  " + var_alloca + " = alloca " + bound_type);
                    emit_line("  store " + bound_type + " " + bound_val + ", ptr " + var_alloca);
                    locals_[ident.name] = VarInfo{var_alloca, bound_type};
                }
            }
        }

        // Execute arm body
        gen_expr(*arm.body);
        if (!block_terminated_) {
            emit_line("  br label %" + label_end);
        }

        // Next check label (if not last arm)
        if (arm_idx + 1 < when.arms.size()) {
            emit_line(next_label + ":");
            block_terminated_ = false;
        }
    }

    // End label
    emit_line(label_end + ":");
    block_terminated_ = false;

    return "0";
}

} // namespace tml::codegen
