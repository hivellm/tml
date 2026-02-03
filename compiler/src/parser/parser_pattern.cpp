//! # Parser - Patterns
//!
//! This file implements pattern parsing for matching and destructuring.
//!
//! ## Pattern Types
//!
//! | Pattern     | Syntax               | Description                 |
//! |-------------|----------------------|-----------------------------|
//! | Identifier  | `x`, `mut x`         | Bind value to name          |
//! | Wildcard    | `_`                  | Match anything, ignore      |
//! | Literal     | `42`, `"hello"`      | Match exact value           |
//! | Tuple       | `(a, b, c)`          | Destructure tuple           |
//! | Array       | `[a, b, ..rest]`     | Destructure array           |
//! | Struct      | `Point { x, y }`     | Destructure struct          |
//! | Enum        | `Just(v)`, `Nothing` | Match enum variant          |
//! | Or          | `A \| B`             | Match either pattern        |
//!
//! ## Struct Pattern Shorthand
//!
//! ```tml
//! Point { x, y }      // Binds x and y to fields of same name
//! Point { x: a, .. }  // Binds x to a, ignores rest
//! ```
//!
//! ## Or Patterns
//!
//! Or patterns allow matching multiple alternatives:
//! ```tml
//! when value {
//!     1 | 2 | 3 => "small"
//!     _ => "other"
//! }
//! ```

#include "parser/parser.hpp"

