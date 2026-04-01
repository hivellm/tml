TML_MODULE("compiler")

//! # THIR to MIR Builder — Control Flow Expressions
//!
//! This file contains the control-flow expression builders for ThirMirBuilder:
//! - build_if, build_block, build_loop, build_while, build_for
//! - build_return, build_break, build_continue, build_when

#include "mir/thir_mir_builder.hpp"

#include <stdexcept>

namespace tml::mir {

auto ThirMirBuilder::build_if(const thir::ThirIfExpr& if_expr) -> Value {
    auto cond = build_expr(if_expr.condition);
    auto result_type = convert_type(if_expr.type);

    auto then_block = create_block("if.then");
    auto else_block = create_block("if.else");
    auto merge_block = create_block("if.merge");

    emit_cond_branch(cond, then_block, else_block);

    switch_to_block(then_block);
    auto then_val = build_expr(if_expr.then_branch);
    bool then_reaches_merge = !is_terminated();
    uint32_t then_exit_block = ctx_.current_block;
    if (then_reaches_merge) {
        emit_branch(merge_block);
    }

    switch_to_block(else_block);
    Value else_val = const_unit();
    if (if_expr.else_branch) {
        else_val = build_expr(*if_expr.else_branch);
    }
    bool else_reaches_merge = !is_terminated();
    uint32_t else_exit_block = ctx_.current_block;
    if (else_reaches_merge) {
        emit_branch(merge_block);
    }

    switch_to_block(merge_block);

    // If the result type is unit, no phi node is needed
    if (result_type && result_type->is_unit()) {
        return const_unit();
    }

    // If only one branch reaches the merge, return its value directly
    if (then_reaches_merge && !else_reaches_merge) {
        return then_val;
    }
    if (!then_reaches_merge && else_reaches_merge) {
        return else_val;
    }
    if (!then_reaches_merge && !else_reaches_merge) {
        // Neither branch reaches merge (both return/break) - unreachable
        emit_unreachable();
        return const_unit();
    }

    // Both branches reach merge - create phi node to select between values
    PhiInst phi;
    phi.incoming.emplace_back(then_val, then_exit_block);
    phi.incoming.emplace_back(else_val, else_exit_block);
    phi.result_type = result_type;
    return emit(std::move(phi), result_type, if_expr.span);
}

auto ThirMirBuilder::build_block(const thir::ThirBlockExpr& block) -> Value {
    for (const auto& stmt : block.stmts) {
        if (build_stmt(*stmt)) {
            return const_unit();
        }
    }

    if (block.expr) {
        return build_expr(*block.expr);
    }

    return const_unit();
}

auto ThirMirBuilder::build_loop(const thir::ThirLoopExpr& loop) -> Value {
    uint32_t entry_block = ctx_.current_block;
    uint32_t header_block = create_block("loop.header");
    uint32_t body_block = create_block("loop.body");
    uint32_t exit_block = create_block("loop.exit");

    // Save pre-loop variable values for phi node creation
    auto pre_loop_vars = ctx_.variables;

    emit_branch(header_block);
    switch_to_block(header_block);

    // Create phi nodes for scalar pre-loop variables so the condition can see
    // updated values on each iteration (SSA loop-carried dependences).
    // Skip:
    //   1. Array-valued variables — large aggregates that cause stack overflow
    //      when phi'd as values (400-byte phi nodes).
    //   2. Alloca-backed variables (mut_struct_vars) — these hold a constant
    //      alloca pointer that never changes across iterations. Creating a phi
    //      for them would produce a self-referential phi (%vN = phi [%v2, entry],
    //      [%vN, body]) which LLVM rejects. The GEP for array indexing reads
    //      from the alloca pointer directly via mut_struct_vars.
    std::unordered_map<std::string, ValueId> phi_map;
    for (const auto& [var_name, var_value] : pre_loop_vars) {
        if (var_value.id == INVALID_VALUE)
            continue;
        // Skip aggregate array values — cannot be safely phi'd as SSA values
        if (var_value.type && var_value.type->is_array())
            continue;
        // Skip alloca-backed variables — pointer is constant, needs no phi
        if (ctx_.mut_struct_vars.count(var_name) > 0)
            continue;

        PhiInst phi;
        phi.incoming = {{var_value, entry_block}};
        phi.result_type = var_value.type;
        Value phi_result = emit(phi, var_value.type);
        phi_map[var_name] = phi_result.id;
        set_variable(var_name, phi_result);
    }

    // Save header-block variable snapshot for use at the exit block
    auto header_vars = ctx_.variables;

    ctx_.loop_stack.push({header_block, exit_block, std::nullopt, {}});

    // Evaluate condition using phi values
    auto cond = build_expr(loop.condition);
    emit_cond_branch(cond, body_block, exit_block);

    // Body
    switch_to_block(body_block);
    ctx_.push_drop_scope();
    (void)build_expr(loop.body);
    emit_scope_drops();
    ctx_.pop_drop_scope();

    uint32_t body_end_block = ctx_.current_block;

    // Complete phi back-edges with the values updated during the body
    if (!is_terminated()) {
        auto* header = ctx_.current_func->get_block(header_block);
        if (header) {
            for (auto& inst : header->instructions) {
                if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                    for (const auto& [var_name, phi_id] : phi_map) {
                        if (inst.result == phi_id) {
                            auto it = ctx_.variables.find(var_name);
                            if (it != ctx_.variables.end()) {
                                phi->incoming.push_back({it->second, body_end_block});
                            }
                            break;
                        }
                    }
                }
            }
        }
        emit_branch(header_block);
    }

