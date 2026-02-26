TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Loop Control Flow
//! Updated: gen_for_iterator uses single @ prefix for function names
//!
//! This file implements block, loop, while, and for expression code generation.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_block(const parser::BlockExpr& block) -> std::string {
    std::string result = "0";

    // Push new scopes for this block
    push_drop_scope();
    push_lifetime_scope();

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

    // Emit drops and lifetime.end for variables in this scope before exiting
    if (!block_terminated_) {
        emit_scope_drops();
        pop_lifetime_scope();
    } else {
        // Block was terminated by return/break/continue - lifetime.end already emitted
        // Just pop the scope without emitting (prevents double emission)
        if (!scope_allocas_.empty()) {
            scope_allocas_.pop_back();
        }
    }

    pop_drop_scope();

    return result;
}

auto LLVMIRGen::gen_loop(const parser::LoopExpr& loop) -> std::string {
    // Canonical LLVM loop form for infinite loop:
    //   preheader -> header -> body -> latch -> header (backedge)
    //                               \-> exit (via break)

    // Handle loop variable declaration: loop (var i: I64 < N)
    // Initialize the variable to 0 before entering the loop
    if (loop.loop_var) {
        const auto& var_decl = *loop.loop_var;
        types::TypePtr semantic_type =
            resolve_parser_type_with_subs(*var_decl.type, current_type_subs_);
        std::string var_type = llvm_type_from_semantic(semantic_type);

        // Allocate and initialize to 0
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + var_type);
        if (var_type != "{}") {
            emit_line("  store " + var_type + " 0, ptr " + alloca_reg);
        }

        // Register in locals_
        locals_[var_decl.name] = VarInfo{alloca_reg, var_type, semantic_type, std::nullopt};
    }

    std::string label_preheader = fresh_label("loop.preheader");
    std::string label_header = fresh_label("loop.header");
    std::string label_body = fresh_label("loop.body");
    std::string label_latch = fresh_label("loop.latch");
    std::string label_exit = fresh_label("loop.exit");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    std::string saved_loop_stack_save = current_loop_stack_save_;
    int saved_loop_metadata_id = current_loop_metadata_id_;
    current_loop_start_ = label_latch; // continue goes to latch (canonical form)
    current_loop_end_ = label_exit;

    // Create loop metadata for optimization hints
    current_loop_metadata_id_ = create_loop_metadata(false, 0);

    // Preheader block - single entry to loop
    emit_line("  br label %" + label_preheader);
    emit_line(label_preheader + ":");
    emit_line("  br label %" + label_header);

    // Header block - condition evaluation (loop now requires condition)
    emit_line(label_header + ":");
    current_block_ = label_header;
    block_terminated_ = false;
    current_loop_stack_save_ = ""; // No stack save for break/continue

    // Evaluate the loop condition (mandatory in new syntax: loop (condition) { ... })
    std::string cond = gen_expr(*loop.condition);

    // If condition is not already i1 (bool), convert it
    if (last_expr_type_ != "i1") {
        std::string cond_type = last_expr_type_.empty() ? "i32" : last_expr_type_;
        std::string bool_cond = fresh_reg();
        emit_line("  " + bool_cond + " = icmp ne " + cond_type + " " + cond + ", 0");
        cond = bool_cond;
    }

    emit_line("  br i1 " + cond + ", label %" + label_body + ", label %" + label_exit);

    // Body block
    emit_line(label_body + ":");
    current_block_ = label_body;
    block_terminated_ = false;

    // No stacksave/stackrestore — allocas are hoisted to entry block by
    // emit_hoisted_alloca(), so LLVM's mem2reg can promote them to SSA registers.
    current_loop_stack_save_ = "";

    // Push a lifetime scope for the loop body so allocas inside are tracked
    // and can have lifetime.end emitted at end of each iteration
    push_lifetime_scope();

    gen_expr(*loop.body);

    if (!block_terminated_) {
        // Emit lifetime.end for all allocas created in this iteration
        emit_scope_lifetime_ends();
        emit_line("  br label %" + label_latch);
    }

    // Clear the loop body scope (lifetime.end already emitted, just cleanup tracking)
    clear_lifetime_scope();

    // Latch block - single backedge
    emit_line(label_latch + ":");
    current_block_ = label_latch;
    block_terminated_ = false;
    // Add loop metadata to back-edge for LLVM optimization hints
    std::string loop_meta = current_loop_metadata_id_ >= 0
                                ? ", !llvm.loop !" + std::to_string(current_loop_metadata_id_)
                                : "";
    emit_line("  br label %" + label_header + loop_meta);

    // Exit block (reached via break)
    emit_line(label_exit + ":");
    current_block_ = label_exit;
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;
    current_loop_stack_save_ = saved_loop_stack_save;
    current_loop_metadata_id_ = saved_loop_metadata_id;

    return "0";
}

