TML_MODULE("compiler")

//! # Type Checker - Special Expressions
//!
//! This file implements type checking for interpolated strings, template literals,
//! cast expressions, is expressions, await, lowlevel blocks, base expressions,
//! new expressions, and lifetime bound validation.

#include "common.hpp"
#include "lexer/token.hpp"
#include "types/checker.hpp"

#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace tml::types {

auto TypeChecker::check_interp_string(const parser::InterpolatedStringExpr& interp) -> TypePtr {
    for (const auto& segment : interp.segments) {
        if (std::holds_alternative<parser::ExprPtr>(segment.content)) {
            const auto& expr = std::get<parser::ExprPtr>(segment.content);
            auto expr_type = check_expr(*expr);
            (void)expr_type;
        }
    }
    return make_str();
}

auto TypeChecker::check_template_literal(const parser::TemplateLiteralExpr& tpl) -> TypePtr {
    // Template literals produce Text type
    for (const auto& segment : tpl.segments) {
        if (std::holds_alternative<parser::ExprPtr>(segment.content)) {
            const auto& expr = std::get<parser::ExprPtr>(segment.content);
            auto expr_type = check_expr(*expr);
            (void)expr_type;
        }
    }
    // Return Text type - this is a named type from std::text
    return std::make_shared<Type>(Type{NamedType{"Text", "", {}}});
}

auto TypeChecker::check_cast(const parser::CastExpr& cast) -> TypePtr {
    // Check the source expression
    auto source_type = check_expr(*cast.expr);
    (void)source_type; // We allow any source type for now

    // Resolve the target type
    auto target_type = resolve_type(*cast.target);

    // Return the target type - the actual cast is handled by codegen
    return target_type;
}

auto TypeChecker::check_is(const parser::IsExpr& is_expr) -> TypePtr {
    // Check the source expression
    auto source_type = check_expr(*is_expr.expr);

    // Resolve the target type
    auto target_type = resolve_type(*is_expr.target);

    // Validate that 'is' makes sense:
    // - Source should be a class type (or interface type)
    // - Target should be a class type
    // For now, we allow any types and let codegen handle it
    (void)source_type;
    (void)target_type;

    // 'is' expression always returns Bool
    return make_bool();
}

auto TypeChecker::check_await(const parser::AwaitExpr& await_expr, SourceSpan span) -> TypePtr {
    // Check that we're in an async function
    if (!in_async_func_) {
        error("Cannot use `.await` outside of an async function", span, "T032");
        return make_unit();
    }

    // Type-check the awaited expression
    auto expr_type = check_expr(*await_expr.expr);

    // The awaited expression should return a Future[T] - extract the Output type
    // For simplicity, we check if it's a named type that implements Future
    // or if the expression is from an async function call (which implicitly returns Future[T])

    // Case 1: Named type that might be a Future
    if (expr_type->is<NamedType>()) {
        auto& named = expr_type->as<NamedType>();

        // Check if this type implements Future behavior
        if (env_.type_implements(named.name, "Future")) {
            // Look up the behavior impl to get the Output associated type
            // For now, if the type has type_args, assume the first is the Output
            if (!named.type_args.empty()) {
                return named.type_args[0];
            }
        }

        // Special case: Poll[T] - awaiting Poll returns T when Ready
        if (named.name == "Poll" && !named.type_args.empty()) {
            return named.type_args[0];
        }
    }

    // Case 2: Function type with is_async = true
    // Async functions return Future[ReturnType], so .await extracts ReturnType
    if (expr_type->is<FuncType>()) {
        auto& func = expr_type->as<FuncType>();
        if (func.is_async) {
            return func.return_type;
        }
    }

    // Case 3: impl Behavior type (ImplBehaviorType)
    if (expr_type->is<ImplBehaviorType>()) {
        auto& impl_behavior = expr_type->as<ImplBehaviorType>();
        if (impl_behavior.behavior_name == "Future") {
            // Return the Output type if available
            if (!impl_behavior.type_args.empty()) {
                return impl_behavior.type_args[0];
            }
        }
    }

    // For now, return the type itself if we can't determine the Future output
    // This allows async code to type-check even without full Future inference
    return expr_type;
}

auto TypeChecker::check_lowlevel(const parser::LowlevelExpr& lowlevel) -> TypePtr {
    // Save previous lowlevel state
    bool was_in_lowlevel = in_lowlevel_;
    in_lowlevel_ = true;

    env_.push_scope();
    TypePtr result = make_unit();

    // Check statements in lowlevel block
    for (const auto& stmt : lowlevel.stmts) {
        result = check_stmt(*stmt);
    }

    // Check trailing expression if present
    if (lowlevel.expr) {
        result = check_expr(**lowlevel.expr);
    }

    env_.pop_scope();
    in_lowlevel_ = was_in_lowlevel;

    return result;
}

