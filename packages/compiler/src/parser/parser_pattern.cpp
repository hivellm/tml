#include "tml/parser/parser.hpp"

namespace tml::parser {

// ============================================================================
// Pattern Parsing
// ============================================================================

auto Parser::parse_pattern() -> Result<PatternPtr, ParseError> {
    auto first = parse_pattern_no_or();
    if (is_err(first)) return first;

    // Check for or pattern: a | b
    if (check(lexer::TokenKind::BitOr)) {
        std::vector<PatternPtr> patterns;
        patterns.push_back(std::move(unwrap(first)));

        while (match(lexer::TokenKind::BitOr)) {
            auto next = parse_pattern_no_or();
            if (is_err(next)) return next;
            patterns.push_back(std::move(unwrap(next)));
        }

        auto span = SourceSpan::merge(patterns.front()->span, patterns.back()->span);
        return make_box<Pattern>(Pattern{
            .kind = OrPattern{
                .patterns = std::move(patterns),
                .span = span
            },
            .span = span
        });
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
        if (is_err(name_result)) return unwrap_err(name_result);
        auto span = SourceSpan::merge(start_span, unwrap(name_result).span);
        return make_ident_pattern(std::string(unwrap(name_result).lexeme), true, span);
    }

    // this pattern (for immutable method receivers)
    if (match(lexer::TokenKind::KwThis)) {
        return make_ident_pattern("this", false, start_span);
    }

    // Literal pattern
    if (check(lexer::TokenKind::IntLiteral) ||
        check(lexer::TokenKind::FloatLiteral) ||
        check(lexer::TokenKind::StringLiteral) ||
        check(lexer::TokenKind::CharLiteral) ||
        check(lexer::TokenKind::BoolLiteral)) {
        auto token = advance();
        return make_box<Pattern>(Pattern{
            .kind = LiteralPattern{
                .literal = std::move(token),
                .span = start_span
            },
            .span = start_span
        });
    }

    // Tuple pattern: (a, b, c)
    if (match(lexer::TokenKind::LParen)) {
        std::vector<PatternPtr> elements;
        skip_newlines();

        while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
            size_t start_pos = pos_;  // Track position to detect infinite loops
            auto elem = parse_pattern();
            if (is_err(elem)) {
                // If parse failed and didn't consume any tokens, avoid infinite loop
                if (pos_ == start_pos) {
                    advance();  // Consume the problematic token
                }
                return elem;
            }
            elements.push_back(std::move(unwrap(elem)));

            // Detect infinite loop - if position didn't advance, break
            if (pos_ == start_pos) {
                return ParseError{
                    .message = "Parser error: infinite loop detected in tuple pattern",
                    .span = peek().span,
                    .notes = {}
                };
            }

            skip_newlines();
            if (!check(lexer::TokenKind::RParen)) {
                auto comma = expect(lexer::TokenKind::Comma, "Expected ','");
                if (is_err(comma)) return unwrap_err(comma);
                skip_newlines();
            }
        }

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')'");
        if (is_err(rparen)) return unwrap_err(rparen);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Pattern>(Pattern{
            .kind = TuplePattern{
                .elements = std::move(elements),
                .span = span
            },
            .span = span
        });
    }

    // Identifier or enum pattern
    if (check(lexer::TokenKind::Identifier)) {
        auto path = parse_type_path();
        if (is_err(path)) return unwrap_err(path);

        // Check for enum payload: Some(x)
        if (match(lexer::TokenKind::LParen)) {
            std::vector<PatternPtr> payload;
            skip_newlines();

            while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
                auto elem = parse_pattern();
                if (is_err(elem)) return elem;
                payload.push_back(std::move(unwrap(elem)));

                skip_newlines();
                if (!check(lexer::TokenKind::RParen)) {
                    auto comma = expect(lexer::TokenKind::Comma, "Expected ','");
                    if (is_err(comma)) return unwrap_err(comma);
                    skip_newlines();
                }
            }

            auto rparen = expect(lexer::TokenKind::RParen, "Expected ')'");
            if (is_err(rparen)) return unwrap_err(rparen);

            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Pattern>(Pattern{
                .kind = EnumPattern{
                    .path = std::move(unwrap(path)),
                    .payload = std::move(payload),
                    .span = span
                },
                .span = span
            });
        }

        // Check for struct pattern: Point { x, y }
        if (check(lexer::TokenKind::LBrace)) {
            // TODO: implement struct patterns
        }

        // Simple identifier pattern
        auto span = unwrap(path).span;
        if (unwrap(path).segments.size() == 1) {
            return make_ident_pattern(std::move(unwrap(path).segments[0]), false, span);
        }

        // Enum pattern without payload (like None)
        return make_box<Pattern>(Pattern{
            .kind = EnumPattern{
                .path = std::move(unwrap(path)),
                .payload = std::nullopt,
                .span = span
            },
            .span = span
        });
    }

    return ParseError{
        .message = "Expected pattern",
        .span = peek().span,
        .notes = {}
    };
}

} // namespace tml::parser
