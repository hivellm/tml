TML_MODULE("compiler")

//! # Parser - Declarations
//!
//! This file implements declaration parsing.
//!
//! ## Declaration Types
//!
//! | Keyword    | Declaration        | Example                      |
//! |------------|--------------------|------------------------------|
//! | `func`     | Function           | `func add(a: I32) -> I32`    |
//! | `type`     | Struct or Enum     | `type Point { x: I32 }`      |
//! | `type =`   | Type Alias         | `type Int = I32`             |
//! | `behavior` | Trait              | `behavior Display { ... }`   |
//! | `impl`     | Implementation     | `impl Display for Point`     |
//! | `const`    | Constant           | `const PI: F64 = 3.14159`    |
//! | `use`      | Import             | `use std::io::print`         |
//! | `mod`      | Module             | `mod utils`                  |
//!
//! ## Visibility
//!
//! - `pub` - Public (visible outside module)
//! - `pub(crate)` - Crate-visible only
//! - (none) - Private (default)
//!
//! ## Decorators
//!
//! ```tml
//! @inline
//! @extern("C")
//! @link("mylib")
//! func foo() { ... }
//! ```
//!
//! ## Struct vs Enum Detection
//!
//! Both use `type Name { }` syntax. Detection heuristic:
//! - `{ field: Type }` → Struct
//! - `{ Variant | Variant(...) }` → Enum

#include "parser/parser.hpp"

namespace tml::parser {

// ============================================================================
// Declaration Parsing
// ============================================================================

auto Parser::parse_visibility() -> Visibility {
    if (match(lexer::TokenKind::KwPub)) {
        // Check for pub(crate)
        if (check(lexer::TokenKind::LParen)) {
            advance(); // consume '('
            if (check(lexer::TokenKind::Identifier) && peek().lexeme == "crate") {
                advance(); // consume 'crate'
                if (match(lexer::TokenKind::RParen)) {
                    return Visibility::PubCrate;
                }
                // Error: expected ')' after 'crate'
                // Fall through to return Public for error recovery
            }
            // Other visibility modifiers like pub(super) could be added here
            // For now, just return Public for unknown modifiers
            // Skip to closing paren if present
            while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
                advance();
            }
            if (check(lexer::TokenKind::RParen)) {
                advance();
            }
        }
        return Visibility::Public;
    }
    return Visibility::Private;
}

auto Parser::parse_decorators() -> Result<std::vector<Decorator>, ParseError> {
    std::vector<Decorator> decorators;

    while (check(lexer::TokenKind::At)) {
        auto start_span = peek().span;
        advance(); // consume '@'

        // Parse decorator name
        auto name_result =
            expect(lexer::TokenKind::Identifier, "Expected decorator name after '@'");
        if (is_err(name_result))
            return unwrap_err(name_result);
        auto name = std::string(unwrap(name_result).lexeme);

        // Optional arguments in parentheses
        std::vector<ExprPtr> args;
        if (match(lexer::TokenKind::LParen)) {
            skip_newlines();
            while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
                auto arg = parse_expr();
                if (is_err(arg))
                    return unwrap_err(arg);
                args.push_back(std::move(unwrap(arg)));

                skip_newlines();
                if (!check(lexer::TokenKind::RParen)) {
                    auto comma =
                        expect(lexer::TokenKind::Comma, "Expected ',' between decorator arguments");
                    if (is_err(comma))
                        return unwrap_err(comma);
                    skip_newlines();
                }
            }
            auto rparen =
                expect(lexer::TokenKind::RParen, "Expected ')' after decorator arguments");
            if (is_err(rparen))
                return unwrap_err(rparen);
        }

        auto end_span = previous().span;
        decorators.push_back(Decorator{.name = std::move(name),
                                       .args = std::move(args),
                                       .span = SourceSpan::merge(start_span, end_span)});

        skip_newlines();
    }

    return decorators;
}

