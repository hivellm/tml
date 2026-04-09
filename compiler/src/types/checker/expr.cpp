TML_MODULE("compiler")

//! # Type Checker - Expressions
//!
//! This file implements type checking for all expression kinds.
//!
//! ## Expression Dispatch
//!
//! `check_expr()` dispatches to specialized handlers based on expression type.
//!
//! ## Literal Type Inference
//!
//! | Literal Type   | Default Type | Suffix Support           |
//! |----------------|--------------|--------------------------|
//! | Integer        | I64          | i8, i16, i32, u8, etc.   |
//! | Float          | F64          | f32, f64                 |
//! | String         | Str          | -                        |
//! | Char           | Char         | -                        |
//! | Bool           | Bool         | -                        |
//!
//! ## Method Call Resolution
//!
//! Method calls are resolved in this order:
//! 1. Check for static methods on primitive type names
//! 2. Look up qualified method in current module
//! 3. Check behavior implementations (for dyn types)
//! 4. Check primitive type builtin methods (core::ops)
//! 5. Check named type methods (Maybe, Outcome, Array, Slice)

#include "common.hpp"
#include "lexer/token.hpp"
#include "types/checker.hpp"

#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace tml::types {

// Forward declarations from helpers.cpp
bool is_integer_type(const TypePtr& type);
bool is_float_type(const TypePtr& type);
bool types_compatible(const TypePtr& expected, const TypePtr& actual);

// Helper to get primitive name as string
static std::string primitive_to_string(PrimitiveKind kind) {
    switch (kind) {
    case PrimitiveKind::I8:
        return "I8";
    case PrimitiveKind::I16:
        return "I16";
    case PrimitiveKind::I32:
        return "I32";
    case PrimitiveKind::I64:
        return "I64";
    case PrimitiveKind::I128:
        return "I128";
    case PrimitiveKind::U8:
        return "U8";
    case PrimitiveKind::U16:
        return "U16";
    case PrimitiveKind::U32:
        return "U32";
    case PrimitiveKind::U64:
        return "U64";
    case PrimitiveKind::U128:
        return "U128";
    case PrimitiveKind::F32:
        return "F32";
    case PrimitiveKind::F64:
        return "F64";
    case PrimitiveKind::Bool:
        return "Bool";
    case PrimitiveKind::Char:
        return "Char";
    case PrimitiveKind::Str:
        return "Str";
    case PrimitiveKind::Unit:
        return "Unit";
    case PrimitiveKind::Never:
        return "Never";
    }
    return "unknown";
}

// Helper to check if a primitive type is unsigned
static bool is_unsigned_primitive(PrimitiveKind kind) {
    return kind == PrimitiveKind::U8 || kind == PrimitiveKind::U16 || kind == PrimitiveKind::U32 ||
           kind == PrimitiveKind::U64 || kind == PrimitiveKind::U128;
}

// Helper to get the maximum value for an integer type
static uint64_t get_int_max_value(PrimitiveKind kind) {
    switch (kind) {
    case PrimitiveKind::I8:
        return 127;
    case PrimitiveKind::I16:
        return 32767;
    case PrimitiveKind::I32:
        return 2147483647;
    case PrimitiveKind::I64:
        return 9223372036854775807ULL;
    case PrimitiveKind::U8:
        return 255;
    case PrimitiveKind::U16:
        return 65535;
    case PrimitiveKind::U32:
        return 4294967295ULL;
    case PrimitiveKind::U64:
        return 18446744073709551615ULL;
    default:
        return 18446744073709551615ULL; // Max for U64/I128/U128 (as much as uint64_t can hold)
    }
}

// Helper to get the minimum value for an integer type (as positive magnitude)
// For signed types, this returns the magnitude of the minimum value (e.g., 128 for I8)
// For unsigned types, this returns 0
// Note: kept for future use with signed overflow validation
[[maybe_unused]] static uint64_t get_int_min_magnitude(PrimitiveKind kind) {
    switch (kind) {
    case PrimitiveKind::I8:
        return 128;
    case PrimitiveKind::I16:
        return 32768;
    case PrimitiveKind::I32:
        return 2147483648ULL;
    case PrimitiveKind::I64:
        return 9223372036854775808ULL; // This is exactly 2^63
    default:
        return 0; // Unsigned types have min of 0
    }
}

