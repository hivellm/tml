// LLVM IR generator - Control flow
// Handles: if, block, loop, while, for, return

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

// Forward declaration for is_bool_expr (defined in llvm_ir_gen_stmt.cpp)
bool is_bool_expr(const parser::Expr& expr,
                  const std::unordered_map<std::string, LLVMIRGen::VarInfo>& locals);

auto LLVMIRGen::gen_if(const parser::IfExpr& if_expr) -> std::string {
    std::string cond = gen_expr(*if_expr.condition);

    // If condition is not already i1 (bool), convert it
    // Use last_expr_type_ which is set by gen_expr for accurate type info
    if (last_expr_type_ != "i1") {
        // Condition is not i1, convert to i1 by comparing with 0
        std::string bool_cond = fresh_reg();
        emit_line("  " + bool_cond + " = icmp ne i32 " + cond + ", 0");
        cond = bool_cond;
    }

    std::string label_then = fresh_label("if.then");
    std::string label_else = fresh_label("if.else");
    std::string label_end = fresh_label("if.end");

    // Check if branches have trailing expressions (i.e., return values)
    // If not, they return Unit and we should not generate a phi node
    bool then_has_value = false;
    bool else_has_value = false;

    if (if_expr.then_branch->is<parser::BlockExpr>()) {
        const auto& block = if_expr.then_branch->as<parser::BlockExpr>();
        then_has_value = block.expr.has_value();
    } else {
        // Non-block expressions always have values
        then_has_value = true;
    }

    if (if_expr.else_branch.has_value()) {
        if (if_expr.else_branch.value()->is<parser::BlockExpr>()) {
            const auto& block = if_expr.else_branch.value()->as<parser::BlockExpr>();
            else_has_value = block.expr.has_value();
        } else {
            else_has_value = true;
        }
    }

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
    if (!block_terminated_) {
        emit_line("  br label %" + label_end);
    }

    // Else block
    std::string else_val = "0";
    std::string else_type = "i32";
    bool else_terminated = false;
    if (if_expr.else_branch.has_value()) {
        emit_line(label_else + ":");
        block_terminated_ = false;
        else_val = gen_expr(*if_expr.else_branch.value());
        else_type = last_expr_type_;
        else_terminated = block_terminated_;
        if (!block_terminated_) {
            emit_line("  br label %" + label_end);
        }
    }

    // If both branches are terminated (by return/break/continue), don't emit end block
    // This can happen with nested if-else where all paths return
    if (then_terminated && else_terminated) {
        block_terminated_ = true; // Mark the overall if as terminated
        last_expr_type_ = "void";
        return "0";
    }

    // End block
    emit_line(label_end + ":");
    block_terminated_ = false;

    // Only generate phi if BOTH branches have trailing expressions (return values)
    // AND neither is terminated (by return/break/continue)
    if (if_expr.else_branch.has_value() && then_has_value && else_has_value && !then_terminated &&
        !else_terminated) {

        // Ensure types match for phi node
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + then_type + " [ " + then_val + ", %" + label_then +
                  " ], [ " + else_val + ", %" + label_else + " ]");
        last_expr_type_ = then_type;
        return result;
    }

    last_expr_type_ = "void";
    return "0";
}

