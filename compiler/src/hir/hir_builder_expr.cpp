//! # HIR Builder - Expression Lowering
//!
//! This file implements expression lowering from AST to HIR.
//!
//! ## Overview
//!
//! Expression lowering transforms parser AST expressions into HIR expressions.
//! This process involves:
//!
//! - **Type inference**: Determining expression types from context
//! - **Desugaring**: Converting syntactic sugar to explicit forms
//! - **Name resolution**: Linking identifiers to their declarations
//!
//! ## Expression Categories
//!
//! | Category     | AST Types                  | HIR Types             |
//! |--------------|----------------------------|-----------------------|
//! | Atoms        | Literal, Ident, Path       | Literal, Var, Enum    |
//! | Operations   | Binary, Unary, Cast        | Binary, Unary, Cast   |
//! | Access       | Call, Method, Field, Index | Call, Method, Field   |
//! | Constructors | Tuple, Array, Struct       | Tuple, Array, Struct  |
//! | Control      | If, When, Loop, For, While | If, When, Loop, For   |
//! | Jumps        | Return, Break, Continue    | Return, Break, Continue|
//! | Special      | Closure, Try, Await, Range | Closure, Try, Await   |
//!
//! ## Key Transformations
//!
//! - `for` loops are desugared to iterator protocol calls
//! - Ternary `a ? b : c` becomes `if a { b } else { c }`
//! - `if let` becomes `when` with two arms
//! - Range `a..b` becomes `Range { start: a, end: b }`
//! - `?` operator becomes explicit error propagation
//!
//! ## See Also
//!
//! - `hir_builder.cpp` - Main builder implementation
//! - `hir_builder_stmt.cpp` - Statement lowering
//! - `hir_builder_pattern.cpp` - Pattern lowering

#include "hir/hir_builder.hpp"
#include "lexer/token.hpp"

namespace tml::hir {

// ============================================================================
// Expression Lowering Dispatch
// ============================================================================
//
// Main entry point for expression lowering. Uses std::visit to dispatch
// to type-specific lowering functions based on the expression variant.
// Each expression type has its own lower_* function below.

auto HirBuilder::lower_expr(const parser::Expr& expr) -> HirExprPtr {
    return std::visit(
        [&](const auto& e) -> HirExprPtr {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
                return lower_literal(e);
            } else if constexpr (std::is_same_v<T, parser::IdentExpr>) {
                return lower_ident(e);
            } else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
                return lower_binary(e);
            } else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
                return lower_unary(e);
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                return lower_call(e);
            } else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
                return lower_method_call(e);
            } else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
                return lower_field(e);
            } else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
                return lower_index(e);
            } else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
                return lower_tuple(e);
            } else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
                return lower_array(e);
            } else if constexpr (std::is_same_v<T, parser::StructExpr>) {
                return lower_struct_expr(e);
            } else if constexpr (std::is_same_v<T, parser::IfExpr>) {
                return lower_if(e);
            } else if constexpr (std::is_same_v<T, parser::TernaryExpr>) {
                return lower_ternary(e);
            } else if constexpr (std::is_same_v<T, parser::IfLetExpr>) {
                return lower_if_let(e);
            } else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
                return lower_when(e);
            } else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
                return lower_loop(e);
            } else if constexpr (std::is_same_v<T, parser::WhileExpr>) {
                return lower_while(e);
            } else if constexpr (std::is_same_v<T, parser::ForExpr>) {
                return lower_for(e);
            } else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
                return lower_block(e);
            } else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
                return lower_return(e);
            } else if constexpr (std::is_same_v<T, parser::BreakExpr>) {
                return lower_break(e);
            } else if constexpr (std::is_same_v<T, parser::ContinueExpr>) {
                return lower_continue(e);
            } else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
                return lower_closure(e);
            } else if constexpr (std::is_same_v<T, parser::RangeExpr>) {
                return lower_range(e);
            } else if constexpr (std::is_same_v<T, parser::CastExpr>) {
                return lower_cast(e);
            } else if constexpr (std::is_same_v<T, parser::TryExpr>) {
                return lower_try(e);
            } else if constexpr (std::is_same_v<T, parser::AwaitExpr>) {
                return lower_await(e);
            } else if constexpr (std::is_same_v<T, parser::PathExpr>) {
                return lower_path(e);
            } else if constexpr (std::is_same_v<T, parser::LowlevelExpr>) {
                return lower_lowlevel(e);
            } else {
                // Fallback for unsupported expressions
                return make_hir_literal(fresh_id(), int64_t(0), types::make_unit(), expr.span);
            }
        },
        expr.kind);
}

