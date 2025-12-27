// Type checker control flow expressions
// Handles: check_if, check_ternary, check_if_let, check_when, check_loop, check_for, check_range, check_return, check_break

#include "tml/types/checker.hpp"

namespace tml::types {

// Forward declarations from helpers.cpp
bool is_integer_type(const TypePtr& type);

auto TypeChecker::check_if(const parser::IfExpr& if_expr) -> TypePtr {
    auto cond_type = check_expr(*if_expr.condition);
    if (!types_equal(env_.resolve(cond_type), make_bool())) {
        error("If condition must be Bool", if_expr.condition->span);
    }

    auto then_type = check_expr(*if_expr.then_branch);

    if (if_expr.else_branch) {
        check_expr(**if_expr.else_branch);
        return then_type;
    }

    return make_unit();
}

auto TypeChecker::check_ternary(const parser::TernaryExpr& ternary) -> TypePtr {
    // Check condition is Bool
    auto cond_type = check_expr(*ternary.condition);
    if (!types_equal(env_.resolve(cond_type), make_bool())) {
        error("Ternary condition must be Bool", ternary.condition->span);
    }

    // Check both branches and ensure they return the same type
    auto true_type = check_expr(*ternary.true_value);
    auto false_type = check_expr(*ternary.false_value);

    // Both branches must have the same type
    if (!types_equal(env_.resolve(true_type), env_.resolve(false_type))) {
        error("Ternary branches must have the same type", ternary.span);
    }

    return true_type;
}

auto TypeChecker::check_if_let(const parser::IfLetExpr& if_let) -> TypePtr {
    // Type check the scrutinee
    auto scrutinee_type = check_expr(*if_let.scrutinee);

    // Type check the then branch with pattern bindings in scope
    env_.push_scope();
    bind_pattern(*if_let.pattern, scrutinee_type);
    auto then_type = check_expr(*if_let.then_branch);
    env_.pop_scope();

    // Type check the else branch if present
    if (if_let.else_branch) {
        check_expr(**if_let.else_branch);
        return then_type;
    }

    return make_unit();
}

auto TypeChecker::check_when(const parser::WhenExpr& when) -> TypePtr {
    auto scrutinee_type = check_expr(*when.scrutinee);
    TypePtr result_type = nullptr;

    for (const auto& arm : when.arms) {
        env_.push_scope();
        bind_pattern(*arm.pattern, scrutinee_type);

        if (arm.guard) {
            check_expr(**arm.guard);
        }

        auto arm_type = check_expr(*arm.body);
        if (!result_type) {
            result_type = arm_type;
        }

        env_.pop_scope();
    }

    return result_type ? result_type : make_unit();
}

auto TypeChecker::check_loop(const parser::LoopExpr& loop) -> TypePtr {
    loop_depth_++;
    check_expr(*loop.body);
    loop_depth_--;
    return make_unit();
}

auto TypeChecker::check_for(const parser::ForExpr& for_expr) -> TypePtr {
    loop_depth_++;
    env_.push_scope();

    auto iter_type = check_expr(*for_expr.iter);

    // Extract element type from slice or collection for pattern binding
    TypePtr element_type = make_unit();
    if (iter_type->is<SliceType>()) {
        element_type = iter_type->as<SliceType>().element;
    } else if (iter_type->is<NamedType>()) {
        // Check if it's a collection type (List, HashMap, Buffer, Vec)
        const auto& named = iter_type->as<NamedType>();
        if (named.name == "List" || named.name == "Vec" || named.name == "Buffer") {
            // For List/Vec/Buffer, elements are I32 (stored as i64 but converted)
            element_type = make_primitive(PrimitiveKind::I32);
        } else if (named.name == "HashMap") {
            // For HashMap iteration, we get values (I32)
            element_type = make_primitive(PrimitiveKind::I32);
        } else if (iter_type->is<PrimitiveType>()) {
            // Allow iteration over integer ranges (for i in 0 to 10)
            element_type = iter_type;
        } else {
            error("For loop requires slice or collection type, found: " + type_to_string(iter_type), for_expr.span);
            element_type = make_unit();
        }
    } else if (iter_type->is<PrimitiveType>()) {
        // Allow iteration over integer ranges (for i in 0 to 10)
        element_type = iter_type;
    } else {
        error("For loop requires slice or collection type, found: " + type_to_string(iter_type), for_expr.span);
        element_type = make_unit();
    }

    bind_pattern(*for_expr.pattern, element_type);

    check_expr(*for_expr.body);

    env_.pop_scope();
    loop_depth_--;

    return make_unit();
}

auto TypeChecker::check_range(const parser::RangeExpr& range) -> TypePtr {
    // Check start expression (if present)
    TypePtr start_type = make_primitive(PrimitiveKind::I64);
    if (range.start) {
        start_type = check_expr(**range.start);
        if (!is_integer_type(start_type)) {
            error("Range start must be an integer type", range.span);
        }
    }

    // Check end expression (if present)
    TypePtr end_type = make_primitive(PrimitiveKind::I64);
    if (range.end) {
        end_type = check_expr(**range.end);
        if (!is_integer_type(end_type)) {
            error("Range end must be an integer type", range.span);
        }
    }

    // Both start and end should have compatible types
    // For simplicity, ranges always produce I64 slices
    return make_slice(make_primitive(PrimitiveKind::I64));
}

// Forward declaration from helpers.cpp
bool types_compatible(const TypePtr& expected, const TypePtr& actual);

auto TypeChecker::check_return(const parser::ReturnExpr& ret) -> TypePtr {
    TypePtr value_type = make_unit();
    if (ret.value) {
        value_type = check_expr(**ret.value);
    }

    // Check return type matches function signature
    if (current_return_type_) {
        TypePtr resolved_expected = env_.resolve(current_return_type_);
        TypePtr resolved_actual = env_.resolve(value_type);

        if (!types_compatible(resolved_expected, resolved_actual)) {
            error("Return type mismatch: expected " + type_to_string(resolved_expected) +
                  ", found " + type_to_string(resolved_actual), SourceSpan{});
        }
    }

    return make_never();
}

auto TypeChecker::check_break(const parser::BreakExpr& brk) -> TypePtr {
    if (loop_depth_ == 0) {
        error("break outside of loop", SourceSpan{});
    }
    if (brk.value) {
        check_expr(**brk.value);
    }
    return make_never();
}

} // namespace tml::types