namespace tml::parser {

// ============================================================================
// Pattern Parsing
// ============================================================================

auto Parser::parse_pattern() -> Result<PatternPtr, ParseError> {
    auto first = parse_pattern_no_or();
    if (is_err(first))
        return first;

    // Check for or pattern: a | b
    if (check(lexer::TokenKind::BitOr)) {
        std::vector<PatternPtr> patterns;
        patterns.push_back(std::move(unwrap(first)));

        while (match(lexer::TokenKind::BitOr)) {
            auto next = parse_pattern_no_or();
            if (is_err(next))
                return next;
            patterns.push_back(std::move(unwrap(next)));
        }

        auto span = SourceSpan::merge(patterns.front()->span, patterns.back()->span);
        return make_box<Pattern>(Pattern{
            .kind = OrPattern{.patterns = std::move(patterns), .span = span}, .span = span});
    }

    return first;
}

auto Parser::parse_pattern_no_or() -> Result<PatternPtr, ParseError> {
    auto start_span = peek().span;

    // Wildcard: _
    if (check(lexer::TokenKind::Identifier) && peek().lexeme == "_") {
        advance();
        return make_wildcard_pattern(start_span);
    }

    // Mutable binding: mut x or mut this
    if (match(lexer::TokenKind::KwMut)) {
        // Check for 'mut this' (mutable self receiver)
        if (match(lexer::TokenKind::KwThis)) {
            auto span = SourceSpan::merge(start_span, previous().span);
            return make_ident_pattern("this", true, span);
        }
        // Regular mutable binding: mut x
        auto name_result = expect(lexer::TokenKind::Identifier, "Expected identifier after 'mut'");
        if (is_err(name_result))
            return unwrap_err(name_result);
        auto span = SourceSpan::merge(start_span, unwrap(name_result).span);
        return make_ident_pattern(std::string(unwrap(name_result).lexeme), true, span);
    }

    // this pattern (for immutable method receivers)
    if (match(lexer::TokenKind::KwThis)) {
        return make_ident_pattern("this", false, start_span);
    }

    // Literal pattern (possibly range pattern)
    if (check(lexer::TokenKind::IntLiteral) || check(lexer::TokenKind::FloatLiteral) ||
        check(lexer::TokenKind::StringLiteral) || check(lexer::TokenKind::CharLiteral) ||
        check(lexer::TokenKind::BoolLiteral) || check(lexer::TokenKind::NullLiteral)) {
        auto token = advance();

        // Check for range pattern: start to end OR start through end
        if (check(lexer::TokenKind::KwTo) || check(lexer::TokenKind::KwThrough)) {
            bool inclusive = check(lexer::TokenKind::KwThrough);
            advance(); // consume 'to' or 'through'

            // Parse end expression (must be a literal for now)
            if (!check(lexer::TokenKind::IntLiteral) && !check(lexer::TokenKind::FloatLiteral) &&
                !check(lexer::TokenKind::CharLiteral)) {
                return ParseError{.message =
                                      "Expected literal after 'to'/'through' in range pattern",
                                  .span = peek().span,
                                  .notes = {},
                                  .fixes = {},
                                  .code = "P014"};
            }
            auto end_token = advance();

            // Create start and end expressions from literals
            auto start_expr = make_box<Expr>(
                Expr{.kind = LiteralExpr{.token = std::move(token), .span = start_span},
                     .span = start_span});
            auto end_span = previous().span;
            auto end_expr = make_box<Expr>(
                Expr{.kind = LiteralExpr{.token = std::move(end_token), .span = end_span},
                     .span = end_span});

            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Pattern>(Pattern{.kind = RangePattern{.start = std::move(start_expr),
                                                                  .end = std::move(end_expr),
                                                                  .inclusive = inclusive,
                                                                  .span = span},
                                             .span = span});
        }

        return make_box<Pattern>(
            Pattern{.kind = LiteralPattern{.literal = std::move(token), .span = start_span},
                    .span = start_span});
    }

    // Array pattern: [a, b, c] or [head, ..rest]
    if (match(lexer::TokenKind::LBracket)) {
        std::vector<PatternPtr> elements;
        std::optional<PatternPtr> rest;
        skip_newlines();

        while (!check(lexer::TokenKind::RBracket) && !is_at_end()) {
            if (match(lexer::TokenKind::DotDot)) {
                if (check(lexer::TokenKind::Identifier)) {
                    auto rest_pattern = parse_pattern();
                    if (is_err(rest_pattern))
                        return rest_pattern;
                    rest = std::move(unwrap(rest_pattern));
                }
                skip_newlines();
                break;
            }

            auto elem = parse_pattern();
            if (is_err(elem))
                return elem;
            elements.push_back(std::move(unwrap(elem)));

            skip_newlines();
            if (!check(lexer::TokenKind::RBracket)) {
                auto comma = expect(lexer::TokenKind::Comma, "Expected ','");
                if (is_err(comma))
                    return unwrap_err(comma);
                skip_newlines();
            }
        }

        auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']'");
        if (is_err(rbracket))
            return unwrap_err(rbracket);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Pattern>(Pattern{.kind = ArrayPattern{.elements = std::move(elements),
                                                              .rest = std::move(rest),
                                                              .span = span},
                                         .span = span});
    }

    // Tuple pattern: (a, b, c)
    if (match(lexer::TokenKind::LParen)) {
        std::vector<PatternPtr> elements;
        skip_newlines();

        while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
            size_t start_pos = pos_; // Track position to detect infinite loops
            auto elem = parse_pattern();
            if (is_err(elem)) {
                // If parse failed and didn't consume any tokens, avoid infinite loop
                if (pos_ == start_pos) {
                    advance(); // Consume the problematic token
                }
                return elem;
            }
            elements.push_back(std::move(unwrap(elem)));

            // Detect infinite loop - if position didn't advance, break
            if (pos_ == start_pos) {
                return ParseError{.message =
                                      "Parser error: infinite loop detected in tuple pattern",
                                  .span = peek().span,
                                  .notes = {},
                                  .fixes = {},
                                  .code = "P043"};
            }

            skip_newlines();
            if (!check(lexer::TokenKind::RParen)) {
                auto comma = expect(lexer::TokenKind::Comma, "Expected ','");
                if (is_err(comma))
                    return unwrap_err(comma);
                skip_newlines();
            }
        }

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')'");
        if (is_err(rparen))
            return unwrap_err(rparen);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Pattern>(Pattern{
            .kind = TuplePattern{.elements = std::move(elements), .span = span}, .span = span});
    }

