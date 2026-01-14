//! # Parser - Expressions
//!
//! This file implements expression parsing using a Pratt parser.
//!
//! ## Pratt Parser Algorithm
//!
//! 1. Parse prefix expression (unary, literal, primary)
//! 2. While next token has higher precedence than minimum:
//!    a. Handle postfix operators (call, index, field)
//!    b. Handle infix operators (binary operations)
//! 3. Return combined expression tree
//!
//! ## Expression Categories
//!
//! | Category      | Examples                               |
//! |---------------|----------------------------------------|
//! | Literals      | `42`, `3.14`, `"hello"`, `true`        |
//! | Identifiers   | `x`, `Point::new`                      |
//! | Unary         | `-x`, `not y`, `ref z`, `*ptr`         |
//! | Binary        | `a + b`, `x and y`, `n == 0`           |
//! | Postfix       | `f()`, `arr[i]`, `obj.field`           |
//! | Control       | `if`, `when`, `loop`, `while`, `for`   |
//! | Special       | `return`, `break`, `continue`, `await` |
//!
//! ## Multi-line Expressions
//!
//! Infix operators can continue across newlines:
//! ```tml
//! let result: Bool = a
//!     or b
//!     or c
//! ```

#include "parser/parser.hpp"

namespace tml::parser {

// ============================================================================
// Expression Parsing (Pratt Parser)
// ============================================================================

auto Parser::parse_expr() -> Result<ExprPtr, ParseError> {
    return parse_expr_with_precedence(precedence::NONE);
}

auto Parser::parse_expr_with_precedence(int min_precedence) -> Result<ExprPtr, ParseError> {
    auto left = parse_prefix_expr();
    if (is_err(left))
        return left;

    while (true) {
        // Skip newlines and check if next token is an infix operator
        // This allows multi-line expressions like:
        //   a
        //       or b
        size_t saved_pos = pos_;
        skip_newlines();

        auto prec = get_precedence(peek().kind);
        auto next_kind = peek().kind;

        // Check if this is a valid infix operator (not a postfix or special case)
        bool is_infix =
            token_to_binary_op(next_kind).has_value() || next_kind == lexer::TokenKind::KwAs ||
            next_kind == lexer::TokenKind::KwIs || next_kind == lexer::TokenKind::Question ||
            next_kind == lexer::TokenKind::KwTo || next_kind == lexer::TokenKind::KwThrough ||
            next_kind == lexer::TokenKind::DotDot;

        // Postfix operators (function calls, indexing, field access) should not
        // continue across newlines to avoid ambiguity
        bool is_postfix =
            next_kind == lexer::TokenKind::LParen || next_kind == lexer::TokenKind::LBracket ||
            next_kind == lexer::TokenKind::Dot || next_kind == lexer::TokenKind::Bang ||
            next_kind == lexer::TokenKind::PlusPlus || next_kind == lexer::TokenKind::MinusMinus;

        // If we skipped newlines and hit a postfix operator, don't continue
        if (saved_pos != pos_ && is_postfix) {
            pos_ = saved_pos;
            break;
        }

        // If we skipped newlines and it's not an infix operator, don't continue
        if (saved_pos != pos_ && !is_infix) {
            pos_ = saved_pos;
            break;
        }

        if (prec <= min_precedence) {
            // Not a continuation - restore position and break
            pos_ = saved_pos;
            break;
        }

        // Handle postfix operators first (only if we didn't skip newlines)
        if (is_postfix) {
            left = parse_postfix_expr(std::move(unwrap(left)));
            if (is_err(left))
                return left;
            continue;
        }

        // Handle range expressions: x to y, x through y, x..y
        if (check(lexer::TokenKind::KwTo) || check(lexer::TokenKind::KwThrough) ||
            check(lexer::TokenKind::DotDot)) {
            bool inclusive = check(lexer::TokenKind::KwThrough);
            advance(); // consume 'to', 'through', or '..'

            auto end_expr = parse_expr_with_precedence(prec);
            if (is_err(end_expr))
                return end_expr;

            auto span = SourceSpan::merge(unwrap(left)->span, unwrap(end_expr)->span);
            left = make_box<Expr>(Expr{.kind = RangeExpr{.start = std::move(unwrap(left)),
                                                         .end = std::move(unwrap(end_expr)),
                                                         .inclusive = inclusive,
                                                         .span = span},
                                       .span = span});
            continue;
        }

        // Handle ternary operator: condition ? true_value : false_value
        if (check(lexer::TokenKind::Question)) {
            advance(); // consume '?'

            // Parse true branch (right-associative, so use prec - 1)
            auto true_val = parse_expr_with_precedence(prec - 1);
            if (is_err(true_val))
                return true_val;

            // Expect ':'
            if (!check(lexer::TokenKind::Colon)) {
                return ParseError{"Expected ':' in ternary expression", peek().span, {}};
            }
            advance(); // consume ':'

            // Parse false branch (right-associative)
            auto false_val = parse_expr_with_precedence(prec - 1);
            if (is_err(false_val))
                return false_val;

            auto span = SourceSpan::merge(unwrap(left)->span, unwrap(false_val)->span);
            left =
                make_box<Expr>(Expr{.kind = TernaryExpr{.condition = std::move(unwrap(left)),
                                                        .true_value = std::move(unwrap(true_val)),
                                                        .false_value = std::move(unwrap(false_val)),
                                                        .span = span},
                                    .span = span});
            continue;
        }

        // Handle type cast: expr as Type
        if (check(lexer::TokenKind::KwAs)) {
            advance(); // consume 'as'

            auto target_type = parse_type();
            if (is_err(target_type))
                return unwrap_err(target_type);

            auto span = SourceSpan::merge(unwrap(left)->span, unwrap(target_type)->span);
            left = make_box<Expr>(Expr{.kind = CastExpr{.expr = std::move(unwrap(left)),
                                                        .target = std::move(unwrap(target_type)),
                                                        .span = span},
                                       .span = span});
            continue;
        }

        // Handle type check: expr is Type
        if (check(lexer::TokenKind::KwIs)) {
            advance(); // consume 'is'

            auto target_type = parse_type();
            if (is_err(target_type))
                return unwrap_err(target_type);

            auto span = SourceSpan::merge(unwrap(left)->span, unwrap(target_type)->span);
            left = make_box<Expr>(Expr{.kind = IsExpr{.expr = std::move(unwrap(left)),
                                                      .target = std::move(unwrap(target_type)),
                                                      .span = span},
                                       .span = span});
            continue;
        }

        // Infix operators
        auto op = token_to_binary_op(peek().kind);
        if (!op) {
            break;
        }

        int actual_prec = is_right_associative(peek().kind) ? prec - 1 : prec;
        left = parse_infix_expr(std::move(unwrap(left)), actual_prec);
        if (is_err(left))
            return left;
    }

    return left;
}

auto Parser::parse_prefix_expr() -> Result<ExprPtr, ParseError> {
    // Allow expressions to continue on next line after binary operators
    // This enables patterns like:
    //   return a or
    //          b
    skip_newlines();

    // Prefix await: 'await expr'
    if (match(lexer::TokenKind::KwAwait)) {
        auto start_span = previous().span;
        auto operand = parse_prefix_expr();
        if (is_err(operand))
            return operand;

        auto span = SourceSpan::merge(start_span, unwrap(operand)->span);
        return make_box<Expr>(Expr{
            .kind = AwaitExpr{.expr = std::move(unwrap(operand)), .span = span}, .span = span});
    }

    // TML syntax: 'mut ref x' for mutable reference
    if (check(lexer::TokenKind::KwMut) && check_next(lexer::TokenKind::KwRef)) {
        auto start_span = peek().span;
        advance(); // consume 'mut'
        advance(); // consume 'ref'

        auto operand = parse_prefix_expr();
        if (is_err(operand))
            return operand;

        auto span = SourceSpan::merge(start_span, unwrap(operand)->span);
        return make_unary_expr(UnaryOp::RefMut, std::move(unwrap(operand)), span);
    }

    auto op = token_to_unary_op(peek().kind);
    if (op) {
        auto start_span = peek().span;
        advance();

        // Special case: &mut (also support for backwards compatibility)
        if (*op == UnaryOp::Ref && match(lexer::TokenKind::KwMut)) {
            op = UnaryOp::RefMut;
        }

        auto operand = parse_prefix_expr();
        if (is_err(operand))
            return operand;

        auto span = SourceSpan::merge(start_span, unwrap(operand)->span);
        return make_unary_expr(*op, std::move(unwrap(operand)), span);
    }

    return parse_primary_with_postfix();
}

// Parse a primary expression followed by all postfix operators (calls, field access, etc.)
// This is used for unary operands to ensure correct precedence: not x.y means not (x.y)
auto Parser::parse_primary_with_postfix() -> Result<ExprPtr, ParseError> {
    auto result = parse_primary_expr();
    if (is_err(result))
        return result;

    // Loop to handle all postfix operators
    while (check(lexer::TokenKind::LParen) || check(lexer::TokenKind::LBracket) ||
           check(lexer::TokenKind::Dot) || check(lexer::TokenKind::Bang) ||
           check(lexer::TokenKind::PlusPlus) || check(lexer::TokenKind::MinusMinus)) {
        result = parse_postfix_expr(std::move(unwrap(result)));
        if (is_err(result))
            return result;
    }

    return result;
}

auto Parser::parse_postfix_expr(ExprPtr left) -> Result<ExprPtr, ParseError> {
    auto start_span = left->span;

    // Function call
    if (match(lexer::TokenKind::LParen)) {
        auto args = parse_call_args();
        if (is_err(args))
            return unwrap_err(args);

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after arguments");
        if (is_err(rparen))
            return unwrap_err(rparen);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_call_expr(std::move(left), std::move(unwrap(args)), span);
    }

    // Index access
    if (match(lexer::TokenKind::LBracket)) {
        auto index = parse_expr();
        if (is_err(index))
            return index;

        auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']' after index");
        if (is_err(rbracket))
            return unwrap_err(rbracket);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{.kind = IndexExpr{.object = std::move(left),
                                                     .index = std::move(unwrap(index)),
                                                     .span = span},
                                   .span = span});
    }

