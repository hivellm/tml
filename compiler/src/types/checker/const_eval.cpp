//! # Type Checker - Const Expression Evaluation
//!
//! This file implements compile-time evaluation of constant expressions.
//!
//! ## Const Evaluation
//!
//! `evaluate_const_expr()` evaluates expressions at compile time for:
//! - Array size specifications: `[T; N]`
//! - Const generic arguments: `Buffer[N]`
//! - Const declarations: `const SIZE: I32 = 100`
//!
//! ## Supported Expressions
//!
//! | Expression     | Example           | Result Type      |
//! |----------------|-------------------|------------------|
//! | Integer literal| `42`, `100i32`    | ConstValue::I64/U64 |
//! | Bool literal   | `true`, `false`   | ConstValue::Bool |
//! | Char literal   | `'a'`             | ConstValue::Char |
//! | Unary ops      | `-10`, `not true` | Same as operand  |
//! | Binary ops     | `2 + 3`, `a and b`| Computed value   |
//! | Const reference| `SIZE`            | Stored value     |
//!
//! ## Const Generic Parameters
//!
//! `extract_const_params()` extracts const generic parameters from
//! declarations like `type Buffer[const N: I64]`.

#include "types/checker.hpp"
#include "types/type.hpp"

namespace tml::types {

// Helper to check if a type is a signed integer
static bool is_signed_integer(const TypePtr& type) {
    if (!type || !type->is<PrimitiveType>()) {
        return false;
    }
    auto kind = type->as<PrimitiveType>().kind;
    switch (kind) {
    case PrimitiveKind::I8:
    case PrimitiveKind::I16:
    case PrimitiveKind::I32:
    case PrimitiveKind::I64:
    case PrimitiveKind::I128:
        return true;
    default:
        return false;
    }
}

auto TypeChecker::evaluate_const_expr(const parser::Expr& expr, TypePtr expected_type)
    -> std::optional<ConstValue> {
    // Handle literal expressions
    if (expr.is<parser::LiteralExpr>()) {
        const auto& lit = expr.as<parser::LiteralExpr>();
        switch (lit.token.kind) {
        case lexer::TokenKind::IntLiteral: {
            // Extract value from IntValue variant
            if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                auto int_val = std::get<lexer::IntValue>(lit.token.value);
                if (is_signed_integer(expected_type)) {
                    return ConstValue::from_i64(static_cast<int64_t>(int_val.value), expected_type);
                } else {
                    return ConstValue::from_u64(int_val.value, expected_type);
                }
            }
            break;
        }
        case lexer::TokenKind::BoolLiteral: {
            if (std::holds_alternative<bool>(lit.token.value)) {
                return ConstValue::from_bool(std::get<bool>(lit.token.value), make_bool());
            }
            break;
        }
        case lexer::TokenKind::CharLiteral: {
            if (std::holds_alternative<lexer::CharValue>(lit.token.value)) {
                auto char_val = std::get<lexer::CharValue>(lit.token.value);
                // Truncate to char (char32_t to char)
                return ConstValue::from_char(static_cast<char>(char_val.value),
                                             make_primitive(PrimitiveKind::Char));
            }
            break;
        }
        default:
            break;
        }
    }

    // Handle identifier expressions (for const generic params)
    if (expr.is<parser::IdentExpr>()) {
        const auto& ident = expr.as<parser::IdentExpr>();
        // Check if this is a const generic parameter
        auto it = current_const_params_.find(ident.name);
        if (it != current_const_params_.end()) {
            // This is a reference to a const generic param - we can't evaluate it
            // to a concrete value, but we can validate it exists
            return std::nullopt; // Will be resolved during monomorphization
        }

        // Check if this is a known constant that was previously evaluated
        auto const_it = const_values_.find(ident.name);
        if (const_it != const_values_.end()) {
            // Return the previously evaluated const value
            return const_it->second;
        }
    }

    // Handle unary expressions (e.g., -10)
    if (expr.is<parser::UnaryExpr>()) {
        const auto& unary = expr.as<parser::UnaryExpr>();
        auto operand = evaluate_const_expr(*unary.operand, expected_type);
        if (!operand) {
            return std::nullopt;
        }

        switch (unary.op) {
        case parser::UnaryOp::Neg: {
            if (std::holds_alternative<int64_t>(operand->value)) {
                return ConstValue::from_i64(-std::get<int64_t>(operand->value), expected_type);
            } else if (std::holds_alternative<uint64_t>(operand->value)) {
                // Negating unsigned is allowed but may overflow
                return ConstValue::from_i64(
                    -static_cast<int64_t>(std::get<uint64_t>(operand->value)), expected_type);
            }
            break;
        }
        case parser::UnaryOp::Not: {
            if (std::holds_alternative<bool>(operand->value)) {
                return ConstValue::from_bool(!std::get<bool>(operand->value), make_bool());
            }
            break;
        }
        case parser::UnaryOp::BitNot: {
            if (std::holds_alternative<int64_t>(operand->value)) {
                return ConstValue::from_i64(~std::get<int64_t>(operand->value), expected_type);
            } else if (std::holds_alternative<uint64_t>(operand->value)) {
                return ConstValue::from_u64(~std::get<uint64_t>(operand->value), expected_type);
            }
            break;
        }
        default:
            break;
        }
    }

