//! # Parser - Complex Expressions
//!
//! This file implements parsing for control flow and complex expression forms.
//!
//! ## Expression Categories (this file)
//!
//! | Category      | Examples                                    |
//! |---------------|---------------------------------------------|
//! | Conditional   | `if`, `if let`, `when`                      |
//! | Loops         | `loop`, `while`, `for`                      |
//! | Control flow  | `return`, `throw`, `break`, `continue`      |
//! | Closures      | `do(x) expr`, `move do(x) expr`             |
//! | Struct init   | `Point { x: 1, y: 2 }`                     |
//! | Call args     | `f(a, b, c)`                                |
//! | Lowlevel      | `lowlevel { ... }`                          |
//! | Base          | `base.method()`                             |
//! | Strings       | `"hello {name}"`, `` `template {x}` ``      |

#include "parser/parser.hpp"

namespace tml::parser {

// ============================================================================
// Control Flow & Complex Expressions
// ============================================================================

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

    // Require '(' for condition
    auto lparen = expect(lexer::TokenKind::LParen,
                         "Expected '(' after 'loop' - syntax is: loop (condition) { body }");
    if (is_err(lparen))
        return unwrap_err(lparen);

    std::optional<LoopVarDecl> loop_var = std::nullopt;
    ExprPtr cond_expr;

    // Check for loop variable syntax: loop (var i: I64 < N)
    if (check(lexer::TokenKind::KwVar)) {
        auto var_start = peek().span;
        advance(); // consume 'var'

        // Parse variable name
        auto name_tok = expect(lexer::TokenKind::Identifier, "Expected variable name after 'var'");
        if (is_err(name_tok))
            return unwrap_err(name_tok);
        std::string var_name = std::string(unwrap(name_tok).lexeme);

        // Expect ':'
        auto colon =
            expect(lexer::TokenKind::Colon, "Expected ':' after variable name in loop declaration");
        if (is_err(colon))
            return unwrap_err(colon);

        // Parse type
        auto var_type = parse_type();
        if (is_err(var_type))
            return unwrap_err(var_type);

        auto var_end = previous().span;
        loop_var = LoopVarDecl{.name = var_name,
                               .type = std::move(unwrap(var_type)),
                               .span = SourceSpan::merge(var_start, var_end)};

        // Expect '<' comparison operator
        if (!check(lexer::TokenKind::Lt)) {
            return ParseError{.message = "Expected '<' after type in loop variable declaration - "
                                         "syntax is: loop (var i: I64 < N)",
                              .span = peek().span,
                              .notes = {},
                              .fixes = {},
                              .code = "P037"};
        }
        advance(); // consume '<'

        // Parse the limit expression (right-hand side of comparison)
        auto limit = parse_expr();
        if (is_err(limit))
            return limit;

        // Build the condition: var_name < limit
        auto var_ref = make_ident_expr(var_name, SourceSpan::merge(var_start, var_end));
        auto limit_span = unwrap(limit)->span;
        cond_expr = make_binary_expr(BinaryOp::Lt, std::move(var_ref), std::move(unwrap(limit)),
                                     SourceSpan::merge(var_start, limit_span));
    } else {
        // Parse regular condition expression
        auto cond = parse_expr();
        if (is_err(cond))
            return cond;
        cond_expr = std::move(unwrap(cond));
    }

    // Require ')'
    auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after loop condition");
    if (is_err(rparen))
        return unwrap_err(rparen);

    // Parse body block
    auto body = parse_block_expr();
    if (is_err(body))
        return body;