auto LLVMIRGen::gen_while(const parser::WhileExpr& while_expr) -> std::string {
    // Canonical LLVM loop form:
    //   preheader -> header -> body -> latch -> header (backedge)
    //                      \-> exit
    std::string label_preheader = fresh_label("while.preheader");
    std::string label_header = fresh_label("while.header");
    std::string label_body = fresh_label("while.body");
    std::string label_latch = fresh_label("while.latch");
    std::string label_exit = fresh_label("while.exit");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    std::string saved_loop_stack_save = current_loop_stack_save_;
    int saved_loop_metadata_id = current_loop_metadata_id_;
    current_loop_start_ = label_latch; // continue goes to latch (canonical form)
    current_loop_end_ = label_exit;

    // Create loop metadata for optimization hints
    current_loop_metadata_id_ = create_loop_metadata(true, 0);

    // Preheader block - single entry to loop (for loop-invariant code motion)
    emit_line("  br label %" + label_preheader);
    emit_line(label_preheader + ":");
    emit_line("  br label %" + label_header);

    // Header block - condition evaluation
    emit_line(label_header + ":");
    current_block_ = label_header;
    block_terminated_ = false;
    current_loop_stack_save_ = ""; // No stack save for break/continue

    std::string cond = gen_expr(*while_expr.condition);

    // If condition is not already i1 (bool), convert it
    if (last_expr_type_ != "i1") {
        std::string cond_type = last_expr_type_.empty() ? "i32" : last_expr_type_;
        std::string bool_cond = fresh_reg();
        emit_line("  " + bool_cond + " = icmp ne " + cond_type + " " + cond + ", 0");
        cond = bool_cond;
    }

    emit_line("  br i1 " + cond + ", label %" + label_body + ", label %" + label_exit);

    // Body block
    emit_line(label_body + ":");
    current_block_ = label_body;
    block_terminated_ = false;

    // No stacksave/stackrestore — allocas are hoisted to entry block
    current_loop_stack_save_ = "";

    // Push a lifetime scope for the loop body
    push_lifetime_scope();

    gen_expr(*while_expr.body);

    if (!block_terminated_) {
        // Emit lifetime.end for allocas in this iteration
        emit_scope_lifetime_ends();
        emit_line("  br label %" + label_latch);
    }

    // Clear the scope (lifetime.end already emitted)
    clear_lifetime_scope();

    // Latch block - single backedge (allows LLVM to identify loop structure)
    emit_line(label_latch + ":");
    current_block_ = label_latch;
    block_terminated_ = false;
    // Add loop metadata to back-edge for LLVM optimization hints
    std::string loop_meta = current_loop_metadata_id_ >= 0
                                ? ", !llvm.loop !" + std::to_string(current_loop_metadata_id_)
                                : "";
    emit_line("  br label %" + label_header + loop_meta);

    // Exit block
    emit_line(label_exit + ":");
    current_block_ = label_exit;
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;
    current_loop_stack_save_ = saved_loop_stack_save;
    current_loop_metadata_id_ = saved_loop_metadata_id;

    return "0";
}