// ============================================================================
// Literal Expressions
// ============================================================================
//
// Literals are compile-time constants. The type is determined by:
// - Explicit suffix: `42i64`, `3.14f32`
// - Default inference: integers default to I32, floats to F64
// - Context: string literals are Str, bool literals are Bool

auto HirBuilder::lower_literal(const parser::LiteralExpr& lit) -> HirExprPtr {
    HirId id = fresh_id();

    switch (lit.token.kind) {
    case lexer::TokenKind::IntLiteral: {
        if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
            auto int_val = std::get<lexer::IntValue>(lit.token.value);
            // Determine type from suffix or default to I32 (like most languages)
            HirType type = types::make_i32();
            if (!int_val.suffix.empty()) {
                const auto& suffix = int_val.suffix;
                if (suffix == "i8")
                    type = types::make_primitive(types::PrimitiveKind::I8);
                else if (suffix == "i16")
                    type = types::make_primitive(types::PrimitiveKind::I16);
                else if (suffix == "i32")
                    type = types::make_i32();
                else if (suffix == "i64")
                    type = types::make_i64();
                else if (suffix == "u8")
                    type = types::make_primitive(types::PrimitiveKind::U8);
                else if (suffix == "u16")
                    type = types::make_primitive(types::PrimitiveKind::U16);
                else if (suffix == "u32")
                    type = types::make_primitive(types::PrimitiveKind::U32);
                else if (suffix == "u64")
                    type = types::make_primitive(types::PrimitiveKind::U64);
            }
            return make_hir_literal(id, static_cast<int64_t>(int_val.value), type, lit.span);
        }
        break;
    }
    case lexer::TokenKind::FloatLiteral: {
        if (std::holds_alternative<lexer::FloatValue>(lit.token.value)) {
            auto float_val = std::get<lexer::FloatValue>(lit.token.value);
            HirType type = types::make_f64();
            if (float_val.suffix == "f32") {
                type = types::make_primitive(types::PrimitiveKind::F32);
            }
            return make_hir_literal(id, float_val.value, type, lit.span);
        }
        break;
    }
    case lexer::TokenKind::StringLiteral: {
        if (std::holds_alternative<lexer::StringValue>(lit.token.value)) {
            auto str_val = std::get<lexer::StringValue>(lit.token.value);
            return make_hir_literal(id, str_val.value, types::make_str(), lit.span);
        }
        break;
    }
    case lexer::TokenKind::CharLiteral: {
        if (std::holds_alternative<lexer::CharValue>(lit.token.value)) {
            auto char_val = std::get<lexer::CharValue>(lit.token.value);
            return make_hir_literal(id, static_cast<char>(char_val.value),
                                    types::make_primitive(types::PrimitiveKind::Char), lit.span);
        }
        break;
    }
    case lexer::TokenKind::BoolLiteral: {
        if (std::holds_alternative<bool>(lit.token.value)) {
            return make_hir_literal(id, std::get<bool>(lit.token.value), types::make_bool(),
                                    lit.span);
        }
        break;
    }
    default:
        break;
    }

    // Fallback
    return make_hir_literal(id, int64_t(0), types::make_unit(), lit.span);
}

// ============================================================================
// Identifier Expressions
// ============================================================================

auto HirBuilder::lower_ident(const parser::IdentExpr& ident) -> HirExprPtr {
    // Look up variable type from type environment
    HirType type = types::make_unit();
    auto scope = type_env_.current_scope();
    if (scope) {
        if (auto var = scope->lookup(ident.name)) {
            type = type_env_.resolve(var->type);
        }
    }
    return make_hir_var(fresh_id(), ident.name, type, ident.span);
}

// ============================================================================
// Binary Expressions
// ============================================================================
//
// Binary expressions include:
// - Arithmetic: +, -, *, /, %
// - Comparison: ==, !=, <, <=, >, >=
// - Logical: and, or
// - Bitwise: &, |, ^, <<, >>
// - Assignment: =, +=, -=, etc.
//
// Assignment and compound assignment are handled separately since they
// don't produce a value like other binary operations.