    // Field/method access
    if (match(lexer::TokenKind::Dot)) {
        // Check for await
        if (match(lexer::TokenKind::KwAwait)) {
            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Expr>(
                Expr{.kind = AwaitExpr{.expr = std::move(left), .span = span}, .span = span});
        }

        // Check for tuple index access: tuple.0, tuple.1, etc.
        std::string name;
        if (check(lexer::TokenKind::IntLiteral)) {
            // Tuple index access
            auto index_token = advance();
            name = std::string(index_token.lexeme);
            // Tuple index is just a field access with numeric name
            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Expr>(
                Expr{.kind = FieldExpr{.object = std::move(left), .field = name, .span = span},
                     .span = span});
        }

        auto name_result = expect(lexer::TokenKind::Identifier, "Expected field or method name");
        if (is_err(name_result))
            return unwrap_err(name_result);
        name = std::string(unwrap(name_result).lexeme);

        // Check if it's a method call first (could have type args)
        // Generic type arguments: .method[T, U]() - note: must be followed by ()
        // Index expressions: .field[0] - this is NOT generic types
        std::vector<TypePtr> type_args;

        // Only parse [T, U] as type args if followed by '(' for method call
        // Otherwise, [0] is an index expression to be handled by next postfix iteration
        if (check(lexer::TokenKind::LBracket)) {
            // Look ahead to see if this is a method call with type args
            // We need to tentatively parse and check if it's followed by '('
            size_t saved_pos = pos_;
            advance(); // consume '['

            // Try to parse as type arguments
            bool looks_like_type_args = true;
            int bracket_depth = 1;

            // Scan forward to find matching ']' and check what follows
            while (bracket_depth > 0 && !is_at_end()) {
                if (check(lexer::TokenKind::LBracket)) {
                    bracket_depth++;
                } else if (check(lexer::TokenKind::RBracket)) {
                    bracket_depth--;
                }
                if (bracket_depth > 0) {
                    advance();
                }
            }

            if (bracket_depth == 0) {
                advance(); // consume final ']'
                // Check if followed by '('
                looks_like_type_args = check(lexer::TokenKind::LParen);
            } else {
                looks_like_type_args = false;
            }

            // Restore position and actually parse if it's type args
            pos_ = saved_pos;

            if (looks_like_type_args) {
                advance(); // consume '['
                if (!check(lexer::TokenKind::RBracket)) {
                    do {
                        skip_newlines();
                        auto type_result = parse_type();
                        if (is_err(type_result))
                            return unwrap_err(type_result);
                        type_args.push_back(std::move(unwrap(type_result)));
                        skip_newlines();
                    } while (match(lexer::TokenKind::Comma));
                }
                auto rbracket =
                    expect(lexer::TokenKind::RBracket, "Expected ']' after type arguments");
                if (is_err(rbracket))
                    return unwrap_err(rbracket);
            }
            // If not type args, leave '[' unconsumed for index parsing on next iteration
        }

        // Check if it's a method call (with or without type args)
        if (check(lexer::TokenKind::LParen)) {
            advance();
            auto args = parse_call_args();
            if (is_err(args))
                return unwrap_err(args);

            auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after arguments");
            if (is_err(rparen))
                return unwrap_err(rparen);

            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Expr>(Expr{.kind = MethodCallExpr{.receiver = std::move(left),
                                                              .method = std::move(name),
                                                              .type_args = std::move(type_args),
                                                              .args = std::move(unwrap(args)),
                                                              .span = span},
                                       .span = span});
        }

        // If we had type args but no parens, that's an error
        if (!type_args.empty()) {
            return ParseError{.message = "Expected '(' after method type arguments",
                              .span = peek().span,
                              .notes = {}};
        }

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{
            .kind = FieldExpr{.object = std::move(left), .field = std::move(name), .span = span},
            .span = span});
    }

    // Try operator (?)
    if (match(lexer::TokenKind::Bang)) {
        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(
            Expr{.kind = TryExpr{.expr = std::move(left), .span = span}, .span = span});
    }

    // Postfix increment (++)
    if (match(lexer::TokenKind::PlusPlus)) {
        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(
            Expr{.kind = UnaryExpr{.op = UnaryOp::Inc, .operand = std::move(left), .span = span},
                 .span = span});
    }

    // Postfix decrement (--)
    if (match(lexer::TokenKind::MinusMinus)) {
        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(
            Expr{.kind = UnaryExpr{.op = UnaryOp::Dec, .operand = std::move(left), .span = span},
                 .span = span});
    }

    return left;
}