auto LLVMIRGen::gen_for(const parser::ForExpr& for_expr) -> std::string {
    // =========================================================================
    // Compile-time loop unrolling for field_count[T]() intrinsic
    // =========================================================================
    // Check if this is a compile-time unrollable loop over struct fields
    // Pattern: for i in 0 to field_count[T]() { ... }
    if (for_expr.iter->is<parser::RangeExpr>()) {
        const auto& range = for_expr.iter->as<parser::RangeExpr>();
        if (range.end.has_value() && range.end.value()->is<parser::CallExpr>()) {
            const auto& call = range.end.value()->as<parser::CallExpr>();
            if (call.callee->is<parser::PathExpr>()) {
                const auto& path_expr = call.callee->as<parser::PathExpr>();
                if (path_expr.path.segments.size() == 1 &&
                    path_expr.path.segments[0] == "field_count" && path_expr.generics &&
                    !path_expr.generics->args.empty()) {
                    // This is field_count[T]() - extract the type and unroll
                    const auto& first_arg = path_expr.generics->args[0];
                    if (first_arg.is_type()) {
                        auto resolved =
                            resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                        if (resolved->is<types::NamedType>()) {
                            std::string type_name = resolved->as<types::NamedType>().name;
                            auto it = struct_fields_.find(type_name);
                            if (it != struct_fields_.end()) {
                                size_t field_count = it->second.size();
                                // Get loop variable name
                                std::string var_name = "_for_idx";
                                if (for_expr.pattern->is<parser::IdentPattern>()) {
                                    var_name = for_expr.pattern->as<parser::IdentPattern>().name;
                                }
                                // Unroll the loop at compile time
                                return gen_for_unrolled(for_expr, var_name, type_name, field_count);
                            }
                        }
                    }
                }
            }
        }
    }

    // =========================================================================
    // Standard for loop codegen
    // =========================================================================
    // Canonical LLVM loop form:
    //   preheader -> header -> body -> latch -> header (backedge)
    //                      \-> exit
    std::string label_preheader = fresh_label("for.preheader");
    std::string label_header = fresh_label("for.header");
    std::string label_body = fresh_label("for.body");
    std::string label_latch = fresh_label("for.latch");
    std::string label_exit = fresh_label("for.exit");

    // Save current loop labels for break/continue
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    std::string saved_loop_stack_save = current_loop_stack_save_;
    int saved_loop_metadata_id = current_loop_metadata_id_;
    current_loop_start_ = label_latch; // continue goes to latch (canonical form)
    current_loop_end_ = label_exit;

    // Create loop metadata for optimization hints
    // For loops are the best candidates for vectorization since they have known bounds
    current_loop_metadata_id_ = create_loop_metadata(true, 4);

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
        // Check if the iter expression is a type implementing Iterator behavior
        auto iter_semantic_type = infer_expr_type(*for_expr.iter);
        if (iter_semantic_type && iter_semantic_type->is<types::NamedType>()) {
            const auto& named = iter_semantic_type->as<types::NamedType>();
            if (env_.type_implements(named.name, "Iterator")) {
                // Restore loop labels before delegating
                current_loop_start_ = saved_loop_start;
                current_loop_end_ = saved_loop_end;
                current_loop_stack_save_ = saved_loop_stack_save;
                current_loop_metadata_id_ = saved_loop_metadata_id;
                return gen_for_iterator(for_expr, named.name);
            }
        }
        // Treat as simple range 0 to iter
        std::string iter_val = gen_expr(*for_expr.iter);
        range_end = iter_val;
        range_type = last_expr_type_;
    }

    // Preheader block - loop initialization (for loop-invariant code motion)
    emit_line("  br label %" + label_preheader);
    emit_line(label_preheader + ":");

    // Allocate and initialize loop variable
    std::string var_alloca = fresh_reg();
    emit_line("  " + var_alloca + " = alloca " + range_type);
    emit_line("  store " + range_type + " " + range_start + ", ptr " + var_alloca);
    locals_[var_name] = VarInfo{var_alloca, range_type, nullptr, std::nullopt};

    // Jump to header
    emit_line("  br label %" + label_header);

    // Header block - condition check
    emit_line(label_header + ":");
    current_block_ = label_header;
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
    emit_line("  br i1 " + cmp_result + ", label %" + label_body + ", label %" + label_exit);

    // Body block
    emit_line(label_body + ":");
    current_block_ = label_body;
    block_terminated_ = false;

    // No stacksave/stackrestore — allocas are hoisted to entry block
    current_loop_stack_save_ = "";

    // Push a lifetime scope for the loop body
    push_lifetime_scope();

    gen_expr(*for_expr.body);

    if (!block_terminated_) {
        // Emit lifetime.end for allocas in this iteration
        emit_scope_lifetime_ends();
        emit_line("  br label %" + label_latch);
    }

    // Clear the scope (lifetime.end already emitted)
    clear_lifetime_scope();

    // Latch block - increment and backedge (single backedge for canonical form)
    emit_line(label_latch + ":");
    current_block_ = label_latch;
    block_terminated_ = false;
    std::string next_val = fresh_reg();
    std::string current2 = fresh_reg();
    emit_line("  " + current2 + " = load " + range_type + ", ptr " + var_alloca);
    emit_line("  " + next_val + " = add nsw " + range_type + " " + current2 + ", 1");
    emit_line("  store " + range_type + " " + next_val + ", ptr " + var_alloca);
    // Add loop metadata to back-edge for LLVM optimization hints
    std::string loop_meta = current_loop_metadata_id_ >= 0
                                ? ", !llvm.loop !" + std::to_string(current_loop_metadata_id_)
                                : "";
    emit_line("  br label %" + label_header + loop_meta);

    // Exit block
    emit_line(label_exit + ":");
    current_block_ = label_exit;
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;
    current_loop_stack_save_ = saved_loop_stack_save;
    current_loop_metadata_id_ = saved_loop_metadata_id;

    return "0";
}

