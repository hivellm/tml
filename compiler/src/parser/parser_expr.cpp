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
//!
//! Method chaining can also continue across newlines with leading `.`:
//! ```tml
//! let json = object()
//!     .ks("name", "John")
//!     .kn("age", 30)
//!     .build()
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

        // Allow `.` to continue across newlines for method chaining:
        //   object()
        //       .method1()
        //       .method2()
        // But block other postfix operators like `(`, `[`, etc. after newlines
        bool is_method_chain_continuation = (next_kind == lexer::TokenKind::Dot);

        // If we skipped newlines and hit a postfix operator (except `.`), don't continue
        if (saved_pos != pos_ && is_postfix && !is_method_chain_continuation) {
            pos_ = saved_pos;
            break;
        }

        // If we skipped newlines and see `*`, don't continue - it's almost always a dereference
        // at the start of a new statement, not multiplication continuing from previous line.
        // Example:
        //   let ptr: *U16 = alloc(8) as *U16
        //   *ptr = 42  // This is dereference assignment, not multiplication
        if (saved_pos != pos_ && next_kind == lexer::TokenKind::Star) {
            pos_ = saved_pos;
            break;
        }

        // Handle postfix operators BEFORE precedence check (they have implicit high precedence)
        // For method chain continuation (`.` after newline), this handles it correctly
        if (is_postfix) {
            left = parse_postfix_expr(std::move(unwrap(left)));
            if (is_err(left))
                return left;
            continue;
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
                return ParseError{
                    "Expected ':' in ternary expression", peek().span, {}, {}, "P035"};
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
                              .notes = {},
                              .fixes = {},
                              .code = "P010"};
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
        return ParseError{.message = "Expected binary operator",
                          .span = op_token.span,
                          .notes = {},
                          .fixes = {},
                          .code = "P019"};
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

    // Template literal: `Hello {name}!` (produces Text type)
    if (check(lexer::TokenKind::TemplateLiteralStart) ||
        check(lexer::TokenKind::TemplateLiteralEnd)) {
        return parse_template_literal_expr();
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

    // Closure: do(x) expr or move do(x) expr
    if (check(lexer::TokenKind::KwDo) || check(lexer::TokenKind::KwMove)) {
        return parse_closure_expr();
    }

    // Lowlevel block: lowlevel { ... }
    if (check(lexer::TokenKind::KwLowlevel)) {
        return parse_lowlevel_expr();
    }

    return ParseError{.message = "Expected expression",
                      .span = peek().span,
                      .notes = {},
                      .fixes = {},
                      .code = "P004"};
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

    // After generic args, check for :: to continue with a method call
    // This supports Type[T]::method() syntax for static method calls
    // We create a MethodCallExpr with the PathExpr as receiver
    if (unwrap(generics).has_value() && match(lexer::TokenKind::ColonColon)) {
        auto method_name_result =
            expect(lexer::TokenKind::Identifier, "Expected method name after '::'");
        if (is_err(method_name_result))
            return unwrap_err(method_name_result);
        std::string method_name = std::string(unwrap(method_name_result).lexeme);

        // Method-level type arguments (e.g., ::method[U]()) are rare and not supported
        // for the :: syntax. Use . syntax if you need method-level type args.
        std::vector<TypePtr> method_type_args;

        // Expect ( for method call
        auto lparen = expect(lexer::TokenKind::LParen, "Expected '(' for method call after '::'");
        if (is_err(lparen))
            return unwrap_err(lparen);

        auto args = parse_call_args();
        if (is_err(args))
            return unwrap_err(args);

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after arguments");
        if (is_err(rparen))
            return unwrap_err(rparen);

        // Create the receiver PathExpr with generics
        auto receiver_span = unwrap(path).span;
        if (unwrap(generics).has_value()) {
            receiver_span = SourceSpan::merge(receiver_span, unwrap(generics)->span);
        }
        auto receiver =
            make_box<Expr>(Expr{.kind = PathExpr{.path = std::move(unwrap(path)),
                                                 .generics = std::move(unwrap(generics)),
                                                 .span = receiver_span},
                                .span = receiver_span});

        auto span = SourceSpan::merge(receiver_span, previous().span);
        return make_box<Expr>(Expr{.kind = MethodCallExpr{.receiver = std::move(receiver),
                                                          .method = method_name,
                                                          .type_args = std::move(method_type_args),
                                                          .args = std::move(unwrap(args)),
                                                          .span = span},
                                   .span = span});
    }

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

} // namespace tml::parser