    auto break_sources = ctx_.loop_stack.top().break_sources;

    switch_to_block(exit_block);
    ctx_.loop_stack.pop();

    // Update variables at exit with header values (condition-false path),
    // merging with any break-source values via phi nodes if needed
    for (const auto& [var_name, header_val] : header_vars) {
        if (header_val.id == INVALID_VALUE)
            continue;

        if (break_sources.empty()) {
            set_variable(var_name, header_val);
        } else {
            bool needs_phi = false;
            for (const auto& [break_block, break_vars] : break_sources) {
                auto it = break_vars.find(var_name);
                if (it != break_vars.end() && it->second.id != header_val.id) {
                    needs_phi = true;
                    break;
                }
            }

            if (needs_phi) {
                PhiInst exit_phi;
                exit_phi.result_type = header_val.type;
                exit_phi.incoming.push_back({header_val, header_block});
                for (const auto& [break_block, break_vars] : break_sources) {
                    auto it = break_vars.find(var_name);
                    if (it != break_vars.end()) {
                        exit_phi.incoming.push_back({it->second, break_block});
                    } else {
                        exit_phi.incoming.push_back({header_val, break_block});
                    }
                }
                Value exit_val = emit(exit_phi, header_val.type);
                set_variable(var_name, exit_val);
            } else {
                set_variable(var_name, header_val);
            }
        }
    }

    return const_unit();
}

