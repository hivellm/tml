//! # Parser - OOP Declarations (C#-style)
//!
//! This file implements parsing for object-oriented programming constructs.
//!
//! ## OOP Declaration Types
//!
//! | Keyword      | Declaration  | Example                              |
//! |--------------|--------------|--------------------------------------|
//! | `class`      | Class        | `class Dog extends Animal { ... }`   |
//! | `interface`  | Interface    | `interface Drawable { ... }`         |
//! | `namespace`  | Namespace    | `namespace MyApp.Core { ... }`       |
//!
//! ## Class Features
//!
//! - Single inheritance (`extends`)
//! - Multiple interface implementation (`implements`)
//! - Fields, methods, properties, constructors
//! - Modifiers: `abstract`, `sealed`, `virtual`, `override`, `static`
//! - Member visibility: `private`, `protected`, `pub`
//!
//! ## Examples
//!
//! ```tml
//! class Animal {
//!     private name: Str
//!
//!     func new(name: Str) { this.name = name }
//!     virtual func speak(this) -> Str { "..." }
//! }
//!
//! class Dog extends Animal implements Friendly {
//!     override func speak(this) -> Str { "Woof!" }
//! }
//!
//! interface Drawable {
//!     func draw(this, canvas: ref Canvas)
//! }
//!
//! namespace MyApp.Core {
//!     class Engine { ... }
//! }
//! ```

#include "parser/parser.hpp"

namespace tml::parser {

// ============================================================================
// Member Visibility
// ============================================================================

auto Parser::parse_member_visibility() -> MemberVisibility {
    if (match(lexer::TokenKind::KwPrivate)) {
        return MemberVisibility::Private;
    }
    if (match(lexer::TokenKind::KwProtected)) {
        return MemberVisibility::Protected;
    }
    if (match(lexer::TokenKind::KwPub)) {
        return MemberVisibility::Public;
    }
    // Default is public for class members
    return MemberVisibility::Public;
}

// ============================================================================
// Class Declaration
// ============================================================================

auto Parser::parse_class_decl(Visibility vis, std::vector<Decorator> decorators,
                              std::optional<std::string> doc) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // Parse modifiers: abstract, sealed
    bool is_abstract = false;
    bool is_sealed = false;

    while (true) {
        if (match(lexer::TokenKind::KwAbstract)) {
            is_abstract = true;
        } else if (match(lexer::TokenKind::KwSealed)) {
            is_sealed = true;
        } else {
            break;
        }
    }

    // Consume 'class' keyword
    auto class_tok = expect(lexer::TokenKind::KwClass, "Expected 'class'");
    if (is_err(class_tok))
        return unwrap_err(class_tok);

    // Parse class name
    auto name_result = expect(lexer::TokenKind::Identifier, "Expected class name");
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

    // Parse extends (single inheritance)
    std::optional<TypePath> extends;
    if (match(lexer::TokenKind::KwExtends)) {
        auto path_result = parse_type_path();
        if (is_err(path_result))
            return unwrap_err(path_result);
        extends = std::move(unwrap(path_result));
    }

    // Parse implements (multiple interfaces, supports generics like IEquatable[T])
    std::vector<Box<Type>> implements;
    if (match(lexer::TokenKind::KwImplements)) {
        do {
            auto type_result = parse_type();
            if (is_err(type_result))
                return unwrap_err(type_result);
            implements.push_back(std::move(unwrap(type_result)));
        } while (match(lexer::TokenKind::Comma));
    }

    // Where clause
    skip_newlines();
    std::optional<WhereClause> where_clause;
    auto where_result = parse_where_clause();
    if (is_err(where_result))
        return unwrap_err(where_result);
    where_clause = std::move(unwrap(where_result));

    // Parse body
    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for class body");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    std::vector<ClassField> fields;
    std::vector<ClassMethod> methods;
    std::vector<PropertyDecl> properties;
    std::vector<ConstructorDecl> constructors;

    skip_newlines();
    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto member_result = parse_class_member(name);
        if (is_err(member_result))
            return unwrap_err(member_result);