auto HirBuilder::lower_binary(const parser::BinaryExpr& binary) -> HirExprPtr {
    // Handle assignment separately
    if (binary.op == parser::BinaryOp::Assign) {
        auto target = lower_expr(*binary.left);
        auto value = lower_expr(*binary.right);

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirAssignExpr{fresh_id(), std::move(target), std::move(value), binary.span};
        return expr;
    }

    // Handle compound assignment
    if (binary.op >= parser::BinaryOp::AddAssign) {
        auto target = lower_expr(*binary.left);
        auto value = lower_expr(*binary.right);

        HirCompoundOp op;
        switch (binary.op) {
        case parser::BinaryOp::AddAssign:
            op = HirCompoundOp::Add;
            break;
        case parser::BinaryOp::SubAssign:
            op = HirCompoundOp::Sub;
            break;
        case parser::BinaryOp::MulAssign:
            op = HirCompoundOp::Mul;
            break;
        case parser::BinaryOp::DivAssign:
            op = HirCompoundOp::Div;
            break;
        case parser::BinaryOp::ModAssign:
            op = HirCompoundOp::Mod;
            break;
        case parser::BinaryOp::BitAndAssign:
            op = HirCompoundOp::BitAnd;
            break;
        case parser::BinaryOp::BitOrAssign:
            op = HirCompoundOp::BitOr;
            break;
        case parser::BinaryOp::BitXorAssign:
            op = HirCompoundOp::BitXor;
            break;
        case parser::BinaryOp::ShlAssign:
            op = HirCompoundOp::Shl;
            break;
        case parser::BinaryOp::ShrAssign:
            op = HirCompoundOp::Shr;
            break;
        default:
            op = HirCompoundOp::Add;
            break;
        }

        auto expr = std::make_unique<HirExpr>();
        expr->kind =
            HirCompoundAssignExpr{fresh_id(), op, std::move(target), std::move(value), binary.span};
        return expr;
    }

    // Regular binary operation
    auto left = lower_expr(*binary.left);
    auto right = lower_expr(*binary.right);

    // Infer result type based on operator and operand types
    HirType type = left->type();

    // Comparison operators always produce Bool
    if (binary.op >= parser::BinaryOp::Eq && binary.op <= parser::BinaryOp::Ge) {
        type = types::make_bool();
    }
    // Logical operators also produce Bool
    else if (binary.op == parser::BinaryOp::And || binary.op == parser::BinaryOp::Or) {
        type = types::make_bool();
    }
    // Arithmetic operators use the left operand type (already set)

    return make_hir_binary(fresh_id(), convert_binary_op(binary.op), std::move(left),
                           std::move(right), type, binary.span);
}

// ============================================================================
// Unary Expressions
// ============================================================================

auto HirBuilder::lower_unary(const parser::UnaryExpr& unary) -> HirExprPtr {
    auto operand = lower_expr(*unary.operand);
    HirType type = operand->type();

    // Adjust type for ref/deref
    if (unary.op == parser::UnaryOp::Ref || unary.op == parser::UnaryOp::RefMut) {
        type = types::make_ref(type, unary.op == parser::UnaryOp::RefMut);
    } else if (unary.op == parser::UnaryOp::Deref) {
        if (type && type->is<types::RefType>()) {
            type = type->as<types::RefType>().inner;
        }
    } else if (unary.op == parser::UnaryOp::Not) {
        type = types::make_bool();
    }

    return make_hir_unary(fresh_id(), convert_unary_op(unary.op), std::move(operand), type,
                          unary.span);
}

// ============================================================================
// Call Expressions
// ============================================================================

auto HirBuilder::lower_call(const parser::CallExpr& call) -> HirExprPtr {
    // Get function name from callee
    std::string func_name;
    if (call.callee->is<parser::IdentExpr>()) {
        func_name = call.callee->as<parser::IdentExpr>().name;
    } else if (call.callee->is<parser::PathExpr>()) {
        const auto& path = call.callee->as<parser::PathExpr>();
        for (size_t i = 0; i < path.path.segments.size(); ++i) {
            if (i > 0) {
                func_name += "::";
            }
            func_name += path.path.segments[i];
        }
    }

    // Lower arguments
    std::vector<HirExprPtr> args;
    for (const auto& arg : call.args) {
        args.push_back(lower_expr(*arg));
    }

    // Look up function return type from type environment
    HirType return_type = types::make_unit();
    if (auto sig = type_env_.lookup_func(func_name)) {
        return_type = type_env_.resolve(sig->return_type);
    } else {
        // Check for class static method (ClassName::method)
        auto pos = func_name.find("::");
        if (pos != std::string::npos) {
            std::string class_name = func_name.substr(0, pos);
            std::string method_name = func_name.substr(pos + 2);

            // Try to find the class and its static method
            if (auto class_def = type_env_.lookup_class(class_name)) {
                for (const auto& method : class_def->methods) {
                    if (method.sig.name == method_name && method.is_static) {
                        return_type = type_env_.resolve(method.sig.return_type);
                        break;
                    }
                }
            }
        }
    }

    // Mangle func_name for class static methods: "Class::method" -> "Class__method"
    std::string mangled_func_name = func_name;
    auto pos = func_name.find("::");
    if (pos != std::string::npos) {
        std::string class_name = func_name.substr(0, pos);
        std::string method_name = func_name.substr(pos + 2);
        // Check if it's a class static method
        if (type_env_.lookup_class(class_name).has_value()) {
            mangled_func_name = class_name + "__" + method_name;
        }
    }

    return make_hir_call(fresh_id(), mangled_func_name, {}, std::move(args), return_type,
                         call.span);
}