auto Parser::parse_infix_expr(ExprPtr left, int precedence) -> Result<ExprPtr, ParseError> {
    auto op_token = advance();
    auto op = token_to_binary_op(op_token.kind);
    if (!op) {
        return ParseError{
            .message = "Expected binary operator", .span = op_token.span, .notes = {}};
    }

    auto right = parse_expr_with_precedence(precedence);
    if (is_err(right))
        return right;

    auto span = SourceSpan::merge(left->span, unwrap(right)->span);
    return make_binary_expr(*op, std::move(left), std::move(unwrap(right)), span);
}

auto Parser::parse_primary_expr() -> Result<ExprPtr, ParseError> {
    // Literals
    if (check(lexer::TokenKind::IntLiteral) || check(lexer::TokenKind::FloatLiteral) ||
        check(lexer::TokenKind::StringLiteral) || check(lexer::TokenKind::CharLiteral) ||
        check(lexer::TokenKind::BoolLiteral) || check(lexer::TokenKind::NullLiteral)) {
        return parse_literal_expr();
    }

    // Interpolated string: "Hello {name}!"
    if (check(lexer::TokenKind::InterpStringStart)) {
        return parse_interp_string_expr();
    }

    // Identifier or path
    if (check(lexer::TokenKind::Identifier)) {
        return parse_ident_or_path_expr();
    }

    // 'this' expression (self reference in methods)
    if (check(lexer::TokenKind::KwThis)) {
        auto span = peek().span;
        advance();
        return make_ident_expr("this", span);
    }

    // 'base' expression (parent class access in methods)
    if (check(lexer::TokenKind::KwBase)) {
        return parse_base_expr();
    }

    // Parenthesized expression or tuple
    if (check(lexer::TokenKind::LParen)) {
        return parse_paren_or_tuple_expr();
    }

    // Array
    if (check(lexer::TokenKind::LBracket)) {
        return parse_array_expr();
    }

    // Block
    if (check(lexer::TokenKind::LBrace)) {
        return parse_block_expr();
    }

    // If
    if (check(lexer::TokenKind::KwIf)) {
        return parse_if_expr();
    }

    // When (match)
    if (check(lexer::TokenKind::KwWhen)) {
        return parse_when_expr();
    }

    // Loop
    if (check(lexer::TokenKind::KwLoop)) {
        return parse_loop_expr();
    }

    // While
    if (check(lexer::TokenKind::KwWhile)) {
        return parse_while_expr();
    }

    // For
    if (check(lexer::TokenKind::KwFor)) {
        return parse_for_expr();
    }

    // Return
    if (check(lexer::TokenKind::KwReturn)) {
        return parse_return_expr();
    }

    // Throw
    if (check(lexer::TokenKind::KwThrow)) {
        return parse_throw_expr();
    }

    // Break
    if (check(lexer::TokenKind::KwBreak)) {
        return parse_break_expr();
    }

    // Continue
    if (check(lexer::TokenKind::KwContinue)) {
        return parse_continue_expr();
    }

    // Closure: do(x) expr
    if (check(lexer::TokenKind::KwDo)) {
        return parse_closure_expr();
    }

    // Lowlevel block: lowlevel { ... }
    if (check(lexer::TokenKind::KwLowlevel)) {
        return parse_lowlevel_expr();
    }

    return ParseError{.message = "Expected expression", .span = peek().span, .notes = {}};
}