auto ThirMirBuilder::build_while(const thir::ThirWhileExpr& while_expr) -> Value {
    uint32_t entry_block = ctx_.current_block;
    uint32_t header_block = create_block("while.header");
    uint32_t body_block = create_block("while.body");
    uint32_t exit_block = create_block("while.exit");

    // Save pre-loop variable values for phi node creation
    auto pre_loop_vars = ctx_.variables;

    emit_branch(header_block);
    switch_to_block(header_block);

    // Create phi nodes for scalar pre-loop variables so the condition sees
    // updated values on each iteration. Skip:
    //   1. Array-valued variables — large aggregates causing stack overflow.
    //   2. Alloca-backed variables (mut_struct_vars) — constant alloca pointer,
    //      creating a phi would yield a self-referential phi that LLVM rejects.
    std::unordered_map<std::string, ValueId> phi_map;
    for (const auto& [var_name, var_value] : pre_loop_vars) {
        if (var_value.id == INVALID_VALUE)
            continue;
        if (var_value.type && var_value.type->is_array())
            continue;
        if (ctx_.mut_struct_vars.count(var_name) > 0)
            continue;

        PhiInst phi;
        phi.incoming = {{var_value, entry_block}};
        phi.result_type = var_value.type;
        Value phi_result = emit(phi, var_value.type);
        phi_map[var_name] = phi_result.id;
        set_variable(var_name, phi_result);
    }

    // Save header-block variable snapshot for use at the exit block
    auto header_vars = ctx_.variables;

    ctx_.loop_stack.push({header_block, exit_block, std::nullopt, {}});

    // Evaluate condition using phi values
    auto cond = build_expr(while_expr.condition);
    emit_cond_branch(cond, body_block, exit_block);

    // Body
    switch_to_block(body_block);
    ctx_.push_drop_scope();
    (void)build_expr(while_expr.body);
    emit_scope_drops();
    ctx_.pop_drop_scope();

    uint32_t body_end_block = ctx_.current_block;

    // Complete phi back-edges with the values updated during the body
    if (!is_terminated()) {
        auto* header = ctx_.current_func->get_block(header_block);
        if (header) {
            for (auto& inst : header->instructions) {
                if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                    for (const auto& [var_name, phi_id] : phi_map) {
                        if (inst.result == phi_id) {
                            auto it = ctx_.variables.find(var_name);
                            if (it != ctx_.variables.end()) {
                                phi->incoming.push_back({it->second, body_end_block});
                            }
                            break;
                        }
                    }
                }
            }
        }
        emit_branch(header_block);
    }

    auto break_sources = ctx_.loop_stack.top().break_sources;

    switch_to_block(exit_block);
    ctx_.loop_stack.pop();

    // Update variables at exit with header values (condition-false path),
    // merging with any break-source values via phi nodes if needed
    for (const auto& [var_name, header_val] : header_vars) {
        if (header_val.id == INVALID_VALUE)
            continue;

        if (break_sources.empty()) {
            set_variable(var_name, header_val);
        } else {
            bool needs_phi = false;
            for (const auto& [break_block, break_vars] : break_sources) {
                auto it = break_vars.find(var_name);
                if (it != break_vars.end() && it->second.id != header_val.id) {
                    needs_phi = true;
                    break;
                }
            }

            if (needs_phi) {
                PhiInst exit_phi;
                exit_phi.result_type = header_val.type;
                exit_phi.incoming.push_back({header_val, header_block});
                for (const auto& [break_block, break_vars] : break_sources) {
                    auto it = break_vars.find(var_name);
                    if (it != break_vars.end()) {
                        exit_phi.incoming.push_back({it->second, break_block});
                    } else {
                        exit_phi.incoming.push_back({header_val, break_block});
                    }
                }
                Value exit_val = emit(exit_phi, header_val.type);
                set_variable(var_name, exit_val);
            } else {
                set_variable(var_name, header_val);
            }
        }
    }

    return const_unit();
}

auto ThirMirBuilder::build_for(const thir::ThirForExpr& for_expr) -> Value {
    auto iter_val = build_expr(for_expr.iter);

    auto header = create_block("for.header");
    auto body = create_block("for.body");
    auto exit = create_block("for.exit");

    BuildContext::LoopContext lc;
    lc.header_block = header;
    lc.exit_block = exit;
    ctx_.loop_stack.push(std::move(lc));

    emit_branch(header);
    switch_to_block(header);

    auto result_type = convert_type(for_expr.type);
    CallInst next_inst;
    next_inst.func_name = "next";
    next_inst.args = {iter_val};
    next_inst.arg_types = {iter_val.type};
    next_inst.return_type = result_type;
    auto next_val = emit(std::move(next_inst), result_type);

    // Simplified: check for value
    auto has_value = const_bool(true);
    emit_cond_branch(has_value, body, exit);

    switch_to_block(body);
    build_pattern_binding(for_expr.pattern, next_val);
    (void)build_expr(for_expr.body);
    if (!is_terminated()) {
        emit_branch(header);
    }

    ctx_.loop_stack.pop();
    switch_to_block(exit);
    return const_unit();
}