    auto end_span = previous().span;
    return make_box<Expr>(Expr{.kind = LoopExpr{.label = std::nullopt,
                                                .loop_var = std::move(loop_var),
                                                .condition = std::move(cond_expr),
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

    // Check for 'move' keyword (move closure)
    bool is_move = false;
    if (match(lexer::TokenKind::KwMove)) {
        is_move = true;
    }

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

    return make_box<Expr>(Expr{.kind = ClosureExpr{.params = std::move(params),
                                                   .return_type = std::move(return_type),
                                                   .body = std::move(body),
                                                   .is_move = is_move,
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
        return ParseError{.message = "Expected interpolated string start",
                          .span = start_token.span,
                          .notes = {},
                          .fixes = {},
                          .code = "P047"};
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
                              .notes = {},
                              .fixes = {},
                              .code = "P048"};
        }
    }

    auto end_span = previous().span;
    return make_box<Expr>(
        Expr{.kind = InterpolatedStringExpr{.segments = std::move(segments),
                                            .span = SourceSpan::merge(start_span, end_span)},
             .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_template_literal_expr() -> Result<ExprPtr, ParseError> {
    // Parse template literal: `Hello {name}, you are {age} years old`
    // Token sequence: TemplateLiteralStart("Hello ") -> Identifier(name) ->
    // TemplateLiteralMiddle(", you are ") -> Identifier(age) -> TemplateLiteralEnd(" years old")
    //
    // Simple case (no interpolation): TemplateLiteralEnd("Hello world")
    //
    // IMPORTANT: The lexer intercepts the '}' and returns TemplateLiteralMiddle/End directly
    // instead of returning RBrace. So after parsing the expression, we expect to see
    // TemplateLiteralMiddle or TemplateLiteralEnd immediately.

    auto start_span = peek().span;
    std::vector<InterpolatedSegment> segments;

    // Check for simple template literal (no interpolation)
    if (check(lexer::TokenKind::TemplateLiteralEnd)) {
        auto end_token = advance();
        const auto& str = std::get<lexer::StringValue>(end_token.value).value;
        if (!str.empty()) {
            segments.push_back(InterpolatedSegment{.content = str, .span = end_token.span});
        }
        return make_box<Expr>(
            Expr{.kind = TemplateLiteralExpr{.segments = std::move(segments), .span = start_span},
                 .span = start_span});
    }

    // First token is TemplateLiteralStart (contains text before first {)
    auto start_token = advance();
    if (!start_token.is(lexer::TokenKind::TemplateLiteralStart)) {
        return ParseError{.message = "Expected template literal start",
                          .span = start_token.span,
                          .notes = {},
                          .fixes = {},
                          .code = "P064"};
    }

    // Add the initial text segment (may be empty)
    const auto& start_str = std::get<lexer::StringValue>(start_token.value).value;
    if (!start_str.empty()) {
        segments.push_back(InterpolatedSegment{.content = start_str, .span = start_token.span});
    }

    // Loop: parse expression, then check for TemplateLiteralMiddle or TemplateLiteralEnd
    while (true) {
        // Parse the interpolated expression
        auto expr = parse_expr();
        if (is_err(expr))
            return expr;

        segments.push_back(
            InterpolatedSegment{.content = std::move(unwrap(expr)), .span = previous().span});

        // After the expression, the lexer should have already consumed the '}'
        // and returned the continuation token (TemplateLiteralMiddle or TemplateLiteralEnd)
        if (check(lexer::TokenKind::TemplateLiteralMiddle)) {
            auto middle_token = advance();
            const auto& middle_str = std::get<lexer::StringValue>(middle_token.value).value;
            if (!middle_str.empty()) {
                segments.push_back(
                    InterpolatedSegment{.content = middle_str, .span = middle_token.span});
            }
            // Continue the loop to parse next expression
        } else if (check(lexer::TokenKind::TemplateLiteralEnd)) {
            auto end_token = advance();
            const auto& end_str = std::get<lexer::StringValue>(end_token.value).value;
            if (!end_str.empty()) {
                segments.push_back(InterpolatedSegment{.content = end_str, .span = end_token.span});
            }
            // Done parsing the template literal
            break;
        } else {
            // Unexpected token
            return ParseError{.message = "Expected '}' to close template expression (got " +
                                         std::string(lexer::token_kind_to_string(peek().kind)) +
                                         ")",
                              .span = peek().span,
                              .notes = {},
                              .fixes = {},
                              .code = "P065"};
        }
    }

    auto end_span = previous().span;
    return make_box<Expr>(
        Expr{.kind = TemplateLiteralExpr{.segments = std::move(segments),
                                         .span = SourceSpan::merge(start_span, end_span)},
             .span = SourceSpan::merge(start_span, end_span)});
}

// ============================================================================
// Type Parsing
// ============================================================================

} // namespace tml::parser