    // Identifier or enum pattern
    if (check(lexer::TokenKind::Identifier)) {
        auto path = parse_type_path();
        if (is_err(path))
            return unwrap_err(path);

        // Check for enum payload: Some(x)
        if (match(lexer::TokenKind::LParen)) {
            std::vector<PatternPtr> payload;
            skip_newlines();

            while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
                auto elem = parse_pattern();
                if (is_err(elem))
                    return elem;
                payload.push_back(std::move(unwrap(elem)));

                skip_newlines();
                if (!check(lexer::TokenKind::RParen)) {
                    auto comma = expect(lexer::TokenKind::Comma, "Expected ','");
                    if (is_err(comma))
                        return unwrap_err(comma);
                    skip_newlines();
                }
            }

            auto rparen = expect(lexer::TokenKind::RParen, "Expected ')'");
            if (is_err(rparen))
                return unwrap_err(rparen);

            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Pattern>(Pattern{.kind = EnumPattern{.path = std::move(unwrap(path)),
                                                                 .payload = std::move(payload),
                                                                 .span = span},
                                             .span = span});
        }

        // Check for struct pattern: Point { x, y } or Point { x: px, y: py, .. }
        if (match(lexer::TokenKind::LBrace)) {
            std::vector<std::pair<std::string, PatternPtr>> fields;
            bool has_rest = false;
            skip_newlines();

            while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
                skip_newlines();

                // Check for rest pattern: ..
                if (check(lexer::TokenKind::DotDot)) {
                    advance();
                    has_rest = true;
                    skip_newlines();
                    break; // .. must be last
                }

                // Parse field name
                auto field_name_result =
                    expect(lexer::TokenKind::Identifier, "Expected field name in struct pattern");
                if (is_err(field_name_result))
                    return unwrap_err(field_name_result);
                std::string field_name = std::string(unwrap(field_name_result).lexeme);

                PatternPtr field_pattern;
                skip_newlines();

                // Check for explicit binding: field: pattern
                if (match(lexer::TokenKind::Colon)) {
                    skip_newlines();
                    auto pat = parse_pattern();
                    if (is_err(pat))
                        return pat;
                    field_pattern = std::move(unwrap(pat));
                } else {
                    // Shorthand: field means field: field
                    field_pattern =
                        make_ident_pattern(field_name, false, unwrap(field_name_result).span);
                }

                fields.emplace_back(field_name, std::move(field_pattern));

                skip_newlines();
                if (!check(lexer::TokenKind::RBrace)) {
                    if (!match(lexer::TokenKind::Comma)) {
                        // Allow trailing comma to be optional
                        if (!check(lexer::TokenKind::RBrace)) {
                            return ParseError{.message = "Expected ',' or '}' in struct pattern",
                                              .span = peek().span,
                                              .notes = {},
                                              .fixes = {},
                                              .code = "P041"};
                        }
                    }
                    skip_newlines();
                }
            }

            auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}'");
            if (is_err(rbrace))
                return unwrap_err(rbrace);

            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Pattern>(Pattern{.kind = StructPattern{.path = std::move(unwrap(path)),
                                                                   .fields = std::move(fields),
                                                                   .has_rest = has_rest,
                                                                   .span = span},
                                             .span = span});
        }

        // Simple identifier pattern
        auto span = unwrap(path).span;
        if (unwrap(path).segments.size() == 1) {
            return make_ident_pattern(std::move(unwrap(path).segments[0]), false, span);
        }

        // Enum pattern without payload (like None)
        return make_box<Pattern>(Pattern{
            .kind =
                EnumPattern{.path = std::move(unwrap(path)), .payload = std::nullopt, .span = span},
            .span = span});
    }

    return ParseError{.message = "Expected pattern",
                      .span = peek().span,
                      .notes = {},
                      .fixes = {},
                      .code = "P007"};
}

} // namespace tml::parser