// Note: extract_type_params moved to expr_call.cpp

auto TypeChecker::check_expr(const parser::Expr& expr) -> TypePtr {
    return std::visit(
        [this, &expr](const auto& e) -> TypePtr {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
                return check_literal(e);
            } else if constexpr (std::is_same_v<T, parser::IdentExpr>) {
                return check_ident(e, expr.span);
            } else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
                return check_binary(e);
            } else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
                return check_unary(e);
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                return check_call(e);
            } else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
                auto result = check_method_call(e);
                env_.set_expr_type(&e, result);
                return result;
            } else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
                return check_field_access(e);
            } else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
                return check_index(e);
            } else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
                return check_block(e);
            } else if constexpr (std::is_same_v<T, parser::IfExpr>) {
                return check_if(e);
            } else if constexpr (std::is_same_v<T, parser::TernaryExpr>) {
                return check_ternary(e);
            } else if constexpr (std::is_same_v<T, parser::IfLetExpr>) {
                return check_if_let(e);
            } else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
                return check_when(e);
            } else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
                return check_loop(e);
            } else if constexpr (std::is_same_v<T, parser::ForExpr>) {
                return check_for(e);
            } else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
                return check_return(e);
            } else if constexpr (std::is_same_v<T, parser::BreakExpr>) {
                return check_break(e);
            } else if constexpr (std::is_same_v<T, parser::ContinueExpr>) {
                return make_never();
            } else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
                return check_tuple(e);
            } else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
                return check_array(e);
            } else if constexpr (std::is_same_v<T, parser::StructExpr>) {
                return check_struct_expr(e);
            } else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
                return check_closure(e);
            } else if constexpr (std::is_same_v<T, parser::TryExpr>) {
                return check_try(e);
            } else if constexpr (std::is_same_v<T, parser::PathExpr>) {
                return check_path(e, expr.span);
            } else if constexpr (std::is_same_v<T, parser::RangeExpr>) {
                return check_range(e);
            } else if constexpr (std::is_same_v<T, parser::InterpolatedStringExpr>) {
                return check_interp_string(e);
            } else if constexpr (std::is_same_v<T, parser::TemplateLiteralExpr>) {
                return check_template_literal(e);
            } else if constexpr (std::is_same_v<T, parser::CastExpr>) {
                return check_cast(e);
            } else if constexpr (std::is_same_v<T, parser::IsExpr>) {
                return check_is(e);
            } else if constexpr (std::is_same_v<T, parser::AwaitExpr>) {
                return check_await(e, expr.span);
            } else if constexpr (std::is_same_v<T, parser::LowlevelExpr>) {
                return check_lowlevel(e);
            } else if constexpr (std::is_same_v<T, parser::BaseExpr>) {
                return check_base(e);
            } else if constexpr (std::is_same_v<T, parser::NewExpr>) {
                return check_new(e);
            } else {
                return make_unit();
            }
        },
        expr.kind);
}