// ============================================================================
// Method Call Expressions
// ============================================================================

auto HirBuilder::lower_method_call(const parser::MethodCallExpr& call) -> HirExprPtr {
    auto receiver = lower_expr(*call.receiver);
    HirType receiver_type = receiver->type();

    // Lower type arguments
    std::vector<HirType> type_args;
    for (const auto& type_arg : call.type_args) {
        type_args.push_back(resolve_type(*type_arg));
    }

    // Lower arguments
    std::vector<HirExprPtr> args;
    for (const auto& arg : call.args) {
        args.push_back(lower_expr(*arg));
    }

    // Look up method return type from type environment
    HirType return_type = types::make_unit();
    // Get type name from receiver to look up method
    std::string type_name;
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        type_name = receiver_type->as<types::NamedType>().name;
    } else if (receiver_type && receiver_type->is<types::ClassType>()) {
        type_name = receiver_type->as<types::ClassType>().name;
    }

    if (!type_name.empty()) {
        std::string method_name = type_name + "::" + call.method;
        if (auto sig = type_env_.lookup_func(method_name)) {
            return_type = type_env_.resolve(sig->return_type);
        } else if (auto class_def = type_env_.lookup_class(type_name)) {
            // Try to find instance method in class definition
            for (const auto& method : class_def->methods) {
                if (method.sig.name == call.method && !method.is_static) {
                    return_type = type_env_.resolve(method.sig.return_type);
                    break;
                }
            }
        }
    }

    return make_hir_method_call(fresh_id(), std::move(receiver), call.method, std::move(type_args),
                                std::move(args), receiver_type, return_type, call.span);
}

// ============================================================================
// Field Access Expressions
// ============================================================================

auto HirBuilder::lower_field(const parser::FieldExpr& field) -> HirExprPtr {
    auto object = lower_expr(*field.object);
    HirType object_type = object->type();

    // Get field index
    std::string type_name;
    int field_index = -1;
    HirType field_type = types::make_unit();

    // Handle tuple types - field name is numeric index like "0", "1"
    if (object_type && object_type->is<types::TupleType>()) {
        const auto& tuple = object_type->as<types::TupleType>();
        try {
            field_index = std::stoi(field.field);
            if (field_index >= 0 && static_cast<size_t>(field_index) < tuple.elements.size()) {
                field_type = type_env_.resolve(tuple.elements[field_index]);
            }
        } catch (...) {
            // Not a numeric field name
            field_index = -1;
        }
    } else {
        if (object_type && object_type->is<types::NamedType>()) {
            type_name = object_type->as<types::NamedType>().name;
        } else if (object_type && object_type->is<types::ClassType>()) {
            type_name = object_type->as<types::ClassType>().name;
        }
        field_index = get_field_index(type_name, field.field);
    }

    // Look up field type from struct definition or class definition
    if (!type_name.empty()) {
        if (auto struct_def = type_env_.lookup_struct(type_name)) {
            for (const auto& f : struct_def->fields) {
                if (f.first == field.field) {
                    field_type = type_env_.resolve(f.second);
                    break;
                }
            }
        } else if (auto class_def = type_env_.lookup_class(type_name)) {
            // Search for field in current class and all ancestor classes
            std::string current_class = type_name;
            bool found = false;
            while (!found && !current_class.empty()) {
                auto cur_class_def = type_env_.lookup_class(current_class);
                if (!cur_class_def.has_value()) {
                    break;
                }
                for (const auto& f : cur_class_def->fields) {
                    if (f.name == field.field) {
                        field_type = type_env_.resolve(f.type);
                        found = true;
                        break;
                    }
                }
                // Move to base class if field not found
                if (!found) {
                    if (cur_class_def->base_class.has_value() &&
                        !cur_class_def->base_class->empty()) {
                        current_class = *cur_class_def->base_class;
                    } else {
                        break;
                    }
                }
            }
        }
    }

    return make_hir_field(fresh_id(), std::move(object), field.field, field_index, field_type,
                          field.span);
}

// ============================================================================
// Index Expressions
// ============================================================================

auto HirBuilder::lower_index(const parser::IndexExpr& index) -> HirExprPtr {
    auto object = lower_expr(*index.object);
    auto idx = lower_expr(*index.index);

    // Get element type from container type
    HirType element_type = types::make_unit();
    HirType container_type = object->type();
    if (container_type) {
        if (container_type->is<types::ArrayType>()) {
            element_type = container_type->as<types::ArrayType>().element;
        } else if (container_type->is<types::SliceType>()) {
            element_type = container_type->as<types::SliceType>().element;
        }
    }

    return make_hir_index(fresh_id(), std::move(object), std::move(idx), element_type, index.span);
}