auto LLVMIRGen::gen_for_iterator(const parser::ForExpr& for_expr, const std::string& type_name)
    -> std::string {
    // =========================================================================
    // Iterator-based for loop desugaring:
    //   for pattern in iter { body }
    // becomes:
    //   let mut _it = iter;   (alloca + store)
    //   loop {
    //       let _next = TypeName::next(mut ref _it);  // call next()
    //       match _next {
    //           Just(x) => { body }
    //           Nothing => break
    //       }
    //   }
    // =========================================================================

    std::string label_preheader = fresh_label("iter.preheader");
    std::string label_header = fresh_label("iter.header");
    std::string label_body = fresh_label("iter.body");
    std::string label_exit = fresh_label("iter.exit");

    // Save/set loop labels
    std::string saved_loop_start = current_loop_start_;
    std::string saved_loop_end = current_loop_end_;
    std::string saved_loop_stack_save = current_loop_stack_save_;
    int saved_loop_metadata_id = current_loop_metadata_id_;
    current_loop_start_ = label_header;
    current_loop_end_ = label_exit;
    current_loop_metadata_id_ = -1;

    // Get pattern variable name
    std::string var_name = "_for_item";
    if (for_expr.pattern->is<parser::IdentPattern>()) {
        var_name = for_expr.pattern->as<parser::IdentPattern>().name;
    }

    // Evaluate the iterable and store it to a mutable alloca so next() can take &mut self
    std::string iter_val = gen_expr(*for_expr.iter);
    std::string iter_llvm_type = last_expr_type_;

    // Look up next() return type to determine item type
    std::string next_fn = "tml_" + type_name + "_next";
    std::string item_llvm_type = "i32"; // fallback
    auto next_sig = env_.lookup_func(type_name + "::next");
    if (next_sig && next_sig->return_type) {
        // next() returns Maybe[Item]; get the struct layout for item extraction
        if (next_sig->return_type->is<types::NamedType>()) {
            const auto& ret = next_sig->return_type->as<types::NamedType>();
            if ((ret.name == "Maybe" || ret.name == "Option") && !ret.type_args.empty()) {
                item_llvm_type = llvm_type_from_semantic(ret.type_args[0]);
            }
        }
    }

    // Determine the LLVM type for Maybe[Item] (the return type of next())
    std::string maybe_llvm_type = iter_llvm_type; // fallback
    if (next_sig && next_sig->return_type) {
        maybe_llvm_type = llvm_type_from_semantic(next_sig->return_type);
    }

    // Preheader: allocate iterator storage
    emit_line("  br label %" + label_preheader);
    emit_line(label_preheader + ":");

    std::string iter_alloca = fresh_reg();
    emit_line("  " + iter_alloca + " = alloca " + iter_llvm_type);
    emit_line("  store " + iter_llvm_type + " " + iter_val + ", ptr " + iter_alloca);

    emit_line("  br label %" + label_header);

    // Header: call next() with mutable reference to iterator
    emit_line(label_header + ":");
    current_block_ = label_header;
    block_terminated_ = false;

    std::string next_result = fresh_reg();
    if (maybe_llvm_type == "void" || maybe_llvm_type == "ptr") {
        // Nullable maybe (ptr types): next returns ptr directly, nullptr = Nothing
        emit_line("  " + next_result + " = call ptr @" + next_fn + "(ptr " + iter_alloca + ")");
        std::string is_null = fresh_reg();
        emit_line("  " + is_null + " = icmp eq ptr " + next_result + ", null");
        emit_line("  br i1 " + is_null + ", label %" + label_exit + ", label %" + label_body);

        // Body: bind item (the non-null ptr)
        emit_line(label_body + ":");
        current_block_ = label_body;
        block_terminated_ = false;

        push_lifetime_scope();
        // Bind pattern to the value
        std::string item_alloca = fresh_reg();
        emit_line("  " + item_alloca + " = alloca ptr");
        emit_line("  store ptr " + next_result + ", ptr " + item_alloca);
        locals_[var_name] = VarInfo{item_alloca, "ptr", nullptr, std::nullopt};
    } else {
        // Struct maybe: { i32 tag, payload }
        // Call next() returning the struct by value
        emit_line("  " + next_result + " = call " + maybe_llvm_type + " @" + next_fn + "(ptr " +
                  iter_alloca + ")");

        // Extract tag (field 0, i32)
        std::string tag_val = fresh_reg();
        emit_line("  " + tag_val + " = extractvalue " + maybe_llvm_type + " " + next_result +
                  ", 0");

        // TML enum convention: Just is tag 0, Nothing is tag 1
        // (confirmed from IR: when tag==0 → Just arm, when tag==1 → Nothing arm)
        std::string is_nothing = fresh_reg();
        emit_line("  " + is_nothing + " = icmp eq i32 " + tag_val + ", 1");
        emit_line("  br i1 " + is_nothing + ", label %" + label_exit + ", label %" + label_body);

        // Body: extract item from Maybe payload (field 1)
        emit_line(label_body + ":");
        current_block_ = label_body;
        block_terminated_ = false;

        push_lifetime_scope();
        std::string item_val = fresh_reg();
        emit_line("  " + item_val + " = extractvalue " + maybe_llvm_type + " " + next_result +
                  ", 1");

        // Store item to alloca so pattern binding works uniformly
        std::string item_alloca = fresh_reg();
        emit_line("  " + item_alloca + " = alloca " + item_llvm_type);
        emit_line("  store " + item_llvm_type + " " + item_val + ", ptr " + item_alloca);
        locals_[var_name] = VarInfo{item_alloca, item_llvm_type, nullptr, std::nullopt};
    }

    // Generate loop body
    gen_expr(*for_expr.body);

    if (!block_terminated_) {
        emit_scope_lifetime_ends();
        emit_line("  br label %" + label_header);
    }
    clear_lifetime_scope();

    // Exit block
    emit_line(label_exit + ":");
    current_block_ = label_exit;
    block_terminated_ = false;

    // Restore loop labels
    current_loop_start_ = saved_loop_start;
    current_loop_end_ = saved_loop_end;
    current_loop_stack_save_ = saved_loop_stack_save;
    current_loop_metadata_id_ = saved_loop_metadata_id;

    return "0";
}