    // Handle binary expressions
    if (expr.is<parser::BinaryExpr>()) {
        const auto& binary = expr.as<parser::BinaryExpr>();
        auto left = evaluate_const_expr(*binary.left, expected_type);
        auto right = evaluate_const_expr(*binary.right, expected_type);

        if (!left || !right) {
            return std::nullopt;
        }

        // Integer operations
        if (std::holds_alternative<int64_t>(left->value) &&
            std::holds_alternative<int64_t>(right->value)) {
            int64_t l = std::get<int64_t>(left->value);
            int64_t r = std::get<int64_t>(right->value);

            switch (binary.op) {
            case parser::BinaryOp::Add:
                return ConstValue::from_i64(l + r, expected_type);
            case parser::BinaryOp::Sub:
                return ConstValue::from_i64(l - r, expected_type);
            case parser::BinaryOp::Mul:
                return ConstValue::from_i64(l * r, expected_type);
            case parser::BinaryOp::Div:
                if (r == 0) {
                    error("Division by zero in const expression", expr.span);
                    return std::nullopt;
                }
                return ConstValue::from_i64(l / r, expected_type);
            case parser::BinaryOp::Mod:
                if (r == 0) {
                    error("Modulo by zero in const expression", expr.span);
                    return std::nullopt;
                }
                return ConstValue::from_i64(l % r, expected_type);
            case parser::BinaryOp::BitAnd:
                return ConstValue::from_i64(l & r, expected_type);
            case parser::BinaryOp::BitOr:
                return ConstValue::from_i64(l | r, expected_type);
            case parser::BinaryOp::BitXor:
                return ConstValue::from_i64(l ^ r, expected_type);
            case parser::BinaryOp::Shl:
                return ConstValue::from_i64(l << r, expected_type);
            case parser::BinaryOp::Shr:
                return ConstValue::from_i64(l >> r, expected_type);
            case parser::BinaryOp::Eq:
                return ConstValue::from_bool(l == r, make_bool());
            case parser::BinaryOp::Ne:
                return ConstValue::from_bool(l != r, make_bool());
            case parser::BinaryOp::Lt:
                return ConstValue::from_bool(l < r, make_bool());
            case parser::BinaryOp::Le:
                return ConstValue::from_bool(l <= r, make_bool());
            case parser::BinaryOp::Gt:
                return ConstValue::from_bool(l > r, make_bool());
            case parser::BinaryOp::Ge:
                return ConstValue::from_bool(l >= r, make_bool());
            default:
                break;
            }
        }

        // Unsigned integer operations
        if (std::holds_alternative<uint64_t>(left->value) &&
            std::holds_alternative<uint64_t>(right->value)) {
            uint64_t l = std::get<uint64_t>(left->value);
            uint64_t r = std::get<uint64_t>(right->value);

            switch (binary.op) {
            case parser::BinaryOp::Add:
                return ConstValue::from_u64(l + r, expected_type);
            case parser::BinaryOp::Sub:
                return ConstValue::from_u64(l - r, expected_type);
            case parser::BinaryOp::Mul:
                return ConstValue::from_u64(l * r, expected_type);
            case parser::BinaryOp::Div:
                if (r == 0) {
                    error("Division by zero in const expression", expr.span);
                    return std::nullopt;
                }
                return ConstValue::from_u64(l / r, expected_type);
            case parser::BinaryOp::Mod:
                if (r == 0) {
                    error("Modulo by zero in const expression", expr.span);
                    return std::nullopt;
                }
                return ConstValue::from_u64(l % r, expected_type);
            case parser::BinaryOp::BitAnd:
                return ConstValue::from_u64(l & r, expected_type);
            case parser::BinaryOp::BitOr:
                return ConstValue::from_u64(l | r, expected_type);
            case parser::BinaryOp::BitXor:
                return ConstValue::from_u64(l ^ r, expected_type);
            case parser::BinaryOp::Shl:
                return ConstValue::from_u64(l << r, expected_type);
            case parser::BinaryOp::Shr:
                return ConstValue::from_u64(l >> r, expected_type);
            case parser::BinaryOp::Eq:
                return ConstValue::from_bool(l == r, make_bool());
            case parser::BinaryOp::Ne:
                return ConstValue::from_bool(l != r, make_bool());
            case parser::BinaryOp::Lt:
                return ConstValue::from_bool(l < r, make_bool());
            case parser::BinaryOp::Le:
                return ConstValue::from_bool(l <= r, make_bool());
            case parser::BinaryOp::Gt:
                return ConstValue::from_bool(l > r, make_bool());
            case parser::BinaryOp::Ge:
                return ConstValue::from_bool(l >= r, make_bool());
            default:
                break;
            }
        }

        // Boolean operations
        if (std::holds_alternative<bool>(left->value) &&
            std::holds_alternative<bool>(right->value)) {
            bool l = std::get<bool>(left->value);
            bool r = std::get<bool>(right->value);

            switch (binary.op) {
            case parser::BinaryOp::And:
                return ConstValue::from_bool(l && r, make_bool());
            case parser::BinaryOp::Or:
                return ConstValue::from_bool(l || r, make_bool());
            case parser::BinaryOp::Eq:
                return ConstValue::from_bool(l == r, make_bool());
            case parser::BinaryOp::Ne:
                return ConstValue::from_bool(l != r, make_bool());
            default:
                break;
            }
        }
    }

    // Unable to evaluate - this may be a non-const expression
    return std::nullopt;
}

auto TypeChecker::extract_const_params(const std::vector<parser::GenericParam>& params)
    -> std::vector<ConstGenericParam> {
    std::vector<ConstGenericParam> const_params;
    for (const auto& param : params) {
        if (param.is_const && param.const_type) {
            TypePtr value_type = resolve_type(**param.const_type);
            const_params.push_back(ConstGenericParam{param.name, value_type});
        }
    }
    return const_params;
}

} // namespace tml::types
