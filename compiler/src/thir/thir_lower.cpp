//! # HIR -> THIR Lowering Implementation
//!
//! Transforms HIR to THIR with explicit coercions, resolved dispatch, and
//! exhaustiveness checking.

#include "thir/thir_lower.hpp"

#include "hir/hir_stmt.hpp"

namespace tml::thir {

// ============================================================================
// Construction
// ============================================================================

ThirLower::ThirLower(const types::TypeEnv& env, traits::TraitSolver& solver)
    : env_(&env), solver_(&solver), normalizer_(env, solver), exhaustiveness_(env) {}

// ============================================================================
// Module-Level Lowering
// ============================================================================

auto ThirLower::lower_module(const hir::HirModule& hir) -> ThirModule {
    ThirModule result;
    result.name = hir.name;
    result.source_path = hir.source_path;
    result.imports = hir.imports;

    // Lower type declarations (no expression lowering needed)
    for (const auto& s : hir.structs) {
        result.structs.push_back(lower_struct(s));
    }
    for (const auto& e : hir.enums) {
        result.enums.push_back(lower_enum(e));
    }
    for (const auto& b : hir.behaviors) {
        result.behaviors.push_back(lower_behavior(b));
    }

    // Lower impl blocks (contains methods with bodies)
    for (const auto& impl : hir.impls) {
        result.impls.push_back(lower_impl(impl));
    }

    // Lower free functions
    for (const auto& f : hir.functions) {
        result.functions.push_back(lower_function(f));
    }

    // Lower constants
    for (const auto& c : hir.constants) {
        result.constants.push_back(lower_const(c));
    }

    return result;
}

auto ThirLower::lower_struct(const hir::HirStruct& s) -> ThirStruct {
    ThirStruct result;
    result.id = s.id;
    result.name = s.name;
    result.mangled_name = s.mangled_name;
    result.is_public = s.is_public;
    result.span = s.span;

    for (const auto& f : s.fields) {
        result.fields.push_back({f.name, f.type, f.is_public, f.span});
    }
    return result;
}

auto ThirLower::lower_enum(const hir::HirEnum& e) -> ThirEnum {
    ThirEnum result;
    result.id = e.id;
    result.name = e.name;
    result.mangled_name = e.mangled_name;
    result.is_public = e.is_public;
    result.span = e.span;

    for (const auto& v : e.variants) {
        result.variants.push_back({v.name, v.index, v.payload_types, v.span});
    }
    return result;
}

auto ThirLower::lower_behavior(const hir::HirBehavior& b) -> ThirBehavior {
    ThirBehavior result;
    result.id = b.id;
    result.name = b.name;
    result.super_behaviors = b.super_behaviors;
    result.is_public = b.is_public;
    result.span = b.span;

    for (const auto& m : b.methods) {
        ThirBehaviorMethod method;
        method.name = m.name;
        method.return_type = m.return_type;
        method.has_default_impl = m.has_default_impl;
        method.span = m.span;

        for (const auto& p : m.params) {
            method.params.push_back({p.name, p.type, p.is_mut, p.span});
        }

        if (m.default_body) {
            method.default_body = lower_expr(*m.default_body);
        }

        result.methods.push_back(std::move(method));
    }
    return result;
}

auto ThirLower::lower_impl(const hir::HirImpl& impl) -> ThirImpl {
    ThirImpl result;
    result.id = impl.id;
    result.behavior_name = impl.behavior_name;
    result.type_name = impl.type_name;
    result.self_type = impl.self_type;
    result.span = impl.span;

    for (const auto& m : impl.methods) {
        result.methods.push_back(lower_function(m));
    }
    return result;
}

auto ThirLower::lower_function(const hir::HirFunction& func) -> ThirFunction {
    ThirFunction result;
    result.id = func.id;
    result.name = func.name;
    result.mangled_name = func.mangled_name;
    result.return_type = func.return_type;
    result.is_public = func.is_public;
    result.is_async = func.is_async;
    result.is_extern = func.is_extern;
    result.extern_abi = func.extern_abi;
    result.attributes = func.attributes;
    result.span = func.span;

    for (const auto& p : func.params) {
        result.params.push_back({p.name, p.type, p.is_mut, p.span});
    }

    if (func.body) {
        result.body = lower_expr(*func.body);
    }

    return result;
}

auto ThirLower::lower_const(const hir::HirConst& c) -> ThirConst {
    ThirConst result;
    result.id = c.id;
    result.name = c.name;
    result.type = c.type;
    result.is_public = c.is_public;
    result.span = c.span;
    result.value = lower_expr(c.value);
    return result;
}

// ============================================================================
// Expression Lowering
// ============================================================================

auto ThirLower::lower_expr(const hir::HirExprPtr& expr) -> ThirExprPtr {
    if (!expr)
        return nullptr;

    return std::visit(
        [this](const auto& e) -> ThirExprPtr {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, hir::HirLiteralExpr>) {
                return lower_literal(e);
            } else if constexpr (std::is_same_v<T, hir::HirVarExpr>) {
                return lower_var(e);
            } else if constexpr (std::is_same_v<T, hir::HirBinaryExpr>) {
                return lower_binary(e);
            } else if constexpr (std::is_same_v<T, hir::HirUnaryExpr>) {
                return lower_unary(e);
            } else if constexpr (std::is_same_v<T, hir::HirCallExpr>) {
                return lower_call(e);
            } else if constexpr (std::is_same_v<T, hir::HirMethodCallExpr>) {
                return lower_method_call(e);
            } else if constexpr (std::is_same_v<T, hir::HirFieldExpr>) {
                return lower_field(e);
            } else if constexpr (std::is_same_v<T, hir::HirIndexExpr>) {
                return lower_index(e);
            } else if constexpr (std::is_same_v<T, hir::HirTupleExpr>) {
                return lower_tuple(e);
            } else if constexpr (std::is_same_v<T, hir::HirArrayExpr>) {
                return lower_array(e);
            } else if constexpr (std::is_same_v<T, hir::HirArrayRepeatExpr>) {
                return lower_array_repeat(e);
            } else if constexpr (std::is_same_v<T, hir::HirStructExpr>) {
                return lower_struct_expr(e);
            } else if constexpr (std::is_same_v<T, hir::HirEnumExpr>) {
                return lower_enum_expr(e);
            } else if constexpr (std::is_same_v<T, hir::HirBlockExpr>) {
                return lower_block(e);
            } else if constexpr (std::is_same_v<T, hir::HirIfExpr>) {
                return lower_if(e);
            } else if constexpr (std::is_same_v<T, hir::HirWhenExpr>) {
                return lower_when(e);
            } else if constexpr (std::is_same_v<T, hir::HirLoopExpr>) {
                return lower_loop(e);
            } else if constexpr (std::is_same_v<T, hir::HirWhileExpr>) {
                return lower_while(e);
            } else if constexpr (std::is_same_v<T, hir::HirForExpr>) {
                return lower_for(e);
            } else if constexpr (std::is_same_v<T, hir::HirReturnExpr>) {
                return lower_return(e);
            } else if constexpr (std::is_same_v<T, hir::HirBreakExpr>) {
                return lower_break(e);
            } else if constexpr (std::is_same_v<T, hir::HirContinueExpr>) {
                return lower_continue(e);
            } else if constexpr (std::is_same_v<T, hir::HirClosureExpr>) {
                return lower_closure(e);
            } else if constexpr (std::is_same_v<T, hir::HirCastExpr>) {
                return lower_cast(e);
            } else if constexpr (std::is_same_v<T, hir::HirTryExpr>) {
                return lower_try(e);
            } else if constexpr (std::is_same_v<T, hir::HirAwaitExpr>) {
                return lower_await(e);
            } else if constexpr (std::is_same_v<T, hir::HirAssignExpr>) {
                return lower_assign(e);
            } else if constexpr (std::is_same_v<T, hir::HirCompoundAssignExpr>) {
                return lower_compound_assign(e);
            } else if constexpr (std::is_same_v<T, hir::HirLowlevelExpr>) {
                return lower_lowlevel(e);
            } else {
                // Should not reach here
                return nullptr;
            }
        },
        expr->kind);
}

