//! # Parser - Types
//!
//! This file implements type expression parsing.
//!
//! ## Type Syntax
//!
//! | Type               | Syntax                      |
//! |--------------------|-----------------------------|
//! | Named              | `I32`, `Point`, `Vec[T]`    |
//! | Reference          | `ref T`, `mut ref T`        |
//! | Pointer            | `*T`, `*mut T`, `*const T`  |
//! | Array              | `[T; N]`                    |
//! | Slice              | `[T]`                       |
//! | Tuple              | `(A, B, C)`                 |
//! | Function           | `func(A, B) -> C`           |
//! | Dynamic trait      | `dyn Behavior`              |
//! | Impl trait         | `impl Behavior`             |
//! | Infer              | `_`                         |
//!
//! ## Generic Arguments
//!
//! Uses `[T]` syntax instead of `<T>` (less ambiguous for LLMs):
//! ```tml
//! Vec[I32]
//! HashMap[Str, I32]
//! Iterator[Item=I32]    // Associated type binding
//! ```
//!
//! ## Type Paths
//!
//! Multi-segment paths: `std::collections::HashMap`

#include "parser/parser.hpp"

namespace tml::parser {

// ============================================================================
// Type Parsing
// ============================================================================

auto Parser::parse_type() -> Result<TypePtr, ParseError> {
    auto start_span = peek().span;

    // TML Reference: ref T, mut ref T
    if (match(lexer::TokenKind::KwMut)) {
        // Must be followed by 'ref'
        if (!match(lexer::TokenKind::KwRef)) {
            return ParseError{"Expected 'ref' after 'mut' in type", peek().span, {}};
        }
        auto inner = parse_type();
        if (is_err(inner))
            return inner;
        return make_ref_type(true, std::move(unwrap(inner)),
                             SourceSpan::merge(start_span, unwrap(inner)->span));
    }
    if (match(lexer::TokenKind::KwRef)) {
        auto inner = parse_type();
        if (is_err(inner))
            return inner;
        return make_ref_type(false, std::move(unwrap(inner)),
                             SourceSpan::merge(start_span, unwrap(inner)->span));
    }

    // Legacy Reference: &T, &mut T
    if (match(lexer::TokenKind::BitAnd)) {
        bool is_mut = match(lexer::TokenKind::KwMut);
        auto inner = parse_type();
        if (is_err(inner))
            return inner;
        return make_ref_type(is_mut, std::move(unwrap(inner)),
                             SourceSpan::merge(start_span, unwrap(inner)->span));
    }

    // Pointer: *const T, *mut T
    // Dyn trait object: dyn Behavior[T] or dyn mut Behavior[T]
    if (match(lexer::TokenKind::KwDyn)) {
        bool is_mut = match(lexer::TokenKind::KwMut);

        // Parse the behavior path
        auto behavior_path = parse_type_path();
        if (is_err(behavior_path))
            return unwrap_err(behavior_path);

        // Parse optional generic arguments
        auto generics = parse_generic_args();
        if (is_err(generics))
            return unwrap_err(generics);

        auto end_span = previous().span;
        auto span = SourceSpan::merge(start_span, end_span);
        return make_box<Type>(Type{.kind = DynType{.behavior = std::move(unwrap(behavior_path)),
                                                   .generics = std::move(unwrap(generics)),
                                                   .is_mut = is_mut,
                                                   .span = span},
                                   .span = span});
    }

    // Impl behavior return type: impl Behavior[T]
    // This is used for opaque return types that implement a behavior
    if (match(lexer::TokenKind::KwImpl)) {
        // Parse the behavior path
        auto behavior_path = parse_type_path();
        if (is_err(behavior_path))
            return unwrap_err(behavior_path);

        // Parse optional generic arguments
        auto generics = parse_generic_args();
        if (is_err(generics))
            return unwrap_err(generics);

        auto end_span = previous().span;
        auto span = SourceSpan::merge(start_span, end_span);
        return make_box<Type>(
            Type{.kind = ImplBehaviorType{.behavior = std::move(unwrap(behavior_path)),
                                          .generics = std::move(unwrap(generics)),
                                          .span = span},
                 .span = span});
    }

    if (match(lexer::TokenKind::Star)) {
        bool is_mut = false;
        if (match(lexer::TokenKind::KwMut)) {
            is_mut = true;
        } else if (match(lexer::TokenKind::KwConst)) {
            is_mut = false;
        }
        auto inner = parse_type();
        if (is_err(inner))
            return inner;
        auto span = SourceSpan::merge(start_span, unwrap(inner)->span);
        return make_box<Type>(
            Type{.kind = PtrType{.is_mut = is_mut, .inner = std::move(unwrap(inner)), .span = span},
                 .span = span});
    }

    // Tuple or function type: (T, U) or (T) -> R
    if (match(lexer::TokenKind::LParen)) {
        // Parse parameter/element types
        std::vector<TypePtr> types;
        while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
            auto elem = parse_type();
            if (is_err(elem))
                return elem;
            types.push_back(std::move(unwrap(elem)));

            if (!check(lexer::TokenKind::RParen)) {
                auto comma = expect(lexer::TokenKind::Comma, "Expected ',' or ')'");
                if (is_err(comma))
                    return unwrap_err(comma);
            }
        }

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')'");
        if (is_err(rparen))
            return unwrap_err(rparen);

        // Check if this is a function type: (...) -> RetType
        if (match(lexer::TokenKind::Arrow)) {
            auto ret = parse_type();
            if (is_err(ret))
                return ret;

            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Type>(Type{.kind = FuncType{.params = std::move(types),
                                                        .return_type = std::move(unwrap(ret)),
                                                        .span = span},
                                       .span = span});
        }

        // Otherwise it's a tuple type (or single element tuple)
        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Type>(
            Type{.kind = TupleType{.elements = std::move(types), .span = span}, .span = span});
    }