// ============================================================================
// Tuple Expressions
// ============================================================================

auto HirBuilder::lower_tuple(const parser::TupleExpr& tuple) -> HirExprPtr {
    std::vector<HirExprPtr> elements;
    std::vector<types::TypePtr> element_types;

    for (const auto& elem : tuple.elements) {
        auto lowered = lower_expr(*elem);
        element_types.push_back(lowered->type());
        elements.push_back(std::move(lowered));
    }

    HirType type = types::make_tuple(std::move(element_types));

    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirTupleExpr{fresh_id(), std::move(elements), type, tuple.span};
    return expr;
}

// ============================================================================
// Array Expressions
// ============================================================================

auto HirBuilder::lower_array(const parser::ArrayExpr& array) -> HirExprPtr {
    if (std::holds_alternative<std::vector<parser::ExprPtr>>(array.kind)) {
        // Element list: [1, 2, 3]
        const auto& elems = std::get<std::vector<parser::ExprPtr>>(array.kind);

        std::vector<HirExprPtr> elements;
        HirType element_type = types::make_unit();

        for (const auto& elem : elems) {
            auto lowered = lower_expr(*elem);
            if (elements.empty()) {
                element_type = lowered->type();
            }
            elements.push_back(std::move(lowered));
        }

        size_t size = elements.size();
        HirType type = types::make_array(element_type, size);

        auto expr = std::make_unique<HirExpr>();
        expr->kind =
            HirArrayExpr{fresh_id(), std::move(elements), element_type, size, type, array.span};
        return expr;
    } else {
        // Repeat syntax: [expr; count]
        const auto& [value_expr, count_expr] =
            std::get<std::pair<parser::ExprPtr, parser::ExprPtr>>(array.kind);

        auto value = lower_expr(*value_expr);
        HirType element_type = value->type();

        // Evaluate count at compile time from literal expression
        size_t count = 0;
        if (count_expr->is<parser::LiteralExpr>()) {
            const auto& lit = count_expr->as<parser::LiteralExpr>();
            if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                count = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
            }
        }

        HirType type = types::make_array(element_type, count);

        auto expr = std::make_unique<HirExpr>();
        expr->kind = HirArrayRepeatExpr{fresh_id(), std::move(value), count, type, array.span};
        return expr;
    }
}

// ============================================================================
// Struct Expressions
// ============================================================================

auto HirBuilder::lower_struct_expr(const parser::StructExpr& struct_expr) -> HirExprPtr {
    std::string struct_name;
    if (!struct_expr.path.segments.empty()) {
        struct_name = struct_expr.path.segments.back();
    }

    // Lower type arguments
    std::vector<HirType> type_args;
    if (struct_expr.generics) {
        for (const auto& arg : struct_expr.generics->args) {
            if (arg.is_type()) {
                type_args.push_back(resolve_type(*arg.as_type()));
            }
        }
    }

    // Lower fields
    std::vector<std::pair<std::string, HirExprPtr>> fields;
    for (const auto& [name, value] : struct_expr.fields) {
        fields.emplace_back(name, lower_expr(*value));
    }

    // Lower base (struct update syntax)
    std::optional<HirExprPtr> base;
    if (struct_expr.base) {
        base = lower_expr(**struct_expr.base);
    }

    // Create struct type
    HirType type = std::make_shared<types::Type>();
    type->kind = types::NamedType{struct_name, "", std::move(type_args)};

    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirStructExpr{fresh_id(), struct_name,     {}, std::move(fields), std::move(base),
                               type,       struct_expr.span};
    return expr;
}

// ============================================================================
// Control Flow Expressions
// ============================================================================

auto HirBuilder::lower_if(const parser::IfExpr& if_expr) -> HirExprPtr {
    auto condition = lower_expr(*if_expr.condition);
    auto then_branch = lower_expr(*if_expr.then_branch);

    std::optional<HirExprPtr> else_branch;
    if (if_expr.else_branch) {
        else_branch = lower_expr(**if_expr.else_branch);
    }

    HirType type = then_branch->type();

    return make_hir_if(fresh_id(), std::move(condition), std::move(then_branch),
                       std::move(else_branch), type, if_expr.span);
}

auto HirBuilder::lower_ternary(const parser::TernaryExpr& ternary) -> HirExprPtr {
    // Desugar to if expression
    auto condition = lower_expr(*ternary.condition);
    auto then_branch = lower_expr(*ternary.true_value);
    auto else_branch = lower_expr(*ternary.false_value);

    HirType type = then_branch->type();

    return make_hir_if(fresh_id(), std::move(condition), std::move(then_branch),
                       std::move(else_branch), type, ternary.span);
}

