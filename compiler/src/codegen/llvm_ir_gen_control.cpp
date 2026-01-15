//! # LLVM IR Generator - Control Flow
//!
//! This file implements control flow expression code generation.
//!
//! ## Control Flow Structures
//!
//! | Expression | Handler       | LLVM Pattern                    |
//! |------------|---------------|---------------------------------|
//! | `if`       | `gen_if`      | br + phi for value-returning    |
//! | `ternary`  | `gen_ternary` | br + phi (always value-returning)|
//! | `if let`   | `gen_if_let`  | Pattern match with branch       |
//! | `when`     | `gen_when`    | Switch or cascading br          |
//! | `block`    | `gen_block`   | Sequential statements           |
//! | `loop`     | `gen_loop`    | Infinite loop with br           |
//! | `while`    | `gen_while`   | Conditional loop                |
//! | `for`      | `gen_for`     | Iterator loop                   |
//! | `return`   | `gen_return`  | ret instruction                 |
//!
//! ## Phi Nodes
//!
//! When control flow merges with values (e.g., if-else expression),
//! LLVM phi nodes select the correct value based on predecessor block.
//!
//! ## Loop Labels
//!
//! `current_loop_start_` and `current_loop_end_` track loop boundaries
//! for break/continue generation.

#include "codegen/llvm_ir_gen.hpp"

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
    current_block_ = label_then;
    block_terminated_ = false;
    std::string then_val = gen_expr(*if_expr.then_branch);
    std::string then_type = last_expr_type_;
    bool then_terminated = block_terminated_;
    std::string then_end_block = current_block_; // Track actual block that flows to end
    if (!block_terminated_) {
        emit_line("  br label %" + label_end);
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
        else_val = gen_expr(*if_expr.else_branch.value());
        else_type = last_expr_type_;
        else_terminated = block_terminated_;
        else_end_block = current_block_; // This may differ from label_else if nested if
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
    current_block_ = label_end;
    block_terminated_ = false;

    // Only generate phi if BOTH branches have trailing expressions (return values)
    // AND neither is terminated (by return/break/continue)
    if (if_expr.else_branch.has_value() && then_has_value && else_has_value && !then_terminated &&
        !else_terminated) {

        // Ensure types match for phi node
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
    current_block_ = label_end;
    block_terminated_ = false;

    return "0";
}

auto LLVMIRGen::gen_block(const parser::BlockExpr& block) -> std::string {
    std::string result = "0";

    // Push a new drop scope for this block
    push_drop_scope();

    for (const auto& stmt : block.stmts) {
        if (block_terminated_) {
            // Block already terminated, skip remaining statements
            // Don't emit drops here - they were emitted by return/break/continue
            break;
        }
        gen_stmt(*stmt);
    }

    if (block.expr.has_value() && !block_terminated_) {
        result = gen_expr(*block.expr.value());
    } else {
        // Block has no trailing expression - it returns Unit (void)
        last_expr_type_ = "void";
    }

    // Emit drops for variables in this scope before exiting
    if (!block_terminated_) {
        emit_scope_drops();
    }

    pop_drop_scope();

    return result;
}

auto LLVMIRGen::gen_loop(const parser::LoopExpr& loop) -> std::string {
    std::string label_start = fresh_label("loop.start");
    std::string label_body = fresh_label("loop.body");
    std::string label_end = fresh_label("loop.end");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    std::string saved_loop_stack_save = current_loop_stack_save_;
    current_loop_start_ = label_start;
    current_loop_end_ = label_end;

    emit_line("  br label %" + label_start);
    emit_line(label_start + ":");
    block_terminated_ = false;

    // Save stack at start of each iteration to reclaim alloca space
    std::string stack_save = fresh_reg();
    emit_line("  " + stack_save + " = call ptr @llvm.stacksave()");
    current_loop_stack_save_ = stack_save;

    gen_expr(*loop.body);

    if (!block_terminated_) {
        // Restore stack before looping back to reclaim allocas from this iteration
        emit_line("  call void @llvm.stackrestore(ptr " + stack_save + ")");
        emit_line("  br label %" + label_start);
    }

    emit_line(label_end + ":");
    current_block_ = label_end;
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;
    current_loop_stack_save_ = saved_loop_stack_save;

    return "0";
}

auto LLVMIRGen::gen_while(const parser::WhileExpr& while_expr) -> std::string {
    std::string label_cond = fresh_label("while.cond");
    std::string label_body = fresh_label("while.body");
    std::string label_end = fresh_label("while.end");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    std::string saved_loop_stack_save = current_loop_stack_save_;
    current_loop_start_ = label_cond;
    current_loop_end_ = label_end;

    // Jump to condition
    emit_line("  br label %" + label_cond);

    // Condition block
    emit_line(label_cond + ":");
    block_terminated_ = false;

    // Save stack at start of each iteration to reclaim alloca space
    std::string stack_save = fresh_reg();
    emit_line("  " + stack_save + " = call ptr @llvm.stacksave()");
    current_loop_stack_save_ = stack_save;

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
        // Restore stack before looping back to reclaim allocas from this iteration
        emit_line("  call void @llvm.stackrestore(ptr " + stack_save + ")");
        emit_line("  br label %" + label_cond);
    }

    // End block
    emit_line(label_end + ":");
    current_block_ = label_end;
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;
    current_loop_stack_save_ = saved_loop_stack_save;

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
    std::string saved_loop_stack_save = current_loop_stack_save_;
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
    locals_[var_name] = VarInfo{var_alloca, range_type, nullptr, std::nullopt};

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

    // Save stack at start of each iteration to reclaim alloca space
    std::string stack_save = fresh_reg();
    emit_line("  " + stack_save + " = call ptr @llvm.stacksave()");
    current_loop_stack_save_ = stack_save;

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
        locals_[var_name] = VarInfo{element_alloca, "i32", nullptr, std::nullopt};
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
    // Restore stack before looping back to reclaim allocas from this iteration
    emit_line("  call void @llvm.stackrestore(ptr " + stack_save + ")");
    emit_line("  br label %" + label_cond);

    // End block
    emit_line(label_end + ":");
    current_block_ = label_end;
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;
    current_loop_stack_save_ = saved_loop_stack_save;

    return "0";
}