auto LLVMIRGen::gen_for_unrolled(const parser::ForExpr& for_expr, const std::string& var_name,
                                 const std::string& type_name, size_t iteration_count)
    -> std::string {
    // Compile-time loop unrolling for struct field iteration
    // This generates code for each iteration with the loop variable as a constant literal
    //
    // Example: for i in 0 to field_count[Point]() { ... }
    // Becomes: i=0: body_code; i=1: body_code; ...

    // Save the original comptime_loop_var_ context
    std::string saved_comptime_var = comptime_loop_var_;
    std::string saved_comptime_type = comptime_loop_type_;
    int64_t saved_comptime_value = comptime_loop_value_;

    // Set up the compile-time loop variable context
    comptime_loop_var_ = var_name;
    comptime_loop_type_ = type_name;

    for (size_t i = 0; i < iteration_count; ++i) {
        // Set the current iteration value
        comptime_loop_value_ = static_cast<int64_t>(i);

        // Create an alloca for the loop variable with the constant value
        // This allows the body to reference the variable normally
        std::string var_alloca = fresh_reg();
        emit_line("  " + var_alloca + " = alloca i64");
        emit_line("  store i64 " + std::to_string(i) + ", ptr " + var_alloca);
        locals_[var_name] = VarInfo{var_alloca, "i64", nullptr, std::nullopt};

        // Push scopes for this iteration
        push_drop_scope();
        push_lifetime_scope();

        // Generate the loop body for this iteration
        gen_expr(*for_expr.body);

        // Emit drops and lifetime ends
        if (!block_terminated_) {
            emit_scope_drops();
            pop_lifetime_scope();
        } else {
            if (!scope_allocas_.empty()) {
                scope_allocas_.pop_back();
            }
            block_terminated_ = false; // Reset for next iteration
        }
        pop_drop_scope();
    }

    // Restore the compile-time loop variable context
    comptime_loop_var_ = saved_comptime_var;
    comptime_loop_type_ = saved_comptime_type;
    comptime_loop_value_ = saved_comptime_value;

    // Remove the loop variable from locals
    locals_.erase(var_name);

    return "0";
}

} // namespace tml::codegen