// ============================================================================
// Individual Expression Lowering
// ============================================================================

auto ThirLower::lower_literal(const hir::HirLiteralExpr& lit) -> ThirExprPtr {
    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirLiteralExpr{lit.id, lit.value, lit.type, lit.span};
    return result;
}

auto ThirLower::lower_var(const hir::HirVarExpr& var) -> ThirExprPtr {
    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirVarExpr{var.id, var.name, var.type, var.span};
    return result;
}

auto ThirLower::lower_binary(const hir::HirBinaryExpr& bin) -> ThirExprPtr {
    auto left = lower_expr(bin.left);
    auto right = lower_expr(bin.right);

    // Check if this is operator overloading (non-primitive types)
    auto left_type = left ? left->type() : nullptr;
    auto right_type = right ? right->type() : nullptr;

    std::optional<ResolvedMethod> op_method;

    // Only desugar operators for non-primitive types
    if (left_type && !is_primitive_numeric(left_type)) {
        auto behavior_method = op_behavior_method(bin.op);
        if (behavior_method) {
            auto& [behavior, method] = *behavior_method;
            // Check if the type implements the behavior
            traits::TraitGoal goal{left_type, behavior, {}, bin.span};
            auto solve_result = solver_->solve(goal);
            if (auto* candidate = std::get_if<traits::TraitCandidate>(&solve_result)) {
                op_method =
                    ResolvedMethod{candidate->impl_type + "::" + method, behavior, {}, false};
            }
        }
    }

    // Insert coercions if operand types don't match and it's a primitive op
    if (!op_method && left_type && right_type) {
        auto coercion = needs_coercion(left_type, right_type);
        if (coercion) {
            left = coerce(std::move(left), right_type);
        } else {
            auto reverse = needs_coercion(right_type, left_type);
            if (reverse) {
                right = coerce(std::move(right), left_type);
            }
        }
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirBinaryExpr{bin.id,   bin.op,    std::move(left), std::move(right),
                                  bin.type, op_method, bin.span};
    return result;
}

auto ThirLower::lower_unary(const hir::HirUnaryExpr& un) -> ThirExprPtr {
    auto operand = lower_expr(un.operand);

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirUnaryExpr{un.id, un.op, std::move(operand), un.type, un.span};
    return result;
}

auto ThirLower::lower_call(const hir::HirCallExpr& call) -> ThirExprPtr {
    std::vector<ThirExprPtr> args;
    for (const auto& arg : call.args) {
        args.push_back(lower_expr(arg));
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirCallExpr{call.id,         call.func_name, call.type_args,
                                std::move(args), call.type,      call.span};
    return result;
}

auto ThirLower::lower_method_call(const hir::HirMethodCallExpr& call) -> ThirExprPtr {
    auto receiver = lower_expr(call.receiver);

    std::vector<ThirExprPtr> args;
    for (const auto& arg : call.args) {
        args.push_back(lower_expr(arg));
    }

    // Resolve method dispatch via trait solver
    auto resolved = resolve_method(call);

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirMethodCallExpr{call.id,         std::move(receiver), std::move(resolved),
                                      std::move(args), call.receiver_type,  call.type,
                                      call.span};
    return result;
}

auto ThirLower::lower_field(const hir::HirFieldExpr& field) -> ThirExprPtr {
    auto object = lower_expr(field.object);

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirFieldExpr{field.id,          std::move(object), field.field_name,
                                 field.field_index, field.type,        field.span};
    return result;
}

auto ThirLower::lower_index(const hir::HirIndexExpr& idx) -> ThirExprPtr {
    auto object = lower_expr(idx.object);
    auto index = lower_expr(idx.index);

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirIndexExpr{idx.id, std::move(object), std::move(index), idx.type, idx.span};
    return result;
}

auto ThirLower::lower_tuple(const hir::HirTupleExpr& tuple) -> ThirExprPtr {
    std::vector<ThirExprPtr> elements;
    for (const auto& elem : tuple.elements) {
        elements.push_back(lower_expr(elem));
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirTupleExpr{tuple.id, std::move(elements), tuple.type, tuple.span};
    return result;
}

auto ThirLower::lower_array(const hir::HirArrayExpr& arr) -> ThirExprPtr {
    std::vector<ThirExprPtr> elements;
    for (const auto& elem : arr.elements) {
        elements.push_back(lower_expr(elem));
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind =
        ThirArrayExpr{arr.id, std::move(elements), arr.element_type, arr.size, arr.type, arr.span};
    return result;
}

auto ThirLower::lower_array_repeat(const hir::HirArrayRepeatExpr& arr) -> ThirExprPtr {
    auto value = lower_expr(arr.value);

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirArrayRepeatExpr{arr.id, std::move(value), arr.count, arr.type, arr.span};
    return result;
}

auto ThirLower::lower_struct_expr(const hir::HirStructExpr& s) -> ThirExprPtr {
    std::vector<std::pair<std::string, ThirExprPtr>> fields;
    for (const auto& [name, expr] : s.fields) {
        fields.push_back({name, lower_expr(expr)});
    }

    std::optional<ThirExprPtr> base;
    if (s.base) {
        base = lower_expr(*s.base);
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirStructExpr{
        s.id, s.struct_name, s.type_args, std::move(fields), std::move(base), s.type, s.span};
    return result;
}

auto ThirLower::lower_enum_expr(const hir::HirEnumExpr& e) -> ThirExprPtr {
    std::vector<ThirExprPtr> payload;
    for (const auto& p : e.payload) {
        payload.push_back(lower_expr(p));
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirEnumExpr{e.id,        e.enum_name,        e.variant_name, e.variant_index,
                                e.type_args, std::move(payload), e.type,         e.span};
    return result;
}

auto ThirLower::lower_block(const hir::HirBlockExpr& block) -> ThirExprPtr {
    std::vector<ThirStmtPtr> stmts;
    for (const auto& stmt : block.stmts) {
        stmts.push_back(lower_stmt(stmt));
    }

    std::optional<ThirExprPtr> expr;
    if (block.expr) {
        expr = lower_expr(*block.expr);
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind =
        ThirBlockExpr{block.id, std::move(stmts), std::move(expr), block.type, block.span};
    return result;
}

auto ThirLower::lower_if(const hir::HirIfExpr& if_expr) -> ThirExprPtr {
    auto condition = lower_expr(if_expr.condition);
    auto then_branch = lower_expr(if_expr.then_branch);
    std::optional<ThirExprPtr> else_branch;
    if (if_expr.else_branch) {
        else_branch = lower_expr(*if_expr.else_branch);
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirIfExpr{
        if_expr.id,   std::move(condition), std::move(then_branch), std::move(else_branch),
        if_expr.type, if_expr.span};
    return result;
}

auto ThirLower::lower_when(const hir::HirWhenExpr& when) -> ThirExprPtr {
    auto scrutinee = lower_expr(when.scrutinee);
    auto scrutinee_type = scrutinee ? scrutinee->type() : nullptr;

    std::vector<ThirWhenArm> arms;
    for (const auto& arm : when.arms) {
        ThirWhenArm thir_arm;
        thir_arm.pattern = lower_pattern(arm.pattern);
        if (arm.guard) {
            thir_arm.guard = lower_expr(*arm.guard);
        }
        thir_arm.body = lower_expr(arm.body);
        thir_arm.span = arm.span;
        arms.push_back(std::move(thir_arm));
    }

    // Build the THIR when expression first
    auto result = std::make_unique<ThirExpr>();
    auto& when_expr = result->kind.emplace<ThirWhenExpr>();
    when_expr.id = when.id;
    when_expr.scrutinee = std::move(scrutinee);
    when_expr.arms = std::move(arms);
    when_expr.type = when.type;
    when_expr.span = when.span;

    // Check pattern exhaustiveness
    if (scrutinee_type) {
        auto missing = exhaustiveness_.check_when(when_expr, scrutinee_type);
        when_expr.is_exhaustive = missing.empty();

        if (!missing.empty()) {
            std::string msg = "non-exhaustive patterns in when expression. Missing: ";
            for (size_t i = 0; i < missing.size(); ++i) {
                if (i > 0)
                    msg += ", ";
                msg += missing[i];
            }
            diagnostics_.push_back(msg);
        }
    }

    return result;
}

auto ThirLower::lower_loop(const hir::HirLoopExpr& loop) -> ThirExprPtr {
    auto condition = lower_expr(loop.condition);
    auto body = lower_expr(loop.body);

    std::optional<ThirLoopVarDecl> loop_var;
    if (loop.loop_var) {
        loop_var = ThirLoopVarDecl{loop.loop_var->name, loop.loop_var->type, loop.loop_var->span};
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirLoopExpr{
        loop.id,   loop.label, std::move(loop_var), std::move(condition), std::move(body),
        loop.type, loop.span};
    return result;
}

auto ThirLower::lower_while(const hir::HirWhileExpr& wh) -> ThirExprPtr {
    auto condition = lower_expr(wh.condition);
    auto body = lower_expr(wh.body);

    auto result = std::make_unique<ThirExpr>();
    result->kind =
        ThirWhileExpr{wh.id, wh.label, std::move(condition), std::move(body), wh.type, wh.span};
    return result;
}

auto ThirLower::lower_for(const hir::HirForExpr& f) -> ThirExprPtr {
    auto pattern = lower_pattern(f.pattern);
    auto iter = lower_expr(f.iter);
    auto body = lower_expr(f.body);

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirForExpr{
        f.id, f.label, std::move(pattern), std::move(iter), std::move(body), f.type, f.span};
    return result;
}

auto ThirLower::lower_return(const hir::HirReturnExpr& ret) -> ThirExprPtr {
    std::optional<ThirExprPtr> value;
    if (ret.value) {
        value = lower_expr(*ret.value);
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirReturnExpr{ret.id, std::move(value), ret.span};
    return result;
}

auto ThirLower::lower_break(const hir::HirBreakExpr& brk) -> ThirExprPtr {
    std::optional<ThirExprPtr> value;
    if (brk.value) {
        value = lower_expr(*brk.value);
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirBreakExpr{brk.id, brk.label, std::move(value), brk.span};
    return result;
}

auto ThirLower::lower_continue(const hir::HirContinueExpr& cont) -> ThirExprPtr {
    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirContinueExpr{cont.id, cont.label, cont.span};
    return result;
}

auto ThirLower::lower_closure(const hir::HirClosureExpr& clos) -> ThirExprPtr {
    auto body = lower_expr(clos.body);

    std::vector<std::pair<std::string, ThirType>> params;
    for (const auto& [name, type] : clos.params) {
        params.push_back({name, type});
    }

    std::vector<ThirCapture> captures;
    for (const auto& cap : clos.captures) {
        captures.push_back({cap.name, cap.type, cap.is_mut, cap.by_move});
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirClosureExpr{
        clos.id, std::move(params), std::move(body), std::move(captures), clos.type, clos.span};
    return result;
}

auto ThirLower::lower_cast(const hir::HirCastExpr& cast) -> ThirExprPtr {
    auto expr = lower_expr(cast.expr);

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirCastExpr{cast.id, std::move(expr), cast.target_type, cast.type, cast.span};
    return result;
}

auto ThirLower::lower_try(const hir::HirTryExpr& try_expr) -> ThirExprPtr {
    auto expr = lower_expr(try_expr.expr);

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirTryExpr{try_expr.id, std::move(expr), try_expr.type, try_expr.span};
    return result;
}

auto ThirLower::lower_await(const hir::HirAwaitExpr& await_expr) -> ThirExprPtr {
    auto expr = lower_expr(await_expr.expr);

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirAwaitExpr{await_expr.id, std::move(expr), await_expr.type, await_expr.span};
    return result;
}

auto ThirLower::lower_assign(const hir::HirAssignExpr& assign) -> ThirExprPtr {
    auto target = lower_expr(assign.target);
    auto value = lower_expr(assign.value);

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirAssignExpr{assign.id, std::move(target), std::move(value), assign.span};
    return result;
}

auto ThirLower::lower_compound_assign(const hir::HirCompoundAssignExpr& assign) -> ThirExprPtr {
    auto target = lower_expr(assign.target);
    auto value = lower_expr(assign.value);

    // Check for operator overloading
    std::optional<ResolvedMethod> op_method;
    auto target_type = target ? target->type() : nullptr;
    if (target_type && !is_primitive_numeric(target_type)) {
        auto behavior_method = compound_op_behavior_method(assign.op);
        if (behavior_method) {
            auto& [behavior, method] = *behavior_method;
            traits::TraitGoal goal{target_type, behavior, {}, assign.span};
            auto solve_result = solver_->solve(goal);
            if (auto* candidate = std::get_if<traits::TraitCandidate>(&solve_result)) {
                op_method =
                    ResolvedMethod{candidate->impl_type + "::" + method, behavior, {}, false};
            }
        }
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirCompoundAssignExpr{assign.id,        assign.op, std::move(target),
                                          std::move(value), op_method, assign.span};
    return result;
}

auto ThirLower::lower_lowlevel(const hir::HirLowlevelExpr& ll) -> ThirExprPtr {
    std::vector<ThirStmtPtr> stmts;
    for (const auto& stmt : ll.stmts) {
        stmts.push_back(lower_stmt(stmt));
    }

    std::optional<ThirExprPtr> expr;
    if (ll.expr) {
        expr = lower_expr(*ll.expr);
    }

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirLowlevelExpr{ll.id, std::move(stmts), std::move(expr), ll.type, ll.span};
    return result;
}

// ============================================================================
// Statement Lowering
// ============================================================================

auto ThirLower::lower_stmt(const hir::HirStmtPtr& stmt) -> ThirStmtPtr {
    if (!stmt)
        return nullptr;

    if (stmt->is<hir::HirLetStmt>()) {
        const auto& let = stmt->as<hir::HirLetStmt>();
        auto pattern = lower_pattern(let.pattern);
        std::optional<ThirExprPtr> init;
        if (let.init) {
            init = lower_expr(*let.init);
        }

        auto result = std::make_unique<ThirStmt>();
        result->kind = ThirLetStmt{let.id,          std::move(pattern), let.type,
                                   std::move(init), let.span,           let.is_volatile};
        return result;
    }

    if (stmt->is<hir::HirExprStmt>()) {
        const auto& expr_stmt = stmt->as<hir::HirExprStmt>();
        auto expr = lower_expr(expr_stmt.expr);

        auto result = std::make_unique<ThirStmt>();
        result->kind = ThirExprStmt{expr_stmt.id, std::move(expr), expr_stmt.span};
        return result;
    }

    return nullptr;
}

// ============================================================================
// Pattern Lowering
// ============================================================================

auto ThirLower::lower_pattern(const hir::HirPatternPtr& pattern) -> ThirPatternPtr {
    if (!pattern)
        return nullptr;

    auto result = std::make_unique<ThirPattern>();

    if (pattern->is<hir::HirWildcardPattern>()) {
        const auto& wp = pattern->as<hir::HirWildcardPattern>();
        result->kind = ThirWildcardPattern{wp.id, wp.span};
    } else if (pattern->is<hir::HirBindingPattern>()) {
        const auto& bp = pattern->as<hir::HirBindingPattern>();
        result->kind = ThirBindingPattern{bp.id, bp.name, bp.is_mut, bp.type, bp.span};
    } else if (pattern->is<hir::HirLiteralPattern>()) {
        const auto& lp = pattern->as<hir::HirLiteralPattern>();
        result->kind = ThirLiteralPattern{lp.id, lp.value, lp.type, lp.span};
    } else if (pattern->is<hir::HirTuplePattern>()) {
        const auto& tp = pattern->as<hir::HirTuplePattern>();
        std::vector<ThirPatternPtr> elements;
        for (const auto& e : tp.elements) {
            elements.push_back(lower_pattern(e));
        }
        result->kind = ThirTuplePattern{tp.id, std::move(elements), tp.type, tp.span};
    } else if (pattern->is<hir::HirStructPattern>()) {
        const auto& sp = pattern->as<hir::HirStructPattern>();
        std::vector<std::pair<std::string, ThirPatternPtr>> fields;
        for (const auto& [name, pat] : sp.fields) {
            fields.push_back({name, lower_pattern(pat)});
        }
        result->kind = ThirStructPattern{sp.id,       sp.struct_name, std::move(fields),
                                         sp.has_rest, sp.type,        sp.span};
    } else if (pattern->is<hir::HirEnumPattern>()) {
        const auto& ep = pattern->as<hir::HirEnumPattern>();
        std::optional<std::vector<ThirPatternPtr>> payload;
        if (ep.payload) {
            std::vector<ThirPatternPtr> lowered;
            for (const auto& p : *ep.payload) {
                lowered.push_back(lower_pattern(p));
            }
            payload = std::move(lowered);
        }
        result->kind = ThirEnumPattern{
            ep.id,   ep.enum_name, ep.variant_name, ep.variant_index, std::move(payload),
            ep.type, ep.span};
    } else if (pattern->is<hir::HirOrPattern>()) {
        const auto& op = pattern->as<hir::HirOrPattern>();
        std::vector<ThirPatternPtr> alternatives;
        for (const auto& alt : op.alternatives) {
            alternatives.push_back(lower_pattern(alt));
        }
        result->kind = ThirOrPattern{op.id, std::move(alternatives), op.type, op.span};
    } else if (pattern->is<hir::HirRangePattern>()) {
        const auto& rp = pattern->as<hir::HirRangePattern>();
        result->kind = ThirRangePattern{rp.id, rp.start, rp.end, rp.inclusive, rp.type, rp.span};
    } else if (pattern->is<hir::HirArrayPattern>()) {
        const auto& ap = pattern->as<hir::HirArrayPattern>();
        std::vector<ThirPatternPtr> elements;
        for (const auto& e : ap.elements) {
            elements.push_back(lower_pattern(e));
        }
        std::optional<ThirPatternPtr> rest;
        if (ap.rest) {
            rest = lower_pattern(*ap.rest);
        }
        result->kind =
            ThirArrayPattern{ap.id, std::move(elements), std::move(rest), ap.type, ap.span};
    }

    return result;
}

// ============================================================================
// Coercion Insertion
// ============================================================================

auto ThirLower::coerce(ThirExprPtr expr, ThirType target) -> ThirExprPtr {
    if (!expr || !target)
        return expr;

    auto source = expr->type();
    if (!source)
        return expr;

    auto coercion = needs_coercion(source, target);
    if (!coercion)
        return expr;

    auto span = expr->span();
    auto id = fresh_id();

    auto result = std::make_unique<ThirExpr>();
    result->kind = ThirCoercionExpr{id, *coercion, std::move(expr), source, target, span};
    return result;
}

auto ThirLower::needs_coercion(ThirType from, ThirType to) -> std::optional<CoercionKind> {
    if (!from || !to)
        return std::nullopt;
    if (types::types_equal(from, to))
        return std::nullopt;

    auto from_kind = primitive_kind(from);
    auto to_kind = primitive_kind(to);

    if (!from_kind || !to_kind) {
        // Check ref coercions
        if (from->is<types::RefType>() && to->is<types::RefType>()) {
            const auto& from_ref = from->as<types::RefType>();
            const auto& to_ref = to->as<types::RefType>();
            if (from_ref.is_mut && !to_ref.is_mut) {
                return CoercionKind::MutToShared;
            }
            // Auto-deref: ref ref T -> ref T
            if (from_ref.inner && from_ref.inner->is<types::RefType>()) {
                return CoercionKind::DerefCoercion;
            }
        }

        // Never coercion
        if (from->is<types::PrimitiveType>()) {
            if (from->as<types::PrimitiveType>().kind == types::PrimitiveKind::Never) {
                return CoercionKind::NeverCoercion;
            }
        }

        // Array to slice
        if (from->is<types::ArrayType>() && to->is<types::SliceType>()) {
            return CoercionKind::UnsizeCoercion;
        }

        return std::nullopt;
    }

    // Numeric coercions
    auto fk = *from_kind;
    auto tk = *to_kind;

    // Signed integer widening
    if (is_integer_type(from) && is_integer_type(to)) {
        // Check if both are signed or both unsigned
        bool from_signed = (fk == types::PrimitiveKind::I8 || fk == types::PrimitiveKind::I16 ||
                            fk == types::PrimitiveKind::I32 || fk == types::PrimitiveKind::I64 ||
                            fk == types::PrimitiveKind::I128);
        bool to_signed = (tk == types::PrimitiveKind::I8 || tk == types::PrimitiveKind::I16 ||
                          tk == types::PrimitiveKind::I32 || tk == types::PrimitiveKind::I64 ||
                          tk == types::PrimitiveKind::I128);

        if (from_signed && to_signed)
            return CoercionKind::IntWidening;
        if (!from_signed && !to_signed)
            return CoercionKind::UintWidening;
    }

    // Float widening
    if (fk == types::PrimitiveKind::F32 && tk == types::PrimitiveKind::F64) {
        return CoercionKind::FloatWidening;
    }

    // Integer to float
    if (is_integer_type(from) && is_float_type(to)) {
        return CoercionKind::IntToFloat;
    }

    return std::nullopt;
}

// ============================================================================
// Method Resolution
// ============================================================================

auto ThirLower::resolve_method(const hir::HirMethodCallExpr& call) -> ResolvedMethod {
    ResolvedMethod resolved;
    resolved.qualified_name = call.method_name;
    resolved.type_args = call.type_args;
    resolved.is_virtual = false;

    if (!call.receiver_type)
        return resolved;

    // Check if receiver is a dyn behavior type (dynamic dispatch)
    if (call.receiver_type->is<types::DynBehaviorType>()) {
        resolved.is_virtual = true;
        const auto& dyn = call.receiver_type->as<types::DynBehaviorType>();
        resolved.behavior_name = dyn.behavior_name;
        resolved.qualified_name = dyn.behavior_name + "::" + call.method_name;
        return resolved;
    }

    // Try to find the method as an inherent method first
    auto type_name = types::type_to_string(call.receiver_type);

    // Check if receiver type is a ref type — unwrap for method lookup
    auto lookup_type = call.receiver_type;
    if (lookup_type->is<types::RefType>()) {
        lookup_type = lookup_type->as<types::RefType>().inner;
    }

    if (lookup_type && lookup_type->is<types::NamedType>()) {
        auto& named = lookup_type->as<types::NamedType>();
        resolved.qualified_name = named.name + "::" + call.method_name;
    }

    // Try to resolve via trait solver — check all behaviors the type implements
    // for a method with this name
    auto* behaviors = env_->get_behavior_list();
    if (behaviors) {
        for (const auto& [bname, bdef] : *behaviors) {
            // Check if the behavior has this method
            bool has_method = false;
            for (const auto& m : bdef.methods) {
                if (m.name == call.method_name) {
                    has_method = true;
                    break;
                }
            }

            if (has_method) {
                traits::TraitGoal goal{call.receiver_type, bname, {}, call.span};
                auto result = solver_->solve(goal);
                if (std::holds_alternative<traits::TraitCandidate>(result)) {
                    resolved.behavior_name = bname;
                    resolved.qualified_name = bname + "::" + call.method_name;
                    break;
                }
            }
        }
    }

    return resolved;
}

// ============================================================================
// Operator Desugaring
// ============================================================================

auto ThirLower::op_behavior_method(hir::HirBinOp op)
    -> std::optional<std::pair<std::string, std::string>> {
    switch (op) {
    case hir::HirBinOp::Add:
        return std::pair{"Add", "add"};
    case hir::HirBinOp::Sub:
        return std::pair{"Sub", "sub"};
    case hir::HirBinOp::Mul:
        return std::pair{"Mul", "mul"};
    case hir::HirBinOp::Div:
        return std::pair{"Div", "div"};
    case hir::HirBinOp::Mod:
        return std::pair{"Mod", "mod"};
    case hir::HirBinOp::Eq:
        return std::pair{"PartialEq", "eq"};
    case hir::HirBinOp::Ne:
        return std::pair{"PartialEq", "ne"};
    case hir::HirBinOp::Lt:
        return std::pair{"PartialOrd", "lt"};
    case hir::HirBinOp::Le:
        return std::pair{"PartialOrd", "le"};
    case hir::HirBinOp::Gt:
        return std::pair{"PartialOrd", "gt"};
    case hir::HirBinOp::Ge:
        return std::pair{"PartialOrd", "ge"};
    case hir::HirBinOp::BitAnd:
        return std::pair{"BitAnd", "bitand"};
    case hir::HirBinOp::BitOr:
        return std::pair{"BitOr", "bitor"};
    case hir::HirBinOp::BitXor:
        return std::pair{"BitXor", "bitxor"};
    case hir::HirBinOp::Shl:
        return std::pair{"Shl", "shl"};
    case hir::HirBinOp::Shr:
        return std::pair{"Shr", "shr"};
    default:
        return std::nullopt;
    }
}

auto ThirLower::compound_op_behavior_method(hir::HirCompoundOp op)
    -> std::optional<std::pair<std::string, std::string>> {
    switch (op) {
    case hir::HirCompoundOp::Add:
        return std::pair{"AddAssign", "add_assign"};
    case hir::HirCompoundOp::Sub:
        return std::pair{"SubAssign", "sub_assign"};
    case hir::HirCompoundOp::Mul:
        return std::pair{"MulAssign", "mul_assign"};
    case hir::HirCompoundOp::Div:
        return std::pair{"DivAssign", "div_assign"};
    case hir::HirCompoundOp::Mod:
        return std::pair{"ModAssign", "mod_assign"};
    case hir::HirCompoundOp::BitAnd:
        return std::pair{"BitAndAssign", "bitand_assign"};
    case hir::HirCompoundOp::BitOr:
        return std::pair{"BitOrAssign", "bitor_assign"};
    case hir::HirCompoundOp::BitXor:
        return std::pair{"BitXorAssign", "bitxor_assign"};
    case hir::HirCompoundOp::Shl:
        return std::pair{"ShlAssign", "shl_assign"};
    case hir::HirCompoundOp::Shr:
        return std::pair{"ShrAssign", "shr_assign"};
    default:
        return std::nullopt;
    }
}

// ============================================================================
// Helpers
// ============================================================================

auto ThirLower::fresh_id() -> ThirId {
    return id_gen_.next();
}

auto ThirLower::is_primitive_numeric(ThirType type) -> bool {
    auto kind = primitive_kind(type);
    if (!kind)
        return false;
    switch (*kind) {
    case types::PrimitiveKind::I8:
    case types::PrimitiveKind::I16:
    case types::PrimitiveKind::I32:
    case types::PrimitiveKind::I64:
    case types::PrimitiveKind::I128:
    case types::PrimitiveKind::U8:
    case types::PrimitiveKind::U16:
    case types::PrimitiveKind::U32:
    case types::PrimitiveKind::U64:
    case types::PrimitiveKind::U128:
    case types::PrimitiveKind::F32:
    case types::PrimitiveKind::F64:
        return true;
    default:
        return false;
    }
}

auto ThirLower::is_integer_type(ThirType type) -> bool {
    auto kind = primitive_kind(type);
    if (!kind)
        return false;
    switch (*kind) {
    case types::PrimitiveKind::I8:
    case types::PrimitiveKind::I16:
    case types::PrimitiveKind::I32:
    case types::PrimitiveKind::I64:
    case types::PrimitiveKind::I128:
    case types::PrimitiveKind::U8:
    case types::PrimitiveKind::U16:
    case types::PrimitiveKind::U32:
    case types::PrimitiveKind::U64:
    case types::PrimitiveKind::U128:
        return true;
    default:
        return false;
    }
}

auto ThirLower::is_float_type(ThirType type) -> bool {
    auto kind = primitive_kind(type);
    if (!kind)
        return false;
    return *kind == types::PrimitiveKind::F32 || *kind == types::PrimitiveKind::F64;
}

auto ThirLower::primitive_kind(ThirType type) -> std::optional<types::PrimitiveKind> {
    if (!type || !type->is<types::PrimitiveType>())
        return std::nullopt;
    return type->as<types::PrimitiveType>().kind;
}

} // namespace tml::thir