        auto& member = unwrap(member_result);
        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ClassField>) {
                    fields.push_back(std::move(arg));
                } else if constexpr (std::is_same_v<T, ClassMethod>) {
                    methods.push_back(std::move(arg));
                } else if constexpr (std::is_same_v<T, PropertyDecl>) {
                    properties.push_back(std::move(arg));
                } else if constexpr (std::is_same_v<T, ConstructorDecl>) {
                    constructors.push_back(std::move(arg));
                }
            },
            member);

        skip_newlines();
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after class body");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto end_span = previous().span;

    auto class_decl = ClassDecl{.doc = std::move(doc),
                                .decorators = std::move(decorators),
                                .vis = vis,
                                .is_abstract = is_abstract,
                                .is_sealed = is_sealed,
                                .name = std::move(name),
                                .generics = std::move(generics),
                                .extends = std::move(extends),
                                .implements = std::move(implements),
                                .fields = std::move(fields),
                                .methods = std::move(methods),
                                .properties = std::move(properties),
                                .constructors = std::move(constructors),
                                .where_clause = std::move(where_clause),
                                .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Decl>(
        Decl{.kind = std::move(class_decl), .span = SourceSpan::merge(start_span, end_span)});
}

// ============================================================================
// Class Member Parsing
// ============================================================================