auto Parser::parse_literal_expr() -> Result<ExprPtr, ParseError> {
    auto token = advance();
    return make_literal_expr(std::move(token));
}

auto Parser::parse_ident_or_path_expr() -> Result<ExprPtr, ParseError> {
    auto path = parse_type_path();
    if (is_err(path))
        return unwrap_err(path);

    // Parse optional generic arguments: List[I32], HashMap[K, V]
    auto generics = parse_generic_args();
    if (is_err(generics))
        return unwrap_err(generics);

    // Check if it's a struct literal
    // Need to distinguish from block expressions: Point { x: 1 } vs { let x = 1 }
    if (check(lexer::TokenKind::LBrace)) {
        // Look ahead to see if this is a struct literal
        // Struct literal starts with: { ident: or { ident, or { .. or { }
        // Block starts with: { let, { for, { if, { expr, etc.
        auto saved_pos = pos_;
        advance(); // consume '{'
        skip_newlines();

        bool is_struct = false;
        if (check(lexer::TokenKind::RBrace)) {
            // Empty braces - could be empty struct
            is_struct = true;
        } else if (check(lexer::TokenKind::DotDot)) {
            // { ..base } is struct update syntax
            is_struct = true;
        } else if (check(lexer::TokenKind::Identifier)) {
            auto saved_inner = pos_;
            advance(); // consume identifier
            // If followed by : or }, it's definitely a struct literal
            // If followed by ,, we need to look further to distinguish from when patterns
            // Struct: { a, b } or { a, b: value }
            // When pattern: { a, b => body } (multiple patterns with comma)
            if (check(lexer::TokenKind::Colon) || check(lexer::TokenKind::RBrace)) {
                is_struct = true;
            } else if (check(lexer::TokenKind::Comma)) {
                // Look ahead past comma-separated identifiers to check for =>
                // If we see =>, it's a when pattern, not a struct
                is_struct = true; // assume struct by default
                while (match(lexer::TokenKind::Comma)) {
                    skip_newlines();
                    if (check(lexer::TokenKind::FatArrow)) {
                        // Found =>, this is a when pattern, not a struct
                        is_struct = false;
                        break;
                    }
                    if (!check(lexer::TokenKind::Identifier)) {
                        break;
                    }
                    advance(); // consume identifier
                }
                // If we see => after identifiers, it's a when pattern
                if (check(lexer::TokenKind::FatArrow)) {
                    is_struct = false;
                }
            }
            pos_ = saved_inner; // restore to before identifier
        }

        pos_ = saved_pos; // restore to before '{'

        if (is_struct) {
            return parse_struct_expr(std::move(unwrap(path)), std::move(unwrap(generics)));
        }
        // Otherwise, it's not a struct literal - just return the identifier/path
    }

    auto span = unwrap(path).span;
    auto has_generics = unwrap(generics).has_value();

    // If we have generics, extend the span
    if (has_generics) {
        span = SourceSpan::merge(span, unwrap(generics)->span);
    }

    // Single identifier without generics -> IdentExpr
    if (unwrap(path).segments.size() == 1 && !has_generics) {
        return make_ident_expr(std::move(unwrap(path).segments[0]), span);
    }

    // Path with or without generics -> PathExpr
    return make_box<Expr>(Expr{.kind = PathExpr{.path = std::move(unwrap(path)),
                                                .generics = std::move(unwrap(generics)),
                                                .span = span},
                               .span = span});
}

auto Parser::parse_paren_or_tuple_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume '('

    skip_newlines();

    // Empty tuple
    if (check(lexer::TokenKind::RParen)) {
        advance();
        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{.kind = TupleExpr{.elements = {}, .span = span}, .span = span});
    }

    auto first = parse_expr();
    if (is_err(first))
        return first;

    skip_newlines();

    // Check if it's a tuple or just parenthesized
    if (match(lexer::TokenKind::Comma)) {
        // It's a tuple
        std::vector<ExprPtr> elements;
        elements.push_back(std::move(unwrap(first)));

        skip_newlines();
        while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
            auto elem = parse_expr();
            if (is_err(elem))
                return elem;
            elements.push_back(std::move(unwrap(elem)));

            skip_newlines();
            if (!check(lexer::TokenKind::RParen)) {
                auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between tuple elements");
                if (is_err(comma))
                    return unwrap_err(comma);
                skip_newlines();
            }
        }

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after tuple");
        if (is_err(rparen))
            return unwrap_err(rparen);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(
            Expr{.kind = TupleExpr{.elements = std::move(elements), .span = span}, .span = span});
    }

    // Just a parenthesized expression
    auto rparen = expect(lexer::TokenKind::RParen, "Expected ')'");
    if (is_err(rparen))
        return unwrap_err(rparen);

    return first;
}

