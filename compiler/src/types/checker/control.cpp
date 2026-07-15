TML_MODULE("compiler")

//! # Type Checker - Control Flow
//!
//! This file implements type checking for control flow expressions.
//!
//! ## Conditional Expressions
//!
//! | Expression   | Handler          | Condition Type | Result Type       |
//! |--------------|------------------|----------------|-------------------|
//! | `if`         | `check_if`       | Bool           | then_type or Unit |
//! | `if cond ? a : b` | `check_ternary` | Bool      | unified branches  |
//! | `if let`     | `check_if_let`   | Pattern match  | then_type or Unit |
//! | `when`       | `check_when`     | Pattern match  | unified arms      |
//!
//! ## Loop Expressions
//!
//! | Expression | Handler       | Return Type |
//! |------------|---------------|-------------|
//! | `loop`     | `check_loop`  | Unit        |
//! | `for in`   | `check_for`   | Unit        |
//!
//! ## Control Flow
//!
//! | Expression | Handler         | Return Type |
//! |------------|-----------------|-------------|
//! | `return`   | `check_return`  | Never       |
//! | `break`    | `check_break`   | Never       |
//! | `to/through` | `check_range` | Range[T] / RangeInclusive[T] |

#include "types/checker.hpp"

namespace tml::types {

// Forward declarations from helpers.cpp
bool is_integer_type(const TypePtr& type);

auto TypeChecker::check_if(const parser::IfExpr& if_expr) -> TypePtr {
    auto cond_type = check_expr(*if_expr.condition);
    if (!types_equal(env_.resolve(cond_type), make_bool())) {
        error("If condition must be Bool", if_expr.condition->span, "T014");
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
        error("Ternary condition must be Bool", ternary.condition->span, "T014");
    }

    // Check both branches and ensure they return the same type
    auto true_type = check_expr(*ternary.true_value);
    auto false_type = check_expr(*ternary.false_value);

    // Handle Never type coercion:
    // - Never coerces to any type (it's a bottom type)
    if (is_never(true_type)) {
        return false_type;
    }
    if (is_never(false_type)) {
        return true_type;
    }

    // Both branches must have the same type
    if (!types_equal(env_.resolve(true_type), env_.resolve(false_type))) {
        error("Ternary branches must have the same type", ternary.span, "T015");
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

        // Pass accumulated result_type as expected type for arm body.
        // This allows integer literals in later arms to match the type
        // established by the first arm (e.g., 0 infers as I32 not I64).
        auto arm_type = result_type ? check_expr(*arm.body, result_type) : check_expr(*arm.body);

        // Unify arm types with Never type coercion:
        // - Never type coerces to any type (it's a bottom type)
        // - If result_type is Never, take the new arm's type
        // - If arm_type is Never, keep the existing result_type
        if (!result_type) {
            result_type = arm_type;
        } else if (is_never(result_type)) {
            // Previous result was Never, take this arm's type
            result_type = arm_type;
        } else if (!is_never(arm_type)) {
            // Both types are non-Never, they should match
            auto resolved_result = env_.resolve(result_type);
            auto resolved_arm = env_.resolve(arm_type);
            if (!types_equal(resolved_result, resolved_arm)) {
                error("When arm type mismatch: expected " + type_to_string(resolved_result) +
                          ", found " + type_to_string(resolved_arm),
                      arm.body->span, "T015");
            }
        }
        // If arm_type is Never, keep result_type as-is

        env_.pop_scope();
    }

    return result_type ? result_type : make_unit();
}

auto TypeChecker::check_loop(const parser::LoopExpr& loop) -> TypePtr {
    loop_depth_++;

    // Handle loop variable declaration: loop (var i: I64 < N)
    if (loop.loop_var.has_value()) {
        env_.push_scope();
        const auto& var_decl = *loop.loop_var;
        TypePtr var_type = resolve_type(*var_decl.type);
        env_.current_scope()->define(var_decl.name, var_type, true /* mutable */, var_decl.span);
    }

    // Check condition (required in new syntax)
    check_expr(*loop.condition);

    // Check body
    check_expr(*loop.body);

    if (loop.loop_var.has_value()) {
        env_.pop_scope();
    }

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
        if ((named.name == "Range" || named.name == "RangeInclusive" ||
             named.name == "RangeFrom") &&
            !named.type_args.empty()) {
            // Range[T] / RangeInclusive[T] / RangeFrom[T] — element type is T
            element_type = named.type_args[0];
        } else if (named.name == "List" || named.name == "Vec" || named.name == "Buffer") {
            // For List/Vec/Buffer, elements are I32 (stored as i64 but converted)
            element_type = make_primitive(PrimitiveKind::I32);
        } else if (named.name == "HashMap") {
            // For HashMap iteration, we get values (I32)
            element_type = make_primitive(PrimitiveKind::I32);
        } else if (env_.type_implements(named.name, "Iterator")) {
            // Any type implementing Iterator: extract Item from next() return type
            // next() returns Maybe[Item], so we unwrap the type arg of Maybe
            auto next_sig = env_.lookup_func(named.name + "::next");
            if (next_sig && next_sig->return_type && next_sig->return_type->is<NamedType>()) {
                const auto& ret_named = next_sig->return_type->as<NamedType>();
                if ((ret_named.name == "Maybe" || ret_named.name == "Option") &&
                    !ret_named.type_args.empty()) {
                    element_type = ret_named.type_args[0];
                } else {
                    element_type = next_sig->return_type;
                }
                // Substitute generic type params from the concrete iterator type.
                // E.g. BTreeMapIter[I64, I64]::next() -> Maybe[MapEntry[K, V]]
                //   => substitute K->I64, V->I64 => MapEntry[I64, I64]
                if (!named.type_args.empty()) {
                    auto struct_def = env_.lookup_struct(named.name);
                    if (struct_def && struct_def->type_params.size() == named.type_args.size()) {
                        std::unordered_map<std::string, TypePtr> subs;
                        for (size_t i = 0; i < struct_def->type_params.size(); ++i) {
                            subs[struct_def->type_params[i]] = named.type_args[i];
                        }
                        element_type = substitute_type(element_type, subs);
                    }
                }
            } else {
                element_type = make_unit();
            }
        } else if (env_.type_implements(named.name, "IntoIterator")) {
            // Types implementing IntoIterator (e.g. BTreeMap) are usable in
            // for-in via the desugaring `for x in expr` =>
            // `for x in expr.into_iter()`. Element type comes from the
            // iterator's `next()` return.
            auto into_iter_sig = env_.lookup_func(named.name + "::into_iter");
            if (into_iter_sig && into_iter_sig->return_type &&
                into_iter_sig->return_type->is<NamedType>()) {
                const auto& iter_named = into_iter_sig->return_type->as<NamedType>();
                auto next_sig = env_.lookup_func(iter_named.name + "::next");
                if (next_sig && next_sig->return_type &&
                    next_sig->return_type->is<NamedType>()) {
                    const auto& ret_named = next_sig->return_type->as<NamedType>();
                    if ((ret_named.name == "Maybe" || ret_named.name == "Option") &&
                        !ret_named.type_args.empty()) {
                        element_type = ret_named.type_args[0];
                    } else {
                        element_type = next_sig->return_type;
                    }
                    // Substitute from the collection's type args. BTreeMap[K,V]'s
                    // IntoIter is BTreeMapIter[K,V] with matching params, so we
                    // can reuse the collection's substitution map.
                    if (!named.type_args.empty()) {
                        auto struct_def = env_.lookup_struct(named.name);
                        if (struct_def &&
                            struct_def->type_params.size() == named.type_args.size()) {
                            std::unordered_map<std::string, TypePtr> subs;
                            for (size_t i = 0; i < struct_def->type_params.size(); ++i) {
                                subs[struct_def->type_params[i]] = named.type_args[i];
                            }
                            element_type = substitute_type(element_type, subs);
                        }
                    }
                } else {
                    element_type = make_unit();
                }
            } else {
                element_type = make_unit();
            }
        } else if (iter_type->is<PrimitiveType>()) {
            // Allow iteration over integer ranges (for i in 0 to 10)
            element_type = iter_type;
        } else {
            error("For loop requires slice or collection type, found: " + type_to_string(iter_type),
                  for_expr.span, "T050");
            element_type = make_unit();
        }
    } else if (iter_type->is<PrimitiveType>()) {
        // Allow iteration over integer ranges (for i in 0 to 10)
        element_type = iter_type;
    } else {
        error("For loop requires slice or collection type, found: " + type_to_string(iter_type),
              for_expr.span, "T050");
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
            error("Range start must be an integer type", range.span, "T051");
        }
    }

    // Check end expression (if present)
    TypePtr end_type = make_primitive(PrimitiveKind::I64);
    if (range.end) {
        end_type = check_expr(**range.end);
        if (!is_integer_type(end_type)) {
            error("Range end must be an integer type", range.span, "T051");
        }
    }

    // Determine the element type T for Range[T] / RangeInclusive[T].
    // Use the start type if present, otherwise the end type.
    // Both default to I64 if neither is present.
    TypePtr elem_type = range.start ? start_type : end_type;

    // Return Range[T] for exclusive (..) or RangeInclusive[T] for inclusive (..=/through)
    std::string range_type_name = range.inclusive ? "RangeInclusive" : "Range";
    auto result = std::make_shared<Type>();
    result->kind = NamedType{range_type_name, "", {elem_type}};
    return result;
}

// Forward declaration from helpers.cpp
bool types_compatible(const TypePtr& expected, const TypePtr& actual);

auto TypeChecker::check_return(const parser::ReturnExpr& ret) -> TypePtr {
    TypePtr value_type = make_unit();
    if (ret.value) {
        // Pass expected return type so array literals can infer their size
        // and so generic enum constructors (e.g. Just(1)) can pick up T from
        // the enclosing function signature.
        value_type = check_expr(**ret.value, current_return_type_);
    }

    // Check return type matches function signature
    if (current_return_type_) {
        TypePtr resolved_expected = env_.resolve(current_return_type_);
        TypePtr resolved_actual = env_.resolve(value_type);

        if (!types_compatible(resolved_expected, resolved_actual)) {
            error("Return type mismatch: expected " + type_to_string(resolved_expected) +
                      ", found " + type_to_string(resolved_actual),
                  SourceSpan{}, "T016");
        }
    }

    return make_never();
}

auto TypeChecker::check_break(const parser::BreakExpr& brk) -> TypePtr {
    if (loop_depth_ == 0) {
        error("break outside of loop", SourceSpan{}, "T030");
    }
    if (brk.value) {
        check_expr(**brk.value);
    }
    return make_never();
}

} // namespace tml::types