auto Parser::parse_class_member([[maybe_unused]] const std::string& class_name)
    -> Result<std::variant<ClassField, ClassMethod, PropertyDecl, ConstructorDecl>, ParseError> {

    // Collect doc comment
    auto doc = collect_doc_comment();

    // Parse decorators
    auto decorators_result = parse_decorators();
    if (is_err(decorators_result))
        return unwrap_err(decorators_result);
    auto decorators = std::move(unwrap(decorators_result));

    // Parse visibility
    auto vis = parse_member_visibility();

    // Parse modifiers: static, virtual, override, abstract, sealed (final)
    bool is_static = false;
    bool is_virtual = false;
    bool is_override = false;
    bool is_abstract = false;
    bool is_final = false; // sealed for methods means "cannot be overridden"

    while (true) {
        if (match(lexer::TokenKind::KwStatic)) {
            is_static = true;
        } else if (match(lexer::TokenKind::KwVirtual)) {
            is_virtual = true;
        } else if (match(lexer::TokenKind::KwOverride)) {
            is_override = true;
        } else if (match(lexer::TokenKind::KwAbstract)) {
            is_abstract = true;
        } else if (match(lexer::TokenKind::KwSealed)) {
            is_final = true; // sealed on a method = final (cannot be overridden)
        } else {
            break;
        }
    }

    auto member_start = peek().span;

    // Helper to check if token is identifier "new"
    auto is_new_ident = [](const lexer::Token& tok) -> bool {
        return tok.kind == lexer::TokenKind::Identifier && tok.lexeme == "new";
    };

    // Check for constructor: new(...) or func new(...)
    if (is_new_ident(peek()) || (check(lexer::TokenKind::KwFunc) && is_new_ident(peek_next()))) {
        // Skip optional 'func' keyword
        match(lexer::TokenKind::KwFunc);
        advance(); // consume 'new'

        auto lparen = expect(lexer::TokenKind::LParen, "Expected '(' after 'new'");
        if (is_err(lparen))
            return unwrap_err(lparen);

        auto params_result = parse_func_params();
        if (is_err(params_result))
            return unwrap_err(params_result);
        auto params = std::move(unwrap(params_result));

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after parameters");
        if (is_err(rparen))
            return unwrap_err(rparen);

        // Optional base constructor call: : base(args)
        std::optional<std::vector<ExprPtr>> base_args;
        if (match(lexer::TokenKind::Colon)) {
            auto base_tok = expect(lexer::TokenKind::KwBase, "Expected 'base' after ':'");
            if (is_err(base_tok))
                return unwrap_err(base_tok);

            auto base_lparen = expect(lexer::TokenKind::LParen, "Expected '(' after 'base'");
            if (is_err(base_lparen))
                return unwrap_err(base_lparen);

            auto args_result = parse_call_args();
            if (is_err(args_result))
                return unwrap_err(args_result);
            base_args = std::move(unwrap(args_result));

            auto base_rparen =
                expect(lexer::TokenKind::RParen, "Expected ')' after base arguments");
            if (is_err(base_rparen))
                return unwrap_err(base_rparen);
        }

        // Parse body
        std::optional<BlockExpr> body;
        skip_newlines();
        if (check(lexer::TokenKind::LBrace)) {
            auto body_result = parse_block_expr();
            if (is_err(body_result))
                return unwrap_err(body_result);
            body = std::move(unwrap(body_result)->as<BlockExpr>());
        }

        auto end_span = previous().span;

        return ConstructorDecl{.doc = std::move(doc),
                               .vis = vis,
                               .params = std::move(params),
                               .base_args = std::move(base_args),
                               .body = std::move(body),
                               .span = SourceSpan::merge(member_start, end_span)};
    }

    // Check for property: prop name: Type { get; set; }
    if (check(lexer::TokenKind::KwProp)) {
        advance(); // consume 'prop'

        auto name_result = expect(lexer::TokenKind::Identifier, "Expected property name");
        if (is_err(name_result))
            return unwrap_err(name_result);
        auto prop_name = std::string(unwrap(name_result).lexeme);

        auto colon = expect(lexer::TokenKind::Colon, "Expected ':' after property name");
        if (is_err(colon))
            return unwrap_err(colon);

        auto type_result = parse_type();
        if (is_err(type_result))
            return unwrap_err(type_result);
        auto prop_type = std::move(unwrap(type_result));

        // Parse property body { get; set; } or { get { ... } set { ... } }
        skip_newlines();
        auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for property body");
        if (is_err(lbrace))
            return unwrap_err(lbrace);

        bool has_getter = false;
        bool has_setter = false;
        std::optional<ExprPtr> getter;
        std::optional<ExprPtr> setter;

        skip_newlines();
        while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
            if (check(lexer::TokenKind::Identifier) && peek().lexeme == "get") {
                advance(); // consume 'get'
                has_getter = true;

                if (check(lexer::TokenKind::LBrace)) {
                    auto getter_body = parse_block_expr();
                    if (is_err(getter_body))
                        return unwrap_err(getter_body);
                    getter = std::move(unwrap(getter_body));
                }
            } else if (check(lexer::TokenKind::Identifier) && peek().lexeme == "set") {
                advance(); // consume 'set'
                has_setter = true;

                if (check(lexer::TokenKind::LBrace)) {
                    auto setter_body = parse_block_expr();
                    if (is_err(setter_body))
                        return unwrap_err(setter_body);
                    setter = std::move(unwrap(setter_body));
                }
            } else {
                return ParseError{.message = "Expected 'get' or 'set' in property body",
                                  .span = peek().span,
                                  .notes = {},
                                  .fixes = {}};
            }

            skip_newlines();
        }

        auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after property body");
        if (is_err(rbrace))
            return unwrap_err(rbrace);

        auto end_span = previous().span;

        return PropertyDecl{.doc = std::move(doc),
                            .vis = vis,
                            .is_static = is_static,
                            .name = std::move(prop_name),
                            .type = std::move(prop_type),
                            .getter = std::move(getter),
                            .setter = std::move(setter),
                            .has_getter = has_getter,
                            .has_setter = has_setter,
                            .span = SourceSpan::merge(member_start, end_span)};
    }

    // Check for method: func name(...) -> Type { ... }
    if (check(lexer::TokenKind::KwFunc)) {
        advance(); // consume 'func'

        auto name_result = expect(lexer::TokenKind::Identifier, "Expected method name");
        if (is_err(name_result))
            return unwrap_err(name_result);
        auto method_name = std::string(unwrap(name_result).lexeme);

        // Generic parameters
        std::vector<GenericParam> generics;
        if (check(lexer::TokenKind::LBracket)) {
            auto gen_result = parse_generic_params();
            if (is_err(gen_result))
                return unwrap_err(gen_result);
            generics = std::move(unwrap(gen_result));
        }

        auto lparen = expect(lexer::TokenKind::LParen, "Expected '(' after method name");
        if (is_err(lparen))
            return unwrap_err(lparen);

        auto params_result = parse_func_params();
        if (is_err(params_result))
            return unwrap_err(params_result);
        auto params = std::move(unwrap(params_result));

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after parameters");
        if (is_err(rparen))
            return unwrap_err(rparen);

        // Return type
        std::optional<TypePtr> return_type;
        if (match(lexer::TokenKind::Arrow)) {
            auto type_result = parse_type();
            if (is_err(type_result))
                return unwrap_err(type_result);
            return_type = std::move(unwrap(type_result));
        }

        // Where clause
        skip_newlines();
        std::optional<WhereClause> where_clause;
        auto where_result = parse_where_clause();
        if (is_err(where_result))
            return unwrap_err(where_result);
        where_clause = std::move(unwrap(where_result));

        // Body (optional for abstract methods)
        std::optional<BlockExpr> body;
        skip_newlines();
        if (check(lexer::TokenKind::LBrace)) {
            auto body_result = parse_block_expr();
            if (is_err(body_result))
                return unwrap_err(body_result);
            body = std::move(unwrap(body_result)->as<BlockExpr>());
        }

        auto end_span = previous().span;

        return ClassMethod{.doc = std::move(doc),
                           .decorators = std::move(decorators),
                           .vis = vis,
                           .is_static = is_static,
                           .is_virtual = is_virtual,
                           .is_override = is_override,
                           .is_abstract = is_abstract,
                           .is_final = is_final,
                           .name = std::move(method_name),
                           .generics = std::move(generics),
                           .params = std::move(params),
                           .return_type = std::move(return_type),
                           .where_clause = std::move(where_clause),
                           .body = std::move(body),
                           .span = SourceSpan::merge(member_start, end_span)};
    }

    // Otherwise it's a field: name: Type or name: Type = value
    auto name_result = expect(lexer::TokenKind::Identifier, "Expected field name");
    if (is_err(name_result))
        return unwrap_err(name_result);
    auto field_name = std::string(unwrap(name_result).lexeme);

    auto colon = expect(lexer::TokenKind::Colon, "Expected ':' after field name");
    if (is_err(colon))
        return unwrap_err(colon);

    auto type_result = parse_type();
    if (is_err(type_result))
        return unwrap_err(type_result);
    auto field_type = std::move(unwrap(type_result));

    // Optional initializer
    std::optional<ExprPtr> init;
    if (match(lexer::TokenKind::Assign)) {
        auto init_result = parse_expr();
        if (is_err(init_result))
            return unwrap_err(init_result);
        init = std::move(unwrap(init_result));
    }

    auto end_span = previous().span;

    return ClassField{.doc = std::move(doc),
                      .vis = vis,
                      .is_static = is_static,
                      .name = std::move(field_name),
                      .type = std::move(field_type),
                      .init = std::move(init),
                      .span = SourceSpan::merge(member_start, end_span)};
}

