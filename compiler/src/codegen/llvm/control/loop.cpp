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
    } else if (!block_terminated_ && closure_wants_implicit_return_ && !block.stmts.empty()) {
        // Closure with non-void return type but no trailing expression.
        // Treat the last ExprStmt's value as the implicit return value.
        // The expression was already emitted by gen_stmt — re-generate it
        // to capture the value. This is safe for pure expressions (no side effects).
        const auto& last_stmt = *block.stmts.back();
        if (last_stmt.is<parser::ExprStmt>()) {
            result = gen_expr(*last_stmt.as<parser::ExprStmt>().expr);
        } else {
            last_expr_type_ = "void";
        }
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

        // Allocate and initialize to zero/null
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + var_type);
        if (var_type != "{}") {
            // Use appropriate zero value for each type
            std::string zero_val;
            if (var_type == "ptr") {
                zero_val = "null";
            } else if (var_type.starts_with("%") || var_type.starts_with("{")) {
                // Struct/aggregate types use zeroinitializer
                zero_val = "zeroinitializer";
            } else {
                zero_val = "0";
            }
            emit_line("  store " + var_type + " " + zero_val + ", ptr " + alloca_reg);
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
    std::string range_type = "i64"; // Default integer type (TML I64)
    if (for_expr.iter->is<parser::RangeExpr>()) {
        const auto& range = for_expr.iter->as<parser::RangeExpr>();
        inclusive = range.inclusive;
        if (range.start.has_value()) {
            range_start = gen_expr(*range.start.value());
        }
        if (range.end.has_value()) {
            range_end = gen_expr(*range.end.value());
            range_type = last_expr_type_;
            // Promote to i64 — TML's native integer. Avoids i32 truncation
            // when loop index is used in i64 contexts (pointer arithmetic, List access).
            if (range_type == "i32" || range_type == "i16" || range_type == "i8") {
                std::string ext_reg = fresh_reg();
                emit_line("  " + ext_reg + " = sext " + range_type + " " + range_end + " to i64");
                range_end = ext_reg;
                range_type = "i64";
            }
        }
    } else {
        auto iter_semantic_type = infer_expr_type(*for_expr.iter);
        if (iter_semantic_type && iter_semantic_type->is<types::NamedType>()) {
            const auto& named = iter_semantic_type->as<types::NamedType>();
            // Check if type implements Iterator directly
            if (env_.type_implements(named.name, "Iterator")) {
                current_loop_start_ = saved_loop_start;
                current_loop_end_ = saved_loop_end;
                current_loop_stack_save_ = saved_loop_stack_save;
                current_loop_metadata_id_ = saved_loop_metadata_id;
                return gen_for_iterator(for_expr, named.name);
            }
            // Check if type implements IntoIterator — call into_iter() first,
            // then use the result as Iterator (like Rust's for-in desugaring).
            if (env_.type_implements(named.name, "IntoIterator")) {
                // Generate the collection expression
                std::string collection_val = gen_expr(*for_expr.iter);
                std::string collection_type = last_expr_type_;

                // Look up the IntoIter associated type to find the iterator type name
                auto into_iter_sig = env_.lookup_func(named.name + "::into_iter");
                std::string iter_type_name;
                if (into_iter_sig && into_iter_sig->return_type &&
                    into_iter_sig->return_type->is<types::NamedType>()) {
                    iter_type_name = into_iter_sig->return_type->as<types::NamedType>().name;
                }
                if (iter_type_name.empty()) {
                    iter_type_name = "ListIter"; // fallback for List's IntoIterator
                }

                // Build mangled into_iter function name with type args
                std::string mangled_collection = named.name;
                if (!named.type_args.empty()) {
                    mangled_collection = named.name + "__" + mangle_type_args(named.type_args);
                }
                std::string into_iter_fn = mangle_impl_method(mangled_collection, "into_iter");

                // Call into_iter(this) — returns the iterator by value
                std::string iter_val = fresh_reg();
                std::string iter_llvm_type = "%struct." + iter_type_name;
                if (!named.type_args.empty()) {
                    iter_llvm_type = "%struct." + iter_type_name + "__" +
                                    mangle_type_args(named.type_args);
                }

                // Store collection to alloca for into_iter (takes ptr to self)
                std::string coll_alloca = fresh_reg();
                emit_line("  " + coll_alloca + " = alloca " + collection_type);
                emit_line("  store " + collection_type + " " + collection_val + ", ptr " +
                          coll_alloca);

                // Call into_iter
                emit_line("  " + iter_val + " = call " + iter_llvm_type + " @" + into_iter_fn +
                          "(ptr " + coll_alloca + ")");
                last_expr_type_ = iter_llvm_type;

                // Request instantiation of into_iter for this generic type
                if (!named.type_args.empty()) {
                    std::unordered_map<std::string, types::TypePtr> subs;
                    auto struct_def = env_.lookup_struct(named.name);
                    if (struct_def) {
                        for (size_t i = 0; i < struct_def->type_params.size() &&
                                           i < named.type_args.size(); ++i) {
                            subs[struct_def->type_params[i]] = named.type_args[i];
                        }
                    }
                    pending_impl_method_instantiations_.push_back(
                        PendingImplMethod{mangled_collection, "into_iter", subs,
                                          named.name, "", true});
                }

                // ============================================================
                // Pointer-stepping optimization for ListIter-based iterators.
                // Instead of calling next() -> Maybe[T] per iteration (which
                // prevents LLVM vectorization due to discriminant branch),
                // emit a direct phi-based pointer-stepping loop:
                //   %ptr = phi ptr [init, preheader], [next, body]
                //   %done = icmp eq ptr %ptr, %end
                //   br i1 %done, exit, body
                //   body: %val = load T, ptr %ptr
                //         %ptr.next = gep i8, ptr %ptr, i64 %stride
                // This is the exact pattern Rust generates for slice::Iter.
                // ============================================================
                if (iter_type_name == "ListIter") {
                    // Determine element LLVM type from collection type args
                    std::string elem_llvm_type;
                    if (!named.type_args.empty()) {
                        elem_llvm_type = llvm_type_from_semantic(named.type_args[0]);
                    }
                    if (elem_llvm_type.empty()) {
                        elem_llvm_type = "i64"; // safe default for List[I64]
                    }

                    return gen_for_pointer_stepping(
                        for_expr, iter_val, iter_llvm_type, elem_llvm_type,
                        saved_loop_start, saved_loop_end, saved_loop_stack_save,
                        saved_loop_metadata_id);
                }

                // Fallback: use gen_for_iterator for non-ListIter iterators
                current_loop_start_ = saved_loop_start;
                current_loop_end_ = saved_loop_end;
                current_loop_stack_save_ = saved_loop_stack_save;
                current_loop_metadata_id_ = saved_loop_metadata_id;

                return gen_for_iterator_with_value(for_expr, iter_type_name, iter_val,
                                                   iter_llvm_type, named.type_args);
            }
        }
        // Treat as simple range 0 to iter
        std::string iter_val = gen_expr(*for_expr.iter);
        range_end = iter_val;
        range_type = last_expr_type_;
    }

    // Preheader block - loop initialization
    emit_line("  br label %" + label_preheader);
    emit_line(label_preheader + ":");

    // Allocate loop variable for address-taking code in body
    std::string var_alloca = fresh_reg();
    emit_line("  " + var_alloca + " = alloca " + range_type);
    emit_line("  store " + range_type + " " + range_start + ", ptr " + var_alloca);
    locals_[var_name] = VarInfo{var_alloca, range_type, nullptr, std::nullopt};

    emit_line("  br label %" + label_header);

    // Header block — PHI-based canonical loop form for LLVM vectorizer.
    // Pre-allocate the next_val register name so the phi can reference it.
    emit_line(label_header + ":");
    current_block_ = label_header;
    block_terminated_ = false;

    std::string phi_var = fresh_reg();
    std::string next_val = fresh_reg(); // pre-allocate for latch increment
    emit_line("  " + phi_var + " = phi " + range_type +
        " [ " + range_start + ", %" + label_preheader +
        " ], [ " + next_val + ", %" + label_latch + " ]");
    // Sync alloca with phi so body code reading from alloca sees correct value
    emit_line("  store " + range_type + " " + phi_var + ", ptr " + var_alloca);

    std::string cmp_result = fresh_reg();
    if (inclusive) {
        emit_line("  " + cmp_result + " = icmp sle " + range_type + " " + phi_var + ", " +
                  range_end);
    } else {
        emit_line("  " + cmp_result + " = icmp slt " + range_type + " " + phi_var + ", " +
                  range_end);
    }
    emit_line("  br i1 " + cmp_result + ", label %" + label_body + ", label %" + label_exit);

    // Body block
    emit_line(label_body + ":");
    current_block_ = label_body;
    block_terminated_ = false;
    current_loop_stack_save_ = "";

    push_lifetime_scope();

    // Emit llvm.assume(i ult n) for 0-based ranges
    if (range_start == "0" && range_type != "i1") {
        std::string assume_cond = fresh_reg();
        emit_line("  " + assume_cond + " = icmp ult " + range_type + " " + phi_var + ", " +
                  range_end);
        emit_line("  call void @llvm.assume(i1 " + assume_cond + ")");
    }

    gen_expr(*for_expr.body);

    if (!block_terminated_) {
        emit_scope_lifetime_ends();
        emit_line("  br label %" + label_latch);
    }
    clear_lifetime_scope();

    // Latch block — increment induction variable using pre-allocated register
    emit_line(label_latch + ":");
    current_block_ = label_latch;
    block_terminated_ = false;
    emit_line("  " + next_val + " = add nsw " + range_type + " " + phi_var + ", 1");
    emit_line("  store " + range_type + " " + next_val + ", ptr " + var_alloca);
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
    std::string iter_val;
    std::string iter_llvm_type;
    if (use_precomputed_iter_) {
        iter_val = precomputed_iter_val_;
        iter_llvm_type = precomputed_iter_type_;
    } else {
        iter_val = gen_expr(*for_expr.iter);
        iter_llvm_type = last_expr_type_;
    }

    // Build the mangled type name for generic iterators (e.g. BTreeMapIter__I64__I64).
    std::string mangled_type_name = type_name;
    {
        auto iter_semantic_type = infer_expr_type(*for_expr.iter);
        if (iter_semantic_type && iter_semantic_type->is<types::NamedType>()) {
            const auto& iter_named = iter_semantic_type->as<types::NamedType>();
            if (!iter_named.type_args.empty()) {
                mangled_type_name = type_name + "__" + mangle_type_args(iter_named.type_args);
            }
        }
    }

    // Look up next() return type to determine item type
    std::string next_fn = mangle_impl_method(mangled_type_name, "next");
    std::string item_llvm_type; // must be resolved — no i32 fallback
    auto next_sig = env_.lookup_func(type_name + "::next");

    // Build generic substitution map for the iterator type.
    // E.g. BTreeMapIter[I64, I64] => {K -> I64, V -> I64} so that
    // MapEntry[K, V] becomes MapEntry[I64, I64] in LLVM type resolution.
    std::unordered_map<std::string, types::TypePtr> iter_subs;
    {
        auto iter_semantic_type = infer_expr_type(*for_expr.iter);
        if (iter_semantic_type && iter_semantic_type->is<types::NamedType>()) {
            const auto& iter_named = iter_semantic_type->as<types::NamedType>();
            if (!iter_named.type_args.empty()) {
                auto struct_def = env_.lookup_struct(iter_named.name);
                if (struct_def && struct_def->type_params.size() == iter_named.type_args.size()) {
                    for (size_t i = 0; i < struct_def->type_params.size(); ++i) {
                        iter_subs[struct_def->type_params[i]] = iter_named.type_args[i];
                    }
                }
            }
        }
    }

    if (next_sig && next_sig->return_type) {
        auto resolved_ret = iter_subs.empty()
            ? next_sig->return_type
            : types::substitute_type(next_sig->return_type, iter_subs);
        // next() returns Maybe[Item]; get the struct layout for item extraction
        if (resolved_ret->is<types::NamedType>()) {
            const auto& ret = resolved_ret->as<types::NamedType>();
            if ((ret.name == "Maybe" || ret.name == "Option") && !ret.type_args.empty()) {
                item_llvm_type = llvm_type_from_semantic(ret.type_args[0]);
            }
        }
    }

    if (item_llvm_type.empty()) {
        emit_line("  ; ERROR: could not resolve Iterator::Item type for " + type_name);
        emit_line("  unreachable");
        block_terminated_ = true;
        current_loop_start_ = saved_loop_start;
        current_loop_end_ = saved_loop_end;
        current_loop_stack_save_ = saved_loop_stack_save;
        current_loop_metadata_id_ = saved_loop_metadata_id;
        return "0";
    }

    // Determine the LLVM type for Maybe[Item] (the return type of next())
    std::string maybe_llvm_type = iter_llvm_type;
    if (next_sig && next_sig->return_type) {
        auto resolved_ret = iter_subs.empty()
            ? next_sig->return_type
            : types::substitute_type(next_sig->return_type, iter_subs);
        maybe_llvm_type = llvm_type_from_semantic(resolved_ret);
    }

    // Request instantiation of the next() method for this generic iterator.
    // Without this, the generic impl Iterator for BTreeMapIter[K,V] won't
    // emit the concrete next() body for BTreeMapIter__I64__I64 etc.
    if (mangled_type_name != type_name) {
        pending_impl_method_instantiations_.push_back(
            PendingImplMethod{mangled_type_name, "next", iter_subs, type_name, "",
                              /*is_library_type=*/true});
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

        // Store the entire Maybe result to memory, then GEP to the payload field.
        // This avoids extractvalue type mismatches when the Maybe enum packs the
        // payload as an array (e.g. MapEntry{i64,i64} stored as [2 x i64]).
        std::string maybe_alloca = fresh_reg();
        emit_line("  " + maybe_alloca + " = alloca " + maybe_llvm_type);
        emit_line("  store " + maybe_llvm_type + " " + next_result + ", ptr " + maybe_alloca);
        // GEP to field 1 (payload) — this ptr aliases the item data
        std::string item_alloca = fresh_reg();
        emit_line("  " + item_alloca + " = getelementptr inbounds " + maybe_llvm_type + ", ptr " +
                  maybe_alloca + ", i32 0, i32 1");
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

auto LLVMIRGen::gen_for_iterator_with_value(
    const parser::ForExpr& for_expr, const std::string& type_name,
    const std::string& precomputed_iter_val, const std::string& precomputed_iter_type,
    const std::vector<types::TypePtr>& collection_type_args) -> std::string {
    // Temporarily override last_expr_type_ and set up state so gen_for_iterator
    // skips the gen_expr(*for_expr.iter) call and uses our precomputed value.
    // We achieve this by storing the iterator to a local and calling gen_for_iterator.
    precomputed_iter_val_ = precomputed_iter_val;
    precomputed_iter_type_ = precomputed_iter_type;
    use_precomputed_iter_ = true;
    auto result = gen_for_iterator(for_expr, type_name);
    use_precomputed_iter_ = false;
    precomputed_iter_val_.clear();
    precomputed_iter_type_.clear();
    return result;
}

auto LLVMIRGen::gen_for_pointer_stepping(
    const parser::ForExpr& for_expr,
    const std::string& iter_val,
    const std::string& iter_llvm_type,
    const std::string& elem_llvm_type,
    const std::string& saved_loop_start,
    const std::string& saved_loop_end,
    const std::string& saved_loop_stack_save,
    int saved_loop_metadata_id) -> std::string {
    // =========================================================================
    // Direct pointer-stepping for-in loop (SIMD-friendly).
    //
    // Emits the same loop structure as Rust's slice::Iter at O2:
    //   preheader: extract ptr, end, stride from ListIter struct
    //   header:    phi ptr [init, preheader], [next, body]
    //              icmp eq ptr %ptr, %end → exit or body
    //   body:      load T from ptr (direct, no Maybe wrapper)
    //              ... user body ...
    //              gep i8, ptr, stride → ptr.next
    //              br header  (with !llvm.loop vectorize metadata)
    //   exit:
    //
    // This bypasses next() → Maybe[T] entirely, eliminating the discriminant
    // branch that blocks LLVM's LoopVectorizer.
    // =========================================================================

    std::string label_preheader = fresh_label("pstep.preheader");
    std::string label_header = fresh_label("pstep.header");
    std::string label_body = fresh_label("pstep.body");
    std::string label_latch = fresh_label("pstep.latch");
    std::string label_exit = fresh_label("pstep.exit");

    // Set loop labels for break/continue
    current_loop_start_ = label_latch; // continue → go to latch (advance pointer)
    current_loop_end_ = label_exit;
    current_loop_stack_save_ = "";

    // Create aggressive vectorization metadata (width=0 means auto-select)
    current_loop_metadata_id_ = create_loop_metadata(true, 0);

    // Get loop variable name from pattern
    std::string var_name = "_for_item";
    if (for_expr.pattern->is<parser::IdentPattern>()) {
        var_name = for_expr.pattern->as<parser::IdentPattern>().name;
    }

    // ---- Preheader: extract ptr, end, stride from ListIter ----
    emit_line("  br label %" + label_preheader);
    emit_line(label_preheader + ":");
    current_block_ = label_preheader;
    block_terminated_ = false;

    // Store iter struct to alloca so we can GEP its fields
    std::string iter_alloca = fresh_reg();
    emit_line("  " + iter_alloca + " = alloca " + iter_llvm_type);
    emit_line("  store " + iter_llvm_type + " " + iter_val + ", ptr " + iter_alloca);

    // Extract field 0: ptr (current position)
    std::string ptr_field = fresh_reg();
    emit_line("  " + ptr_field + " = getelementptr inbounds " + iter_llvm_type + ", ptr " +
              iter_alloca + ", i32 0, i32 0");
    std::string ptr_init = fresh_reg();
    emit_line("  " + ptr_init + " = load ptr, ptr " + ptr_field);

    // Extract field 1: end (one-past-end pointer)
    std::string end_field = fresh_reg();
    emit_line("  " + end_field + " = getelementptr inbounds " + iter_llvm_type + ", ptr " +
              iter_alloca + ", i32 0, i32 1");
    std::string end_ptr = fresh_reg();
    emit_line("  " + end_ptr + " = load ptr, ptr " + end_field);

    // Extract field 2: stride (bytes per element)
    std::string stride_field = fresh_reg();
    emit_line("  " + stride_field + " = getelementptr inbounds " + iter_llvm_type + ", ptr " +
              iter_alloca + ", i32 0, i32 2");
    std::string stride_val = fresh_reg();
    emit_line("  " + stride_val + " = load i64, ptr " + stride_field);

    emit_line("  br label %" + label_header);

    // ---- Header: phi-based pointer comparison ----
    emit_line(label_header + ":");
    current_block_ = label_header;
    block_terminated_ = false;

    // Pre-allocate next pointer reg name for phi back-reference from latch
    std::string phi_ptr = fresh_reg();
    std::string ptr_next = fresh_reg();
    emit_line("  " + phi_ptr + " = phi ptr [ " + ptr_init + ", %" + label_preheader +
              " ], [ " + ptr_next + ", %" + label_latch + " ]");

    std::string done_cmp = fresh_reg();
    emit_line("  " + done_cmp + " = icmp eq ptr " + phi_ptr + ", " + end_ptr);
    emit_line("  br i1 " + done_cmp + ", label %" + label_exit + ", label %" + label_body);

    // ---- Body: direct element load + user code ----
    emit_line(label_body + ":");
    current_block_ = label_body;
    block_terminated_ = false;

    push_lifetime_scope();

    // Load element directly from pointer — no Maybe wrapper, no discriminant
    std::string elem_val = fresh_reg();
    emit_line("  " + elem_val + " = load " + elem_llvm_type + ", ptr " + phi_ptr + ", align 8");

    // Bind loop variable: store to alloca so body code can take address
    std::string var_alloca = fresh_reg();
    emit_line("  " + var_alloca + " = alloca " + elem_llvm_type);
    emit_line("  store " + elem_llvm_type + " " + elem_val + ", ptr " + var_alloca);
    locals_[var_name] = VarInfo{var_alloca, elem_llvm_type, nullptr, std::nullopt};

    // Generate user body
    gen_expr(*for_expr.body);

    if (!block_terminated_) {
        emit_scope_lifetime_ends();
        // Fall through to latch
        emit_line("  br label %" + label_latch);
    }
    clear_lifetime_scope();

    // ---- Latch: advance pointer and branch back to header ----
    // Separate latch block ensures the phi predecessor is always this block,
    // regardless of how many blocks the body generates (overflow checks, etc.)
    emit_line(label_latch + ":");
    current_block_ = label_latch;
    block_terminated_ = false;

    emit_line("  " + ptr_next + " = getelementptr inbounds i8, ptr " + phi_ptr +
              ", i64 " + stride_val);
    std::string loop_meta = current_loop_metadata_id_ >= 0
                                ? ", !llvm.loop !" + std::to_string(current_loop_metadata_id_)
                                : "";
    emit_line("  br label %" + label_header + loop_meta);

    // ---- Exit ----
    emit_line(label_exit + ":");
    current_block_ = label_exit;
    block_terminated_ = false;

    // Restore saved loop labels
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