auto HirBuilder::lower_if_let(const parser::IfLetExpr& if_let) -> HirExprPtr {
    // Desugar if let to when expression with two arms:
    // 1. The pattern match arm (then branch)
    // 2. A wildcard arm (else branch)
    auto scrutinee = lower_expr(*if_let.scrutinee);
    HirType scrutinee_type = scrutinee->type();

    std::vector<HirWhenArm> arms;

    // First arm: the pattern match
    HirWhenArm match_arm;
    match_arm.pattern = lower_pattern(*if_let.pattern, scrutinee_type);
    match_arm.body = lower_expr(*if_let.then_branch);
    match_arm.span = if_let.then_branch->span;
    arms.push_back(std::move(match_arm));

    // Second arm: wildcard for else branch (or unit if no else)
    HirWhenArm else_arm;
    else_arm.pattern = make_hir_wildcard_pattern(fresh_id(), if_let.span);
    if (if_let.else_branch) {
        else_arm.body = lower_expr(**if_let.else_branch);
        else_arm.span = (*if_let.else_branch)->span;
    } else {
        // Create unit expression for missing else
        else_arm.body = make_hir_literal(fresh_id(), int64_t(0), types::make_unit(), if_let.span);
        else_arm.span = if_let.span;
    }
    arms.push_back(std::move(else_arm));

    HirType type = arms[0].body->type();

    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirWhenExpr{fresh_id(), std::move(scrutinee), std::move(arms), type, if_let.span};
    return expr;
}

auto HirBuilder::lower_when(const parser::WhenExpr& when) -> HirExprPtr {
    auto scrutinee = lower_expr(*when.scrutinee);

    std::vector<HirWhenArm> arms;
    for (const auto& arm : when.arms) {
        HirWhenArm hir_arm;
        hir_arm.pattern = lower_pattern(*arm.pattern, scrutinee->type());
        if (arm.guard) {
            hir_arm.guard = lower_expr(**arm.guard);
        }
        hir_arm.body = lower_expr(*arm.body);
        hir_arm.span = arm.span;
        arms.push_back(std::move(hir_arm));
    }

    HirType type = arms.empty() ? types::make_unit() : arms[0].body->type();

    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirWhenExpr{fresh_id(), std::move(scrutinee), std::move(arms), type, when.span};
    return expr;
}

auto HirBuilder::lower_loop(const parser::LoopExpr& loop) -> HirExprPtr {
    HirType type = types::make_unit(); // Conditional loops return unit

    // Handle loop variable declaration: loop (var i: I64 < N)
    std::optional<HirLoopVarDecl> hir_loop_var;
    if (loop.loop_var) {
        // Push new scope for the loop variable
        scopes_.emplace_back();
        type_env_.push_scope();

        HirType var_type = resolve_type(*loop.loop_var->type);
        hir_loop_var = HirLoopVarDecl{
            .name = loop.loop_var->name,
            .type = var_type,
            .span = loop.loop_var->span
        };

        // Register variable in scope
        scopes_.back().insert(loop.loop_var->name);
        if (auto scope = type_env_.current_scope()) {
            scope->define(loop.loop_var->name, var_type, true /* mutable */, loop.loop_var->span);
        }
    }

    auto condition = lower_expr(*loop.condition);
    auto body = lower_expr(*loop.body);

    if (loop.loop_var) {
        // Pop the scope
        type_env_.pop_scope();
        scopes_.pop_back();
    }

    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirLoopExpr{fresh_id(), loop.label, std::move(hir_loop_var), std::move(condition), std::move(body), type, loop.span};
    return expr;
}

auto HirBuilder::lower_while(const parser::WhileExpr& while_expr) -> HirExprPtr {
    auto condition = lower_expr(*while_expr.condition);
    auto body = lower_expr(*while_expr.body);

    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirWhileExpr{fresh_id(),      while_expr.label,   std::move(condition),
                              std::move(body), types::make_unit(), while_expr.span};
    return expr;
}

auto HirBuilder::lower_for(const parser::ForExpr& for_expr) -> HirExprPtr {
    HirType iter_type = get_expr_type(*for_expr.iter);
    auto pattern = lower_pattern(*for_expr.pattern, iter_type);
    auto iter = lower_expr(*for_expr.iter);
    auto body = lower_expr(*for_expr.body);

    auto expr = std::make_unique<HirExpr>();
    expr->kind =
        HirForExpr{fresh_id(),      for_expr.label,     std::move(pattern), std::move(iter),
                   std::move(body), types::make_unit(), for_expr.span};
    return expr;
}

