//! # Parser Core
//!
//! This file implements the core parser infrastructure.
//!
//! ## Token Navigation
//!
//! | Method        | Description                        |
//! |---------------|------------------------------------|
//! | `peek()`      | Look at current token              |
//! | `peek_next()` | Look at next token                 |
//! | `advance()`   | Consume and return current token   |
//! | `previous()`  | Get last consumed token            |
//! | `match()`     | Consume token if it matches        |
//! | `check()`     | Check current token without consuming |
//! | `expect()`    | Require specific token or error    |
//!
//! ## Error Recovery
//!
//! | Method              | Strategy                         |
//! |---------------------|----------------------------------|
//! | `synchronize()`     | Skip to statement boundary       |
//! | `synchronize_to_stmt()` | Skip to next statement       |
//! | `synchronize_to_decl()` | Skip to next declaration     |
//! | `synchronize_to_brace()` | Match brace nesting         |
//!
//! ## Operator Handling
//!
//! Precedence levels from lowest to highest:
//! - Assignment (=, +=, etc.)
//! - Ternary (?)
//! - Or (or, ||)
//! - And (and, &&)
//! - Comparison (==, !=, <, etc.)
//! - Bit operations (&, |, ^, <<, >>)
//! - Term (+, -)
//! - Factor (*, /, %)
//! - Cast (as)
//! - Call (., [], ())

#include "parser/parser.hpp"

