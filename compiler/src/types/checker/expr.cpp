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

// Helper to check if a literal value is zero
static bool is_literal_zero(const parser::Expr& expr) {
    if (expr.is<parser::LiteralExpr>()) {
        const auto& lit = expr.as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::IntLiteral) {
            return lit.token.int_value().value == 0;
        }
        if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
            return lit.token.float_value().value == 0.0;
        }
    }
    return false;
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
                return check_method_call(e);
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
                return check_literal(e, expected_type);
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
                    return check_literal(e.operand->template as<parser::LiteralExpr>(),
                                         expected_type, true /* is_negated */);
                }
                return check_unary(e);
            } else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
                // For array expressions, propagate expected type for element coercion
                return check_array(e, expected_type);
            } else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
                // For tuple expressions, propagate expected element types for coercion
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

        // Check imported types
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

        // Check if it's an imported constant
        auto const_imported_path = env_.resolve_imported_symbol(ident.name);
        if (const_imported_path.has_value()) {
            std::string const_module_path;
            size_t const_pos = const_imported_path->rfind("::");
            if (const_pos != std::string::npos) {
                const_module_path = const_imported_path->substr(0, const_pos);
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

auto TypeChecker::check_binary(const parser::BinaryExpr& binary) -> TypePtr {
    auto left = check_expr(*binary.left);

    // For arithmetic operations, propagate left operand's type as expected type
    // for the right operand. This allows unsuffixed integer literals to infer
    // the correct type (e.g., `x * 3` where x is I32 → 3 infers as I32).
    TypePtr right;
    switch (binary.op) {
    case parser::BinaryOp::Add:
    case parser::BinaryOp::Sub:
    case parser::BinaryOp::Mul:
    case parser::BinaryOp::Div:
    case parser::BinaryOp::Mod:
        right = check_expr(*binary.right, left);
        break;
    default:
        right = check_expr(*binary.right);
        break;
    }

    auto check_binary_types = [&](const char* op_name) {
        TypePtr resolved_left = env_.resolve(left);
        TypePtr resolved_right = env_.resolve(right);
        if (!types_compatible(resolved_left, resolved_right)) {
            error(std::string("Binary operator '") + op_name + "' requires matching types, found " +
                      type_to_string(resolved_left) + " and " + type_to_string(resolved_right),
                  binary.left->span);
        }
    };

    auto check_assignable = [&]() {
        if (binary.left->is<parser::IdentExpr>()) {
            const auto& ident = binary.left->as<parser::IdentExpr>();
            auto sym = env_.current_scope()->lookup(ident.name);
            if (sym && !sym->is_mutable) {
                // Allow assignment through mutable references (mut ref T)
                // Even if the variable itself isn't mutable, we can assign through it
                auto resolved = env_.resolve(sym->type);
                if (resolved && resolved->is<RefType>() && resolved->as<RefType>().is_mut) {
                    // This is a mutable reference, assignment through it is allowed
                    return;
                }
                error("Cannot assign to immutable variable '" + ident.name + "'", binary.left->span,
                      "T013");
            }
        }
    };

    switch (binary.op) {
    case parser::BinaryOp::Add: {
        // Pointer arithmetic: ptr + int = ptr
        TypePtr resolved_left = env_.resolve(left);
        TypePtr resolved_right = env_.resolve(right);
        if (resolved_left && resolved_left->is<PtrType>()) {
            // ptr + int is valid pointer arithmetic
            if (is_integer_type(resolved_right)) {
                return left; // Result is the same pointer type
            }
        }
        check_binary_types("+");
        return left;
    }
    case parser::BinaryOp::Sub: {
        // Pointer arithmetic: ptr - int = ptr, ptr - ptr = int
        TypePtr resolved_left = env_.resolve(left);
        TypePtr resolved_right = env_.resolve(right);
        if (resolved_left && resolved_left->is<PtrType>()) {
            if (is_integer_type(resolved_right)) {
                // ptr - int = ptr
                return left;
            }
            if (resolved_right && resolved_right->is<PtrType>()) {
                // ptr - ptr = I64 (pointer difference)
                return make_i64();
            }
        }
        check_binary_types("-");
        return left;
    }
    case parser::BinaryOp::Mul:
        check_binary_types("*");
        return left;
    case parser::BinaryOp::Div:
    case parser::BinaryOp::Mod: {
        // Check for division by zero literal
        if (is_literal_zero(*binary.right)) {
            error("Division by zero", binary.right->span, "T052");
        }
        check_binary_types(binary.op == parser::BinaryOp::Div ? "/" : "%");
        return left;
    }
    case parser::BinaryOp::Lt:
    case parser::BinaryOp::Le:
    case parser::BinaryOp::Gt:
    case parser::BinaryOp::Ge:
    case parser::BinaryOp::Eq:
    case parser::BinaryOp::Ne:
        check_binary_types("comparison");
        return make_bool();
    case parser::BinaryOp::And:
    case parser::BinaryOp::Or:
        return make_bool();
    case parser::BinaryOp::BitAnd:
    case parser::BinaryOp::BitOr:
    case parser::BinaryOp::BitXor:
    case parser::BinaryOp::Shl:
    case parser::BinaryOp::Shr:
        return left;
    case parser::BinaryOp::Assign: {
        check_assignable();
        // For assignment through mutable references, check if LHS is mut ref T
        // In that case, RHS should be compatible with T (the inner type)
        TypePtr resolved_left = env_.resolve(left);
        TypePtr resolved_right = env_.resolve(right);
        if (resolved_left && resolved_left->is<RefType>() && resolved_left->as<RefType>().is_mut) {
            // Assigning through mut ref T - check RHS against inner type T
            TypePtr inner = env_.resolve(resolved_left->as<RefType>().inner);
            if (!types_compatible(inner, resolved_right)) {
                error(std::string("Cannot assign value of type ") + type_to_string(resolved_right) +
                          " through reference of type " + type_to_string(resolved_left),
                      binary.left->span);
            }
        } else {
            check_binary_types("=");
        }
        return make_unit();
    }
    case parser::BinaryOp::AddAssign:
    case parser::BinaryOp::SubAssign:
    case parser::BinaryOp::MulAssign:
    case parser::BinaryOp::BitAndAssign:
    case parser::BinaryOp::BitOrAssign:
    case parser::BinaryOp::BitXorAssign:
    case parser::BinaryOp::ShlAssign:
    case parser::BinaryOp::ShrAssign:
        check_assignable();
        return make_unit();
    case parser::BinaryOp::DivAssign:
    case parser::BinaryOp::ModAssign:
        // Check for division by zero literal
        if (is_literal_zero(*binary.right)) {
            error("Division by zero", binary.right->span, "T052");
        }
        check_assignable();
        return make_unit();
    }
    return make_unit();
}

auto TypeChecker::check_unary(const parser::UnaryExpr& unary) -> TypePtr {
    auto operand = check_expr(*unary.operand);

    switch (unary.op) {
    case parser::UnaryOp::Neg:
        return operand;
    case parser::UnaryOp::Not:
        return make_bool();
    case parser::UnaryOp::BitNot:
        return operand;
    case parser::UnaryOp::Ref:
        // In lowlevel blocks, & returns raw pointer (*T) instead of reference (ref T)
        if (in_lowlevel_) {
            if (operand->is<RefType>()) {
                return make_ptr(operand->as<RefType>().inner, false);
            }
            return make_ptr(operand, false);
        }
        // Reborrowing: ref (ref T) -> ref T (like Rust's automatic reborrow)
        if (operand->is<RefType>()) {
            return make_ref(operand->as<RefType>().inner, false);
        }
        return make_ref(operand, false);
    case parser::UnaryOp::RefMut:
        // In lowlevel blocks, &mut returns raw mutable pointer (*mut T)
        if (in_lowlevel_) {
            if (operand->is<RefType>()) {
                return make_ptr(operand->as<RefType>().inner, true);
            }
            return make_ptr(operand, true);
        }
        // Reborrowing: mut ref (mut ref T) -> mut ref T (like Rust's automatic reborrow)
        if (operand->is<RefType>() && operand->as<RefType>().is_mut) {
            return operand; // Already a mutable ref, just return it
        }
        // Allow reborrow from mutable to mutable
        if (operand->is<RefType>()) {
            return make_ref(operand->as<RefType>().inner, true);
        }
        return make_ref(operand, true);
    case parser::UnaryOp::Deref:
        if (operand->is<RefType>()) {
            return operand->as<RefType>().inner;
        }
        if (operand->is<PtrType>()) {
            return operand->as<PtrType>().inner;
        }
        // Handle NamedType cases for pointer and smart pointer types
        if (operand->is<NamedType>()) {
            const auto& named = operand->as<NamedType>();
            // Handle Ptr[T] which is stored as NamedType{name="Ptr", type_args=[T]}
            // This is common in generic contexts where Ptr[Node[T]] appears
            if (named.name == "Ptr" && !named.type_args.empty()) {
                return named.type_args[0];
            }
            // Handle smart pointer types that implement Deref behavior
            // Dereferencing these returns the inner type T
            static const std::unordered_set<std::string> deref_types = {
                "Arc",
                "Rc",
                "Box",
                "Heap",
                "Shared",
                "Sync",
                "MutexGuard",
                "RwLockReadGuard",
                "RwLockWriteGuard",
                "Ref",
                "RefMut",
            };
            if (deref_types.count(named.name) > 0 && !named.type_args.empty()) {
                return named.type_args[0];
            }
        }
        error("Cannot dereference non-reference type", unary.operand->span, "T017");
        return make_unit();
    case parser::UnaryOp::Inc:
    case parser::UnaryOp::Dec:
        return operand;
    }
    return make_unit();
}

// Note: check_call moved to expr_call.cpp

// Note: check_method_call moved to expr_call.cpp

auto TypeChecker::check_field_access(const parser::FieldExpr& field) -> TypePtr {
    // Handle static field access: ClassName.staticField
    if (field.object->is<parser::IdentExpr>()) {
        const auto& ident = field.object->as<parser::IdentExpr>();
        auto class_def = env_.lookup_class(ident.name);
        if (class_def.has_value()) {
            // Look for static field
            for (const auto& f : class_def->fields) {
                if (f.name == field.field && f.is_static) {
                    // Check visibility for static field access
                    if (!check_member_visibility(f.vis, ident.name, field.field,
                                                 field.object->span)) {
                        return f.type; // Return type for error recovery
                    }
                    return f.type;
                }
            }
            // If we're here, it might be a non-static field accessed statically (error)
            // Or the field doesn't exist - fall through to regular handling
        }
    }

    auto obj_type = check_expr(*field.object);

    if (obj_type->is<RefType>()) {
        obj_type = obj_type->as<RefType>().inner;
    }

    // Handle class type field access with visibility checking
    if (obj_type->is<ClassType>()) {
        auto& class_type = obj_type->as<ClassType>();
        // Search for the field in this class and its parent classes
        std::string current_class = class_type.name;
        while (!current_class.empty()) {
            auto current_def = env_.lookup_class(current_class);
            if (!current_def.has_value())
                break;

            // Look for the field in this class
            for (const auto& f : current_def->fields) {
                if (f.name == field.field) {
                    // Check visibility (defining class is current_class, not class_type.name)
                    if (!check_member_visibility(f.vis, current_class, field.field,
                                                 field.object->span)) {
                        return f.type; // Return type anyway for error recovery
                    }
                    return f.type;
                }
            }

            // Check parent class
            if (current_def->base_class.has_value()) {
                current_class = current_def->base_class.value();
            } else {
                break;
            }
        }
        error("Unknown field: " + field.field + " on class " + class_type.name, field.object->span,
              "T073");
    }

    if (obj_type->is<NamedType>()) {
        auto& named = obj_type->as<NamedType>();

        // First check if this is a class (NamedType can refer to classes too)
        auto class_def = env_.lookup_class(named.name);
        if (class_def.has_value()) {
            // Search for the field in this class and its parent classes
            std::string current_class = named.name;
            while (!current_class.empty()) {
                auto current_def = env_.lookup_class(current_class);
                if (!current_def.has_value())
                    break;

                // Look for the field in this class
                for (const auto& f : current_def->fields) {
                    if (f.name == field.field) {
                        // Check visibility
                        if (!check_member_visibility(f.vis, current_class, field.field,
                                                     field.object->span)) {
                            return f.type; // Return type anyway for error recovery
                        }
                        return f.type;
                    }
                }

                // Check parent class
                if (current_def->base_class.has_value()) {
                    current_class = current_def->base_class.value();
                } else {
                    break;
                }
            }
            error("Unknown field: " + field.field + " on class " + named.name, field.object->span,
                  "T073");
            return make_unit();
        }

        // Handle Ptr[T] - dereference through to inner type for field access
        // This allows (*ptr).field syntax to work by auto-dereferencing Ptr[T] to T
        if (named.name == "Ptr" && !named.type_args.empty()) {
            auto inner_type = named.type_args[0];
            if (inner_type->is<NamedType>()) {
                auto& inner_named = inner_type->as<NamedType>();
                auto inner_struct = env_.lookup_struct(inner_named.name);
                if (inner_struct) {
                    std::unordered_map<std::string, TypePtr> inner_subs;
                    if (!inner_struct->type_params.empty() && !inner_named.type_args.empty()) {
                        for (size_t i = 0; i < inner_struct->type_params.size() &&
                                           i < inner_named.type_args.size();
                             ++i) {
                            inner_subs[inner_struct->type_params[i]] = inner_named.type_args[i];
                        }
                    }
                    for (const auto& fld : inner_struct->fields) {
                        if (fld.name == field.field) {
                            if (!inner_subs.empty()) {
                                return substitute_type(fld.type, inner_subs);
                            }
                            return fld.type;
                        }
                    }
                    error("Unknown field: " + field.field + " on Ptr[" + inner_named.name + "]",
                          field.object->span, "T074");
                    return make_unit();
                }
            }
        }

        // Otherwise check if it's a struct
        auto struct_def = env_.lookup_struct(named.name);
        if (struct_def) {
            std::unordered_map<std::string, TypePtr> subs;
            if (!struct_def->type_params.empty() && !named.type_args.empty()) {
                for (size_t i = 0; i < struct_def->type_params.size() && i < named.type_args.size();
                     ++i) {
                    subs[struct_def->type_params[i]] = named.type_args[i];
                }
            }

            for (const auto& fld : struct_def->fields) {
                if (fld.name == field.field) {
                    if (!subs.empty()) {
                        return substitute_type(fld.type, subs);
                    }
                    return fld.type;
                }
            }

            // Deref coercion: if field not found and type implements Deref, try inner type
            // This handles smart pointers like Arc[T], Box[T], MutexGuard[T], etc.
            static const std::unordered_set<std::string> deref_types = {
                "Arc",
                "Rc",
                "Box",
                "Heap",
                "Shared",
                "Sync",
                "MutexGuard",
                "RwLockReadGuard",
                "RwLockWriteGuard",
                "Ref",
                "RefMut",
                "Ptr", // Allow (*ptr).field to access fields through Ptr[T]
            };

            if (deref_types.count(named.name) > 0 && !named.type_args.empty()) {
                // Get the inner type (T in Arc[T])
                auto inner_type = named.type_args[0];

                // Recursively look up field on the inner type
                if (inner_type->is<NamedType>()) {
                    auto& inner_named = inner_type->as<NamedType>();
                    auto inner_struct = env_.lookup_struct(inner_named.name);
                    if (inner_struct) {
                        std::unordered_map<std::string, TypePtr> inner_subs;
                        if (!inner_struct->type_params.empty() && !inner_named.type_args.empty()) {
                            for (size_t i = 0; i < inner_struct->type_params.size() &&
                                               i < inner_named.type_args.size();
                                 ++i) {
                                inner_subs[inner_struct->type_params[i]] = inner_named.type_args[i];
                            }
                        }

                        for (const auto& fld : inner_struct->fields) {
                            if (fld.name == field.field) {
                                if (!inner_subs.empty()) {
                                    return substitute_type(fld.type, inner_subs);
                                }
                                return fld.type;
                            }
                        }
                    }
                }
            }

            error("Unknown field: " + field.field, field.object->span, "T005");
        }
    }

    if (obj_type->is<TupleType>()) {
        auto& tuple = obj_type->as<TupleType>();
        try {
            size_t idx = std::stoul(field.field);
            if (idx < tuple.elements.size()) {
                return tuple.elements[idx];
            }
        } catch (...) {}
        error("Invalid tuple field: " + field.field, field.object->span, "T036");
    }

    return make_unit();
}

auto TypeChecker::check_index(const parser::IndexExpr& idx) -> TypePtr {
    auto obj_type = check_expr(*idx.object);
    check_expr(*idx.index);

    // Resolve the type in case it's a type alias
    auto resolved = env_.resolve(obj_type);

    // Unwrap RefType to handle ref [T] and ref [T; N] indexing
    if (resolved->is<RefType>()) {
        resolved = env_.resolve(resolved->as<RefType>().inner);
    }

    if (resolved->is<ArrayType>()) {
        return resolved->as<ArrayType>().element;
    }
    if (resolved->is<SliceType>()) {
        return resolved->as<SliceType>().element;
    }

    return make_unit();
}

auto TypeChecker::check_block(const parser::BlockExpr& block) -> TypePtr {
    TML_DEBUG_LN("[check_block] Entering block with " << block.stmts.size() << " statements");
    env_.push_scope();
    TypePtr result = make_unit();

    for (const auto& stmt : block.stmts) {
        TML_DEBUG_LN("[check_block] Checking statement at index " << stmt->kind.index());
        result = check_stmt(*stmt);
    }

    if (block.expr) {
        TML_DEBUG_LN("[check_block] Checking trailing expression");
        // Pass expected return type for implicit returns (array literal inference)
        result = check_expr(**block.expr, current_return_type_);
    }

    env_.pop_scope();
    TML_DEBUG_LN("[check_block] Exiting block");
    return result;
}

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