auto ThirMirBuilder::build_return(const thir::ThirReturnExpr& ret) -> Value {
    if (ret.value) {
        // Propagate function return type to value expression for literal coercion
        auto saved_context = context_type_;
        context_type_ = current_return_type_;
        auto val = build_expr(*ret.value);
        context_type_ = saved_context;
        emit_all_drops(); // Drop all local variables before explicit return
        emit_return(val);
    } else {
        emit_all_drops(); // Drop all local variables before explicit return
        emit_return();
    }
    return const_unit();
}

auto ThirMirBuilder::build_break(const thir::ThirBreakExpr& /*brk*/) -> Value {
    if (ctx_.loop_stack.empty())
        return const_unit();

    emit_scope_drops();

    auto& loop_ctx = ctx_.loop_stack.top();
    // Record current variable values so the exit block can build phi nodes
    loop_ctx.break_sources.push_back({ctx_.current_block, ctx_.variables});

    emit_branch(loop_ctx.exit_block);
    return const_unit();
}

auto ThirMirBuilder::build_continue(const thir::ThirContinueExpr& /*cont*/) -> Value {
    if (ctx_.loop_stack.empty())
        return const_unit();

    emit_scope_drops();
    emit_branch(ctx_.loop_stack.top().header_block);
    return const_unit();
}

auto ThirMirBuilder::build_when(const thir::ThirWhenExpr& when) -> Value {
    auto scrutinee = build_expr(when.scrutinee);
    auto result_type = convert_type(when.type);
    auto merge_block = create_block("when.merge");

    // Collect (value, exit_block_id) pairs for phi node construction
    std::vector<std::pair<Value, uint32_t>> phi_incoming;

    for (size_t i = 0; i < when.arms.size(); ++i) {
        const auto& arm = when.arms[i];
        auto match_block = create_block("when.arm." + std::to_string(i));
        auto next_block = (i + 1 < when.arms.size())
                              ? create_block("when.next." + std::to_string(i))
                              : merge_block;

        auto cond = build_pattern_match(arm.pattern, scrutinee);

        if (arm.guard) {
            auto guard_block = create_block("when.guard." + std::to_string(i));
            emit_cond_branch(cond, guard_block, next_block);
            switch_to_block(guard_block);
            auto guard_cond = build_expr(*arm.guard);
            emit_cond_branch(guard_cond, match_block, next_block);
        } else {
            emit_cond_branch(cond, match_block, next_block);
        }

        switch_to_block(match_block);
        build_pattern_binding(arm.pattern, scrutinee);
        auto arm_val = build_expr(arm.body);
        if (!is_terminated()) {
            uint32_t arm_exit_block = ctx_.current_block;
            emit_branch(merge_block);
            phi_incoming.emplace_back(arm_val, arm_exit_block);
        }

        if (i + 1 < when.arms.size()) {
            switch_to_block(next_block);
        }
    }

    switch_to_block(merge_block);

    // If the result type is unit, no phi node is needed
    if (result_type && result_type->is_unit()) {
        return const_unit();
    }

    // If no arms reach merge, this is unreachable
    if (phi_incoming.empty()) {
        emit_unreachable();
        return const_unit();
    }

    // If exactly one arm reaches merge, return its value directly
    if (phi_incoming.size() == 1) {
        return phi_incoming[0].first;
    }

    // Multiple arms reach merge - create phi node
    PhiInst phi;
    phi.incoming = std::move(phi_incoming);
    phi.result_type = result_type;
    return emit(std::move(phi), result_type, when.span);
}

} // namespace tml::mir
