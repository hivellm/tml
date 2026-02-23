TML_MODULE("compiler")

//! # Parser - Declarations (Impl, Aliases, Generics, Parameters)
//!
//! This file implements the second half of declaration parsing:
//!
//! ## Declaration Types
//!
//! | Keyword    | Declaration        | Example                        |
//! |------------|--------------------|--------------------------------|
//! | `impl`     | Implementation     | `impl Display for Point`       |
//! | `type =`   | Type Alias         | `type Int = I32`               |
//! | `type = |` | Sum Type           | `type Color = Red | Green`     |
//! | `const`    | Constant           | `const PI: F64 = 3.14159`      |
//! | `use`      | Import             | `use std::io::print`           |
//! | `mod`      | Module             | `mod utils`                    |
//!
//! ## Helpers
//!
//! - Generic parameter parsing (`[T, U: Display]`)
//! - Where clause parsing (`where T: Display`)
//! - Function parameter parsing (`a: I32, b: Str`)
//!
//! The first half (func, struct, enum, behavior) is in parser_decl.cpp.

#include "parser/parser.hpp"

namespace tml::parser {

// ============================================================================
// Impl Declarations
// ============================================================================

auto Parser::parse_impl_decl(std::optional<std::string> doc) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // Consume 'impl' keyword
    auto impl_tok = expect(lexer::TokenKind::KwImpl, "Expected 'impl'");
    if (is_err(impl_tok))
        return unwrap_err(impl_tok);

    // Generic parameters
    std::vector<GenericParam> generics;
    if (check(lexer::TokenKind::LBracket)) {
        auto gen_result = parse_generic_params();
        if (is_err(gen_result))
            return unwrap_err(gen_result);
        generics = std::move(unwrap(gen_result));
    }

    // Parse first type (could be trait or self type)
    auto first_type = parse_type();
    if (is_err(first_type))
        return unwrap_err(first_type);

    TypePtr trait_type = nullptr;
    TypePtr self_type;

    skip_newlines();

    // Check for 'for' keyword (impl Trait for Type)
    if (match(lexer::TokenKind::KwFor)) {
        // First type was the trait, now parse self type
        trait_type = std::move(unwrap(first_type));

        auto st = parse_type();
        if (is_err(st))
            return unwrap_err(st);
        self_type = std::move(unwrap(st));
    } else {
        // No trait, just impl Type
        self_type = std::move(unwrap(first_type));
    }

    // Parse optional where clause
    skip_newlines();
    std::optional<WhereClause> where_clause;
    auto where_result = parse_where_clause();
    if (is_err(where_result))
        return unwrap_err(where_result);
    where_clause = std::move(unwrap(where_result));

    // Parse body
    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for impl body");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    std::vector<AssociatedTypeBinding> type_bindings;
    std::vector<ConstDecl> constants;
    std::vector<FuncDecl> methods;
    skip_newlines();

    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        // Parse decorators before visibility (e.g., @allocates pub func ...)
        auto method_decos_result = parse_decorators();
        if (is_err(method_decos_result))
            return unwrap_err(method_decos_result);
        auto method_decorators = std::move(unwrap(method_decos_result));
        auto method_vis = parse_visibility();