namespace tml::parser {

Parser::Parser(std::vector<lexer::Token> tokens) : tokens_(std::move(tokens)) {}

auto Parser::peek() const -> const lexer::Token& {
    if (pos_ >= tokens_.size()) {
        return tokens_.back(); // Should be EOF
    }
    return tokens_[pos_];
}

auto Parser::peek_next() const -> const lexer::Token& {
    if (pos_ + 1 >= tokens_.size()) {
        return tokens_.back();
    }
    return tokens_[pos_ + 1];
}

auto Parser::previous() const -> const lexer::Token& {
    if (pos_ == 0) {
        return tokens_[0];
    }
    return tokens_[pos_ - 1];
}

auto Parser::advance() -> const lexer::Token& {
    if (!is_at_end()) {
        ++pos_;
    }
    return previous();
}

auto Parser::is_at_end() const -> bool {
    return peek().is_eof();
}

auto Parser::check(lexer::TokenKind kind) const -> bool {
    return peek().kind == kind;
}

auto Parser::check_next(lexer::TokenKind kind) const -> bool {
    return peek_next().kind == kind;
}

auto Parser::match(lexer::TokenKind kind) -> bool {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

auto Parser::expect(lexer::TokenKind kind, const std::string& message)
    -> Result<lexer::Token, ParseError> {
    if (check(kind)) {
        return advance();
    }
    return ParseError{.message = message + ", found '" +
                                 std::string(lexer::token_kind_to_string(peek().kind)) + "'",
                      .span = peek().span,
                      .notes = {},
                      .fixes = {}};
}

void Parser::skip_newlines() {
    while (check(lexer::TokenKind::Newline) || check(lexer::TokenKind::DocComment) ||
           check(lexer::TokenKind::ModuleDocComment)) {
        advance();
    }
}

auto Parser::collect_doc_comment() -> std::optional<std::string> {
    std::optional<std::string> doc;

    // Skip newlines but collect the last doc comment before an item
    while (check(lexer::TokenKind::Newline) || check(lexer::TokenKind::DocComment) ||
           check(lexer::TokenKind::ModuleDocComment)) {
        if (check(lexer::TokenKind::DocComment)) {
            // Get the doc comment content from the token
            const auto& token = peek();
            doc = token.doc_value().content;
        }
        advance();
    }

    return doc;
}

void Parser::report_error(const std::string& message) {
    report_error(message, peek().span);
}

void Parser::report_error(const std::string& message, SourceSpan span) {
    errors_.push_back(ParseError{.message = message, .span = span, .notes = {}, .fixes = {}});
}

void Parser::report_error_with_fix(const std::string& message, SourceSpan span,
                                   const std::vector<FixItHint>& fixes) {
    errors_.push_back(ParseError{.message = message, .span = span, .notes = {}, .fixes = fixes});
}

void Parser::synchronize() {
    advance();

    while (!is_at_end()) {
        // Synchronize at statement boundaries
        if (previous().kind == lexer::TokenKind::Semi ||
            previous().kind == lexer::TokenKind::Newline) {
            return;
        }

        // Or at declaration keywords
        switch (peek().kind) {
        case lexer::TokenKind::KwFunc:
        case lexer::TokenKind::KwType:
        case lexer::TokenKind::KwBehavior:
        case lexer::TokenKind::KwImpl:
        case lexer::TokenKind::KwLet:
        case lexer::TokenKind::KwConst:
        case lexer::TokenKind::KwMod:
        case lexer::TokenKind::KwUse:
        case lexer::TokenKind::KwPub:
            return;
        default:
            advance();
        }
    }
}

void Parser::synchronize_to_stmt() {
    while (!is_at_end()) {
        // Stop at statement terminators
        if (check(lexer::TokenKind::Semi) || check(lexer::TokenKind::Newline)) {
            advance();
            skip_newlines();
            return;
        }

        // Stop at statement-starting keywords
        switch (peek().kind) {
        case lexer::TokenKind::KwLet:
        case lexer::TokenKind::KwVar:
        case lexer::TokenKind::KwIf:
        case lexer::TokenKind::KwLoop:
        case lexer::TokenKind::KwWhile:
        case lexer::TokenKind::KwFor:
        case lexer::TokenKind::KwReturn:
        case lexer::TokenKind::KwThrow:
        case lexer::TokenKind::KwBreak:
        case lexer::TokenKind::KwContinue:
        case lexer::TokenKind::KwWhen:
            return;
        case lexer::TokenKind::RBrace:
            return; // End of block
        default:
            advance();
        }
    }
}

void Parser::synchronize_to_decl() {
    while (!is_at_end()) {
        // Stop at declaration keywords
        switch (peek().kind) {
        case lexer::TokenKind::KwFunc:
        case lexer::TokenKind::KwType:
        case lexer::TokenKind::KwBehavior:
        case lexer::TokenKind::KwImpl:
        case lexer::TokenKind::KwConst:
        case lexer::TokenKind::KwMod:
        case lexer::TokenKind::KwUse:
        case lexer::TokenKind::KwPub:
        case lexer::TokenKind::At: // Decorator
            return;
        default:
            advance();
        }
    }
}

void Parser::synchronize_to_brace() {
    int brace_depth = 1;
    while (!is_at_end() && brace_depth > 0) {
        if (check(lexer::TokenKind::LBrace)) {
            brace_depth++;
        } else if (check(lexer::TokenKind::RBrace)) {
            brace_depth--;
            if (brace_depth == 0) {
                return; // Don't consume the closing brace
            }
        }
        advance();
    }
}

bool Parser::try_recover_missing_semi() {
    // If we're at a newline or statement-starting keyword, we might have a missing semicolon
    if (check(lexer::TokenKind::Newline)) {
        skip_newlines();
        return true;
    }

    // Check if next token looks like the start of a new statement
    switch (peek().kind) {
    case lexer::TokenKind::KwLet:
    case lexer::TokenKind::KwVar:
    case lexer::TokenKind::KwIf:
    case lexer::TokenKind::KwLoop:
    case lexer::TokenKind::KwWhile:
    case lexer::TokenKind::KwFor:
    case lexer::TokenKind::KwReturn:
    case lexer::TokenKind::KwThrow:
    case lexer::TokenKind::KwBreak:
    case lexer::TokenKind::KwContinue:
    case lexer::TokenKind::RBrace:
        return true;
    default:
        return false;
    }
}

bool Parser::skip_until(lexer::TokenKind kind) {
    while (!is_at_end()) {
        if (check(kind)) {
            return true;
        }
        advance();
    }
    return false;
}

bool Parser::skip_until_any(std::initializer_list<lexer::TokenKind> kinds) {
    while (!is_at_end()) {
        for (auto kind : kinds) {
            if (check(kind)) {
                return true;
            }
        }
        advance();
    }
    return false;
}

// ============================================================================
// Fix-it Hint Helpers
// ============================================================================

auto Parser::make_insertion_fix(const SourceSpan& at, const std::string& text,
                                const std::string& desc) -> FixItHint {
    // For insertion, span start and end are the same (insert point)
    return FixItHint{.span = SourceSpan{.start = at.end, .end = at.end},
                     .replacement = text,
                     .description = desc};
}

auto Parser::make_replacement_fix(const SourceSpan& span, const std::string& text,
                                  const std::string& desc) -> FixItHint {
    return FixItHint{.span = span, .replacement = text, .description = desc};
}

auto Parser::make_deletion_fix(const SourceSpan& span, const std::string& desc) -> FixItHint {
    return FixItHint{.span = span, .replacement = "", .description = desc};
}

// ============================================================================
// Module Parsing
// ============================================================================

auto Parser::parse_module(const std::string& name) -> Result<Module, std::vector<ParseError>> {
    std::vector<DeclPtr> decls;
    std::vector<std::string> module_docs;
    auto start_span = peek().span;

    // Collect module-level doc comments (//!) at the start of the file
    while (check(lexer::TokenKind::Newline) || check(lexer::TokenKind::ModuleDocComment)) {
        if (check(lexer::TokenKind::ModuleDocComment)) {
            const auto& token = peek();
            module_docs.push_back(token.doc_value().content);
        }
        advance();
    }

    while (!is_at_end()) {
        auto decl_result = parse_decl();
        if (is_ok(decl_result)) {
            decls.push_back(std::move(unwrap(decl_result)));
        } else {
            errors_.push_back(unwrap_err(decl_result));
            synchronize();
        }
        skip_newlines();
    }

    if (has_errors()) {
        return errors_;
    }

    auto end_span = previous().span;
    return Module{.name = name,
                  .module_docs = std::move(module_docs),
                  .decls = std::move(decls),
                  .span = SourceSpan::merge(start_span, end_span)};
}

// ============================================================================
// Operator Helpers
// ============================================================================

auto Parser::get_precedence(lexer::TokenKind kind) -> int {
    switch (kind) {
    case lexer::TokenKind::Assign:
    case lexer::TokenKind::PlusAssign:
    case lexer::TokenKind::MinusAssign:
    case lexer::TokenKind::StarAssign:
    case lexer::TokenKind::SlashAssign:
    case lexer::TokenKind::PercentAssign:
    case lexer::TokenKind::BitAndAssign:
    case lexer::TokenKind::BitOrAssign:
    case lexer::TokenKind::BitXorAssign:
    case lexer::TokenKind::ShlAssign:
    case lexer::TokenKind::ShrAssign:
        return precedence::ASSIGN;

    case lexer::TokenKind::Question:
        return precedence::TERNARY;

    case lexer::TokenKind::KwOr:
    case lexer::TokenKind::OrOr:
        return precedence::OR;

    case lexer::TokenKind::KwAnd:
    case lexer::TokenKind::AndAnd:
        return precedence::AND;

    case lexer::TokenKind::Eq:
    case lexer::TokenKind::Ne:
    case lexer::TokenKind::Lt:
    case lexer::TokenKind::Gt:
    case lexer::TokenKind::Le:
    case lexer::TokenKind::Ge:
        return precedence::COMPARISON;

    case lexer::TokenKind::BitOr:
        return precedence::BITOR;

    case lexer::TokenKind::BitXor:
    case lexer::TokenKind::KwXor:
        return precedence::BITXOR;

    case lexer::TokenKind::BitAnd:
        return precedence::BITAND;

    case lexer::TokenKind::Shl:
    case lexer::TokenKind::Shr:
    case lexer::TokenKind::KwShl:
    case lexer::TokenKind::KwShr:
        return precedence::SHIFT;

    case lexer::TokenKind::Plus:
    case lexer::TokenKind::Minus:
        return precedence::TERM;

    case lexer::TokenKind::Star:
    case lexer::TokenKind::Slash:
    case lexer::TokenKind::Percent:
        return precedence::FACTOR;

    case lexer::TokenKind::KwAs:
    case lexer::TokenKind::KwIs:
        return precedence::CAST;

    case lexer::TokenKind::LParen:
    case lexer::TokenKind::LBracket:
    case lexer::TokenKind::Dot:
    case lexer::TokenKind::Bang:
    case lexer::TokenKind::PlusPlus:
    case lexer::TokenKind::MinusMinus:
        return precedence::CALL;

    case lexer::TokenKind::DotDot:
    case lexer::TokenKind::KwTo:
    case lexer::TokenKind::KwThrough:
        return precedence::RANGE;

    default:
        return precedence::NONE;
    }
}

auto Parser::is_right_associative(lexer::TokenKind kind) -> bool {
    // Assignment and ternary are right-associative
    switch (kind) {
    case lexer::TokenKind::Assign:
    case lexer::TokenKind::PlusAssign:
    case lexer::TokenKind::MinusAssign:
    case lexer::TokenKind::StarAssign:
    case lexer::TokenKind::SlashAssign:
    case lexer::TokenKind::PercentAssign:
    case lexer::TokenKind::BitAndAssign:
    case lexer::TokenKind::BitOrAssign:
    case lexer::TokenKind::BitXorAssign:
    case lexer::TokenKind::ShlAssign:
    case lexer::TokenKind::ShrAssign:
    case lexer::TokenKind::Question:
        return true;
    default:
        return false;
    }
}

auto Parser::token_to_binary_op(lexer::TokenKind kind) -> std::optional<BinaryOp> {
    switch (kind) {
    case lexer::TokenKind::Plus:
        return BinaryOp::Add;
    case lexer::TokenKind::Minus:
        return BinaryOp::Sub;
    case lexer::TokenKind::Star:
        return BinaryOp::Mul;
    case lexer::TokenKind::Slash:
        return BinaryOp::Div;
    case lexer::TokenKind::Percent:
        return BinaryOp::Mod;

    case lexer::TokenKind::Eq:
        return BinaryOp::Eq;
    case lexer::TokenKind::Ne:
        return BinaryOp::Ne;
    case lexer::TokenKind::Lt:
        return BinaryOp::Lt;
    case lexer::TokenKind::Gt:
        return BinaryOp::Gt;
    case lexer::TokenKind::Le:
        return BinaryOp::Le;
    case lexer::TokenKind::Ge:
        return BinaryOp::Ge;

    case lexer::TokenKind::KwAnd:
    case lexer::TokenKind::AndAnd:
        return BinaryOp::And;
    case lexer::TokenKind::KwOr:
    case lexer::TokenKind::OrOr:
        return BinaryOp::Or;

    case lexer::TokenKind::BitAnd:
        return BinaryOp::BitAnd;
    case lexer::TokenKind::BitOr:
        return BinaryOp::BitOr;
    case lexer::TokenKind::BitXor:
    case lexer::TokenKind::KwXor:
        return BinaryOp::BitXor;
    case lexer::TokenKind::Shl:
    case lexer::TokenKind::KwShl:
        return BinaryOp::Shl;
    case lexer::TokenKind::Shr:
    case lexer::TokenKind::KwShr:
        return BinaryOp::Shr;

    case lexer::TokenKind::Assign:
        return BinaryOp::Assign;
    case lexer::TokenKind::PlusAssign:
        return BinaryOp::AddAssign;
    case lexer::TokenKind::MinusAssign:
        return BinaryOp::SubAssign;
    case lexer::TokenKind::StarAssign:
        return BinaryOp::MulAssign;
    case lexer::TokenKind::SlashAssign:
        return BinaryOp::DivAssign;
    case lexer::TokenKind::PercentAssign:
        return BinaryOp::ModAssign;
    case lexer::TokenKind::BitAndAssign:
        return BinaryOp::BitAndAssign;
    case lexer::TokenKind::BitOrAssign:
        return BinaryOp::BitOrAssign;
    case lexer::TokenKind::BitXorAssign:
        return BinaryOp::BitXorAssign;
    case lexer::TokenKind::ShlAssign:
        return BinaryOp::ShlAssign;
    case lexer::TokenKind::ShrAssign:
        return BinaryOp::ShrAssign;

    default:
        return std::nullopt;
    }
}

auto Parser::token_to_unary_op(lexer::TokenKind kind) -> std::optional<UnaryOp> {
    switch (kind) {
    case lexer::TokenKind::Minus:
        return UnaryOp::Neg;
    case lexer::TokenKind::KwNot:
    case lexer::TokenKind::Bang: // ! as prefix is logical NOT (same as 'not')
        return UnaryOp::Not;
    case lexer::TokenKind::BitNot:
        return UnaryOp::BitNot;
    case lexer::TokenKind::BitAnd:
        return UnaryOp::Ref;
    case lexer::TokenKind::KwRef:
        return UnaryOp::Ref; // TML uses 'ref x' syntax
    case lexer::TokenKind::Star:
        return UnaryOp::Deref;
    default:
        return std::nullopt;
    }
}

} // namespace tml::parser
