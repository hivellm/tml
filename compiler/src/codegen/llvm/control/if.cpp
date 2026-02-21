//! # LLVM IR Generator - If/Ternary Control Flow
//!
//! This file implements if, ternary, and if-let expression code generation.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_if(const parser::IfExpr& if_expr) -> std::string {
    std::string cond = gen_expr(*if_expr.condition);

    // If condition is not already i1 (bool), convert it
    // Use last_expr_type_ which is set by gen_expr for accurate type info
    if (last_expr_type_ != "i1") {
        // Condition is not i1, convert to i1 by comparing with 0
        std::string cond_type = last_expr_type_.empty() ? "i32" : last_expr_type_;
        std::string bool_cond = fresh_reg();
        emit_line("  " + bool_cond + " = icmp ne " + cond_type + " " + cond + ", 0");
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
    current_block_ = label_then;
    block_terminated_ = false;
    size_t temps_before_then = temp_drops_.size();
    std::string then_val = gen_expr(*if_expr.then_branch);
    std::string then_type = last_expr_type_;
    bool then_terminated = block_terminated_;
    std::string then_end_block = current_block_; // Track actual block that flows to end
    if (!block_terminated_) {
        // Drop Str temps created within this branch before branching to merge
        if (temp_drops_.size() > temps_before_then) {
            for (auto it = temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_then);
                 it != temp_drops_.end(); ++it) {
                if (it->is_heap_str) {
                    emit_drop_call(*it);
                }
            }
            temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_then),
                              temp_drops_.end());
        }
        emit_line("  br label %" + label_end);
    } else {
        // Branch terminated (return/break) — discard temps without emitting drops
        // (the return/break path handles its own cleanup)
        if (temp_drops_.size() > temps_before_then) {
            temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_then),
                              temp_drops_.end());
        }
    }

    // Else block
    std::string else_val = "0";
    std::string else_type = "i32";
    bool else_terminated = false;
    std::string else_end_block = label_else; // Track actual block that flows to end
    if (if_expr.else_branch.has_value()) {
        emit_line(label_else + ":");
        current_block_ = label_else;
        block_terminated_ = false;
        size_t temps_before_else = temp_drops_.size();
        else_val = gen_expr(*if_expr.else_branch.value());
        else_type = last_expr_type_;
        else_terminated = block_terminated_;
        else_end_block = current_block_; // This may differ from label_else if nested if
        if (!block_terminated_) {
            // Drop Str temps created within this branch before branching to merge
            if (temp_drops_.size() > temps_before_else) {
                for (auto it = temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_else);
                     it != temp_drops_.end(); ++it) {
                    if (it->is_heap_str) {
                        emit_drop_call(*it);
                    }
                }
                temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_else),
                                  temp_drops_.end());
            }
            emit_line("  br label %" + label_end);
        } else {
            if (temp_drops_.size() > temps_before_else) {
                temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_else),
                                  temp_drops_.end());
            }
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
    current_block_ = label_end;
    block_terminated_ = false;

    // Only generate phi if BOTH branches have trailing expressions (return values)
    // AND neither is terminated (by return/break/continue)
    // AND both branches produce the same type (type mismatch means we can't merge values)
    if (if_expr.else_branch.has_value() && then_has_value && else_has_value && !then_terminated &&
        !else_terminated && then_type == else_type && then_type != "void") {

        // Use actual end blocks as PHI predecessors, not the original labels
        // This is critical for nested if-else expressions where control flow
        // may pass through multiple blocks before reaching the end
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + then_type + " [ " + then_val + ", %" +
                  then_end_block + " ], [ " + else_val + ", %" + else_end_block + " ]");
        last_expr_type_ = then_type;
        return result;
    }

    last_expr_type_ = "void";
    return "0";
}