auto TypeChecker::check_base(const parser::BaseExpr& base) -> TypePtr {
    // Verify we're in a class context with a parent class
    if (!current_self_type_) {
        error("'base' can only be used inside a class method", base.span, "T048");
        return make_unit();
    }

    // Check if self type is a ClassType
    if (!current_self_type_->is<ClassType>()) {
        error("'base' can only be used inside a class method", base.span, "T048");
        return make_unit();
    }

    const auto& class_type = current_self_type_->as<ClassType>();
    auto class_def = env_.lookup_class(class_type.name);

    if (!class_def.has_value()) {
        error("Class '" + class_type.name + "' not found", base.span, "T046");
        return make_unit();
    }

    if (!class_def->base_class.has_value()) {
        error("Class '" + class_type.name + "' has no base class", base.span, "T076");
        return make_unit();
    }

    const std::string& base_class_name = class_def->base_class.value();
    auto base_class_def = env_.lookup_class(base_class_name);

    if (!base_class_def.has_value()) {
        error("Base class '" + base_class_name + "' not found", base.span, "T046");
        return make_unit();
    }

    if (base.is_method_call) {
        // Look up the method in the base class
        for (const auto& method : base_class_def->methods) {
            if (method.sig.name == base.member) {
                // Check arguments
                for (size_t i = 0; i < base.args.size(); ++i) {
                    check_expr(*base.args[i]);
                }

                // Return the method's return type
                return method.sig.return_type;
            }
        }

        error("Method '" + base.member + "' not found in base class '" + base_class_name + "'",
              base.span, "T077");
        return make_unit();
    } else {
        // Field access on base class
        for (const auto& field : base_class_def->fields) {
            if (field.name == base.member) {
                return field.type;
            }
        }

        error("Field '" + base.member + "' not found in base class '" + base_class_name + "'",
              base.span, "T067");
        return make_unit();
    }
}

auto TypeChecker::check_new(const parser::NewExpr& new_expr) -> TypePtr {
    // Resolve the class type
    std::string class_name;
    if (!new_expr.class_type.segments.empty()) {
        class_name = new_expr.class_type.segments.back();
    } else {
        error("Invalid class name in new expression", new_expr.span, "T002");
        return make_unit();
    }

    auto class_def = env_.lookup_class(class_name);

    if (!class_def.has_value()) {
        error("Class '" + class_name + "' not found", new_expr.span, "T075");
        return make_unit();
    }

    // Check if class is abstract
    if (class_def->is_abstract) {
        error("Cannot instantiate abstract class '" + class_name + "'", new_expr.span, "T040");
        return make_unit();
    }

    // Check constructor arguments
    for (const auto& arg : new_expr.args) {
        check_expr(*arg);
    }

    // Return the class type
    auto result = std::make_shared<Type>();
    result->kind = ClassType{class_name, "", {}};
    return result;
}

// ============================================================================
// Lifetime Bound Validation (Phase 9: Higher-Kinded Lifetime Bounds)
// ============================================================================

bool TypeChecker::type_satisfies_lifetime_bound(TypePtr type, const std::string& lifetime_bound) {
    if (!type) {
        return true; // Null types trivially satisfy bounds (error already reported)
    }

    // For 'static lifetime bound, check that type contains no non-static references
    if (lifetime_bound == "static") {
        // Primitive types satisfy 'static
        if (type->is<PrimitiveType>()) {
            return true;
        }

        // References only satisfy 'static if they have explicit static lifetime
        if (type->is<RefType>()) {
            const auto& ref = type->as<RefType>();
            if (ref.lifetime.has_value() && ref.lifetime.value() == "static") {
                // ref[static] T satisfies 'static if inner type also satisfies 'static
                return type_satisfies_lifetime_bound(ref.inner, "static");
            }
            // Non-static references don't satisfy 'static bound
            return false;
        }

        // Pointer types satisfy 'static (raw pointers have no lifetime)
        if (type->is<PtrType>()) {
            return true;
        }

        // Tuple types satisfy 'static if all elements satisfy 'static
        if (type->is<TupleType>()) {
            const auto& tuple = type->as<TupleType>();
            for (const auto& elem : tuple.elements) {
                if (!type_satisfies_lifetime_bound(elem, "static")) {
                    return false;
                }
            }
            return true;
        }

        // Array types satisfy 'static if element type satisfies 'static
        if (type->is<ArrayType>()) {
            const auto& arr = type->as<ArrayType>();
            return type_satisfies_lifetime_bound(arr.element, "static");
        }

        // Function types satisfy 'static (function pointers have no captured state)
        if (type->is<FuncType>()) {
            return true;
        }

        // Named types (structs, enums): check if they contain references
        if (type->is<NamedType>()) {
            const auto& named = type->as<NamedType>();

            // Built-in primitive-like types satisfy 'static
            static const std::unordered_set<std::string> static_types = {
                "I8",   "I16", "I32", "I64",  "I128", "U8",  "U16",  "U32",  "U64",
                "U128", "F32", "F64", "Bool", "Char", "Str", "Unit", "Never"};
            if (static_types.count(named.name)) {
                return true;
            }

            // Check struct definition if available
            auto struct_def = env_.lookup_struct(named.name);
            if (struct_def.has_value()) {
                // Recursively check all fields
                for (const auto& field : struct_def->fields) {
                    if (!type_satisfies_lifetime_bound(field.type, "static")) {
                        return false;
                    }
                }
                return true;
            }

            // Check enum definition if available
            auto enum_def = env_.lookup_enum(named.name);
            if (enum_def.has_value()) {
                // Check all variant payload types
                for (const auto& [variant_name, payload_types] : enum_def->variants) {
                    for (const auto& payload_type : payload_types) {
                        if (!type_satisfies_lifetime_bound(payload_type, "static")) {
                            return false;
                        }
                    }
                }
                return true;
            }

            // Unknown named types - assume they satisfy 'static for now
            // (could be a type parameter or external type)
            return true;
        }

        // Generic types: need to check substitution
        if (type->is<GenericType>()) {
            // Generic type parameters may or may not satisfy 'static
            // This should be handled by the caller who has the substitution map
            return true;
        }

        // Default: assume types satisfy 'static unless proven otherwise
        return true;
    }

    // For named lifetime bounds (e.g., 'a), we need more sophisticated analysis
    // For now, assume all types satisfy named lifetime bounds
    // Full implementation would track lifetime relationships
    return true;
}

} // namespace tml::types
