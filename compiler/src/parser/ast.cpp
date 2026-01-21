//! # AST Factory Functions
//!
//! This file provides factory functions for creating AST nodes.
//!
//! ## Factory Functions
//!
//! | Function              | Creates                    |
//! |-----------------------|----------------------------|
//! | `make_literal_expr`   | Literal from token         |
//! | `make_ident_expr`     | Identifier expression      |
//! | `make_binary_expr`    | Binary operation           |
//! | `make_unary_expr`     | Unary operation            |
//! | `make_call_expr`      | Function call              |
//! | `make_block_expr`     | Block with statements      |
//! | `make_named_type`     | Named type (e.g., `I32`)   |
//! | `make_ref_type`       | Reference type             |
//! | `make_ident_pattern`  | Identifier pattern binding |
//! | `make_wildcard_pattern` | Wildcard `_` pattern     |
//!
//! ## Usage
//!
//! These functions wrap node construction in `Box<T>` for proper ownership.

#include "parser/ast.hpp"

namespace tml::parser {

auto make_literal_expr(lexer::Token token) -> ExprPtr {
    auto span = token.span;
    return make_box<Expr>(
        Expr{.kind = LiteralExpr{.token = std::move(token), .span = span}, .span = span});
}

auto make_ident_expr(std::string name, SourceSpan span) -> ExprPtr {
    return make_box<Expr>(
        Expr{.kind = IdentExpr{.name = std::move(name), .span = span}, .span = span});
}

auto make_binary_expr(BinaryOp op, ExprPtr left, ExprPtr right, SourceSpan span) -> ExprPtr {
    return make_box<Expr>(Expr{
        .kind =
            BinaryExpr{.op = op, .left = std::move(left), .right = std::move(right), .span = span},
        .span = span});
}

auto make_unary_expr(UnaryOp op, ExprPtr operand, SourceSpan span) -> ExprPtr {
    return make_box<Expr>(Expr{
        .kind = UnaryExpr{.op = op, .operand = std::move(operand), .span = span}, .span = span});
}

auto make_call_expr(ExprPtr callee, std::vector<ExprPtr> args, SourceSpan span) -> ExprPtr {
    return make_box<Expr>(
        Expr{.kind = CallExpr{.callee = std::move(callee), .args = std::move(args), .span = span},
             .span = span});
}

auto make_block_expr(std::vector<StmtPtr> stmts, std::optional<ExprPtr> expr, SourceSpan span)
    -> ExprPtr {
    return make_box<Expr>(
        Expr{.kind = BlockExpr{.stmts = std::move(stmts), .expr = std::move(expr), .span = span},
             .span = span});
}

auto make_named_type(std::string name, SourceSpan span) -> TypePtr {
    TypePath path;
    path.segments.push_back(std::move(name));
    path.span = span;

    return make_box<Type>(
        Type{.kind = NamedType{.path = std::move(path), .generics = std::nullopt, .span = span},
             .span = span});
}

auto make_ref_type(bool is_mut, TypePtr inner, SourceSpan span, std::optional<std::string> lifetime)
    -> TypePtr {
    return make_box<Type>(Type{.kind = RefType{.is_mut = is_mut,
                                               .inner = std::move(inner),
                                               .lifetime = std::move(lifetime),
                                               .span = span},
                               .span = span});
}

auto make_ident_pattern(std::string name, bool is_mut, SourceSpan span) -> PatternPtr {
    return make_box<Pattern>(Pattern{.kind = IdentPattern{.name = std::move(name),
                                                          .is_mut = is_mut,
                                                          .type_annotation = std::nullopt,
                                                          .span = span},
                                     .span = span});
}

auto make_wildcard_pattern(SourceSpan span) -> PatternPtr {
    return make_box<Pattern>(Pattern{.kind = WildcardPattern{.span = span}, .span = span});
}

} // namespace tml::parser