auto TypeChecker::check_expr(const parser::Expr& expr, TypePtr expected_type) -> TypePtr {
    return std::visit(
        [this, &expr, &expected_type](const auto& e) -> TypePtr {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
                auto result = check_literal(e, expected_type);
                // W3: record the resolved literal type so HIR lowering can
                // use it instead of defaulting to I32 in `lower_literal`.
                // Without this, an unsuffixed `138` in `i <= 138` (where
                // `i: I64`) gets emitted as `sext i32 138 to i64` at the
                // LLVM level.
                if (result) {
                    env_.set_expr_type(&e, result);
                }
                return result;
            } else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
                // For unary expressions like -5, propagate expected type to operand
                if (e.op == parser::UnaryOp::Neg && e.operand->template is<parser::LiteralExpr>()) {
                    // Check for negative literal assigned to unsigned type
                    if (expected_type &&
                        std::holds_alternative<PrimitiveType>(expected_type->kind)) {
                        auto prim = std::get<PrimitiveType>(expected_type->kind);
                        if (is_unsigned_primitive(prim.kind)) {
                            error("Cannot assign negative value to unsigned type " +
                                      primitive_to_string(prim.kind),
                                  expr.span, "T050");
                            return expected_type; // Return expected type to continue checking
                        }
                    }
                    auto result = check_literal(e.operand->template as<parser::LiteralExpr>(),
                                                expected_type, true /* is_negated */);
                    // W3: record the resolved literal type for HIR lowering.
                    // The HIR lowerer consults this via `expr_types_` before
                    // falling back to the I32 default. Store against the
                    // LiteralExpr node (the actual leaf HIR will lower).
                    if (result) {
                        env_.set_expr_type(&e.operand->template as<parser::LiteralExpr>(), result);
                    }
                    return result;
                }
                return check_unary(e);
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                // For call expressions, propagate expected return type for generic type inference
                // (e.g., Outcome::from_output(42) with expected Outcome[I32, Str] → infers E=Str)
                auto result = check_call(e, expected_type);
                // W4: record the resolved type for HIR lowering. When a bare
                // enum-variant call like `Just(1)` flows through bidirectional
                // inference with an expected type (e.g., `Maybe[I64]`), the
                // HIR builder consults this map to synthesize a
                // `HirEnumExpr` with fully concretized type arguments.
                if (result) {
                    env_.set_expr_type(&e, result);
                }
                return result;
            } else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
                // For array expressions, propagate expected type for element coercion
                return check_array(e, expected_type);
            } else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
                // For tuple expressions, propagate expected element types for coercion
                // so that e.g. `return (1, Just(1))` with expected `(I64, Maybe[I64])`
                // infers each element against the expected tuple-element type.
                std::vector<TypePtr> element_types;
                const TupleType* expected_tuple = nullptr;
                if (expected_type && expected_type->is<TupleType>()) {
                    expected_tuple = &expected_type->as<TupleType>();
                }
                for (size_t i = 0; i < e.elements.size(); ++i) {
                    TypePtr expected_elem = nullptr;
                    if (expected_tuple && i < expected_tuple->elements.size()) {
                        expected_elem = expected_tuple->elements[i];
                    }
                    element_types.push_back(check_expr(*e.elements[i], expected_elem));
                }
                return make_tuple(std::move(element_types));
            } else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
                // For closures, propagate expected type to resolve untyped params
                auto result = check_closure(e);
                // If expected type is a FuncType, unify closure param types with it
                if (expected_type && result) {
                    if (expected_type->template is<FuncType>() &&
                        result->template is<ClosureType>()) {
                        const auto& func = expected_type->template as<FuncType>();
                        const auto& clos = result->template as<ClosureType>();
                        for (size_t i = 0; i < func.params.size() && i < clos.params.size(); ++i) {
                            env_.unify(clos.params[i], func.params[i]);
                        }
                        if (func.return_type && clos.return_type) {
                            env_.unify(clos.return_type, func.return_type);
                        }
                    }
                }
                // Re-store inferred types after unification
                if (result && result->template is<ClosureType>()) {
                    const auto& clos = result->template as<ClosureType>();
                    e.inferred_param_types.clear();
                    for (const auto& pt : clos.params) {
                        e.inferred_param_types.push_back(
                            std::static_pointer_cast<void>(env_.resolve(pt)));
                    }
                    e.inferred_return_type =
                        std::static_pointer_cast<void>(env_.resolve(clos.return_type));
                }
                return result;
            } else {
                // For other expressions, fall back to regular check_expr
                return check_expr(expr);
            }
        },
        expr.kind);
}

auto TypeChecker::check_literal(const parser::LiteralExpr& lit) -> TypePtr {
    return check_literal(lit, nullptr);
}