    // Array or slice: [T; N] or [T]
    if (match(lexer::TokenKind::LBracket)) {
        auto elem = parse_type();
        if (is_err(elem))
            return elem;

        if (match(lexer::TokenKind::Semi)) {
            // Array: [T; N]
            auto size = parse_expr();
            if (is_err(size))
                return unwrap_err(size);

            auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']'");
            if (is_err(rbracket))
                return unwrap_err(rbracket);

            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Type>(Type{.kind = ArrayType{.element = std::move(unwrap(elem)),
                                                         .size = std::move(unwrap(size)),
                                                         .span = span},
                                       .span = span});
        }

        // Slice: [T]
        auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']'");
        if (is_err(rbracket))
            return unwrap_err(rbracket);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Type>(Type{
            .kind = SliceType{.element = std::move(unwrap(elem)), .span = span}, .span = span});
    }

    // Infer: _
    if (check(lexer::TokenKind::Identifier) && peek().lexeme == "_") {
        advance();
        return make_box<Type>(Type{.kind = InferType{.span = start_span}, .span = start_span});
    }

    // Function type: func(Params) -> RetType (TML syntax)
    if (match(lexer::TokenKind::KwFunc)) {
        auto lparen = expect(lexer::TokenKind::LParen, "Expected '(' after 'func'");
        if (is_err(lparen))
            return unwrap_err(lparen);

        // Parse parameter types
        std::vector<TypePtr> param_types;
        while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
            auto param_type = parse_type();
            if (is_err(param_type))
                return param_type;
            param_types.push_back(std::move(unwrap(param_type)));

            if (!check(lexer::TokenKind::RParen)) {
                auto comma =
                    expect(lexer::TokenKind::Comma, "Expected ',' or ')' in function type");
                if (is_err(comma))
                    return unwrap_err(comma);
            }
        }

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after function parameters");
        if (is_err(rparen))
            return unwrap_err(rparen);

        // Parse return type (optional, defaults to Unit)
        TypePtr return_type = nullptr;
        if (match(lexer::TokenKind::Arrow)) {
            auto ret = parse_type();
            if (is_err(ret))
                return ret;
            return_type = std::move(unwrap(ret));
        }

        auto end_span = previous().span;
        auto span = SourceSpan::merge(start_span, end_span);
        return make_box<Type>(Type{.kind = FuncType{.params = std::move(param_types),
                                                    .return_type = std::move(return_type),
                                                    .span = span},
                                   .span = span});
    }

    // Function type: Fn(Params) -> RetType (alternative syntax)
    if (check(lexer::TokenKind::Identifier) && peek().lexeme == "Fn") {
        advance(); // consume 'Fn'

        auto lparen = expect(lexer::TokenKind::LParen, "Expected '(' after 'Fn'");
        if (is_err(lparen))
            return unwrap_err(lparen);

        // Parse parameter types
        std::vector<TypePtr> param_types;
        while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
            auto param_type = parse_type();
            if (is_err(param_type))
                return param_type;
            param_types.push_back(std::move(unwrap(param_type)));

            if (!check(lexer::TokenKind::RParen)) {
                auto comma =
                    expect(lexer::TokenKind::Comma, "Expected ',' or ')' in function type");
                if (is_err(comma))
                    return unwrap_err(comma);
            }
        }

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after function parameters");
        if (is_err(rparen))
            return unwrap_err(rparen);

        // Parse return type (optional, defaults to Unit)
        TypePtr return_type = nullptr;
        if (match(lexer::TokenKind::Arrow)) {
            auto ret = parse_type();
            if (is_err(ret))
                return ret;
            return_type = std::move(unwrap(ret));
        }

        auto end_span = previous().span;
        auto span = SourceSpan::merge(start_span, end_span);
        return make_box<Type>(Type{.kind = FuncType{.params = std::move(param_types),
                                                    .return_type = std::move(return_type),
                                                    .span = span},
                                   .span = span});
    }

    // Named type: Ident or Path::To::Type
    auto path = parse_type_path();
    if (is_err(path))
        return unwrap_err(path);

    auto generics = parse_generic_args();
    if (is_err(generics))
        return unwrap_err(generics);

    auto span = unwrap(path).span;
    return make_box<Type>(Type{.kind = NamedType{.path = std::move(unwrap(path)),
                                                 .generics = std::move(unwrap(generics)),
                                                 .span = span},
                               .span = span});
}