auto LLVMIRGen::gen_ternary(const parser::TernaryExpr& ternary) -> std::string {
    // Pre-infer the result type so we allocate the correct size.
    // Without this, struct-typed ternaries (e.g., Maybe[Str]) write 16+ bytes
    // into a 4-byte alloca, corrupting the stack.
    std::string alloca_type = "i32"; // default for scalar types
    {
        auto semantic_type = infer_expr_type(*ternary.true_value);
        if (semantic_type) {
            std::string inferred = llvm_type_from_semantic(semantic_type);
            if (!inferred.empty() && inferred != "void") {
                alloca_type = inferred;
            }
        }
    }

    // Evaluate condition
    std::string cond = gen_expr(*ternary.condition);

    // Convert condition to i1 if needed
    if (last_expr_type_ != "i1") {
        std::string cond_type = last_expr_type_.empty() ? "i32" : last_expr_type_;
        std::string bool_cond = fresh_reg();
        emit_line("  " + bool_cond + " = icmp ne " + cond_type + " " + cond + ", 0");
        cond = bool_cond;
    }

    // Allocate temporary for result with the correct type size
    std::string result_ptr = fresh_reg();
    emit_line("  " + result_ptr + " = alloca " + alloca_type);

    std::string label_true = fresh_label("ternary.true");
    std::string label_false = fresh_label("ternary.false");
    std::string label_end = fresh_label("ternary.end");

    // Branch on condition
    emit_line("  br i1 " + cond + ", label %" + label_true + ", label %" + label_false);

    // True branch
    emit_line(label_true + ":");
    block_terminated_ = false;
    size_t temps_before_true = temp_drops_.size();
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
        // Drop Str temps created within this branch
        if (temp_drops_.size() > temps_before_true) {
            for (auto it = temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_true);
                 it != temp_drops_.end(); ++it) {
                if (it->is_heap_str) {
                    emit_drop_call(*it);
                }
            }
            temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_true),
                              temp_drops_.end());
        }
        emit_line("  br label %" + label_end);
    } else {
        if (temp_drops_.size() > temps_before_true) {
            temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_true),
                              temp_drops_.end());
        }
    }

    // False branch
    emit_line(label_false + ":");
    block_terminated_ = false;
    size_t temps_before_false = temp_drops_.size();
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
        // Drop Str temps created within this branch
        if (temp_drops_.size() > temps_before_false) {
            for (auto it = temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_false);
                 it != temp_drops_.end(); ++it) {
                if (it->is_heap_str) {
                    emit_drop_call(*it);
                }
            }
            temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_false),
                              temp_drops_.end());
        }
        emit_line("  br label %" + label_end);
    } else {
        if (temp_drops_.size() > temps_before_false) {
            temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_false),
                              temp_drops_.end());
        }
    }

    // End block - load result
    emit_line(label_end + ":");
    current_block_ = label_end;
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

    // If scrutinee_type is ptr, infer the actual struct type from the expression
    if (scrutinee_type == "ptr") {
        types::TypePtr semantic_type = infer_expr_type(*if_let.scrutinee);
        if (semantic_type) {
            scrutinee_type = llvm_type_from_semantic(semantic_type);
        }
    }

    std::string label_then = fresh_label("iflet.then");
    std::string label_else = fresh_label("iflet.else");
    std::string label_end = fresh_label("iflet.end");

    // Check if pattern matches and bind variables
    // For enum patterns, check the tag
    if (if_let.pattern->is<parser::EnumPattern>()) {
        const auto& enum_pat = if_let.pattern->as<parser::EnumPattern>();
        std::string variant_name = enum_pat.path.segments.back();

        // Get pointer to scrutinee (either use directly if already ptr, or alloca/store)
        std::string scrutinee_ptr;
        bool was_ptr = (last_expr_type_ == "ptr");
        if (was_ptr) {
            // scrutinee is already a pointer, use it directly
            scrutinee_ptr = scrutinee;
        } else {
            // Allocate space for scrutinee
            scrutinee_ptr = fresh_reg();
            emit_line("  " + scrutinee_ptr + " = alloca " + scrutinee_type);
            emit_line("  store " + scrutinee_type + " " + scrutinee + ", ptr " + scrutinee_ptr);
        }

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
                locals_[ident.name] = VarInfo{var_alloca, bound_type, nullptr, std::nullopt};
            }
        }
    } else {
        // For other patterns (wildcard, ident), always match
        emit_line("  br label %" + label_then);
        emit_line(label_then + ":");
        block_terminated_ = false;
    }

    // Execute then branch
    size_t temps_before_then2 = temp_drops_.size();
    std::string then_val = gen_expr(*if_let.then_branch);
    if (!block_terminated_) {
        if (temp_drops_.size() > temps_before_then2) {
            for (auto it = temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_then2);
                 it != temp_drops_.end(); ++it) {
                if (it->is_heap_str) {
                    emit_drop_call(*it);
                }
            }
            temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_then2),
                              temp_drops_.end());
        }
        emit_line("  br label %" + label_end);
    } else {
        if (temp_drops_.size() > temps_before_then2) {
            temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_then2),
                              temp_drops_.end());
        }
    }

    // Else block
    if (if_let.else_branch.has_value()) {
        emit_line(label_else + ":");
        block_terminated_ = false;
        size_t temps_before_else2 = temp_drops_.size();
        std::string else_val = gen_expr(*if_let.else_branch.value());
        if (!block_terminated_) {
            if (temp_drops_.size() > temps_before_else2) {
                for (auto it = temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_else2);
                     it != temp_drops_.end(); ++it) {
                    if (it->is_heap_str) {
                        emit_drop_call(*it);
                    }
                }
                temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_else2),
                                  temp_drops_.end());
            }
            emit_line("  br label %" + label_end);
        } else {
            if (temp_drops_.size() > temps_before_else2) {
                temp_drops_.erase(temp_drops_.begin() + static_cast<ptrdiff_t>(temps_before_else2),
                                  temp_drops_.end());
            }
        }
    }

    // End block
    emit_line(label_end + ":");
    current_block_ = label_end;
    block_terminated_ = false;

    return "0";
}

} // namespace tml::codegen