auto Parser::parse_array_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume '['

    skip_newlines();

    // Empty array
    if (check(lexer::TokenKind::RBracket)) {
        advance();
        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(
            Expr{.kind = ArrayExpr{.kind = std::vector<ExprPtr>{}, .span = span}, .span = span});
    }

    auto first = parse_expr();
    if (is_err(first))
        return first;

    skip_newlines();

    // Check for repeat syntax: [expr; count]
    if (match(lexer::TokenKind::Semi)) {
        auto count = parse_expr();
        if (is_err(count))
            return count;

        auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']'");
        if (is_err(rbracket))
            return unwrap_err(rbracket);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{
            .kind = ArrayExpr{.kind = std::pair{std::move(unwrap(first)), std::move(unwrap(count))},
                              .span = span},
            .span = span});
    }

    // Regular array literal
    std::vector<ExprPtr> elements;
    elements.push_back(std::move(unwrap(first)));

    while (match(lexer::TokenKind::Comma)) {
        skip_newlines();
        if (check(lexer::TokenKind::RBracket))
            break;

        auto elem = parse_expr();
        if (is_err(elem))
            return elem;
        elements.push_back(std::move(unwrap(elem)));
        skip_newlines();
    }

    auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']'");
    if (is_err(rbracket))
        return unwrap_err(rbracket);

    auto span = SourceSpan::merge(start_span, previous().span);
    return make_box<Expr>(
        Expr{.kind = ArrayExpr{.kind = std::move(elements), .span = span}, .span = span});
}

auto Parser::parse_block_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{'");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    std::vector<StmtPtr> stmts;
    std::optional<ExprPtr> expr;

    skip_newlines();

    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto stmt = parse_stmt();
        if (is_err(stmt))
            return unwrap_err(stmt);

        skip_newlines();

        // Check if this is the trailing expression (no semicolon)
        if (check(lexer::TokenKind::RBrace)) {
            // The statement is an expression statement, and it becomes the trailing expr
            if (unwrap(stmt)->is<ExprStmt>()) {
                expr = std::move(unwrap(stmt)->as<ExprStmt>().expr);
            } else {
                stmts.push_back(std::move(unwrap(stmt)));
            }
        } else {
            stmts.push_back(std::move(unwrap(stmt)));
            // Consume optional semicolon or require newline
            match(lexer::TokenKind::Semi);
            skip_newlines();
        }
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}'");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto span = SourceSpan::merge(start_span, previous().span);
    return make_block_expr(std::move(stmts), std::move(expr), span);
}

