TML_MODULE("compiler")

//! # THIR to MIR Builder — Control Flow Expressions
//!
//! This file contains the control-flow expression builders for ThirMirBuilder:
//! - build_if, build_block, build_loop, build_while, build_for
//! - build_return, build_break, build_continue, build_when

#include "mir/thir_mir_builder.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

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
    // Check if the iterator is a Range/RangeInclusive struct — desugar to counter loop.
    // Range { start, end } is created by HIR desugaring of `a to b` / `a through b`.
    auto iter_type = for_expr.iter->type();
    bool is_range = false;
    bool inclusive = false;
    if (iter_type && iter_type->is<types::NamedType>()) {
        const auto& named = iter_type->as<types::NamedType>();
        if (named.name == "Range") {
            is_range = true;
            inclusive = false;
        } else if (named.name == "RangeInclusive") {
            is_range = true;
            inclusive = true;
        }
    }

    if (is_range) {
        return build_for_range(for_expr, inclusive);
    }

    // Generic iterator path: call next() in a loop
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

auto ThirMirBuilder::build_for_range(const thir::ThirForExpr& for_expr, bool inclusive) -> Value {
    // Desugar `for i in a to b { body }` into a counter loop.
    // The iter expression is a Range { start, end } struct created by HIR desugaring.
    // Instead of building the Range struct and extracting fields, we directly access
    // the start/end sub-expressions from the ThirStructExpr to avoid type mangling issues.

    // Determine the element type (I32 or I64) from Range[T]
    auto elem_type = mir::make_i32_type();
    auto iter_type = for_expr.iter->type();
    if (iter_type && iter_type->is<types::NamedType>()) {
        const auto& named = iter_type->as<types::NamedType>();
        if (!named.type_args.empty()) {
            elem_type = convert_type(named.type_args[0]);
        }
    }

    // Extract start/end values directly from the Range struct expression
    Value start_val = const_int(0);
    Value end_val = const_int(0);

    // The iter expr should be a ThirStructExpr with fields "start" and "end"
    if (auto* struct_expr = std::get_if<thir::ThirStructExpr>(&for_expr.iter->kind)) {
        for (const auto& [name, expr] : struct_expr->fields) {
            if (name == "start") {
                start_val = build_expr(expr);
            } else if (name == "end") {
                end_val = build_expr(expr);
            }
        }
    } else {
        // Fallback: build the range value and extract fields
        auto range_val = build_expr(for_expr.iter);
        start_val = range_val;
        end_val = range_val;
    }

    // Allocate the loop counter variable
    AllocaInst counter_alloca;
    counter_alloca.alloc_type = elem_type;
    auto counter_ptr = emit(std::move(counter_alloca), mir::make_ptr_type(), for_expr.span);

    // Store initial value (start)
    StoreInst init_store;
    init_store.ptr = counter_ptr;
    init_store.value = start_val;
    init_store.value_type = elem_type;
    (void)emit(std::move(init_store), nullptr, for_expr.span);

    // Create loop blocks
    auto header = create_block("for.header");
    auto body_block = create_block("for.body");
    auto latch = create_block("for.latch");
    auto exit = create_block("for.exit");

    BuildContext::LoopContext lc;
    lc.header_block = latch; // continue goes to latch (increment + backedge)
    lc.exit_block = exit;
    ctx_.loop_stack.push(std::move(lc));

    emit_branch(header);
    switch_to_block(header);

    // Load current counter value
    LoadInst load_counter;
    load_counter.ptr = counter_ptr;
    load_counter.result_type = elem_type;
    auto current = emit(std::move(load_counter), elem_type, for_expr.span);

    // Compare: current < end (exclusive) or current <= end (inclusive)
    BinaryInst cmp;
    cmp.op = inclusive ? BinOp::Le : BinOp::Lt;
    cmp.left = current;
    cmp.right = end_val;
    auto cond = emit(std::move(cmp), mir::make_bool_type(), for_expr.span);

    emit_cond_branch(cond, body_block, exit);

    // Body: bind pattern to current value, execute body
    switch_to_block(body_block);
    build_pattern_binding(for_expr.pattern, current);
    (void)build_expr(for_expr.body);
    if (!is_terminated()) {
        emit_branch(latch);
    }

    // Latch: increment counter and loop back to header
    switch_to_block(latch);
    LoadInst reload;
    reload.ptr = counter_ptr;
    reload.result_type = elem_type;
    auto current_latch = emit(std::move(reload), elem_type, for_expr.span);

    // Determine bit width from element type (I32=32, I64=64)
    int bit_width = 32;
    if (elem_type) {
        if (auto* prim = std::get_if<MirPrimitiveType>(&elem_type->kind)) {
            if (prim->kind == PrimitiveType::I64 || prim->kind == PrimitiveType::U64) {
                bit_width = 64;
            }
        }
    }
    auto one = const_int(1, bit_width);
    BinaryInst increment;
    increment.op = BinOp::Add;
    increment.left = current_latch;
    increment.right = one;
    auto next_counter = emit(std::move(increment), elem_type, for_expr.span);

    StoreInst store_next;
    store_next.ptr = counter_ptr;
    store_next.value = next_counter;
    store_next.value_type = elem_type;
    (void)emit(std::move(store_next), nullptr, for_expr.span);

    emit_branch(header);

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

// Classification of `when` arms for switch lowering. Each arm is either
// a constant case (integer literal or payloadless enum variant pattern),
// a catch-all (wildcard / binding / unmatched last arm), or an unhandled
// pattern that forces fallback to linear if/else codegen.
namespace {

enum class WhenArmKind : uint8_t {
    ConstCase, // literal int or enum-tag case with known int discriminator
    Wildcard,  // _ or binding — becomes default
    Unhandled, // guard, tuple, range, or or-pattern — bail out to linear
};

struct WhenArmClass {
    WhenArmKind kind = WhenArmKind::Unhandled;
    int64_t case_value = 0; // valid only when kind == ConstCase
};

// Switch lowering uses the tag field of an enum when the scrutinee is an
// enum type and all arms reference the same enum. We track which mode we
// are in so the code generator knows whether to extract the tag.
enum class SwitchDiscKind : uint8_t {
    Integer, // scrutinee itself is integer-typed
    EnumTag, // discriminator is enum.tag (extractvalue idx 0)
};

constexpr size_t kMinSwitchArms = 4;

} // namespace

auto ThirMirBuilder::build_when(const thir::ThirWhenExpr& when) -> Value {
    auto scrutinee = build_expr(when.scrutinee);
    auto result_type = convert_type(when.type);
    auto merge_block = create_block("when.merge");

    // ---------------------------------------------------------------------
    // Phase 1: classify arms to decide between switch and linear lowering.
    //
    // Switch form requires: all non-default arms are constant integer or
    // payloadless enum-variant patterns with no guards, at most one default
    // (wildcard/binding) arm, and >= kMinSwitchArms cases. This rules out
    // or-patterns, ranges, and tuple/struct destructuring — those still go
    // through the linear path since they can't be expressed as LLVM switch
    // cases directly.
    // ---------------------------------------------------------------------
    std::vector<WhenArmClass> arm_classes;
    arm_classes.reserve(when.arms.size());

    SwitchDiscKind disc_kind = SwitchDiscKind::Integer;
    bool can_use_switch = true;
    bool has_enum_arm = false;
    bool has_int_arm = false;
    size_t const_case_count = 0;
    std::ptrdiff_t default_arm_index = -1;

    for (size_t i = 0; i < when.arms.size(); ++i) {
        const auto& arm = when.arms[i];
        WhenArmClass cls;

        if (arm.guard) {
            cls.kind = WhenArmKind::Unhandled;
            can_use_switch = false;
        } else if (!arm.pattern) {
            cls.kind = WhenArmKind::Wildcard;
        } else if (arm.pattern->is<thir::ThirWildcardPattern>() ||
                   arm.pattern->is<thir::ThirBindingPattern>()) {
            cls.kind = WhenArmKind::Wildcard;
        } else if (arm.pattern->is<thir::ThirLiteralPattern>()) {
            const auto& lp = arm.pattern->as<thir::ThirLiteralPattern>();
            if (std::holds_alternative<int64_t>(lp.value)) {
                cls.kind = WhenArmKind::ConstCase;
                cls.case_value = std::get<int64_t>(lp.value);
                has_int_arm = true;
                ++const_case_count;
            } else if (std::holds_alternative<uint64_t>(lp.value)) {
                cls.kind = WhenArmKind::ConstCase;
                cls.case_value = static_cast<int64_t>(std::get<uint64_t>(lp.value));
                has_int_arm = true;
                ++const_case_count;
            } else {
                cls.kind = WhenArmKind::Unhandled;
                can_use_switch = false;
            }
        } else if (arm.pattern->is<thir::ThirEnumPattern>()) {
            const auto& ep = arm.pattern->as<thir::ThirEnumPattern>();
            // Payloadless variant -> pure tag switch. Variants with payload
            // bindings still need per-case extractvalue logic that the
            // linear path handles via build_pattern_binding, so defer those.
            bool has_bindings = ep.payload.has_value() && !ep.payload->empty();
            if (!has_bindings) {
                cls.kind = WhenArmKind::ConstCase;
                cls.case_value = static_cast<int64_t>(ep.variant_index);
                has_enum_arm = true;
                ++const_case_count;
            } else {
                cls.kind = WhenArmKind::Unhandled;
                can_use_switch = false;
            }
        } else {
            cls.kind = WhenArmKind::Unhandled;
            can_use_switch = false;
        }

        if (cls.kind == WhenArmKind::Wildcard) {
            // Only the last wildcard can act as default; earlier wildcards
            // in a `when` are legal TML but make switch lowering ambiguous
            // because they short-circuit later cases.
            if (i + 1 == when.arms.size()) {
                default_arm_index = static_cast<std::ptrdiff_t>(i);
            } else {
                can_use_switch = false;
            }
        }

        arm_classes.push_back(cls);
    }

    // Can't mix integer and enum arms in one switch (they refer to
    // different discriminators). Also require a minimum case count so
    // small `when` forms don't pay the jump-table overhead.
    if (has_enum_arm && has_int_arm) {
        can_use_switch = false;
    }
    if (const_case_count < kMinSwitchArms) {
        can_use_switch = false;
    }

    // Detect duplicate case values. LLVM `switch` terminators require
    // unique cases; duplicates in TML source mean the later arm is
    // unreachable but not a hard error, so we fall back to linear
    // lowering to preserve first-match semantics.
    if (can_use_switch) {
        std::vector<int64_t> seen;
        seen.reserve(const_case_count);
        for (const auto& cls : arm_classes) {
            if (cls.kind != WhenArmKind::ConstCase)
                continue;
            for (int64_t v : seen) {
                if (v == cls.case_value) {
                    can_use_switch = false;
                    break;
                }
            }
            if (!can_use_switch)
                break;
            seen.push_back(cls.case_value);
        }
    }

    if (has_enum_arm) {
        disc_kind = SwitchDiscKind::EnumTag;
    }

    // ---------------------------------------------------------------------
    // Phase 2a: fast path — emit a single `switch` terminator.
    // ---------------------------------------------------------------------
    if (can_use_switch) {
        Value discriminant;
        if (disc_kind == SwitchDiscKind::EnumTag) {
            // Extract the tag (i32 at index 0 of the enum struct).
            ExtractValueInst extract_tag;
            extract_tag.aggregate = scrutinee;
            extract_tag.indices = {0};
            extract_tag.aggregate_type = scrutinee.type;
            extract_tag.result_type = make_i32_type();
            discriminant = emit(std::move(extract_tag), make_i32_type());
        } else {
            discriminant = scrutinee;
        }

        // Create one block per const case; the default block is either
        // the trailing wildcard arm or a freshly-minted unreachable block.
        std::vector<uint32_t> arm_blocks(when.arms.size(), 0);
        for (size_t i = 0; i < when.arms.size(); ++i) {
            arm_blocks[i] = create_block("when.arm." + std::to_string(i));
        }
        uint32_t default_block = 0;
        bool synthetic_default = false;
        if (default_arm_index >= 0) {
            default_block = arm_blocks[static_cast<size_t>(default_arm_index)];
        } else {
            default_block = create_block("when.default");
            synthetic_default = true;
        }

        std::vector<std::pair<int64_t, uint32_t>> switch_cases;
        switch_cases.reserve(const_case_count);
        for (size_t i = 0; i < when.arms.size(); ++i) {
            if (arm_classes[i].kind == WhenArmKind::ConstCase) {
                switch_cases.emplace_back(arm_classes[i].case_value, arm_blocks[i]);
            }
        }

        emit_switch(discriminant, std::move(switch_cases), default_block);

        std::vector<std::pair<Value, uint32_t>> phi_incoming;
        phi_incoming.reserve(when.arms.size());

        // Emit each case block with its body + branch to merge.
        for (size_t i = 0; i < when.arms.size(); ++i) {
            switch_to_block(arm_blocks[i]);
            // Wildcard binding (e.g. `let x => ...`) still needs its
            // pattern binding so the body can reference the scrutinee.
            build_pattern_binding(when.arms[i].pattern, scrutinee);
            auto arm_val = build_expr(when.arms[i].body);
            if (!is_terminated()) {
                uint32_t arm_exit_block = ctx_.current_block;
                emit_branch(merge_block);
                phi_incoming.emplace_back(arm_val, arm_exit_block);
            }
        }

        // Synthetic default: no user-supplied catch-all, so the case is
        // semantically unreachable (checker must have proved exhaustiveness).
        if (synthetic_default) {
            switch_to_block(default_block);
            emit_unreachable();
        }

        switch_to_block(merge_block);

        if (result_type && result_type->is_unit()) {
            return const_unit();
        }
        if (phi_incoming.empty()) {
            emit_unreachable();
            return const_unit();
        }
        if (phi_incoming.size() == 1) {
            return phi_incoming[0].first;
        }

        PhiInst phi;
        phi.incoming = std::move(phi_incoming);
        phi.result_type = result_type;
        return emit(std::move(phi), result_type, when.span);
    }

    // ---------------------------------------------------------------------
    // Phase 2b: linear if/else chain for patterns switch can't express.
    // ---------------------------------------------------------------------
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