auto TypeChecker::check_literal(const parser::LiteralExpr& lit, TypePtr expected_type,
                                bool is_negated) -> TypePtr {
    // Helper lambda to check integer overflow
    // When is_negated is true, allow the minimum magnitude for signed types
    // (e.g., -9223372036854775808 is valid for I64 even though the positive value overflows)
    auto check_int_overflow = [this, &lit, is_negated](uint64_t value, PrimitiveKind kind) {
        uint64_t max_val = get_int_max_value(kind);
        // For negated signed types, allow up to the minimum magnitude
        if (is_negated && !is_unsigned_primitive(kind)) {
            uint64_t min_mag = get_int_min_magnitude(kind);
            if (min_mag > 0 && value <= min_mag) {
                return; // Valid: the negated value fits in the signed type
            }
        }
        if (value > max_val) {
            error("Integer literal " + std::to_string(value) + " overflows type " +
                      primitive_to_string(kind) + " (max " + std::to_string(max_val) + ")",
                  lit.token.span, "T051");
        }
    };

    switch (lit.token.kind) {
    case lexer::TokenKind::IntLiteral: {
        const auto& int_val = lit.token.int_value();
        if (!int_val.suffix.empty()) {
            const auto& suffix = int_val.suffix;
            // Map suffix to PrimitiveKind and validate
            PrimitiveKind target_kind;
            if (suffix == "i8")
                target_kind = PrimitiveKind::I8;
            else if (suffix == "i16")
                target_kind = PrimitiveKind::I16;
            else if (suffix == "i32")
                target_kind = PrimitiveKind::I32;
            else if (suffix == "i64")
                target_kind = PrimitiveKind::I64;
            else if (suffix == "i128")
                target_kind = PrimitiveKind::I128;
            else if (suffix == "u8")
                target_kind = PrimitiveKind::U8;
            else if (suffix == "u16")
                target_kind = PrimitiveKind::U16;
            else if (suffix == "u32")
                target_kind = PrimitiveKind::U32;
            else if (suffix == "u64")
                target_kind = PrimitiveKind::U64;
            else if (suffix == "u128")
                target_kind = PrimitiveKind::U128;
            else
                return make_i64(); // Unknown suffix, default to I64

            // Check overflow for suffixed literals
            check_int_overflow(int_val.value, target_kind);
            return make_primitive(target_kind);
        }
        // If no suffix, use expected_type if it's an integer type
        if (expected_type && std::holds_alternative<PrimitiveType>(expected_type->kind)) {
            auto prim = std::get<PrimitiveType>(expected_type->kind);
            switch (prim.kind) {
            case PrimitiveKind::I8:
            case PrimitiveKind::I16:
            case PrimitiveKind::I32:
            case PrimitiveKind::I64:
            case PrimitiveKind::I128:
            case PrimitiveKind::U8:
            case PrimitiveKind::U16:
            case PrimitiveKind::U32:
            case PrimitiveKind::U64:
            case PrimitiveKind::U128:
                // Check overflow for unsuffixed literals assigned to specific types
                check_int_overflow(int_val.value, prim.kind);
                return expected_type;
            default:
                break;
            }
        }
        return make_i64();
    }
    case lexer::TokenKind::FloatLiteral: {
        const auto& float_val = lit.token.float_value();
        if (!float_val.suffix.empty()) {
            if (float_val.suffix == "f32")
                return make_primitive(PrimitiveKind::F32);
            if (float_val.suffix == "f64")
                return make_primitive(PrimitiveKind::F64);
        }
        // If no suffix, use expected_type if it's a float type
        if (expected_type && std::holds_alternative<PrimitiveType>(expected_type->kind)) {
            auto prim = std::get<PrimitiveType>(expected_type->kind);
            if (prim.kind == PrimitiveKind::F32 || prim.kind == PrimitiveKind::F64) {
                return expected_type;
            }
        }
        return make_f64();
    }
    case lexer::TokenKind::StringLiteral:
        return make_str();
    case lexer::TokenKind::CharLiteral:
        return make_primitive(PrimitiveKind::Char);
    case lexer::TokenKind::BoolLiteral:
        return make_bool();
    case lexer::TokenKind::NullLiteral:
        return make_ptr(make_unit()); // null has type Ptr[Unit]
    default:
        return make_unit();
    }
}