auto Parser::parse_if_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'if'

    // Check for if-let syntax
    if (match(lexer::TokenKind::KwLet)) {
        return parse_if_let_expr(start_span);
    }

    auto cond = parse_expr();
    if (is_err(cond))
        return cond;

    // TML supports both syntaxes:
    // - if cond then expr else expr  (with 'then' keyword)
    // - if cond { block } else { block }  (with braces)
    ExprPtr then_branch;
    bool uses_then = match(lexer::TokenKind::KwThen);

    if (uses_then) {
        // With 'then': parse expression but stop at 'else'
        // Use ASSIGN precedence to avoid consuming binary operators at top level
        auto expr = parse_expr_with_precedence(precedence::ASSIGN + 1);
        if (is_err(expr))
            return expr;
        then_branch = std::move(unwrap(expr));
    } else {
        // Without 'then': expect block
        auto block = parse_block_expr();
        if (is_err(block))
            return block;
        then_branch = std::move(unwrap(block));
    }

    std::optional<ExprPtr> else_branch;
    skip_newlines();
    if (match(lexer::TokenKind::KwElse)) {
        skip_newlines();
        if (check(lexer::TokenKind::KwIf)) {
            auto else_if = parse_if_expr();
            if (is_err(else_if))
                return else_if;
            else_branch = std::move(unwrap(else_if));
        } else if (uses_then) {
            // With 'then': else branch is also an expression
            auto expr = parse_expr_with_precedence(precedence::ASSIGN + 1);
            if (is_err(expr))
                return expr;
            else_branch = std::move(unwrap(expr));
        } else {
            // Without 'then': else branch is a block
            auto else_block = parse_block_expr();
            if (is_err(else_block))
                return else_block;
            else_branch = std::move(unwrap(else_block));
        }
    }

    auto end_span = previous().span;
    return make_box<Expr>(Expr{.kind = IfExpr{.condition = std::move(unwrap(cond)),
                                              .then_branch = std::move(then_branch),
                                              .else_branch = std::move(else_branch),
                                              .span = SourceSpan::merge(start_span, end_span)},
                               .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_if_let_expr(SourceSpan start_span) -> Result<ExprPtr, ParseError> {
    // Parse pattern
    auto pattern = parse_pattern();
    if (is_err(pattern))
        return unwrap_err(pattern);

    // Expect '='
    auto eq = expect(lexer::TokenKind::Assign, "Expected '=' after pattern in if-let");
    if (is_err(eq))
        return unwrap_err(eq);

    // Parse scrutinee expression
    auto scrutinee = parse_expr();
    if (is_err(scrutinee))
        return scrutinee;

    // Parse then branch (must be a block)
    auto then_block = parse_block_expr();
    if (is_err(then_block))
        return then_block;

    // Parse optional else branch
    std::optional<ExprPtr> else_branch;
    skip_newlines();
    if (match(lexer::TokenKind::KwElse)) {
        skip_newlines();
        if (check(lexer::TokenKind::KwIf)) {
            // else if or else if let
            auto else_if = parse_if_expr();
            if (is_err(else_if))
                return else_if;
            else_branch = std::move(unwrap(else_if));
        } else {
            // else block
            auto else_block = parse_block_expr();
            if (is_err(else_block))
                return else_block;
            else_branch = std::move(unwrap(else_block));
        }
    }

    auto end_span = previous().span;
    return make_box<Expr>(Expr{.kind = IfLetExpr{.pattern = std::move(unwrap(pattern)),
                                                 .scrutinee = std::move(unwrap(scrutinee)),
                                                 .then_branch = std::move(unwrap(then_block)),
                                                 .else_branch = std::move(else_branch),
                                                 .span = SourceSpan::merge(start_span, end_span)},
                               .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_when_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'when'

    auto scrutinee = parse_expr();
    if (is_err(scrutinee))
        return scrutinee;

    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{'");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    std::vector<WhenArm> arms;
    skip_newlines();

    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto first_pattern = parse_pattern();
        if (is_err(first_pattern))
            return unwrap_err(first_pattern);

        // Check for comma-separated patterns: Pattern1, Pattern2, ... => body
        // These are combined into an OrPattern
        PatternPtr pattern;
        if (check(lexer::TokenKind::Comma) && peek_next().kind != lexer::TokenKind::FatArrow) {
            // We have multiple comma-separated patterns
            std::vector<PatternPtr> patterns;
            patterns.push_back(std::move(unwrap(first_pattern)));

            while (match(lexer::TokenKind::Comma)) {
                skip_newlines();
                // Stop if next token is => (end of patterns)
                if (check(lexer::TokenKind::FatArrow))
                    break;
                // Also stop if we hit } (malformed but let error handling deal with it)
                if (check(lexer::TokenKind::RBrace))
                    break;

                auto next = parse_pattern();
                if (is_err(next))
                    return unwrap_err(next);
                patterns.push_back(std::move(unwrap(next)));
            }

            auto span = SourceSpan::merge(patterns.front()->span, patterns.back()->span);
            pattern = make_box<Pattern>(Pattern{
                .kind = OrPattern{.patterns = std::move(patterns), .span = span}, .span = span});
        } else {
            pattern = std::move(unwrap(first_pattern));
        }

        std::optional<ExprPtr> guard;
        if (match(lexer::TokenKind::KwIf)) {
            auto g = parse_expr();
            if (is_err(g))
                return g;
            guard = std::move(unwrap(g));
        }

        auto arrow = expect(lexer::TokenKind::FatArrow, "Expected '=>'");
        if (is_err(arrow))
            return unwrap_err(arrow);

        auto body = parse_expr();
        if (is_err(body))
            return body;

        auto arm_span = SourceSpan::merge(pattern->span, unwrap(body)->span);
        arms.push_back(WhenArm{.pattern = std::move(pattern),
                               .guard = std::move(guard),
                               .body = std::move(unwrap(body)),
                               .span = arm_span});

        skip_newlines();
        match(lexer::TokenKind::Comma);
        skip_newlines();
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}'");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto end_span = previous().span;
    return make_box<Expr>(Expr{.kind = WhenExpr{.scrutinee = std::move(unwrap(scrutinee)),
                                                .arms = std::move(arms),
                                                .span = SourceSpan::merge(start_span, end_span)},
                               .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_loop_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'loop'

    auto body = parse_block_expr();
    if (is_err(body))
        return body;

    auto end_span = previous().span;
    return make_box<Expr>(Expr{.kind = LoopExpr{.label = std::nullopt,
                                                .body = std::move(unwrap(body)),
                                                .span = SourceSpan::merge(start_span, end_span)},
                               .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_while_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'while'

    auto cond = parse_expr();
    if (is_err(cond))
        return cond;

    auto body = parse_block_expr();
    if (is_err(body))
        return body;

    auto end_span = previous().span;
    return make_box<Expr>(Expr{.kind = WhileExpr{.label = std::nullopt,
                                                 .condition = std::move(unwrap(cond)),
                                                 .body = std::move(unwrap(body)),
                                                 .span = SourceSpan::merge(start_span, end_span)},
                               .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_for_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'for'

    auto pattern = parse_pattern();
    if (is_err(pattern))
        return unwrap_err(pattern);

    auto in_tok = expect(lexer::TokenKind::KwIn, "Expected 'in'");
    if (is_err(in_tok))
        return unwrap_err(in_tok);

    auto iter = parse_expr();
    if (is_err(iter))
        return iter;

    auto body = parse_block_expr();
    if (is_err(body))
        return body;

    auto end_span = previous().span;
    return make_box<Expr>(Expr{.kind = ForExpr{.label = std::nullopt,
                                               .pattern = std::move(unwrap(pattern)),
                                               .iter = std::move(unwrap(iter)),
                                               .body = std::move(unwrap(body)),
                                               .span = SourceSpan::merge(start_span, end_span)},
                               .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_return_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'return'

    std::optional<ExprPtr> value;
    if (!check(lexer::TokenKind::Semi) && !check(lexer::TokenKind::Newline) &&
        !check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto v = parse_expr();
        if (is_err(v))
            return v;
        value = std::move(unwrap(v));
    }

    auto end_span = previous().span;
    return make_box<Expr>(Expr{.kind = ReturnExpr{.value = std::move(value),
                                                  .span = SourceSpan::merge(start_span, end_span)},
                               .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_throw_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'throw'

    // Throw requires an expression (e.g., `throw new Error("message")`)
    auto expr = parse_expr();
    if (is_err(expr))
        return expr;

    auto end_span = previous().span;
    return make_box<Expr>(Expr{.kind = ThrowExpr{.expr = std::move(unwrap(expr)),
                                                 .span = SourceSpan::merge(start_span, end_span)},
                               .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_break_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'break'

    std::optional<ExprPtr> value;
    if (!check(lexer::TokenKind::Semi) && !check(lexer::TokenKind::Newline) &&
        !check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto v = parse_expr();
        if (is_err(v))
            return v;
        value = std::move(unwrap(v));
    }

    auto end_span = previous().span;
    return make_box<Expr>(Expr{.kind = BreakExpr{.label = std::nullopt,
                                                 .value = std::move(value),
                                                 .span = SourceSpan::merge(start_span, end_span)},
                               .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_continue_expr() -> Result<ExprPtr, ParseError> {
    auto span = peek().span;
    advance(); // consume 'continue'

    return make_box<Expr>(
        Expr{.kind = ContinueExpr{.label = std::nullopt, .span = span}, .span = span});
}

auto Parser::parse_closure_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;

    // Consume 'do' keyword
    auto do_tok = expect(lexer::TokenKind::KwDo, "Expected 'do'");
    if (is_err(do_tok))
        return unwrap_err(do_tok);

    // Parse parameters in parentheses
    auto lparen = expect(lexer::TokenKind::LParen, "Expected '(' after 'do'");
    if (is_err(lparen))
        return unwrap_err(lparen);

    std::vector<std::pair<PatternPtr, std::optional<TypePtr>>> params;
    skip_newlines();

    while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
        // Parse pattern
        auto pattern = parse_pattern();
        if (is_err(pattern))
            return unwrap_err(pattern);

        // Optional type annotation
        std::optional<TypePtr> type;
        if (match(lexer::TokenKind::Colon)) {
            auto t = parse_type();
            if (is_err(t))
                return unwrap_err(t);
            type = std::move(unwrap(t));
        }

        params.emplace_back(std::move(unwrap(pattern)), std::move(type));

        skip_newlines();
        if (!check(lexer::TokenKind::RParen)) {
            auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between closure parameters");
            if (is_err(comma))
                return unwrap_err(comma);
            skip_newlines();
        }
    }

    auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after closure parameters");
    if (is_err(rparen))
        return unwrap_err(rparen);

    // Optional return type
    std::optional<TypePtr> return_type;
    if (match(lexer::TokenKind::Arrow)) {
        auto t = parse_type();
        if (is_err(t))
            return unwrap_err(t);
        return_type = std::move(unwrap(t));
    }

    // Parse body (either block or expression)
    skip_newlines();
    ExprPtr body;
    if (check(lexer::TokenKind::LBrace)) {
        auto b = parse_block_expr();
        if (is_err(b))
            return b;
        body = std::move(unwrap(b));
    } else {
        auto b = parse_expr();
        if (is_err(b))
            return b;
        body = std::move(unwrap(b));
    }

    auto end_span = previous().span;

    return make_box<Expr>(
        Expr{.kind = ClosureExpr{.params = std::move(params),
                                 .return_type = std::move(return_type),
                                 .body = std::move(body),
                                 .is_move = false, // TML doesn't have move closures in this syntax
                                 .span = SourceSpan::merge(start_span, end_span),
                                 .captured_vars = {}},
             .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_struct_expr(TypePath path, std::optional<GenericArgs> generics)
    -> Result<ExprPtr, ParseError> {
    auto start_span = path.span;

    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{'");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    std::vector<std::pair<std::string, ExprPtr>> fields;
    std::optional<ExprPtr> base;

    skip_newlines();
    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        // Check for ..base
        if (match(lexer::TokenKind::DotDot)) {
            auto base_expr = parse_expr();
            if (is_err(base_expr))
                return base_expr;
            base = std::move(unwrap(base_expr));
            skip_newlines();
            break;
        }

        auto field_name_result = expect(lexer::TokenKind::Identifier, "Expected field name");
        if (is_err(field_name_result))
            return unwrap_err(field_name_result);
        auto field_name = std::string(unwrap(field_name_result).lexeme);

        ExprPtr value;
        if (match(lexer::TokenKind::Colon)) {
            auto v = parse_expr();
            if (is_err(v))
                return v;
            value = std::move(unwrap(v));
        } else {
            // Shorthand: field name is both name and value
            value = make_ident_expr(field_name, unwrap(field_name_result).span);
        }

        fields.emplace_back(std::move(field_name), std::move(value));

        skip_newlines();
        if (!check(lexer::TokenKind::RBrace) && !check(lexer::TokenKind::DotDot)) {
            match(lexer::TokenKind::Comma);
            skip_newlines();
        }
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}'");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto end_span = previous().span;
    return make_box<Expr>(Expr{.kind = StructExpr{.path = std::move(path),
                                                  .generics = std::move(generics),
                                                  .fields = std::move(fields),
                                                  .base = std::move(base),
                                                  .span = SourceSpan::merge(start_span, end_span)},
                               .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_call_args() -> Result<std::vector<ExprPtr>, ParseError> {
    std::vector<ExprPtr> args;

    skip_newlines();
    while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
        auto arg = parse_expr();
        if (is_err(arg))
            return unwrap_err(arg);
        args.push_back(std::move(unwrap(arg)));

        skip_newlines();
        if (!check(lexer::TokenKind::RParen)) {
            auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between arguments");
            if (is_err(comma))
                return unwrap_err(comma);
            skip_newlines();
        }
    }

    return args;
}

auto Parser::parse_lowlevel_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    auto lowlevel_tok = expect(lexer::TokenKind::KwLowlevel, "Expected 'lowlevel'");
    if (is_err(lowlevel_tok))
        return unwrap_err(lowlevel_tok);

    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' after 'lowlevel'");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    std::vector<StmtPtr> stmts;
    std::optional<ExprPtr> expr;

    skip_newlines();

    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto stmt = parse_stmt();
        if (is_err(stmt))
            return unwrap_err(stmt);

        skip_newlines();

        if (check(lexer::TokenKind::RBrace)) {
            if (unwrap(stmt)->is<ExprStmt>()) {
                expr = std::move(unwrap(stmt)->as<ExprStmt>().expr);
            } else {
                stmts.push_back(std::move(unwrap(stmt)));
            }
        } else {
            stmts.push_back(std::move(unwrap(stmt)));
            match(lexer::TokenKind::Semi);
            skip_newlines();
        }
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}'");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto span = SourceSpan::merge(start_span, previous().span);
    return make_box<Expr>(
        Expr{.kind = LowlevelExpr{.stmts = std::move(stmts), .expr = std::move(expr), .span = span},
             .span = span});
}

auto Parser::parse_base_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    auto base_tok = expect(lexer::TokenKind::KwBase, "Expected 'base'");
    if (is_err(base_tok))
        return unwrap_err(base_tok);

    // Require dot for member access: base.member
    auto dot = expect(lexer::TokenKind::Dot, "Expected '.' after 'base'");
    if (is_err(dot))
        return unwrap_err(dot);

    // Parse member name
    auto member_tok = expect(lexer::TokenKind::Identifier, "Expected member name after 'base.'");
    if (is_err(member_tok))
        return unwrap_err(member_tok);

    std::string member = std::string(previous().lexeme);
    std::vector<TypePtr> type_args;
    std::vector<ExprPtr> args;
    bool is_method_call = false;

    // Optional generic arguments
    if (check(lexer::TokenKind::LBracket)) {
        auto generics = parse_generic_args();
        if (is_err(generics))
            return unwrap_err(generics);
        if (unwrap(generics).has_value()) {
            for (auto& arg : unwrap(generics).value().args) {
                if (arg.is_type()) {
                    type_args.push_back(std::get<TypePtr>(std::move(arg.value)));
                }
            }
        }
    }

    // Check for method call
    if (check(lexer::TokenKind::LParen)) {
        is_method_call = true;
        advance(); // consume '('

        auto call_args = parse_call_args();
        if (is_err(call_args))
            return unwrap_err(call_args);
        args = std::move(unwrap(call_args));

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after arguments");
        if (is_err(rparen))
            return unwrap_err(rparen);
    }

    auto span = SourceSpan::merge(start_span, previous().span);
    return make_box<Expr>(Expr{.kind = BaseExpr{.member = std::move(member),
                                                .type_args = std::move(type_args),
                                                .args = std::move(args),
                                                .is_method_call = is_method_call,
                                                .span = span},
                               .span = span});
}

auto Parser::parse_interp_string_expr() -> Result<ExprPtr, ParseError> {
    // Parse interpolated string: "Hello {name}, you are {age} years old"
    // Token sequence: InterpStringStart("Hello ") -> Identifier(name) -> InterpStringMiddle(", you
    // are ")
    //                 -> Identifier(age) -> InterpStringEnd(" years old")
    //
    // IMPORTANT: The lexer intercepts the '}' and returns InterpStringMiddle/End directly
    // instead of returning RBrace. So after parsing the expression, we expect to see
    // InterpStringMiddle or InterpStringEnd immediately.

    auto start_span = peek().span;
    std::vector<InterpolatedSegment> segments;

    // First token is InterpStringStart (contains text before first {)
    auto start_token = advance();
    if (!start_token.is(lexer::TokenKind::InterpStringStart)) {
        return ParseError{
            .message = "Expected interpolated string start", .span = start_token.span, .notes = {}};
    }

    // Add the initial text segment (may be empty)
    const auto& start_str = std::get<lexer::StringValue>(start_token.value).value;
    if (!start_str.empty()) {
        segments.push_back(InterpolatedSegment{.content = start_str, .span = start_token.span});
    }

    // Loop: parse expression, then check for InterpStringMiddle or InterpStringEnd
    while (true) {
        // Parse the interpolated expression
        auto expr = parse_expr();
        if (is_err(expr))
            return expr;

        segments.push_back(
            InterpolatedSegment{.content = std::move(unwrap(expr)), .span = previous().span});

        // After the expression, the lexer should have already consumed the '}'
        // and returned the continuation token (InterpStringMiddle or InterpStringEnd)
        if (check(lexer::TokenKind::InterpStringMiddle)) {
            auto middle_token = advance();
            const auto& middle_str = std::get<lexer::StringValue>(middle_token.value).value;
            if (!middle_str.empty()) {
                segments.push_back(
                    InterpolatedSegment{.content = middle_str, .span = middle_token.span});
            }
            // Continue the loop to parse next expression
        } else if (check(lexer::TokenKind::InterpStringEnd)) {
            auto end_token = advance();
            const auto& end_str = std::get<lexer::StringValue>(end_token.value).value;
            if (!end_str.empty()) {
                segments.push_back(InterpolatedSegment{.content = end_str, .span = end_token.span});
            }
            // Done parsing the interpolated string
            break;
        } else {
            // Unexpected token
            return ParseError{.message = "Expected '}' to close interpolated expression (got " +
                                         std::string(lexer::token_kind_to_string(peek().kind)) +
                                         ")",
                              .span = peek().span,
                              .notes = {}};
        }
    }

    auto end_span = previous().span;
    return make_box<Expr>(
        Expr{.kind = InterpolatedStringExpr{.segments = std::move(segments),
                                            .span = SourceSpan::merge(start_span, end_span)},
             .span = SourceSpan::merge(start_span, end_span)});
}

// ============================================================================
// Type Parsing
// ============================================================================

} // namespace tml::parser