// ============================================================================
// Interface Declaration
// ============================================================================

auto Parser::parse_interface_decl(Visibility vis, std::vector<Decorator> decorators,
                                  std::optional<std::string> doc) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // Consume 'interface' keyword
    auto interface_tok = expect(lexer::TokenKind::KwInterface, "Expected 'interface'");
    if (is_err(interface_tok))
        return unwrap_err(interface_tok);

    // Parse interface name
    auto name_result = expect(lexer::TokenKind::Identifier, "Expected interface name");
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

    // Parse extends (multiple interface inheritance)
    std::vector<TypePath> extends;
    if (match(lexer::TokenKind::KwExtends)) {
        do {
            auto path_result = parse_type_path();
            if (is_err(path_result))
                return unwrap_err(path_result);
            extends.push_back(std::move(unwrap(path_result)));
        } while (match(lexer::TokenKind::Comma));
    }

    // Where clause
    skip_newlines();
    std::optional<WhereClause> where_clause;
    auto where_result = parse_where_clause();
    if (is_err(where_result))
        return unwrap_err(where_result);
    where_clause = std::move(unwrap(where_result));

    // Parse body
    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for interface body");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    std::vector<InterfaceMethod> methods;

    skip_newlines();
    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto method_doc = collect_doc_comment();
        auto method_start = peek().span;

        // Parse static modifier
        bool is_static = match(lexer::TokenKind::KwStatic);

        // Expect 'func' keyword
        auto func_tok = expect(lexer::TokenKind::KwFunc, "Expected 'func' in interface");
        if (is_err(func_tok))
            return unwrap_err(func_tok);

        auto method_name_result = expect(lexer::TokenKind::Identifier, "Expected method name");
        if (is_err(method_name_result))
            return unwrap_err(method_name_result);
        auto method_name = std::string(unwrap(method_name_result).lexeme);

        // Generic parameters
        std::vector<GenericParam> method_generics;
        if (check(lexer::TokenKind::LBracket)) {
            auto gen_result = parse_generic_params();
            if (is_err(gen_result))
                return unwrap_err(gen_result);
            method_generics = std::move(unwrap(gen_result));
        }

        auto lparen = expect(lexer::TokenKind::LParen, "Expected '(' after method name");
        if (is_err(lparen))
            return unwrap_err(lparen);

        auto params_result = parse_func_params();
        if (is_err(params_result))
            return unwrap_err(params_result);
        auto params = std::move(unwrap(params_result));

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after parameters");
        if (is_err(rparen))
            return unwrap_err(rparen);

        // Return type
        std::optional<TypePtr> return_type;
        if (match(lexer::TokenKind::Arrow)) {
            auto type_result = parse_type();
            if (is_err(type_result))
                return unwrap_err(type_result);
            return_type = std::move(unwrap(type_result));
        }

        // Where clause
        skip_newlines();
        std::optional<WhereClause> method_where;
        auto method_where_result = parse_where_clause();
        if (is_err(method_where_result))
            return unwrap_err(method_where_result);
        method_where = std::move(unwrap(method_where_result));

        // Optional default body
        std::optional<BlockExpr> default_body;
        skip_newlines();
        if (check(lexer::TokenKind::LBrace)) {
            auto body_result = parse_block_expr();
            if (is_err(body_result))
                return unwrap_err(body_result);
            default_body = std::move(unwrap(body_result)->as<BlockExpr>());
        }

        auto method_end = previous().span;

        methods.push_back(InterfaceMethod{.doc = std::move(method_doc),
                                          .name = std::move(method_name),
                                          .generics = std::move(method_generics),
                                          .params = std::move(params),
                                          .return_type = std::move(return_type),
                                          .where_clause = std::move(method_where),
                                          .default_body = std::move(default_body),
                                          .is_static = is_static,
                                          .span = SourceSpan::merge(method_start, method_end)});

        skip_newlines();
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after interface body");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto end_span = previous().span;

    auto interface_decl = InterfaceDecl{.doc = std::move(doc),
                                        .decorators = std::move(decorators),
                                        .vis = vis,
                                        .name = std::move(name),
                                        .generics = std::move(generics),
                                        .extends = std::move(extends),
                                        .methods = std::move(methods),
                                        .where_clause = std::move(where_clause),
                                        .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Decl>(
        Decl{.kind = std::move(interface_decl), .span = SourceSpan::merge(start_span, end_span)});
}