auto LLVMIRGen::gen_ternary(const parser::TernaryExpr& ternary) -> std::string {
    // Evaluate condition
    std::string cond = gen_expr(*ternary.condition);

    // Convert condition to i1 if needed
    if (last_expr_type_ != "i1") {
        std::string bool_cond = fresh_reg();
        emit_line("  " + bool_cond + " = icmp ne i32 " + cond + ", 0");
        cond = bool_cond;
    }

    // Allocate temporary for result (use i32 initially, will work for most types)
    std::string result_ptr = fresh_reg();
    emit_line("  " + result_ptr + " = alloca i32");

    std::string label_true = fresh_label("ternary.true");
    std::string label_false = fresh_label("ternary.false");
    std::string label_end = fresh_label("ternary.end");

    // Branch on condition
    emit_line("  br i1 " + cond + ", label %" + label_true + ", label %" + label_false);

    // True branch
    emit_line(label_true + ":");
    block_terminated_ = false;
    std::string true_val = gen_expr(*ternary.true_value);
    std::string true_type = last_expr_type_;
    // Store result
    if (!block_terminated_) {
        // Convert i1 to i32 if needed
        if (true_type == "i1") {
            std::string converted = fresh_reg();
            emit_line("  " + converted + " = zext i1 " + true_val + " to i32");
            emit_line("  store i32 " + converted + ", ptr " + result_ptr);
        } else {
            emit_line("  store " + true_type + " " + true_val + ", ptr " + result_ptr);
        }
        emit_line("  br label %" + label_end);
    }

    // False branch
    emit_line(label_false + ":");
    block_terminated_ = false;
    std::string false_val = gen_expr(*ternary.false_value);
    std::string false_type = last_expr_type_;
    // Store result
    if (!block_terminated_) {
        // Convert i1 to i32 if needed
        if (false_type == "i1") {
            std::string converted = fresh_reg();
            emit_line("  " + converted + " = zext i1 " + false_val + " to i32");
            emit_line("  store i32 " + converted + ", ptr " + result_ptr);
        } else {
            emit_line("  store " + false_type + " " + false_val + ", ptr " + result_ptr);
        }
        emit_line("  br label %" + label_end);
    }

    // End block - load result
    emit_line(label_end + ":");
    block_terminated_ = false;

    std::string result = fresh_reg();
    std::string result_type = (true_type == "i1") ? "i32" : true_type;
    emit_line("  " + result + " = load " + result_type + ", ptr " + result_ptr);
    last_expr_type_ = result_type;
    return result;
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
        emit_line("  " + tag_ptr + " = getelementptr inbounds " + scrutinee_type + ", ptr " +
                  scrutinee_ptr + ", i32 0, i32 0");
        std::string tag = fresh_reg();
        emit_line("  " + tag + " = load i32, ptr " + tag_ptr);

        // Find variant index
        // First, try using scrutinee type for generic enum lookup
        int variant_tag = -1;
        std::string scrutinee_enum_name;
        if (scrutinee_type.starts_with("%struct.")) {
            scrutinee_enum_name = scrutinee_type.substr(8); // Remove "%struct."
        }

        // Try lookup with scrutinee-derived enum name (for generic enums)
        if (!scrutinee_enum_name.empty()) {
            std::string key = scrutinee_enum_name + "::" + variant_name;
            auto it = enum_variants_.find(key);
            if (it != enum_variants_.end()) {
                variant_tag = it->second;
            }
        }

        // Fallback: try non-generic enums from type environment
        if (variant_tag < 0) {
            for (const auto& [enum_name, enum_def] : env_.all_enums()) {
                for (size_t v_idx = 0; v_idx < enum_def.variants.size(); ++v_idx) {
                    if (enum_def.variants[v_idx].first == variant_name) {
                        variant_tag = static_cast<int>(v_idx);
                        break;
                    }
                }
                if (variant_tag >= 0)
                    break;
            }
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
            emit_line("  " + payload_ptr + " = getelementptr inbounds " + scrutinee_type +
                      ", ptr " + scrutinee_ptr + ", i32 0, i32 1");
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
    if (last_expr_type_ != "i1") {
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
    // And collection iteration: for item in list { ... }

    std::string label_init = fresh_label("for.init");
    std::string label_cond = fresh_label("for.cond");
    std::string label_body = fresh_label("for.body");
    std::string label_incr = fresh_label("for.incr");
    std::string label_end = fresh_label("for.end");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    current_loop_start_ = label_incr; // continue goes to increment
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
    std::string range_type = "i32"; // Default type for range
    bool is_collection_iter = false;
    std::string collection_ptr;
    std::string idx_var_name = "_for_collection_idx";

    if (for_expr.iter->is<parser::RangeExpr>()) {
        const auto& range = for_expr.iter->as<parser::RangeExpr>();
        inclusive = range.inclusive;
        if (range.start.has_value()) {
            range_start = gen_expr(*range.start.value());
        }
        if (range.end.has_value()) {
            range_end = gen_expr(*range.end.value());
            range_type = last_expr_type_; // Use type of end value
        }
    } else {
        // Check if it's a collection (List, HashMap, Buffer)
        std::string iter_val = gen_expr(*for_expr.iter);
        std::string iter_type = last_expr_type_;

        // Check if iter_type is ptr (collections are pointers)
        if (iter_type == "ptr") {
            is_collection_iter = true;

            // Store collection pointer in an alloca so we can use it in the loop body
            std::string collection_alloca = fresh_reg();
            emit_line("  " + collection_alloca + " = alloca ptr");
            emit_line("  store ptr " + iter_val + ", ptr " + collection_alloca);

            // Load it back to call _len
            std::string collection_loaded = fresh_reg();
            emit_line("  " + collection_loaded + " = load ptr, ptr " + collection_alloca);
            collection_ptr = collection_alloca; // Store the alloca, not the value

            // Call the appropriate _len function to get collection size
            std::string len_result = fresh_reg();
            emit_line("  " + len_result + " = call i64 @list_len(ptr " + collection_loaded + ")");

            // Convert i64 to i32 for loop counter
            std::string len_i32 = fresh_reg();
            emit_line("  " + len_i32 + " = trunc i64 " + len_result + " to i32");
            range_end = len_i32;
            range_type = "i32";
        } else {
            // Fallback: treat as simple range 0 to iter
            range_end = iter_val;
            range_type = iter_type;
        }
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
        emit_line("  " + cmp_result + " = icmp sle " + range_type + " " + current + ", " +
                  range_end);
    } else {
        emit_line("  " + cmp_result + " = icmp slt " + range_type + " " + current + ", " +
                  range_end);
    }
    emit_line("  br i1 " + cmp_result + ", label %" + label_body + ", label %" + label_end);

    // Body block
    emit_line(label_body + ":");
    block_terminated_ = false;

    // If iterating over a collection, get the element and bind it to the loop variable
    if (is_collection_iter) {
        // Load the collection pointer
        std::string collection_loaded = fresh_reg();
        emit_line("  " + collection_loaded + " = load ptr, ptr " + collection_ptr);

        // Get current index
        std::string idx = fresh_reg();
        emit_line("  " + idx + " = load " + range_type + ", ptr " + var_alloca);

        // Convert i32 index to i64 for list_get call
        std::string idx_i64 = fresh_reg();
        emit_line("  " + idx_i64 + " = sext i32 " + idx + " to i64");

        // Call list_get(collection, index)
        std::string element = fresh_reg();
        emit_line("  " + element + " = call i64 @list_get(ptr " + collection_loaded + ", i64 " +
                  idx_i64 + ")");

        // Convert i64 result to i32 and store in actual loop variable
        std::string element_i32 = fresh_reg();
        emit_line("  " + element_i32 + " = trunc i64 " + element + " to i32");

        std::string element_alloca = fresh_reg();
        emit_line("  " + element_alloca + " = alloca i32");
        emit_line("  store i32 " + element_i32 + ", ptr " + element_alloca);
        locals_[var_name] = VarInfo{element_alloca, "i32", nullptr};
    }

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
    emit_line("  " + tag_ptr + " = getelementptr inbounds " + scrutinee_type + ", ptr " +
              scrutinee_ptr + ", i32 0, i32 0");
    std::string tag = fresh_reg();
    emit_line("  " + tag + " = load i32, ptr " + tag_ptr);

    // Generate labels for each arm + end
    std::vector<std::string> arm_labels;
    for (size_t i = 0; i < when.arms.size(); ++i) {
        arm_labels.push_back(fresh_label("when_arm"));
    }
    std::string label_end = fresh_label("when_end");

    // Allocate temporary for result
    std::string result_ptr = fresh_reg();
    std::string result_type = "i32"; // Will be updated by first arm
    emit_line("  " + result_ptr + " = alloca i32");

    // Generate switch based on pattern
    // For now, simplified: each arm is checked sequentially
    for (size_t arm_idx = 0; arm_idx < when.arms.size(); ++arm_idx) {
        const auto& arm = when.arms[arm_idx];
        std::string next_label =
            (arm_idx + 1 < when.arms.size()) ? fresh_label("when_next") : label_end;

        // Check if pattern matches
        if (arm.pattern->is<parser::EnumPattern>()) {
            const auto& enum_pat = arm.pattern->as<parser::EnumPattern>();
            std::string enum_name;
            std::string variant_name;

            // Extract enum name and variant name from path
            if (enum_pat.path.segments.size() >= 2) {
                enum_name = enum_pat.path.segments[enum_pat.path.segments.size() - 2];
                variant_name = enum_pat.path.segments.back();
            } else if (enum_pat.path.segments.size() == 1) {
                variant_name = enum_pat.path.segments[0];
            }

            // Find variant index in the correct enum
            // First, try to extract enum name from scrutinee type (e.g., %struct.Maybe__I32 ->
            // Maybe__I32)
            int variant_tag = -1;
            std::string scrutinee_enum_name;
            if (scrutinee_type.starts_with("%struct.")) {
                scrutinee_enum_name = scrutinee_type.substr(8); // Remove "%struct."
            }

            // Try lookup with scrutinee-derived enum name (for generic enums)
            if (!scrutinee_enum_name.empty()) {
                std::string key = scrutinee_enum_name + "::" + variant_name;
                auto it = enum_variants_.find(key);
                if (it != enum_variants_.end()) {
                    variant_tag = it->second;
                }
            }

            // Fallback: Try full path from pattern segments
            if (variant_tag < 0) {
                std::string full_path;
                for (size_t i = 0; i < enum_pat.path.segments.size(); ++i) {
                    if (i > 0)
                        full_path += "::";
                    full_path += enum_pat.path.segments[i];
                }

                auto it = enum_variants_.find(full_path);
                if (it != enum_variants_.end()) {
                    variant_tag = it->second;
                }
            }

            if (variant_tag >= 0) {
                // Compare tag
                std::string cmp = fresh_reg();
                emit_line("  " + cmp + " = icmp eq i32 " + tag + ", " +
                          std::to_string(variant_tag));
                emit_line("  br i1 " + cmp + ", label %" + arm_labels[arm_idx] + ", label %" +
                          next_label);
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
                emit_line("  " + payload_ptr + " = getelementptr inbounds " + scrutinee_type +
                          ", ptr " + scrutinee_ptr + ", i32 0, i32 1");
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
        std::string arm_value = gen_expr(*arm.body);
        std::string arm_type = last_expr_type_;

        // Update result_type from first arm
        if (arm_idx == 0) {
            result_type = arm_type;
        }

        // Store arm value to result (with type conversion if needed)
        // Don't store void types - they don't produce values
        if (!block_terminated_ && arm_type != "void") {
            std::string store_value = arm_value;
            std::string store_type = arm_type;

            // Convert i1 to i32 for storage compatibility
            if (arm_type == "i1") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = zext i1 " + arm_value + " to i32");
                store_value = converted;
                store_type = "i32";
            }

            emit_line("  store " + store_type + " " + store_value + ", ptr " + result_ptr);
            emit_line("  br label %" + label_end);
        } else if (!block_terminated_) {
            // Void arm - just branch to end
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

    // If result type is void, don't load anything
    if (result_type == "void") {
        last_expr_type_ = "void";
        return "0";
    }

    // Load result (and convert back if needed)
    std::string result = fresh_reg();
    if (result_type == "i1") {
        // i1 was stored as i32, load as i32 and convert back
        std::string loaded_i32 = fresh_reg();
        emit_line("  " + loaded_i32 + " = load i32, ptr " + result_ptr);
        emit_line("  " + result + " = trunc i32 " + loaded_i32 + " to i1");
    } else {
        emit_line("  " + result + " = load " + result_type + ", ptr " + result_ptr);
    }
    last_expr_type_ = result_type;
    return result;
}

} // namespace tml::codegen