auto Parser::parse_type_path() -> Result<TypePath, ParseError> {
    std::vector<std::string> segments;
    auto start_span = peek().span;

    // Accept 'This' keyword as first segment (for This::Item associated type syntax)
    if (match(lexer::TokenKind::KwThisType)) {
        segments.push_back("This");
    } else {
        auto first = expect(lexer::TokenKind::Identifier, "Expected type name");
        if (is_err(first))
            return unwrap_err(first);
        segments.push_back(std::string(unwrap(first).lexeme));
    }

    while (match(lexer::TokenKind::ColonColon)) {
        auto segment = expect(lexer::TokenKind::Identifier, "Expected identifier after '::'");
        if (is_err(segment))
            return unwrap_err(segment);
        segments.push_back(std::string(unwrap(segment).lexeme));
    }

    auto end_span = previous().span;
    return TypePath{.segments = std::move(segments),
                    .span = SourceSpan::merge(start_span, end_span)};
}

auto Parser::parse_generic_args() -> Result<std::optional<GenericArgs>, ParseError> {
    if (!check(lexer::TokenKind::LBracket)) {
        return std::nullopt;
    }

    // Lookahead to distinguish generic args from index expressions
    // Generic args: List[I32], HashMap[K, V] - contain type names (PascalCase)
    // Index expressions: arr[0], arr[i] - contain values/expressions
    // Heuristics:
    //   - Literals (number, string, etc.) -> definitely index
    //   - Identifier starting with lowercase -> likely variable -> index
    //   - Identifier starting with uppercase -> likely type -> generic arg
    auto saved_pos = pos_;
    advance(); // consume '[' for lookahead

    bool is_definitely_index =
        check(lexer::TokenKind::IntLiteral) || check(lexer::TokenKind::FloatLiteral) ||
        check(lexer::TokenKind::StringLiteral) || check(lexer::TokenKind::BoolLiteral) ||
        check(lexer::TokenKind::CharLiteral) || check(lexer::TokenKind::NullLiteral);

    // If it's an identifier, check if it starts with lowercase (likely a variable)
    if (!is_definitely_index && check(lexer::TokenKind::Identifier)) {
        auto lexeme = peek().lexeme;
        if (!lexeme.empty() && lexeme[0] >= 'a' && lexeme[0] <= 'z') {
            // Lowercase identifier -> likely a variable used as index
            is_definitely_index = true;
        }
    }

    pos_ = saved_pos; // restore position

    if (is_definitely_index) {
        return std::nullopt;
    }

    auto start_span = peek().span;
    advance(); // consume '['

    std::vector<GenericArg> args;
    while (!check(lexer::TokenKind::RBracket) && !is_at_end()) {
        auto arg_span = peek().span;

        // Check if this is a const expression (integer literal or identifier that looks like const)
        // For now, support: integer literals, simple identifiers (for const params like N)
        if (check(lexer::TokenKind::IntLiteral)) {
            // Parse as const expression
            auto expr = parse_expr();
            if (is_err(expr))
                return unwrap_err(expr);
            args.push_back(GenericArg::from_const(std::move(unwrap(expr)), arg_span));
        } else if (check(lexer::TokenKind::Identifier)) {
            // Could be:
            // 1. A type name: I32, Maybe[T]
            // 2. An associated type binding: Item=I32
            // Lookahead to check for '='
            auto binding_saved_pos = pos_;
            auto name_token = advance();

            if (check(lexer::TokenKind::Assign)) {
                // This is an associated type binding: Name=Type
                advance(); // consume '='
                auto type_result = parse_type();
                if (is_err(type_result))
                    return unwrap_err(type_result);
                args.push_back(GenericArg::from_binding(std::string(name_token.lexeme),
                                                        std::move(unwrap(type_result)), arg_span));
            } else {
                // Not a binding, restore position and parse as type
                pos_ = binding_saved_pos;
                auto type_result = parse_type();
                if (is_err(type_result))
                    return unwrap_err(type_result);
                args.push_back(GenericArg::from_type(std::move(unwrap(type_result)), arg_span));
            }
        } else {
            // Try to parse as type (handles ref, mut ref, dyn, etc.)
            auto type_result = parse_type();
            if (is_err(type_result))
                return unwrap_err(type_result);
            args.push_back(GenericArg::from_type(std::move(unwrap(type_result)), arg_span));
        }

        if (!check(lexer::TokenKind::RBracket)) {
            auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between type arguments");
            if (is_err(comma))
                return unwrap_err(comma);
        }
    }

    auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']'");
    if (is_err(rbracket))
        return unwrap_err(rbracket);

    return GenericArgs{.args = std::move(args),
                       .span = SourceSpan::merge(start_span, previous().span)};
}

// ============================================================================
// Pattern Parsing
// ============================================================================

} // namespace tml::parser