// ============================================================================
// Namespace Declaration
// ============================================================================

auto Parser::parse_namespace_decl() -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // Consume 'namespace' keyword
    auto ns_tok = expect(lexer::TokenKind::KwNamespace, "Expected 'namespace'");
    if (is_err(ns_tok))
        return unwrap_err(ns_tok);

    // Parse namespace path: MyApp.Core.Utils
    std::vector<std::string> path;

    auto first = expect(lexer::TokenKind::Identifier, "Expected namespace name");
    if (is_err(first))
        return unwrap_err(first);
    path.push_back(std::string(unwrap(first).lexeme));

    while (match(lexer::TokenKind::Dot)) {
        auto seg = expect(lexer::TokenKind::Identifier, "Expected namespace segment");
        if (is_err(seg))
            return unwrap_err(seg);
        path.push_back(std::string(unwrap(seg).lexeme));
    }

    // Parse body
    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for namespace body");
    if (is_err(lbrace))
        return unwrap_err(lbrace);

    std::vector<DeclPtr> items;

    skip_newlines();
    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto decl_result = parse_decl();
        if (is_err(decl_result))
            return unwrap_err(decl_result);
        items.push_back(std::move(unwrap(decl_result)));
        skip_newlines();
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after namespace body");
    if (is_err(rbrace))
        return unwrap_err(rbrace);

    auto end_span = previous().span;

    auto ns_decl = NamespaceDecl{.path = std::move(path),
                                 .items = std::move(items),
                                 .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Decl>(
        Decl{.kind = std::move(ns_decl), .span = SourceSpan::merge(start_span, end_span)});
}

} // namespace tml::parser