auto HirBuilder::lower_block(const parser::BlockExpr& block) -> HirExprPtr {
    scopes_.emplace_back();
    type_env_.push_scope(); // Push type environment scope for variable types

    std::vector<HirStmtPtr> stmts;
    for (const auto& stmt : block.stmts) {
        stmts.push_back(lower_stmt(*stmt));
    }

    std::optional<HirExprPtr> expr;
    if (block.expr) {
        expr = lower_expr(**block.expr);
    }

    HirType type = expr ? (*expr)->type() : types::make_unit();

    type_env_.pop_scope(); // Pop type environment scope
    scopes_.pop_back();

    return make_hir_block(fresh_id(), std::move(stmts), std::move(expr), type, block.span);
}

// ============================================================================
// Control Flow Statements as Expressions
// ============================================================================

auto HirBuilder::lower_return(const parser::ReturnExpr& ret) -> HirExprPtr {
    std::optional<HirExprPtr> value;
    if (ret.value) {
        value = lower_expr(**ret.value);
    }
    return make_hir_return(fresh_id(), std::move(value), ret.span);
}

auto HirBuilder::lower_break(const parser::BreakExpr& brk) -> HirExprPtr {
    std::optional<HirExprPtr> value;
    if (brk.value) {
        value = lower_expr(**brk.value);
    }
    return make_hir_break(fresh_id(), brk.label, std::move(value), brk.span);
}

auto HirBuilder::lower_continue(const parser::ContinueExpr& cont) -> HirExprPtr {
    return make_hir_continue(fresh_id(), cont.label, cont.span);
}

// ============================================================================
// Closure Expressions
// ============================================================================

auto HirBuilder::lower_closure(const parser::ClosureExpr& closure) -> HirExprPtr {
    std::vector<std::pair<std::string, HirType>> params;

    // Lower parameters
    for (const auto& [pattern, type_opt] : closure.params) {
        std::string name = "_";
        if (pattern->is<parser::IdentPattern>()) {
            name = pattern->as<parser::IdentPattern>().name;
        }

        HirType type = types::make_unit();
        if (type_opt) {
            type = resolve_type(**type_opt);
        }

        params.emplace_back(name, type);
    }

    // Collect captures before lowering body
    auto captures = collect_captures(closure);

    // Enter closure scope
    scopes_.emplace_back();
    for (const auto& [name, _] : params) {
        scopes_.back().insert(name);
    }

    auto body = lower_expr(*closure.body);

    scopes_.pop_back();

    // Build closure type
    std::vector<types::TypePtr> param_types;
    for (const auto& [_, type] : params) {
        param_types.push_back(type);
    }
    HirType return_type = body->type();

    std::vector<types::CapturedVar> captured_vars;
    for (const auto& cap : captures) {
        captured_vars.push_back({cap.name, cap.type, cap.is_mut});
    }

    HirType type =
        types::make_closure(std::move(param_types), return_type, std::move(captured_vars));

    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirClosureExpr{fresh_id(), std::move(params), std::move(body), std::move(captures),
                                type,       closure.span};
    return expr;
}

// ============================================================================
// Other Expressions
// ============================================================================

auto HirBuilder::lower_range(const parser::RangeExpr& range) -> HirExprPtr {
    // Desugar range to Range struct construction
    // Range types: Range (half-open), RangeInclusive (closed)
    std::optional<HirExprPtr> start;
    std::optional<HirExprPtr> end;

    if (range.start) {
        start = lower_expr(**range.start);
    }
    if (range.end) {
        end = lower_expr(**range.end);
    }

    // Determine element type from bounds
    HirType elem_type = types::make_i64(); // Default to I64
    if (start) {
        elem_type = (*start)->type();
    } else if (end) {
        elem_type = (*end)->type();
    }

    // Determine range type name based on inclusivity and bounds
    std::string range_type;
    if (!start && !end) {
        range_type = "RangeFull";
    } else if (!start) {
        range_type = range.inclusive ? "RangeToInclusive" : "RangeTo";
    } else if (!end) {
        range_type = "RangeFrom";
    } else {
        range_type = range.inclusive ? "RangeInclusive" : "Range";
    }

    // Create Range type with element type parameter
    HirType type = std::make_shared<types::Type>();
    type->kind = types::NamedType{range_type, "", {elem_type}};

    // Build struct fields
    std::vector<std::pair<std::string, HirExprPtr>> fields;
    if (start) {
        fields.emplace_back("start", std::move(*start));
    }
    if (end) {
        fields.emplace_back("end", std::move(*end));
    }

    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirStructExpr{fresh_id(),   range_type, {},        std::move(fields),
                               std::nullopt, type,       range.span};
    return expr;
}