        // Check for associated type binding: type Name = ConcreteType or type Name[T] =
        // ConcreteType[T]
        if (check(lexer::TokenKind::KwType)) {
            auto type_span = peek().span;
            advance(); // consume 'type'

            auto type_name_result =
                expect(lexer::TokenKind::Identifier, "Expected associated type name");
            if (is_err(type_name_result))
                return unwrap_err(type_name_result);
            auto type_name = std::string(unwrap(type_name_result).lexeme);

            // Optional GAT generic parameters: type Item[T] = Vec[T]
            std::vector<GenericParam> gat_generics;
            if (check(lexer::TokenKind::LBracket)) {
                auto generics_result = parse_generic_params();
                if (is_err(generics_result))
                    return unwrap_err(generics_result);
                gat_generics = std::move(unwrap(generics_result));
            }

            auto eq_result =
                expect(lexer::TokenKind::Assign, "Expected '=' after associated type name");
            if (is_err(eq_result))
                return unwrap_err(eq_result);

            auto concrete_type = parse_type();
            if (is_err(concrete_type))
                return unwrap_err(concrete_type);

            type_bindings.push_back(AssociatedTypeBinding{.name = std::move(type_name),
                                                          .generics = std::move(gat_generics),
                                                          .type = std::move(unwrap(concrete_type)),
                                                          .span = type_span});
        } else if (check(lexer::TokenKind::KwConst)) {
            // Associated constant: const NAME: Type = value
            auto const_result = parse_const_decl(method_vis);
            if (is_err(const_result))
                return const_result;

            auto& const_decl = unwrap(const_result)->as<ConstDecl>();
            constants.push_back(std::move(const_decl));
        } else {
            auto func_result = parse_func_decl(method_vis, std::move(method_decorators));
            if (is_err(func_result))
                return func_result;

            auto& func = unwrap(func_result)->as<FuncDecl>();
            methods.push_back(std::move(func));
        }

        skip_newlines();
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after impl body");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto end_span = previous().span;

    auto impl_decl = ImplDecl{.doc = std::move(doc),
                              .generics = std::move(generics),
                              .trait_type = std::move(trait_type),
                              .self_type = std::move(self_type),
                              .type_bindings = std::move(type_bindings),
                              .constants = std::move(constants),
                              .methods = std::move(methods),
                              .where_clause = std::move(where_clause),
                              .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Decl>(
        Decl{.kind = std::move(impl_decl), .span = SourceSpan::merge(start_span, end_span)});
}

// ============================================================================
// Type Alias and Sum Type Declarations
// ============================================================================

auto Parser::parse_type_alias_decl(Visibility vis, std::optional<std::string> doc)
    -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    auto type_result = expect(lexer::TokenKind::KwType, "Expected 'type'");
    if (is_err(type_result))
        return unwrap_err(type_result);

    auto name_result = expect(lexer::TokenKind::Identifier, "Expected type alias name");
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

    auto eq = expect(lexer::TokenKind::Assign, "Expected '=' in type alias");
    if (is_err(eq))
        return unwrap_err(eq);

    auto aliased_type = parse_type();
    if (is_err(aliased_type))
        return unwrap_err(aliased_type);

    auto end_span = previous().span;