auto TypeChecker::check_ident(const parser::IdentExpr& ident, SourceSpan span) -> TypePtr {
    // S014: Track variable reads for unused variable detection
    read_vars_.insert(ident.name);

    auto sym = env_.current_scope()->lookup(ident.name);
    if (!sym) {
        // Check if it's a function
        auto func = env_.lookup_func(ident.name);
        if (func) {
            return make_func(func->params, func->return_type);
        }

        // Check if it's an enum constructor
        for (const auto& [enum_name, enum_def] : env_.all_enums()) {
            for (const auto& [variant_name, payload_types] : enum_def.variants) {
                if (variant_name == ident.name) {
                    if (payload_types.empty()) {
                        auto enum_type = std::make_shared<Type>();
                        enum_type->kind = NamedType{enum_name, "", {}};
                        return enum_type;
                    } else {
                        auto enum_type = std::make_shared<Type>();
                        enum_type->kind = NamedType{enum_name, "", {}};
                        return make_func(payload_types, enum_type);
                    }
                }
            }
        }

        // Check if it's a type name
        // First check locally defined types (with empty module_path)
        auto local_struct_it = env_.all_structs().find(ident.name);
        if (local_struct_it != env_.all_structs().end()) {
            auto type = std::make_shared<Type>();
            type->kind = NamedType{ident.name, "", {}};
            return type;
        }

        auto local_enum_it = env_.all_enums().find(ident.name);
        if (local_enum_it != env_.all_enums().end()) {
            auto type = std::make_shared<Type>();
            type->kind = NamedType{ident.name, "", {}};
            return type;
        }

        // Check imported types (result cached below for reuse in constant lookup)
        auto imported_path = env_.resolve_imported_symbol(ident.name);
        if (imported_path.has_value()) {
            std::string module_path;
            size_t pos = imported_path->rfind("::");
            if (pos != std::string::npos) {
                module_path = imported_path->substr(0, pos);
            }

            auto module = env_.get_module(module_path);
            if (module) {
                if (module->structs.find(ident.name) != module->structs.end()) {
                    auto type = std::make_shared<Type>();
                    type->kind = NamedType{ident.name, module_path, {}};
                    return type;
                }
                if (module->enums.find(ident.name) != module->enums.end()) {
                    auto type = std::make_shared<Type>();
                    type->kind = NamedType{ident.name, module_path, {}};
                    return type;
                }
            }
        }

        // Check if it's an enum constructor from an imported module
        for (const auto& [import_name, import_info] : env_.all_imports()) {
            auto imported_module = env_.get_module(import_info.module_path);
            if (!imported_module)
                continue;

            for (const auto& [imported_enum_name, imported_enum_def] : imported_module->enums) {
                for (const auto& [variant_name, payload_types] : imported_enum_def.variants) {
                    if (variant_name == ident.name) {
                        if (payload_types.empty()) {
                            auto enum_type = std::make_shared<Type>();
                            enum_type->kind =
                                NamedType{imported_enum_name, import_info.module_path, {}};
                            return enum_type;
                        } else {
                            auto enum_type = std::make_shared<Type>();
                            enum_type->kind =
                                NamedType{imported_enum_name, import_info.module_path, {}};
                            return make_func(payload_types, enum_type);
                        }
                    }
                }
            }
        }

        // Check if it's an imported constant (reuse cached imported_path from type lookup above)
        if (imported_path.has_value()) {
            std::string const_module_path;
            size_t const_pos = imported_path->rfind("::");
            if (const_pos != std::string::npos) {
                const_module_path = imported_path->substr(0, const_pos);
            }

            auto const_module = env_.get_module(const_module_path);
            if (const_module) {
                auto const_it = const_module->constants.find(ident.name);
                if (const_it != const_module->constants.end()) {
                    // Found a constant - use the stored type info
                    const std::string& tml_type = const_it->second.tml_type;
                    if (tml_type == "I8")
                        return make_primitive(PrimitiveKind::I8);
                    if (tml_type == "I16")
                        return make_primitive(PrimitiveKind::I16);
                    if (tml_type == "I32")
                        return make_primitive(PrimitiveKind::I32);
                    if (tml_type == "I64")
                        return make_primitive(PrimitiveKind::I64);
                    if (tml_type == "I128")
                        return make_primitive(PrimitiveKind::I128);
                    if (tml_type == "U8")
                        return make_primitive(PrimitiveKind::U8);
                    if (tml_type == "U16")
                        return make_primitive(PrimitiveKind::U16);
                    if (tml_type == "U32")
                        return make_primitive(PrimitiveKind::U32);
                    if (tml_type == "U64")
                        return make_primitive(PrimitiveKind::U64);
                    if (tml_type == "U128")
                        return make_primitive(PrimitiveKind::U128);
                    if (tml_type == "Char")
                        return make_primitive(PrimitiveKind::U32);
                    if (tml_type == "Bool")
                        return make_primitive(PrimitiveKind::Bool);
                    if (tml_type == "F32")
                        return make_primitive(PrimitiveKind::F32);
                    if (tml_type == "F64")
                        return make_primitive(PrimitiveKind::F64);
                    if (tml_type == "Str")
                        return make_primitive(PrimitiveKind::Str);
                    // Handle tuple types like "(U8, U8, U8)"
                    if (tml_type.size() >= 2 && tml_type.front() == '(' && tml_type.back() == ')') {
                        std::string inner = tml_type.substr(1, tml_type.size() - 2);
                        std::vector<TypePtr> elem_types;
                        size_t start = 0;
                        int depth = 0;
                        for (size_t ci = 0; ci <= inner.size(); ++ci) {
                            if (ci < inner.size() && inner[ci] == '(')
                                depth++;
                            else if (ci < inner.size() && inner[ci] == ')')
                                depth--;
                            else if ((ci == inner.size() || (inner[ci] == ',' && depth == 0))) {
                                std::string elem = inner.substr(start, ci - start);
                                // Trim whitespace
                                size_t fs = elem.find_first_not_of(' ');
                                size_t ls = elem.find_last_not_of(' ');
                                if (fs != std::string::npos)
                                    elem = elem.substr(fs, ls - fs + 1);
                                if (!elem.empty()) {
                                    // Recursively resolve element type
                                    if (elem == "I8")
                                        elem_types.push_back(make_primitive(PrimitiveKind::I8));
                                    else if (elem == "I16")
                                        elem_types.push_back(make_primitive(PrimitiveKind::I16));
                                    else if (elem == "I32")
                                        elem_types.push_back(make_primitive(PrimitiveKind::I32));
                                    else if (elem == "I64")
                                        elem_types.push_back(make_primitive(PrimitiveKind::I64));
                                    else if (elem == "I128")
                                        elem_types.push_back(make_primitive(PrimitiveKind::I128));
                                    else if (elem == "U8")
                                        elem_types.push_back(make_primitive(PrimitiveKind::U8));
                                    else if (elem == "U16")
                                        elem_types.push_back(make_primitive(PrimitiveKind::U16));
                                    else if (elem == "U32")
                                        elem_types.push_back(make_primitive(PrimitiveKind::U32));
                                    else if (elem == "U64")
                                        elem_types.push_back(make_primitive(PrimitiveKind::U64));
                                    else if (elem == "U128")
                                        elem_types.push_back(make_primitive(PrimitiveKind::U128));
                                    else if (elem == "F32")
                                        elem_types.push_back(make_primitive(PrimitiveKind::F32));
                                    else if (elem == "F64")
                                        elem_types.push_back(make_primitive(PrimitiveKind::F64));
                                    else if (elem == "Bool")
                                        elem_types.push_back(make_primitive(PrimitiveKind::Bool));
                                    else if (elem == "Char")
                                        elem_types.push_back(make_primitive(PrimitiveKind::U32));
                                    else if (elem == "Str")
                                        elem_types.push_back(make_primitive(PrimitiveKind::Str));
                                    else
                                        elem_types.push_back(make_primitive(PrimitiveKind::I64));
                                }
                                start = ci + 1;
                            }
                        }
                        if (!elem_types.empty())
                            return make_tuple(std::move(elem_types));
                    }
                    // Fallback for backward compatibility
                    if (const_module_path.find("char") != std::string::npos) {
                        return make_primitive(PrimitiveKind::U32);
                    }
                    // Default to I64 for unknown types
                    return make_primitive(PrimitiveKind::I64);
                }
            }
        }

        // Check if it's a const generic parameter (e.g., N in impl[T, const N: I64])
        auto const_param_it = current_const_params_.find(ident.name);
        if (const_param_it != current_const_params_.end()) {
            return const_param_it->second.value_type ? const_param_it->second.value_type
                                                     : make_i64();
        }

        // Build error message with suggestions
        std::string msg = "Undefined variable: " + ident.name;
        auto all_names = get_all_known_names();
        auto similar = find_similar_names(ident.name, all_names);
        if (!similar.empty()) {
            msg += ". Did you mean: ";
            for (size_t i = 0; i < similar.size(); ++i) {
                if (i > 0)
                    msg += ", ";
                msg += "`" + similar[i] + "`";
            }
            msg += "?";
        }
        error(msg, span);
        return make_unit();
    }
    return sym->type;
}

// Note: check_binary, check_unary, check_field_access, check_index, check_block moved to
// expr_ops.cpp Note: check_interp_string, check_template_literal, check_cast, check_is,
// check_await,
//       check_lowlevel, check_base, check_new, type_satisfies_lifetime_bound moved to
//       expr_special.cpp
// Note: check_call, check_method_call moved to expr_call.cpp

} // namespace tml::types