auto HirBuilder::lower_cast(const parser::CastExpr& cast) -> HirExprPtr {
    auto expr = lower_expr(*cast.expr);
    HirType target_type = resolve_type(*cast.target);

    auto result = std::make_unique<HirExpr>();
    result->kind = HirCastExpr{fresh_id(), std::move(expr), target_type, target_type, cast.span};
    return result;
}

auto HirBuilder::lower_try(const parser::TryExpr& try_expr) -> HirExprPtr {
    auto expr = lower_expr(*try_expr.expr);
    HirType original_type = expr->type();

    // Extract inner type from Maybe[T] or Outcome[T,E]
    HirType type = original_type;
    if (original_type && original_type->is<types::NamedType>()) {
        const auto& named = original_type->as<types::NamedType>();
        if ((named.name == "Maybe" || named.name == "Outcome") && !named.type_args.empty()) {
            // Extract the success type (first type argument)
            type = named.type_args[0];
        }
    }

    auto result = std::make_unique<HirExpr>();
    result->kind = HirTryExpr{fresh_id(), std::move(expr), type, try_expr.span};
    return result;
}

auto HirBuilder::lower_await(const parser::AwaitExpr& await) -> HirExprPtr {
    auto expr = lower_expr(*await.expr);
    HirType original_type = expr->type();

    // Extract inner type from Future[T] or Poll[T]
    HirType type = original_type;
    if (original_type && original_type->is<types::NamedType>()) {
        const auto& named = original_type->as<types::NamedType>();
        if ((named.name == "Future" || named.name == "Poll") && !named.type_args.empty()) {
            // Extract the output type (first type argument)
            type = named.type_args[0];
        }
    }

    auto result = std::make_unique<HirExpr>();
    result->kind = HirAwaitExpr{fresh_id(), std::move(expr), type, await.span};
    return result;
}

auto HirBuilder::lower_path(const parser::PathExpr& path) -> HirExprPtr {
    // Handle path expressions (enum variants, static methods, etc.)
    std::string full_path;
    for (size_t i = 0; i < path.path.segments.size(); ++i) {
        if (i > 0) {
            full_path += "::";
        }
        full_path += path.path.segments[i];
    }

    // Check if this is an enum variant
    if (path.path.segments.size() >= 2) {
        std::string enum_name = path.path.segments[path.path.segments.size() - 2];
        std::string variant_name = path.path.segments.back();

        int variant_index = get_variant_index(enum_name, variant_name);
        if (variant_index >= 0) {
            // Lower type arguments
            std::vector<HirType> type_args;
            if (path.generics) {
                for (const auto& arg : path.generics->args) {
                    if (arg.is_type()) {
                        type_args.push_back(resolve_type(*arg.as_type()));
                    }
                }
            }

            // Create enum type
            HirType type = std::make_shared<types::Type>();
            type->kind = types::NamedType{enum_name, "", std::move(type_args)};

            auto expr = std::make_unique<HirExpr>();
            expr->kind = HirEnumExpr{fresh_id(), enum_name, variant_name, variant_index,
                                     {},         {},        type,         path.span};
            return expr;
        }
    }

    // Otherwise treat as a variable/function reference
    // Look up variable type from type environment
    HirType type = types::make_unit();
    auto scope = type_env_.current_scope();
    if (scope) {
        // Try the last segment as variable name
        std::string var_name = path.path.segments.empty() ? full_path : path.path.segments.back();
        if (auto var = scope->lookup(var_name)) {
            type = type_env_.resolve(var->type);
        }
    }
    // Also check for function type
    if (type == types::make_unit()) {
        if (auto sig = type_env_.lookup_func(full_path)) {
            type = type_env_.resolve(sig->return_type);
        }
    }
    return make_hir_var(fresh_id(), full_path, type, path.span);
}

auto HirBuilder::lower_lowlevel(const parser::LowlevelExpr& lowlevel) -> HirExprPtr {
    scopes_.emplace_back();

    std::vector<HirStmtPtr> stmts;
    for (const auto& stmt : lowlevel.stmts) {
        stmts.push_back(lower_stmt(*stmt));
    }

    std::optional<HirExprPtr> expr;
    if (lowlevel.expr) {
        expr = lower_expr(**lowlevel.expr);
    }

    HirType type = expr ? (*expr)->type() : types::make_unit();

    scopes_.pop_back();

    auto result = std::make_unique<HirExpr>();
    result->kind =
        HirLowlevelExpr{fresh_id(), std::move(stmts), std::move(expr), type, lowlevel.span};
    return result;
}

} // namespace tml::hir