// Helper to parse tuple type string into element types
static std::vector<std::string> parse_tuple_types_for_coercion(const std::string& tuple_type) {
    std::vector<std::string> element_types;
    if (tuple_type.size() > 2 && tuple_type.front() == '{' && tuple_type.back() == '}') {
        // Parse "{ i32, i64, ptr }" -> ["i32", "i64", "ptr"]
        std::string inner = tuple_type.substr(2, tuple_type.size() - 4);
        int brace_depth = 0;
        int bracket_depth = 0;
        std::string current;

        for (size_t i = 0; i < inner.size(); ++i) {
            char c = inner[i];
            if (c == '{') {
                brace_depth++;
                current += c;
            } else if (c == '}') {
                brace_depth--;
                current += c;
            } else if (c == '[') {
                bracket_depth++;
                current += c;
            } else if (c == ']') {
                bracket_depth--;
                current += c;
            } else if (c == ',' && brace_depth == 0 && bracket_depth == 0) {
                size_t start = current.find_first_not_of(" ");
                size_t end = current.find_last_not_of(" ");
                if (start != std::string::npos) {
                    element_types.push_back(current.substr(start, end - start + 1));
                }
                current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            size_t start = current.find_first_not_of(" ");
            size_t end = current.find_last_not_of(" ");
            if (start != std::string::npos) {
                element_types.push_back(current.substr(start, end - start + 1));
            }
        }
    }
    return element_types;
}

auto LLVMIRGen::gen_return(const parser::ReturnExpr& ret) -> std::string {
    // Emit drops for all variables in all scopes before returning
    emit_all_drops();

    if (ret.value.has_value()) {
        std::string val = gen_expr(*ret.value.value());
        std::string val_type = last_expr_type_;

        // For async functions, wrap the return value in Poll.Ready
        if (current_func_is_async_ && !current_poll_type_.empty()) {
            std::string wrapped = wrap_in_poll_ready(val, val_type);
            emit_line("  ret " + current_poll_type_ + " " + wrapped);
        } else {
            // Check if we need tuple coercion (e.g., { i32, i32 } -> { i32, i64 })
            if (val_type != current_ret_type_ && val_type.front() == '{' &&
                current_ret_type_.front() == '{') {
                // Parse element types for both
                auto actual_elems = parse_tuple_types_for_coercion(val_type);
                auto expected_elems = parse_tuple_types_for_coercion(current_ret_type_);

                if (actual_elems.size() == expected_elems.size()) {
                    bool needs_conversion = false;
                    for (size_t i = 0; i < actual_elems.size(); ++i) {
                        if (actual_elems[i] != expected_elems[i]) {
                            needs_conversion = true;
                            break;
                        }
                    }

                    if (needs_conversion) {
                        // Store original tuple to memory
                        std::string src_ptr = fresh_reg();
                        emit_line("  " + src_ptr + " = alloca " + val_type);
                        emit_line("  store " + val_type + " " + val + ", ptr " + src_ptr);

                        // Allocate destination tuple
                        std::string dst_ptr = fresh_reg();
                        emit_line("  " + dst_ptr + " = alloca " + current_ret_type_);

                        // Convert each element
                        for (size_t i = 0; i < actual_elems.size(); ++i) {
                            std::string elem_ptr = fresh_reg();
                            emit_line("  " + elem_ptr + " = getelementptr inbounds " + val_type +
                                      ", ptr " + src_ptr + ", i32 0, i32 " + std::to_string(i));
                            std::string elem_val = fresh_reg();
                            emit_line("  " + elem_val + " = load " + actual_elems[i] + ", ptr " +
                                      elem_ptr);

                            std::string conv_val = elem_val;
                            if (actual_elems[i] != expected_elems[i]) {
                                conv_val = fresh_reg();
                                // Integer extension/truncation
                                if (expected_elems[i] == "i64" && actual_elems[i] == "i32") {
                                    emit_line("  " + conv_val + " = sext i32 " + elem_val +
                                              " to i64");
                                } else if (expected_elems[i] == "i32" && actual_elems[i] == "i64") {
                                    emit_line("  " + conv_val + " = trunc i64 " + elem_val +
                                              " to i32");
                                } else if (expected_elems[i] == "i64" && actual_elems[i] == "i16") {
                                    emit_line("  " + conv_val + " = sext i16 " + elem_val +
                                              " to i64");
                                } else if (expected_elems[i] == "i64" && actual_elems[i] == "i8") {
                                    emit_line("  " + conv_val + " = sext i8 " + elem_val +
                                              " to i64");
                                } else if (expected_elems[i] == "i32" && actual_elems[i] == "i16") {
                                    emit_line("  " + conv_val + " = sext i16 " + elem_val +
                                              " to i32");
                                } else if (expected_elems[i] == "i32" && actual_elems[i] == "i8") {
                                    emit_line("  " + conv_val + " = sext i8 " + elem_val +
                                              " to i32");
                                } else {
                                    // Same type or unhandled - just use original
                                    conv_val = elem_val;
                                }
                            }

                            std::string dst_elem_ptr = fresh_reg();
                            emit_line("  " + dst_elem_ptr + " = getelementptr inbounds " +
                                      current_ret_type_ + ", ptr " + dst_ptr + ", i32 0, i32 " +
                                      std::to_string(i));
                            emit_line("  store " + expected_elems[i] + " " + conv_val + ", ptr " +
                                      dst_elem_ptr);
                        }

                        // Load converted tuple
                        std::string result = fresh_reg();
                        emit_line("  " + result + " = load " + current_ret_type_ + ", ptr " +
                                  dst_ptr);
                        emit_line("  ret " + current_ret_type_ + " " + result);
                        block_terminated_ = true;
                        return "void";
                    }
                }
            }
            // Handle integer type extension when actual differs from expected
            std::string final_val = val;
            if (val_type != current_ret_type_) {
                // Integer extension: i32 -> i64, i16 -> i64, i8 -> i64
                if (current_ret_type_ == "i64" &&
                    (val_type == "i32" || val_type == "i16" || val_type == "i8")) {
                    std::string ext_reg = fresh_reg();
                    emit_line("  " + ext_reg + " = sext " + val_type + " " + val + " to i64");
                    final_val = ext_reg;
                } else if (current_ret_type_ == "i32" && (val_type == "i16" || val_type == "i8")) {
                    std::string ext_reg = fresh_reg();
                    emit_line("  " + ext_reg + " = sext " + val_type + " " + val + " to i32");
                    final_val = ext_reg;
                }
                // Integer truncation: larger -> smaller (for negative literals)
                else if (current_ret_type_ == "i8" && (val_type == "i32" || val_type == "i64")) {
                    std::string trunc_reg = fresh_reg();
                    emit_line("  " + trunc_reg + " = trunc " + val_type + " " + val + " to i8");
                    final_val = trunc_reg;
                } else if (current_ret_type_ == "i16" && (val_type == "i32" || val_type == "i64")) {
                    std::string trunc_reg = fresh_reg();
                    emit_line("  " + trunc_reg + " = trunc " + val_type + " " + val + " to i16");
                    final_val = trunc_reg;
                } else if (current_ret_type_ == "i32" && val_type == "i64") {
                    std::string trunc_reg = fresh_reg();
                    emit_line("  " + trunc_reg + " = trunc " + val_type + " " + val + " to i32");
                    final_val = trunc_reg;
                }
            }
            emit_line("  ret " + current_ret_type_ + " " + final_val);
        }
    } else {
        emit_line("  ret void");
    }
    block_terminated_ = true;
    return "void";
}

auto LLVMIRGen::gen_throw(const parser::ThrowExpr& thr) -> std::string {
    // Generate the expression being thrown (e.g., new Error("message"))
    std::string thrown_val = gen_expr(*thr.expr);
    std::string thrown_type = last_expr_type_;

    // If the thrown value is a pointer to an Error-like object with a 'message' field,
    // extract the message and pass it to panic
    std::string panic_msg = "null";

    if (thrown_type == "ptr" || thrown_type.starts_with("%class.") ||
        thrown_type.starts_with("%struct.")) {
        // Try to access a 'message' field at index 0 (common convention for Error types)
        // This handles `new Error("message")` where Error has a message field
        std::string msg_ptr = fresh_reg();
        std::string msg_val = fresh_reg();

        // Assume Error-like objects have message as first field (ptr to char)
        emit_line("  ; throw expression - extracting error message");
        emit_line("  " + msg_ptr + " = getelementptr inbounds ptr, ptr " + thrown_val + ", i32 0");
        emit_line("  " + msg_val + " = load ptr, ptr " + msg_ptr);
        panic_msg = msg_val;
    }

    // Call panic to terminate the program (panic is declared by emit_runtime_decls)
    // This integrates with @should_panic test infrastructure
    emit_line("  call void @panic(ptr " + panic_msg + ")");
    emit_line("  unreachable");

    block_terminated_ = true;
    return "void";
}

// Helper: generate comparison for a single pattern against scrutinee
// Returns the comparison result register, or empty string for always-match patterns
auto LLVMIRGen::gen_pattern_cmp(const parser::Pattern& pattern, const std::string& scrutinee,
                                const std::string& scrutinee_type, const std::string& tag,
                                bool is_primitive) -> std::string {
    if (pattern.is<parser::LiteralPattern>()) {
        const auto& lit_pat = pattern.as<parser::LiteralPattern>();
        std::string lit_val;

        // Get literal value based on token type
        if (lit_pat.literal.kind == lexer::TokenKind::IntLiteral) {
            // Convert to decimal for LLVM IR (handles 0x, 0b, 0o prefixes)
            lit_val = std::to_string(lit_pat.literal.int_value().value);
        } else if (lit_pat.literal.kind == lexer::TokenKind::BoolLiteral) {
            lit_val = lit_pat.literal.bool_value() ? "1" : "0";
        } else if (lit_pat.literal.kind == lexer::TokenKind::FloatLiteral) {
            lit_val = std::to_string(lit_pat.literal.float_value().value);
        } else {
            // Unsupported literal type
            return "";
        }

        std::string cmp = fresh_reg();
        if (scrutinee_type == "float" || scrutinee_type == "double") {
            emit_line("  " + cmp + " = fcmp oeq " + scrutinee_type + " " + scrutinee + ", " +
                      lit_val);
        } else {
            emit_line("  " + cmp + " = icmp eq " + scrutinee_type + " " + scrutinee + ", " +
                      lit_val);
        }
        return cmp;
    } else if (pattern.is<parser::EnumPattern>()) {
        const auto& enum_pat = pattern.as<parser::EnumPattern>();
        std::string variant_name;
        if (enum_pat.path.segments.size() >= 1) {
            variant_name = enum_pat.path.segments.back();
        }

        // Find variant tag
        int variant_tag = -1;
        std::string scrutinee_enum_name;
        if (scrutinee_type.starts_with("%struct.")) {
            scrutinee_enum_name = scrutinee_type.substr(8);
        }

        if (!scrutinee_enum_name.empty()) {
            std::string key = scrutinee_enum_name + "::" + variant_name;
            auto it = enum_variants_.find(key);
            if (it != enum_variants_.end()) {
                variant_tag = it->second;
            }
        }

        // Fallback: Try full path
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
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq i32 " + tag + ", " + std::to_string(variant_tag));
            return cmp;
        }
        return "";
    } else if (pattern.is<parser::IdentPattern>()) {
        const auto& ident_pat = pattern.as<parser::IdentPattern>();

        // For primitives, IdentPattern is a binding (always matches)
        if (is_primitive) {
            return "";
        }

        // For enums, check if it's a unit variant
        int variant_tag = -1;
        std::string scrutinee_enum_name;
        if (scrutinee_type.starts_with("%struct.")) {
            scrutinee_enum_name = scrutinee_type.substr(8);
        }

        if (!scrutinee_enum_name.empty()) {
            std::string key = scrutinee_enum_name + "::" + ident_pat.name;
            auto it = enum_variants_.find(key);
            if (it != enum_variants_.end()) {
                variant_tag = it->second;
            }
        }

        if (variant_tag >= 0) {
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq i32 " + tag + ", " + std::to_string(variant_tag));
            return cmp;
        }
        return ""; // Binding pattern - always matches
    } else if (pattern.is<parser::WildcardPattern>()) {
        return ""; // Always matches
    } else if (pattern.is<parser::RangePattern>()) {
        const auto& range_pat = pattern.as<parser::RangePattern>();

        // Generate comparisons for range bounds
        // Range pattern: start to end (exclusive) or start through end (inclusive)
        std::string cmp_start;
        std::string cmp_end;

        // Generate start comparison: scrutinee >= start
        if (range_pat.start.has_value()) {
            std::string start_val = gen_expr(*range_pat.start.value());
            std::string start_type = last_expr_type_;

            // Ensure types match
            if (start_type != scrutinee_type && !start_type.empty()) {
                // Try to convert if needed
                if ((scrutinee_type == "i64" && start_type == "i32") ||
                    (scrutinee_type == "i32" && start_type == "i64")) {
                    std::string conv = fresh_reg();
                    if (scrutinee_type == "i64" && start_type == "i32") {
                        emit_line("  " + conv + " = sext i32 " + start_val + " to i64");
                    } else {
                        emit_line("  " + conv + " = trunc i64 " + start_val + " to i32");
                    }
                    start_val = conv;
                }
            }

            cmp_start = fresh_reg();
            if (scrutinee_type == "float" || scrutinee_type == "double") {
                emit_line("  " + cmp_start + " = fcmp oge " + scrutinee_type + " " + scrutinee +
                          ", " + start_val);
            } else {
                emit_line("  " + cmp_start + " = icmp sge " + scrutinee_type + " " + scrutinee +
                          ", " + start_val);
            }
        }

        // Generate end comparison: scrutinee < end (exclusive) or scrutinee <= end (inclusive)
        if (range_pat.end.has_value()) {
            std::string end_val = gen_expr(*range_pat.end.value());
            std::string end_type = last_expr_type_;

            // Ensure types match
            if (end_type != scrutinee_type && !end_type.empty()) {
                if ((scrutinee_type == "i64" && end_type == "i32") ||
                    (scrutinee_type == "i32" && end_type == "i64")) {
                    std::string conv = fresh_reg();
                    if (scrutinee_type == "i64" && end_type == "i32") {
                        emit_line("  " + conv + " = sext i32 " + end_val + " to i64");
                    } else {
                        emit_line("  " + conv + " = trunc i64 " + end_val + " to i32");
                    }
                    end_val = conv;
                }
            }

            cmp_end = fresh_reg();
            if (scrutinee_type == "float" || scrutinee_type == "double") {
                if (range_pat.inclusive) {
                    emit_line("  " + cmp_end + " = fcmp ole " + scrutinee_type + " " + scrutinee +
                              ", " + end_val);
                } else {
                    emit_line("  " + cmp_end + " = fcmp olt " + scrutinee_type + " " + scrutinee +
                              ", " + end_val);
                }
            } else {
                if (range_pat.inclusive) {
                    emit_line("  " + cmp_end + " = icmp sle " + scrutinee_type + " " + scrutinee +
                              ", " + end_val);
                } else {
                    emit_line("  " + cmp_end + " = icmp slt " + scrutinee_type + " " + scrutinee +
                              ", " + end_val);
                }
            }
        }

        // Combine comparisons
        if (!cmp_start.empty() && !cmp_end.empty()) {
            std::string combined = fresh_reg();
            emit_line("  " + combined + " = and i1 " + cmp_start + ", " + cmp_end);
            return combined;
        } else if (!cmp_start.empty()) {
            return cmp_start; // Only lower bound
        } else if (!cmp_end.empty()) {
            return cmp_end; // Only upper bound
        }
        return ""; // Open range - always matches
    }
    return ""; // Default: always matches
}