auto Parser::parse_decl() -> Result<DeclPtr, ParseError> {
    // Collect doc comment while skipping newlines
    auto doc = collect_doc_comment();

    // Parse decorators first
    auto decorators_result = parse_decorators();
    if (is_err(decorators_result))
        return unwrap_err(decorators_result);
    auto decorators = std::move(unwrap(decorators_result));

    auto vis = parse_visibility();

    switch (peek().kind) {
    case lexer::TokenKind::KwAsync:    // async func
    case lexer::TokenKind::KwLowlevel: // lowlevel func (unsafe)
    case lexer::TokenKind::KwFunc:
        return parse_func_decl(vis, std::move(decorators), std::move(doc));
    case lexer::TokenKind::KwType: {
        // Could be struct, enum, or type alias - look ahead
        auto type_pos = pos_;
        advance(); // consume 'type'

        if (peek().kind != lexer::TokenKind::Identifier) {
            return ParseError{.message = "Expected identifier after 'type'",
                              .span = peek().span,
                              .notes = {},
                              .fixes = {},
                              .code = "P022"};
        }

        advance(); // consume name

        // Skip generic params if present
        if (check(lexer::TokenKind::LBracket)) {
            int bracket_depth = 1;
            advance();
            while (bracket_depth > 0 && !is_at_end()) {
                if (check(lexer::TokenKind::LBracket))
                    bracket_depth++;
                else if (check(lexer::TokenKind::RBracket))
                    bracket_depth--;
                advance();
            }
        }

        skip_newlines();

        if (check(lexer::TokenKind::Assign)) {
            // Could be type alias or sum type - look ahead for '|' pattern
            // Sum type: type Foo = Bar | Baz or type Foo = Bar(T) | Baz(U)
            // Type alias: type Foo = SomeType[T]
            advance(); // consume '='
            skip_newlines();

            // Look for '|' at the top level (not inside brackets/parens)
            bool is_sum_type = false;
            int bracket_depth = 0;
            int paren_depth = 0;
            int brace_depth = 0;

            while (!is_at_end() && !check(lexer::TokenKind::Newline) &&
                   !check(lexer::TokenKind::Semi)) {
                if (check(lexer::TokenKind::LBracket))
                    bracket_depth++;
                else if (check(lexer::TokenKind::RBracket))
                    bracket_depth--;
                else if (check(lexer::TokenKind::LParen))
                    paren_depth++;
                else if (check(lexer::TokenKind::RParen))
                    paren_depth--;
                else if (check(lexer::TokenKind::LBrace))
                    brace_depth++;
                else if (check(lexer::TokenKind::RBrace))
                    brace_depth--;
                else if (check(lexer::TokenKind::BitOr) && bracket_depth == 0 && paren_depth == 0 &&
                         brace_depth == 0) {
                    // Found '|' at top level - this is a sum type
                    is_sum_type = true;
                    break;
                }
                advance();
            }

            pos_ = type_pos;

            if (is_sum_type) {
                return parse_sum_type_decl(vis, std::move(decorators), std::move(doc));
            } else {
                return parse_type_alias_decl(vis, std::move(doc));
            }
        }

        if (!check(lexer::TokenKind::LBrace)) {
            pos_ = type_pos;
            return parse_struct_decl(vis, std::move(decorators), std::move(doc));
        }

        // Look inside braces to determine struct vs enum
        // Struct: { field: Type }
        // Enum: { Variant | Variant(...) | Variant { } }
        advance(); // consume '{'
        skip_newlines();

        bool is_enum = false;
        if (check(lexer::TokenKind::Identifier)) {
            advance(); // consume first identifier
            // If followed by '(' or '{' or ',' or '}' or newline, it's an enum
            // If followed by ':', it's a struct
            if (check(lexer::TokenKind::LParen) || check(lexer::TokenKind::LBrace) ||
                check(lexer::TokenKind::Comma) || check(lexer::TokenKind::RBrace) ||
                check(lexer::TokenKind::Newline)) {
                is_enum = true;
            }
        } else if (check(lexer::TokenKind::RBrace)) {
            // Empty braces - could be either, default to struct
            is_enum = false;
        }

        // Reset and parse properly
        pos_ = type_pos;

        if (is_enum) {
            advance(); // consume 'type'
            return parse_enum_decl(vis, std::move(decorators), std::move(doc));
        } else {
            return parse_struct_decl(vis, std::move(decorators), std::move(doc));
        }
    }
    case lexer::TokenKind::KwBehavior:
        return parse_trait_decl(vis, std::move(decorators), std::move(doc));
    case lexer::TokenKind::KwUnion:
        return parse_union_decl(vis, std::move(decorators), std::move(doc));
    case lexer::TokenKind::KwImpl:
        return parse_impl_decl(std::move(doc));
    case lexer::TokenKind::KwConst:
        return parse_const_decl(vis, std::move(doc));
    case lexer::TokenKind::KwUse:
        return parse_use_decl(vis);
    case lexer::TokenKind::KwMod:
        return parse_mod_decl(vis);
    // OOP declarations (C#-style)
    case lexer::TokenKind::KwClass:
    case lexer::TokenKind::KwAbstract:
    case lexer::TokenKind::KwSealed:
        return parse_class_decl(vis, std::move(decorators), std::move(doc));
    case lexer::TokenKind::KwInterface:
        return parse_interface_decl(vis, std::move(decorators), std::move(doc));
    case lexer::TokenKind::KwNamespace:
        return parse_namespace_decl();
    default:
        return ParseError{.message = "Expected declaration",
                          .span = peek().span,
                          .notes = {},
                          .fixes = {},
                          .code = "P001"};
    }
}