    auto alias = TypeAliasDecl{.doc = std::move(doc),
                               .vis = vis,
                               .name = std::move(name),
                               .generics = std::move(generics),
                               .type = std::move(unwrap(aliased_type)),
                               .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Decl>(
        Decl{.kind = std::move(alias), .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_sum_type_decl(Visibility vis, std::vector<Decorator> decorators,
                                 std::optional<std::string> doc) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    auto type_result = expect(lexer::TokenKind::KwType, "Expected 'type'");
    if (is_err(type_result))
        return unwrap_err(type_result);

    auto name_result = expect(lexer::TokenKind::Identifier, "Expected type name");
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
    auto eq = expect(lexer::TokenKind::Assign, "Expected '=' in sum type");
    if (is_err(eq))
        return unwrap_err(eq);

    // Parse variants separated by '|'
    std::vector<EnumVariant> variants;
    skip_newlines();

    // Handle optional leading '|' (for multiline format like: type Foo = | V1 | V2)
    match(lexer::TokenKind::BitOr);
    skip_newlines();

    // Parse first variant (must exist)
    auto first_variant = parse_sum_type_variant();
    if (is_err(first_variant))
        return unwrap_err(first_variant);
    variants.push_back(std::move(unwrap(first_variant)));

    // Parse additional variants after '|'
    skip_newlines();
    while (match(lexer::TokenKind::BitOr)) {
        skip_newlines();
        auto variant = parse_sum_type_variant();
        if (is_err(variant))
            return unwrap_err(variant);
        variants.push_back(std::move(unwrap(variant)));
        skip_newlines();
    }

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

auto Parser::parse_sum_type_variant() -> Result<EnumVariant, ParseError> {
    auto start_span = peek().span;

    auto name_result = expect(lexer::TokenKind::Identifier, "Expected variant name");
    if (is_err(name_result))
        return unwrap_err(name_result);
    auto variant_name = std::string(unwrap(name_result).lexeme);

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
                auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between tuple fields");
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
            auto field_doc = collect_doc_comment();
            auto field_vis = parse_visibility();

            auto field_name_result = expect(lexer::TokenKind::Identifier, "Expected field name");
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
    return EnumVariant{.doc = std::nullopt,
                       .name = std::move(variant_name),
                       .tuple_fields = std::move(tuple_fields),
                       .struct_fields = std::move(struct_fields),
                       .span = SourceSpan::merge(start_span, variant_end)};
}

// ============================================================================
// Const, Use, and Mod Declarations
// ============================================================================

auto Parser::parse_const_decl(Visibility vis, std::optional<std::string> doc)
    -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // Consume 'const' keyword
    auto const_tok = expect(lexer::TokenKind::KwConst, "Expected 'const'");
    if (is_err(const_tok))
        return unwrap_err(const_tok);

    // Parse identifier name
    auto name_tok = expect(lexer::TokenKind::Identifier, "Expected const name");
    if (is_err(name_tok))
        return unwrap_err(name_tok);
    std::string name = std::string(unwrap(name_tok).lexeme);

    // Type annotation is required
    auto colon = expect(lexer::TokenKind::Colon, "Expected ':' and type annotation");
    if (is_err(colon))
        return unwrap_err(colon);

    auto type = parse_type();
    if (is_err(type))
        return unwrap_err(type);

    // Const must have an initializer
    auto assign = expect(lexer::TokenKind::Assign, "Expected '=' for const initializer");
    if (is_err(assign))
        return unwrap_err(assign);

    auto value = parse_expr();
    if (is_err(value))
        return unwrap_err(value);

    auto end_span = previous().span;

    return make_box<Decl>(Decl{.kind = ConstDecl{.doc = std::move(doc),
                                                 .vis = vis,
                                                 .name = std::move(name),
                                                 .type = std::move(unwrap(type)),
                                                 .value = std::move(unwrap(value)),
                                                 .span = SourceSpan::merge(start_span, end_span)},
                               .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_use_decl(Visibility vis) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // Consume 'use' keyword
    auto use_result = expect(lexer::TokenKind::KwUse, "Expected 'use'");
    if (is_err(use_result))
        return unwrap_err(use_result);

    // Parse path manually to handle grouped imports: std::time::{Instant, Duration}
    // and glob imports: std::time::*
    TypePath path;

    // First segment - could be identifier, 'super', or 'self'
    if (check(lexer::TokenKind::KwSuper)) {
        auto super_tok = advance();
        path.segments.push_back("super");
        path.span = super_tok.span;
    } else if (check(lexer::TokenKind::KwThis)) {
        // 'self' module reference (using KwThis as 'self')
        auto self_tok = advance();
        path.segments.push_back("self");
        path.span = self_tok.span;
    } else {
        auto first = expect(lexer::TokenKind::Identifier, "Expected identifier");
        if (is_err(first))
            return unwrap_err(first);
        path.segments.push_back(std::string(unwrap(first).lexeme));
        path.span = unwrap(first).span;
    }

    // Continue parsing path segments
    // Support both :: (module style) and . (namespace style) separators
    std::optional<std::vector<std::string>> symbols;
    bool is_glob = false;

    while (match(lexer::TokenKind::ColonColon) || match(lexer::TokenKind::Dot)) {
        // Check for glob import: *
        if (match(lexer::TokenKind::Star)) {
            is_glob = true;
            break;
        }

        // Check for grouped imports: {Instant, Duration}
        if (check(lexer::TokenKind::LBrace)) {
            advance(); // consume '{'
            std::vector<std::string> names;
            skip_newlines();

            while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
                auto name_result =
                    expect(lexer::TokenKind::Identifier, "Expected identifier in use group");
                if (is_err(name_result))
                    return unwrap_err(name_result);
                names.push_back(std::string(unwrap(name_result).lexeme));

                skip_newlines();
                if (!check(lexer::TokenKind::RBrace)) {
                    if (!match(lexer::TokenKind::Comma)) {
                        break;
                    }
                    skip_newlines();
                }
            }

            auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}'");
            if (is_err(rbrace))
                return unwrap_err(rbrace);

            // Store all symbols for grouped imports
            symbols = std::move(names);
            break;
        }

        auto seg = expect(lexer::TokenKind::Identifier, "Expected identifier after path separator");
        if (is_err(seg))
            return unwrap_err(seg);
        path.segments.push_back(std::string(unwrap(seg).lexeme));
        path.span = SourceSpan::merge(path.span, unwrap(seg).span);
    }

    // Optional alias: as Alias (not valid for glob imports)
    std::optional<std::string> alias;
    if (!is_glob && match(lexer::TokenKind::KwAs)) {
        auto alias_result = expect(lexer::TokenKind::Identifier, "Expected alias name");
        if (is_err(alias_result))
            return unwrap_err(alias_result);
        alias = std::string(unwrap(alias_result).lexeme);
    }

    auto end_span = previous().span;

    auto use = UseDecl{.vis = vis,
                       .path = std::move(path),
                       .alias = alias,
                       .symbols = std::move(symbols),
                       .is_glob = is_glob,
                       .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Decl>(
        Decl{.kind = std::move(use), .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_mod_decl(Visibility vis) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // Consume 'mod' keyword
    advance(); // consume 'mod'

    // Parse module name
    if (!check(lexer::TokenKind::Identifier)) {
        return ParseError{.message = "Expected module name after 'mod'",
                          .span = peek().span,
                          .notes = {},
                          .fixes = {},
                          .code = "P026"};
    }
    std::string name = std::string(advance().lexeme);

    // For now, only support external module references (mod foo;)
    // Inline modules (mod foo { ... }) not yet supported
    auto end_span = previous().span;

    ModDecl mod_decl{.vis = vis,
                     .name = std::move(name),
                     .items = std::nullopt, // External module reference
                     .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Decl>(
        Decl{.kind = std::move(mod_decl), .span = SourceSpan::merge(start_span, end_span)});
}

// ============================================================================
// Generic Parsing
// ============================================================================

auto Parser::parse_generic_params() -> Result<std::vector<GenericParam>, ParseError> {
    auto lbracket = expect(lexer::TokenKind::LBracket, "Expected '['");
    if (is_err(lbracket))
        return unwrap_err(lbracket);

    std::vector<GenericParam> params;

    while (!check(lexer::TokenKind::RBracket) && !is_at_end()) {
        auto param_span = peek().span;
        bool is_const = false;
        bool is_lifetime = false;
        std::optional<TypePtr> const_type = std::nullopt;

        // Check for const generic: const N: U64
        if (match(lexer::TokenKind::KwConst)) {
            is_const = true;
        }
        // Check for lifetime parameter: life a, life static
        else if (match(lexer::TokenKind::KwLife)) {
            is_lifetime = true;
        }

        std::string name;
        if (is_lifetime && check(lexer::TokenKind::KwStatic)) {
            // Special case: life static
            advance();
            name = "static";
        } else {
            auto name_result = expect(lexer::TokenKind::Identifier, "Expected type parameter name");
            if (is_err(name_result))
                return unwrap_err(name_result);
            name = std::string(unwrap(name_result).lexeme);
        }

        std::vector<TypePtr> bounds;
        std::optional<TypePtr> default_type = std::nullopt;

        std::optional<std::string> lifetime_bound = std::nullopt;

        if (match(lexer::TokenKind::Colon)) {
            if (is_const) {
                // For const generics, parse the type: const N: U64
                auto type_result = parse_type();
                if (is_err(type_result))
                    return unwrap_err(type_result);
                const_type = std::move(unwrap(type_result));
            } else if (is_lifetime) {
                // Lifetime bounds not yet supported - skip for now
                // Future: life a: b (a outlives b)
            } else {
                // Parse bounds: T: Trait + OtherTrait or T: life static or T: Trait + life a
                // Bounds can be behavior bounds (types) or lifetime bounds (life keyword)
                do {
                    // Check for lifetime bound: T: life static or T: life a
                    if (check(lexer::TokenKind::KwLife)) {
                        advance(); // consume 'life'
                        if (check(lexer::TokenKind::KwStatic)) {
                            advance(); // consume 'static'
                            lifetime_bound = "static";
                        } else if (check(lexer::TokenKind::Identifier)) {
                            lifetime_bound = std::string(peek().lexeme);
                            advance(); // consume lifetime name
                        } else {
                            return ParseError{
                                "Expected lifetime name after 'life'", peek().span, {}, {}, "P056"};
                        }
                    } else {
                        // Regular behavior bound
                        auto bound = parse_type();
                        if (is_err(bound))
                            return unwrap_err(bound);
                        bounds.push_back(std::move(unwrap(bound)));
                    }
                } while (match(lexer::TokenKind::Plus));
            }
        }

        // Parse default type: T = DefaultType (not for lifetimes)
        if (!is_const && !is_lifetime && match(lexer::TokenKind::Assign)) {
            auto type_result = parse_type();
            if (is_err(type_result))
                return unwrap_err(type_result);
            default_type = std::move(unwrap(type_result));
        }

        params.push_back(GenericParam{.name = std::move(name),
                                      .bounds = std::move(bounds),
                                      .is_const = is_const,
                                      .is_lifetime = is_lifetime,
                                      .const_type = std::move(const_type),
                                      .default_type = std::move(default_type),
                                      .lifetime_bound = std::move(lifetime_bound),
                                      .span = param_span});

        if (!check(lexer::TokenKind::RBracket)) {
            auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between type parameters");
            if (is_err(comma))
                return unwrap_err(comma);
        }
    }

    auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']' after type parameters");
    if (is_err(rbracket))
        return unwrap_err(rbracket);

    return params;
}

auto Parser::parse_where_clause() -> Result<std::optional<WhereClause>, ParseError> {
    // Check for 'where' keyword
    if (!check(lexer::TokenKind::KwWhere)) {
        return std::nullopt;
    }

    auto start_span = peek().span;
    advance(); // consume 'where'

    std::vector<std::pair<TypePtr, std::vector<TypePtr>>> constraints;
    std::vector<std::pair<TypePtr, TypePtr>> type_equalities;

    // Parse constraints: T: Trait, U: Trait2, T = U, T: Trait[A, B], ...
    do {
        // Parse type parameter (e.g., T)
        auto type_result = parse_type();
        if (is_err(type_result))
            return unwrap_err(type_result);
        auto type_param = std::move(unwrap(type_result));

        // Check for ':' (trait bound) or '=' (type equality)
        if (match(lexer::TokenKind::Colon)) {
            // Parse trait bounds (e.g., Trait1 or Trait1 + Trait2 or Trait[A, B])
            std::vector<TypePtr> bounds;

            do {
                // Parse trait bound as a type to support generic arguments
                auto bound_result = parse_type();
                if (is_err(bound_result))
                    return unwrap_err(bound_result);
                bounds.push_back(std::move(unwrap(bound_result)));

                // Check for '+' to continue parsing bounds
                if (!match(lexer::TokenKind::Plus)) {
                    break;
                }
            } while (true);

            constraints.push_back({std::move(type_param), std::move(bounds)});
        } else if (match(lexer::TokenKind::Assign)) {
            // Parse type equality: T = U
            auto rhs_result = parse_type();
            if (is_err(rhs_result))
                return unwrap_err(rhs_result);
            auto rhs_type = std::move(unwrap(rhs_result));

            type_equalities.push_back({std::move(type_param), std::move(rhs_type)});
        } else {
            return ParseError{"Expected ':' or '=' after type parameter in where clause",
                              peek().span,
                              {},
                              {},
                              "P032"};
        }

        // Check for ',' to continue parsing constraints
        if (!match(lexer::TokenKind::Comma)) {
            break;
        }

        skip_newlines();
    } while (!check(lexer::TokenKind::LBrace) && !is_at_end());

    auto end_span = previous().span;

    return std::optional<WhereClause>(WhereClause{.constraints = std::move(constraints),
                                                  .type_equalities = std::move(type_equalities),
                                                  .span = SourceSpan::merge(start_span, end_span)});
}

// ============================================================================
// Function Parameter Parsing
// ============================================================================

auto Parser::parse_func_params() -> Result<std::vector<FuncParam>, ParseError> {
    std::vector<FuncParam> params;

    skip_newlines();
    while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
        auto param = parse_func_param();
        if (is_err(param))
            return unwrap_err(param);
        params.push_back(std::move(unwrap(param)));

        skip_newlines();
        if (!check(lexer::TokenKind::RParen)) {
            auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between parameters");
            if (is_err(comma))
                return unwrap_err(comma);
            skip_newlines();
        }
    }

    return params;
}

auto Parser::parse_func_param() -> Result<FuncParam, ParseError> {
    auto pattern_result = parse_pattern();
    if (is_err(pattern_result))
        return unwrap_err(pattern_result);
    auto pattern = std::move(unwrap(pattern_result));

    // Special case: 'this'/'self' or 'mut this'/'mut self' parameter doesn't require a type
    // annotation Note: 'self' is accepted as an alias for 'this' (Rust compatibility)
    bool is_this_param =
        pattern->is<IdentPattern>() &&
        (pattern->as<IdentPattern>().name == "this" || pattern->as<IdentPattern>().name == "self");

    if (is_this_param && !check(lexer::TokenKind::Colon)) {
        auto span = pattern->span;
        bool is_mut_this = pattern->as<IdentPattern>().is_mut;

        if (is_mut_this) {
            // 'mut this' without type - use 'mut ref This' type implicitly
            auto this_named = make_box<Type>(
                Type{.kind = NamedType{TypePath{{"This"}, span}, {}, span}, .span = span});
            auto this_type = make_box<Type>(Type{.kind = RefType{.is_mut = true,
                                                                 .inner = std::move(this_named),
                                                                 .lifetime = std::nullopt,
                                                                 .span = span},
                                                 .span = span});
            return FuncParam{
                .pattern = std::move(pattern), .type = std::move(this_type), .span = span};
        } else {
            // 'this' without type - use This type implicitly (immutable, passed by value/ref)
            auto this_type = make_box<Type>(
                Type{.kind = NamedType{TypePath{{"This"}, span}, {}, span}, .span = span});
            return FuncParam{
                .pattern = std::move(pattern), .type = std::move(this_type), .span = span};
        }
    }

    auto colon = expect(lexer::TokenKind::Colon, "Expected ':' after parameter name");
    if (is_err(colon))
        return unwrap_err(colon);

    auto type_result = parse_type();
    if (is_err(type_result))
        return unwrap_err(type_result);
    auto type = std::move(unwrap(type_result));

    auto span = SourceSpan::merge(pattern->span, type->span);

    return FuncParam{.pattern = std::move(pattern), .type = std::move(type), .span = span};
}

} // namespace tml::parser