auto LLVMIRGen::gen_when(const parser::WhenExpr& when) -> std::string {
    // Evaluate scrutinee
    std::string scrutinee = gen_expr(*when.scrutinee);
    std::string scrutinee_type = last_expr_type_;

    // If scrutinee_type is ptr, infer the actual struct type from the expression
    std::string scrutinee_ptr;
    if (scrutinee_type == "ptr") {
        types::TypePtr semantic_type = infer_expr_type(*when.scrutinee);
        if (semantic_type) {
            scrutinee_type = llvm_type_from_semantic(semantic_type);
        }
        // scrutinee is already a pointer, use it directly
        scrutinee_ptr = scrutinee;
    } else {
        // Allocate space for scrutinee and store the value
        scrutinee_ptr = fresh_reg();
        emit_line("  " + scrutinee_ptr + " = alloca " + scrutinee_type);
        emit_line("  store " + scrutinee_type + " " + scrutinee + ", ptr " + scrutinee_ptr);
    }

    // Check if scrutinee is a simple primitive type (not enum/struct)
    bool is_primitive_scrutinee =
        (scrutinee_type == "i8" || scrutinee_type == "i16" || scrutinee_type == "i32" ||
         scrutinee_type == "i64" || scrutinee_type == "i128" || scrutinee_type == "float" ||
         scrutinee_type == "double" || scrutinee_type == "i1");

    // For enums/structs, extract tag; for primitives, we'll compare directly
    std::string tag;
    if (!is_primitive_scrutinee) {
        // Extract tag (assumes enum is { i32, i64 })
        std::string tag_ptr = fresh_reg();
        emit_line("  " + tag_ptr + " = getelementptr inbounds " + scrutinee_type + ", ptr " +
                  scrutinee_ptr + ", i32 0, i32 0");
        tag = fresh_reg();
        emit_line("  " + tag + " = load i32, ptr " + tag_ptr);
    }

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
        // Handle OrPattern: generate comparisons for each sub-pattern and OR them together
        if (arm.pattern->is<parser::OrPattern>()) {
            const auto& or_pat = arm.pattern->as<parser::OrPattern>();
            std::vector<std::string> cmp_results;

            for (const auto& sub_pattern : or_pat.patterns) {
                std::string cmp = gen_pattern_cmp(*sub_pattern, scrutinee, scrutinee_type, tag,
                                                  is_primitive_scrutinee);
                if (!cmp.empty()) {
                    cmp_results.push_back(cmp);
                }
            }

            if (cmp_results.empty()) {
                // All patterns always match (all wildcards)
                emit_line("  br label %" + arm_labels[arm_idx]);
            } else if (cmp_results.size() == 1) {
                emit_line("  br i1 " + cmp_results[0] + ", label %" + arm_labels[arm_idx] +
                          ", label %" + next_label);
            } else {
                // Combine with OR
                std::string combined = cmp_results[0];
                for (size_t i = 1; i < cmp_results.size(); ++i) {
                    std::string new_combined = fresh_reg();
                    emit_line("  " + new_combined + " = or i1 " + combined + ", " + cmp_results[i]);
                    combined = new_combined;
                }
                emit_line("  br i1 " + combined + ", label %" + arm_labels[arm_idx] + ", label %" +
                          next_label);
            }
        } else {
            // Single pattern
            std::string cmp = gen_pattern_cmp(*arm.pattern, scrutinee, scrutinee_type, tag,
                                              is_primitive_scrutinee);
            if (cmp.empty()) {
                // Pattern always matches (wildcard, binding, etc.)
                emit_line("  br label %" + arm_labels[arm_idx]);
            } else {
                emit_line("  br i1 " + cmp + ", label %" + arm_labels[arm_idx] + ", label %" +
                          next_label);
            }
        }

        // Generate arm body
        emit_line(arm_labels[arm_idx] + ":");
        block_terminated_ = false;

        // Bind pattern variables
        if (arm.pattern->is<parser::EnumPattern>()) {
            const auto& enum_pat = arm.pattern->as<parser::EnumPattern>();

            if (enum_pat.payload.has_value() && !enum_pat.payload->empty()) {
                // Extract payload pointer (points to the data bytes of the enum)
                std::string payload_ptr = fresh_reg();
                emit_line("  " + payload_ptr + " = getelementptr inbounds " + scrutinee_type +
                          ", ptr " + scrutinee_ptr + ", i32 0, i32 1");

                // Get the semantic type of the scrutinee to find the payload type
                types::TypePtr scrutinee_semantic = infer_expr_type(*when.scrutinee);

                // Check if the payload pattern is a TuplePattern
                if (enum_pat.payload->at(0)->is<parser::TuplePattern>()) {
                    const auto& tuple_pat = enum_pat.payload->at(0)->as<parser::TuplePattern>();

                    // Get the tuple type from the enum's type arguments
                    // For Outcome[T, E], Ok's payload is T
                    // For Maybe[T], Just's payload is T
                    types::TypePtr payload_type = nullptr;
                    if (scrutinee_semantic && scrutinee_semantic->is<types::NamedType>()) {
                        const auto& named = scrutinee_semantic->as<types::NamedType>();
                        std::string variant_name;
                        if (!enum_pat.path.segments.empty()) {
                            variant_name = enum_pat.path.segments.back();
                        }

                        // For Outcome: Ok uses type_args[0] (T), Err uses type_args[1] (E)
                        if (named.name == "Outcome" && named.type_args.size() >= 2) {
                            if (variant_name == "Ok") {
                                payload_type = named.type_args[0];
                            } else if (variant_name == "Err") {
                                payload_type = named.type_args[1];
                            }
                        }
                        // For Maybe: Just uses type_args[0] (T)
                        else if (named.name == "Maybe" && !named.type_args.empty()) {
                            if (variant_name == "Just") {
                                payload_type = named.type_args[0];
                            }
                        }
                    }

                    // The payload is a tuple stored as an anonymous struct
                    // Get the element types from the payload_type if it's a TupleType
                    std::vector<types::TypePtr> element_types;
                    if (payload_type && payload_type->is<types::TupleType>()) {
                        element_types = payload_type->as<types::TupleType>().elements;
                    }

                    // Get the LLVM type of the tuple for proper GEP
                    std::string tuple_llvm_type =
                        payload_type ? llvm_type_from_semantic(payload_type, true) : "{ i64, i64 }";

                    // For each element in the tuple pattern, extract and bind
                    for (size_t i = 0; i < tuple_pat.elements.size(); ++i) {
                        const auto& elem_pat = tuple_pat.elements[i];

                        if (elem_pat->is<parser::IdentPattern>()) {
                            const auto& ident = elem_pat->as<parser::IdentPattern>();

                            // Skip wildcard patterns like _stride
                            if (ident.name.empty() || ident.name[0] == '_') {
                                continue;
                            }

                            // Get the element type (from inference or default to i64)
                            std::string elem_type = "i64";
                            types::TypePtr elem_semantic_type = nullptr;
                            if (i < element_types.size()) {
                                elem_semantic_type = element_types[i];
                                elem_type = llvm_type_from_semantic(elem_semantic_type, true);
                            }

                            // Extract the i-th element from the tuple
                            // The tuple is stored at payload_ptr as tuple_llvm_type
                            std::string elem_ptr = fresh_reg();
                            emit_line("  " + elem_ptr + " = getelementptr inbounds " +
                                      tuple_llvm_type + ", ptr " + payload_ptr + ", i32 0, i32 " +
                                      std::to_string(i));

                            // For struct types, we just use the pointer directly
                            // For primitives, we load the value
                            if (elem_type.starts_with("%struct.") || elem_type.starts_with("{")) {
                                // Struct/tuple type - variable is the pointer
                                locals_[ident.name] =
                                    VarInfo{elem_ptr, elem_type, elem_semantic_type, std::nullopt};
                            } else {
                                // Primitive type - load and store
                                std::string elem_val = fresh_reg();
                                emit_line("  " + elem_val + " = load " + elem_type + ", ptr " +
                                          elem_ptr);

                                std::string var_alloca = fresh_reg();
                                emit_line("  " + var_alloca + " = alloca " + elem_type);
                                emit_line("  store " + elem_type + " " + elem_val + ", ptr " +
                                          var_alloca);
                                locals_[ident.name] = VarInfo{var_alloca, elem_type,
                                                              elem_semantic_type, std::nullopt};
                            }
                        } else if (elem_pat->is<parser::WildcardPattern>()) {
                            // Wildcard _ - skip binding
                            continue;
                        }
                    }
                } else if (enum_pat.payload->at(0)->is<parser::IdentPattern>()) {
                    // Simple ident pattern - original behavior
                    const auto& ident = enum_pat.payload->at(0)->as<parser::IdentPattern>();

                    // Get payload type from enum type args
                    types::TypePtr payload_type = nullptr;
                    if (scrutinee_semantic && scrutinee_semantic->is<types::NamedType>()) {
                        const auto& named = scrutinee_semantic->as<types::NamedType>();
                        std::string variant_name;
                        if (!enum_pat.path.segments.empty()) {
                            variant_name = enum_pat.path.segments.back();
                        }

                        if (named.name == "Outcome" && named.type_args.size() >= 2) {
                            if (variant_name == "Ok") {
                                payload_type = named.type_args[0];
                            } else if (variant_name == "Err") {
                                payload_type = named.type_args[1];
                            }
                        } else if (named.name == "Maybe" && !named.type_args.empty()) {
                            if (variant_name == "Just") {
                                payload_type = named.type_args[0];
                            }
                        }
                    }

                    std::string bound_type =
                        payload_type ? llvm_type_from_semantic(payload_type, true) : "i64";

                    // For struct/tuple types, the variable is a pointer to the payload
                    if (bound_type.starts_with("%struct.") || bound_type.starts_with("{")) {
                        locals_[ident.name] =
                            VarInfo{payload_ptr, bound_type, payload_type, std::nullopt};
                    } else {
                        // For primitives, load from payload
                        std::string payload = fresh_reg();
                        emit_line("  " + payload + " = load " + bound_type + ", ptr " +
                                  payload_ptr);

                        std::string var_alloca = fresh_reg();
                        emit_line("  " + var_alloca + " = alloca " + bound_type);
                        emit_line("  store " + bound_type + " " + payload + ", ptr " + var_alloca);
                        locals_[ident.name] =
                            VarInfo{var_alloca, bound_type, payload_type, std::nullopt};
                    }
                }
            }
        }
        // Bind struct pattern variables
        else if (arm.pattern->is<parser::StructPattern>()) {
            const auto& struct_pat = arm.pattern->as<parser::StructPattern>();

            // Get the struct type name from the pattern path
            std::string struct_name;
            if (!struct_pat.path.segments.empty()) {
                struct_name = struct_pat.path.segments.back();
            }

            // Get semantic type info for field types
            types::TypePtr scrutinee_semantic = infer_expr_type(*when.scrutinee);

            // Look up struct field info from struct_fields_
            auto struct_it = struct_fields_.find(struct_name);

            for (size_t i = 0; i < struct_pat.fields.size(); ++i) {
                const auto& [field_name, field_pattern] = struct_pat.fields[i];

                // Only handle ident patterns for now
                if (!field_pattern->is<parser::IdentPattern>()) {
                    continue;
                }

                const auto& ident = field_pattern->as<parser::IdentPattern>();
                if (ident.name.empty() || ident.name == "_") {
                    continue;
                }

                // Find field index in struct
                int field_idx = -1;
                std::string field_type = "i64"; // Default
                if (struct_it != struct_fields_.end()) {
                    const auto& fields = struct_it->second;
                    for (size_t fi = 0; fi < fields.size(); ++fi) {
                        if (fields[fi].name == field_name) {
                            field_idx = fields[fi].index;
                            field_type = fields[fi].llvm_type;
                            break;
                        }
                    }
                }

                if (field_idx < 0) {
                    // Field not found in struct_fields_, try to use sequential index
                    field_idx = static_cast<int>(i);
                }

                // Extract field pointer from scrutinee
                std::string field_ptr = fresh_reg();
                emit_line("  " + field_ptr + " = getelementptr inbounds " + scrutinee_type +
                          ", ptr " + scrutinee_ptr + ", i32 0, i32 " + std::to_string(field_idx));

                // For struct/complex types, store pointer directly
                if (field_type.starts_with("%struct.") || field_type.starts_with("{")) {
                    locals_[ident.name] = VarInfo{field_ptr, field_type, nullptr, std::nullopt};
                } else {
                    // For primitives, load and store
                    std::string field_val = fresh_reg();
                    emit_line("  " + field_val + " = load " + field_type + ", ptr " + field_ptr);

                    std::string var_alloca = fresh_reg();
                    emit_line("  " + var_alloca + " = alloca " + field_type);
                    emit_line("  store " + field_type + " " + field_val + ", ptr " + var_alloca);
                    locals_[ident.name] = VarInfo{var_alloca, field_type, nullptr, std::nullopt};
                }
            }
        }
        // Bind tuple pattern variables
        else if (arm.pattern->is<parser::TuplePattern>()) {
            const auto& tuple_pat = arm.pattern->as<parser::TuplePattern>();

            // Get semantic type for the scrutinee
            types::TypePtr scrutinee_semantic = infer_expr_type(*when.scrutinee);

            // Use the existing helper function for tuple pattern binding
            gen_tuple_pattern_binding(tuple_pat, scrutinee, scrutinee_type, scrutinee_semantic);
        }
        // Bind array pattern variables: [a, b, c] or [head, ..rest]
        else if (arm.pattern->is<parser::ArrayPattern>()) {
            const auto& array_pat = arm.pattern->as<parser::ArrayPattern>();

            // Get semantic type for the scrutinee
            types::TypePtr scrutinee_semantic = infer_expr_type(*when.scrutinee);

            // Parse element type from the array type string (e.g., "[5 x i32]" -> "i32")
            std::string elem_type = "i32"; // Default
            size_t x_pos = scrutinee_type.find(" x ");
            if (x_pos != std::string::npos) {
                size_t end_pos = scrutinee_type.rfind(']');
                if (end_pos != std::string::npos && end_pos > x_pos + 3) {
                    elem_type = scrutinee_type.substr(x_pos + 3, end_pos - x_pos - 3);
                }
            }

            // Get semantic element type if available
            types::TypePtr semantic_elem = nullptr;
            if (scrutinee_semantic && scrutinee_semantic->is<types::ArrayType>()) {
                const auto& arr = scrutinee_semantic->as<types::ArrayType>();
                semantic_elem = arr.element;
            }

            // Store the array value to a temporary so we can GEP into it
            std::string array_ptr = fresh_reg();
            emit_line("  " + array_ptr + " = alloca " + scrutinee_type);
            emit_line("  store " + scrutinee_type + " " + scrutinee + ", ptr " + array_ptr);

            // Bind each element pattern
            for (size_t i = 0; i < array_pat.elements.size(); ++i) {
                const auto& elem_pattern = *array_pat.elements[i];

                // Get pointer to element
                std::string elem_ptr = fresh_reg();
                emit_line("  " + elem_ptr + " = getelementptr inbounds " + scrutinee_type +
                          ", ptr " + array_ptr + ", i32 0, i32 " + std::to_string(i));

                // Load the element
                std::string elem_val = fresh_reg();
                emit_line("  " + elem_val + " = load " + elem_type + ", ptr " + elem_ptr);

                // Bind based on pattern type
                if (elem_pattern.is<parser::IdentPattern>()) {
                    const auto& ident = elem_pattern.as<parser::IdentPattern>();
                    if (!ident.name.empty() && ident.name != "_") {
                        std::string alloca_reg = fresh_reg();
                        emit_line("  " + alloca_reg + " = alloca " + elem_type);
                        emit_line("  store " + elem_type + " " + elem_val + ", ptr " + alloca_reg);
                        locals_[ident.name] =
                            VarInfo{alloca_reg, elem_type, semantic_elem, std::nullopt};
                    }
                } else if (elem_pattern.is<parser::WildcardPattern>()) {
                    // Ignore the value
                }
                // Note: Nested patterns could be added here recursively
            }

            // Handle rest pattern if present (e.g., [a, b, ..rest])
            // Rest pattern binds to the remaining elements as a slice/array
            if (array_pat.rest) {
                const auto& rest_pattern = *array_pat.rest;
                if (rest_pattern->is<parser::IdentPattern>()) {
                    const auto& rest_ident = rest_pattern->as<parser::IdentPattern>();
                    if (!rest_ident.name.empty() && rest_ident.name != "_") {
                        // Calculate remaining elements pointer
                        size_t rest_start = array_pat.elements.size();
                        std::string rest_ptr = fresh_reg();
                        emit_line("  " + rest_ptr + " = getelementptr inbounds " + scrutinee_type +
                                  ", ptr " + array_ptr + ", i32 0, i32 " +
                                  std::to_string(rest_start));
                        // Bind as a pointer to the rest of the array
                        locals_[rest_ident.name] =
                            VarInfo{rest_ptr, "ptr", scrutinee_semantic, std::nullopt};
                    }
                }
            }
        }
        // Bind ident pattern (simple variable binding to scrutinee)
        else if (arm.pattern->is<parser::IdentPattern>()) {
            const auto& ident = arm.pattern->as<parser::IdentPattern>();
            if (!ident.name.empty() && ident.name != "_") {
                // Bind the entire scrutinee to the variable
                if (scrutinee_type.starts_with("%struct.") || scrutinee_type.starts_with("{")) {
                    locals_[ident.name] =
                        VarInfo{scrutinee_ptr, scrutinee_type, nullptr, std::nullopt};
                } else {
                    std::string var_alloca = fresh_reg();
                    emit_line("  " + var_alloca + " = alloca " + scrutinee_type);
                    emit_line("  store " + scrutinee_type + " " + scrutinee + ", ptr " +
                              var_alloca);
                    locals_[ident.name] =
                        VarInfo{var_alloca, scrutinee_type, nullptr, std::nullopt};
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
    current_block_ = label_end;
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