auto Parser::parse_func_decl(Visibility vis, std::vector<Decorator> decorators,
                             std::optional<std::string> doc) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // Check for async/unsafe modifiers
    bool is_async = false;
    bool is_unsafe = false;

    while (true) {
        if (match(lexer::TokenKind::KwAsync)) {
            is_async = true;
        } else if (match(lexer::TokenKind::KwLowlevel)) {
            is_unsafe = true;
        } else {
            break;
        }
    }

    auto func_result = expect(lexer::TokenKind::KwFunc, "Expected 'func'");
    if (is_err(func_result))
        return unwrap_err(func_result);

    auto name_result = expect(lexer::TokenKind::Identifier, "Expected function name");
    if (is_err(name_result))
        return unwrap_err(name_result);
    auto name = std::string(unwrap(name_result).lexeme);

    // Generic parameters
    std::vector<GenericParam> generics;
    if (check(lexer::TokenKind::LBracket)) {
        auto gen_result = parse_generic_params();
        if (is_err(gen_result))
            return unwrap_err(gen_result);
        generics = std::move(unwrap(gen_result));
    }

    // Parameters
    if (!check(lexer::TokenKind::LParen)) {
        auto fix = make_insertion_fix(previous().span, "()", "add parameter list");
        return ParseError{
            .message = "Expected '(' after function name",
            .span = peek().span,
            .notes = {"Every function needs a parameter list, even if empty: func name()"},
            .fixes = {fix},
            .code = "P010"};
    }
    advance(); // consume '('

    auto params_result = parse_func_params();
    if (is_err(params_result))
        return unwrap_err(params_result);
    auto params = std::move(unwrap(params_result));

    if (!check(lexer::TokenKind::RParen)) {
        auto fix = make_insertion_fix(previous().span, ")", "add closing parenthesis");
        return ParseError{.message = "Expected ')' after parameters",
                          .span = peek().span,
                          .notes = {},
                          .fixes = {fix},
                          .code = "P017"};
    }
    advance(); // consume ')'

    // Return type
    std::optional<TypePtr> return_type;
    if (match(lexer::TokenKind::Arrow)) {
        auto type_result = parse_type();
        if (is_err(type_result))
            return unwrap_err(type_result);
        return_type = std::move(unwrap(type_result));
    }

    // Where clause (may be on next line)
    skip_newlines();
    std::optional<WhereClause> where_clause;
    auto where_result = parse_where_clause();
    if (is_err(where_result))
        return unwrap_err(where_result);
    where_clause = std::move(unwrap(where_result));

    // Body
    std::optional<BlockExpr> body;
    skip_newlines();
    if (check(lexer::TokenKind::LBrace)) {
        auto body_result = parse_block_expr();
        if (is_err(body_result))
            return unwrap_err(body_result);
        auto& block_expr = unwrap(body_result);
        body = std::move(block_expr->as<BlockExpr>());
    }

    auto end_span = previous().span;

    // Process @extern and @link decorators for FFI
    std::optional<std::string> extern_abi;
    std::optional<std::string> extern_name;
    std::vector<std::string> link_libs;

    for (const auto& dec : decorators) {
        if (dec.name == "extern") {
            // @extern("symbol") - single arg is the extern symbol name
            // @extern("abi", name = "symbol") - explicit abi and symbol name
            if (!dec.args.empty() && dec.args[0]->is<LiteralExpr>()) {
                const auto& lit = dec.args[0]->as<LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                    std::string first_arg = lit.token.string_value().value;

                    // Check if it's a known ABI name
                    bool is_abi = (first_arg == "c" || first_arg == "c++" ||
                                   first_arg == "stdcall" || first_arg == "fastcall" ||
                                   first_arg == "thiscall" || first_arg == "system");

                    if (is_abi || dec.args.size() > 1) {
                        // @extern("c") or @extern("c", name = "...")
                        extern_abi = first_arg;
                    } else {
                        // @extern("symbol_name") - single arg is the symbol name
                        extern_abi = "c"; // Default to C ABI
                        extern_name = first_arg;
                    }
                }
            }
            // Check for name = "symbol" argument (for @extern("abi", name = "symbol"))
            for (size_t i = 1; i < dec.args.size(); ++i) {
                if (dec.args[i]->is<BinaryExpr>()) {
                    const auto& bin = dec.args[i]->as<BinaryExpr>();
                    if (bin.op == BinaryOp::Assign && bin.left->is<IdentExpr>() &&
                        bin.left->as<IdentExpr>().name == "name" && bin.right->is<LiteralExpr>()) {
                        const auto& lit = bin.right->as<LiteralExpr>();
                        if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                            extern_name = lit.token.string_value().value;
                        }
                    }
                }
            }
        } else if (dec.name == "link") {
            // @link("library") or @link("path/to/lib.dll")
            if (!dec.args.empty() && dec.args[0]->is<LiteralExpr>()) {
                const auto& lit = dec.args[0]->as<LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                    link_libs.push_back(lit.token.string_value().value);
                }
            }
        }
    }

    auto func = FuncDecl{.doc = std::move(doc),
                         .decorators = std::move(decorators),
                         .vis = vis,
                         .name = std::move(name),
                         .generics = std::move(generics),
                         .params = std::move(params),
                         .return_type = std::move(return_type),
                         .where_clause = std::move(where_clause),
                         .body = std::move(body),
                         .is_async = is_async,
                         .is_unsafe = is_unsafe,
                         .span = SourceSpan::merge(start_span, end_span),
                         .extern_abi = std::move(extern_abi),
                         .extern_name = std::move(extern_name),
                         .link_libs = std::move(link_libs)};

    return make_box<Decl>(
        Decl{.kind = std::move(func), .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_struct_decl(Visibility vis, std::vector<Decorator> decorators,
                               std::optional<std::string> doc) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    auto type_result = expect(lexer::TokenKind::KwType, "Expected 'type'");
    if (is_err(type_result))
        return unwrap_err(type_result);

    auto name_result = expect(lexer::TokenKind::Identifier, "Expected struct name");
    if (is_err(name_result))
        return unwrap_err(name_result);
    auto name = std::string(unwrap(name_result).lexeme);

    // Generic parameters
    std::vector<GenericParam> generics;
    if (check(lexer::TokenKind::LBracket)) {
        auto gen_result = parse_generic_params();
        if (is_err(gen_result))
            return unwrap_err(gen_result);
        generics = std::move(unwrap(gen_result));
    }

    // Where clause before body
    std::optional<WhereClause> where_clause;
    auto where_result = parse_where_clause();
    if (is_err(where_result))
        return unwrap_err(where_result);
    where_clause = std::move(unwrap(where_result));

    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for struct body");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    // Parse fields
    std::vector<StructField> fields;
    skip_newlines();
    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        // Collect doc comment for field
        auto field_doc = collect_doc_comment();
        auto field_vis = parse_visibility();

        auto field_name_result = expect(lexer::TokenKind::Identifier, "Expected field name");
        if (is_err(field_name_result))
            return unwrap_err(field_name_result);
        auto field_name = std::string(unwrap(field_name_result).lexeme);

        if (!check(lexer::TokenKind::Colon)) {
            auto fix = make_insertion_fix(previous().span, ": Type", "add type annotation");
            return ParseError{.message = "Expected ':' after field name",
                              .span = peek().span,
                              .notes = {"Struct fields require type annotations: field_name: Type"},
                              .fixes = {fix},
                              .code = "P045"};
        }
        advance(); // consume ':'

        auto field_type_result = parse_type();
        if (is_err(field_type_result))
            return unwrap_err(field_type_result);

        // Check for default value: `field: Type = default_expr`
        std::optional<ExprPtr> default_value;
        if (match(lexer::TokenKind::Assign)) {
            auto default_expr_result = parse_expr();
            if (is_err(default_expr_result))
                return unwrap_err(default_expr_result);
            default_value = std::move(unwrap(default_expr_result));
        }

        fields.push_back(StructField{
            .doc = std::move(field_doc),
            .vis = field_vis,
            .name = std::move(field_name),
            .type = std::move(unwrap(field_type_result)),
            .default_value = std::move(default_value),
            .span = SourceSpan::merge(unwrap(field_name_result).span, previous().span)});

        skip_newlines();
        if (!check(lexer::TokenKind::RBrace)) {
            // Optional comma or newline between fields
            match(lexer::TokenKind::Comma);
            skip_newlines();
        }
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after struct fields");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto end_span = previous().span;

    auto struct_decl = StructDecl{.doc = std::move(doc),
                                  .decorators = std::move(decorators),
                                  .vis = vis,
                                  .name = std::move(name),
                                  .generics = std::move(generics),
                                  .fields = std::move(fields),
                                  .where_clause = std::move(where_clause),
                                  .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Decl>(
        Decl{.kind = std::move(struct_decl), .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_union_decl(Visibility vis, std::vector<Decorator> decorators,
                              std::optional<std::string> doc) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    auto union_result = expect(lexer::TokenKind::KwUnion, "Expected 'union'");
    if (is_err(union_result))
        return unwrap_err(union_result);

    auto name_result = expect(lexer::TokenKind::Identifier, "Expected union name");
    if (is_err(name_result))
        return unwrap_err(name_result);
    auto name = std::string(unwrap(name_result).lexeme);

    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for union body");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    // Parse fields (same syntax as struct fields)
    std::vector<StructField> fields;
    skip_newlines();
    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto field_doc = collect_doc_comment();
        auto field_vis = parse_visibility();

        auto field_name_result = expect(lexer::TokenKind::Identifier, "Expected field name");
        if (is_err(field_name_result))
            return unwrap_err(field_name_result);
        auto field_name = std::string(unwrap(field_name_result).lexeme);

        if (!check(lexer::TokenKind::Colon)) {
            auto fix = make_insertion_fix(previous().span, ": Type", "add type annotation");
            return ParseError{.message = "Expected ':' after field name",
                              .span = peek().span,
                              .notes = {"Union fields require type annotations: field_name: Type"},
                              .fixes = {fix},
                              .code = "P045"};
        }
        advance(); // consume ':'

        auto field_type_result = parse_type();
        if (is_err(field_type_result))
            return unwrap_err(field_type_result);

        fields.push_back(StructField{
            .doc = std::move(field_doc),
            .vis = field_vis,
            .name = std::move(field_name),
            .type = std::move(unwrap(field_type_result)),
            .default_value = std::nullopt, // Unions don't support default values
            .span = SourceSpan::merge(unwrap(field_name_result).span, previous().span)});

        skip_newlines();
        if (!check(lexer::TokenKind::RBrace)) {
            match(lexer::TokenKind::Comma);
            skip_newlines();
        }
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after union fields");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto end_span = previous().span;

    auto union_decl = UnionDecl{.doc = std::move(doc),
                                .decorators = std::move(decorators),
                                .vis = vis,
                                .name = std::move(name),
                                .fields = std::move(fields),
                                .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Decl>(
        Decl{.kind = std::move(union_decl), .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_enum_decl(Visibility vis, std::vector<Decorator> decorators,
                             std::optional<std::string> doc) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // 'type' keyword already consumed by parse_decl
    // We're called when parse_decl determines this is an enum

    auto name_result = expect(lexer::TokenKind::Identifier, "Expected enum name");
    if (is_err(name_result))
        return unwrap_err(name_result);
    auto name = std::string(unwrap(name_result).lexeme);

    // Generic parameters
    std::vector<GenericParam> generics;
    if (check(lexer::TokenKind::LBracket)) {
        auto gen_result = parse_generic_params();
        if (is_err(gen_result))
            return unwrap_err(gen_result);
        generics = std::move(unwrap(gen_result));
    }

    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for enum body");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    std::vector<EnumVariant> variants;
    skip_newlines();

    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        // Collect doc comment for variant
        auto variant_doc = collect_doc_comment();

        auto variant_name_result = expect(lexer::TokenKind::Identifier, "Expected variant name");
        if (is_err(variant_name_result))
            return unwrap_err(variant_name_result);
        auto variant_name = std::string(unwrap(variant_name_result).lexeme);
        auto variant_start = unwrap(variant_name_result).span;

        std::optional<std::vector<TypePtr>> tuple_fields;
        std::optional<std::vector<StructField>> struct_fields;

        // Check for tuple variant: Variant(T1, T2)
        if (match(lexer::TokenKind::LParen)) {
            std::vector<TypePtr> fields;
            skip_newlines();

            while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
                auto field_type = parse_type();
                if (is_err(field_type))
                    return unwrap_err(field_type);
                fields.push_back(std::move(unwrap(field_type)));

                skip_newlines();
                if (!check(lexer::TokenKind::RParen)) {
                    auto comma =
                        expect(lexer::TokenKind::Comma, "Expected ',' between tuple fields");
                    if (is_err(comma))
                        return unwrap_err(comma);
                    skip_newlines();
                }
            }

            auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after tuple fields");
            if (is_err(rparen))
                return unwrap_err(rparen);

            tuple_fields = std::move(fields);
        }
        // Check for struct variant: Variant { field: Type }
        else if (match(lexer::TokenKind::LBrace)) {
            std::vector<StructField> fields;
            skip_newlines();

            while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
                // Collect doc comment for struct variant field
                auto field_doc = collect_doc_comment();
                auto field_vis = parse_visibility();

                auto field_name_result =
                    expect(lexer::TokenKind::Identifier, "Expected field name");
                if (is_err(field_name_result))
                    return unwrap_err(field_name_result);
                auto field_name = std::string(unwrap(field_name_result).lexeme);

                auto colon = expect(lexer::TokenKind::Colon, "Expected ':' after field name");
                if (is_err(colon))
                    return unwrap_err(colon);

                auto field_type = parse_type();
                if (is_err(field_type))
                    return unwrap_err(field_type);

                // Check for default value in enum struct variant field
                std::optional<ExprPtr> default_value;
                if (match(lexer::TokenKind::Assign)) {
                    auto default_expr_result = parse_expr();
                    if (is_err(default_expr_result))
                        return unwrap_err(default_expr_result);
                    default_value = std::move(unwrap(default_expr_result));
                }

                fields.push_back(StructField{
                    .doc = std::move(field_doc),
                    .vis = field_vis,
                    .name = std::move(field_name),
                    .type = std::move(unwrap(field_type)),
                    .default_value = std::move(default_value),
                    .span = SourceSpan::merge(unwrap(field_name_result).span, previous().span)});

                skip_newlines();
                if (!check(lexer::TokenKind::RBrace)) {
                    match(lexer::TokenKind::Comma);
                    skip_newlines();
                }
            }

            auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after struct fields");
            if (is_err(rbrace))
                return unwrap_err(rbrace);

            struct_fields = std::move(fields);
        }

        auto variant_end = previous().span;
        variants.push_back(EnumVariant{.doc = std::move(variant_doc),
                                       .name = std::move(variant_name),
                                       .tuple_fields = std::move(tuple_fields),
                                       .struct_fields = std::move(struct_fields),
                                       .span = SourceSpan::merge(variant_start, variant_end)});

        skip_newlines();
        match(lexer::TokenKind::Comma);
        skip_newlines();
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after enum variants");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto end_span = previous().span;

    auto enum_decl = EnumDecl{.doc = std::move(doc),
                              .decorators = std::move(decorators),
                              .vis = vis,
                              .name = std::move(name),
                              .generics = std::move(generics),
                              .variants = std::move(variants),
                              .where_clause = std::nullopt,
                              .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Decl>(
        Decl{.kind = std::move(enum_decl), .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_trait_decl(Visibility vis, std::vector<Decorator> decorators,
                              std::optional<std::string> doc) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // Consume 'behavior' keyword
    auto behavior_tok = expect(lexer::TokenKind::KwBehavior, "Expected 'behavior'");
    if (is_err(behavior_tok))
        return unwrap_err(behavior_tok);

    // Parse name
    auto name_result = expect(lexer::TokenKind::Identifier, "Expected behavior name");
    if (is_err(name_result))
        return unwrap_err(name_result);
    auto name = std::string(unwrap(name_result).lexeme);

    // Generic parameters
    std::vector<GenericParam> generics;
    if (check(lexer::TokenKind::LBracket)) {
        auto gen_result = parse_generic_params();
        if (is_err(gen_result))
            return unwrap_err(gen_result);
        generics = std::move(unwrap(gen_result));
    }

    // Super traits (behavior Foo: Bar + Baz, behavior Foo[T]: Borrow[T])
    std::vector<TypePtr> super_traits;
    if (match(lexer::TokenKind::Colon)) {
        do {
            skip_newlines();
            auto trait_type = parse_type();
            if (is_err(trait_type))
                return unwrap_err(trait_type);
            super_traits.push_back(std::move(unwrap(trait_type)));
            skip_newlines();
        } while (match(lexer::TokenKind::Plus));
    }

    // Parse body
    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for behavior body");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    std::vector<AssociatedType> associated_types;
    std::vector<FuncDecl> methods;
    skip_newlines();

    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        // Parse method signature, default implementation, or associated type
        auto method_vis = parse_visibility();

        // Check for associated type: type Name or type Name: Bounds
        if (check(lexer::TokenKind::KwType)) {
            auto type_span = peek().span;
            advance(); // consume 'type'

            auto type_name_result =
                expect(lexer::TokenKind::Identifier, "Expected associated type name");
            if (is_err(type_name_result))
                return unwrap_err(type_name_result);
            auto type_name = std::string(unwrap(type_name_result).lexeme);

            // Optional GAT generic parameters: type Item[T]
            std::vector<GenericParam> gat_generics;
            if (check(lexer::TokenKind::LBracket)) {
                auto generics_result = parse_generic_params();
                if (is_err(generics_result))
                    return unwrap_err(generics_result);
                gat_generics = std::move(unwrap(generics_result));
            }

            // Optional bounds: type Item: Display + Debug or type Item: Container[I32]
            std::vector<TypePtr> bounds;
            if (match(lexer::TokenKind::Colon)) {
                do {
                    skip_newlines();
                    auto bound_type = parse_type();
                    if (is_err(bound_type))
                        return unwrap_err(bound_type);
                    bounds.push_back(std::move(unwrap(bound_type)));
                    skip_newlines();
                } while (match(lexer::TokenKind::Plus));
            }

            // Optional default type: type Item = I32
            std::optional<TypePtr> default_type = std::nullopt;
            if (match(lexer::TokenKind::Assign)) {
                skip_newlines();
                auto type_result = parse_type();
                if (is_err(type_result))
                    return unwrap_err(type_result);
                default_type = std::move(unwrap(type_result));
            }

            associated_types.push_back(AssociatedType{.name = std::move(type_name),
                                                      .generics = std::move(gat_generics),
                                                      .bounds = std::move(bounds),
                                                      .default_type = std::move(default_type),
                                                      .span = type_span});
        } else {
            auto func_result = parse_func_decl(method_vis);
            if (is_err(func_result))
                return func_result;

            auto& func = unwrap(func_result)->as<FuncDecl>();
            methods.push_back(std::move(func));
        }

        skip_newlines();
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after behavior body");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto end_span = previous().span;

    auto trait_decl = TraitDecl{.doc = std::move(doc),
                                .decorators = std::move(decorators),
                                .vis = vis,
                                .name = std::move(name),
                                .generics = std::move(generics),
                                .super_traits = std::move(super_traits),
                                .associated_types = std::move(associated_types),
                                .methods = std::move(methods),
                                .where_clause = std::nullopt,
                                .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Decl>(
        Decl{.kind = std::move(trait_decl), .span = SourceSpan::merge(start_span, end_span)});
}

// ============================================================================
// Continued in parser_decl_impl.cpp
// ============================================================================
// The following declarations are in parser_decl_impl.cpp:
//   - parse_impl_decl
//   - parse_type_alias_decl, parse_sum_type_decl, parse_sum_type_variant
//   - parse_const_decl, parse_use_decl, parse_mod_decl
//   - parse_generic_params, parse_where_clause
//   - parse_func_params, parse_func_param
// OOP parsing (class, interface, namespace) is in parser_oop.cpp.

} // namespace tml::parser
