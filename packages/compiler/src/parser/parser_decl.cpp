#include "tml/parser/parser.hpp"

namespace tml::parser {

// ============================================================================
// Declaration Parsing
// ============================================================================

auto Parser::parse_visibility() -> Visibility {
    if (match(lexer::TokenKind::KwPub)) {
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
        auto name_result = expect(lexer::TokenKind::Identifier, "Expected decorator name after '@'");
        if (is_err(name_result)) return unwrap_err(name_result);
        auto name = std::string(unwrap(name_result).lexeme);
        
        // Optional arguments in parentheses
        std::vector<ExprPtr> args;
        if (match(lexer::TokenKind::LParen)) {
            skip_newlines();
            while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
                auto arg = parse_expr();
                if (is_err(arg)) return unwrap_err(arg);
                args.push_back(std::move(unwrap(arg)));
                
                skip_newlines();
                if (!check(lexer::TokenKind::RParen)) {
                    auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between decorator arguments");
                    if (is_err(comma)) return unwrap_err(comma);
                    skip_newlines();
                }
            }
            auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after decorator arguments");
            if (is_err(rparen)) return unwrap_err(rparen);
        }
        
        auto end_span = previous().span;
        decorators.push_back(Decorator{
            .name = std::move(name),
            .args = std::move(args),
            .span = SourceSpan::merge(start_span, end_span)
        });
        
        skip_newlines();
    }
    
    return decorators;
}

auto Parser::parse_decl() -> Result<DeclPtr, ParseError> {
    skip_newlines();
    
    // Parse decorators first
    auto decorators_result = parse_decorators();
    if (is_err(decorators_result)) return unwrap_err(decorators_result);
    auto decorators = std::move(unwrap(decorators_result));

    auto vis = parse_visibility();

    switch (peek().kind) {
        case lexer::TokenKind::KwAsync:  // async func
        case lexer::TokenKind::KwLowlevel:  // lowlevel func (unsafe)
        case lexer::TokenKind::KwFunc:
            return parse_func_decl(vis, std::move(decorators));
        case lexer::TokenKind::KwType: {
            // Could be struct, enum, or type alias - look ahead
            auto type_pos = pos_;
            advance(); // consume 'type'
            
            if (peek().kind != lexer::TokenKind::Identifier) {
                return ParseError{
                    .message = "Expected identifier after 'type'",
                    .span = peek().span,
                    .notes = {}
                };
            }
            
            advance(); // consume name
            
            // Skip generic params if present
            if (check(lexer::TokenKind::LBracket)) {
                int bracket_depth = 1;
                advance();
                while (bracket_depth > 0 && !is_at_end()) {
                    if (check(lexer::TokenKind::LBracket)) bracket_depth++;
                    else if (check(lexer::TokenKind::RBracket)) bracket_depth--;
                    advance();
                }
            }
            
            skip_newlines();
            
            if (check(lexer::TokenKind::Assign)) {
                // Type alias: type Foo = Bar
                pos_ = type_pos;
                return parse_type_alias_decl(vis);
            }
            
            if (!check(lexer::TokenKind::LBrace)) {
                pos_ = type_pos;
                return parse_struct_decl(vis, std::move(decorators));
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
                if (check(lexer::TokenKind::LParen) || 
                    check(lexer::TokenKind::LBrace) ||
                    check(lexer::TokenKind::Comma) ||
                    check(lexer::TokenKind::RBrace) ||
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
                return parse_enum_decl(vis, std::move(decorators));
            } else {
                return parse_struct_decl(vis, std::move(decorators));
            }
        }
        case lexer::TokenKind::KwBehavior:
            return parse_trait_decl(vis, std::move(decorators));
        case lexer::TokenKind::KwImpl:
            return parse_impl_decl();
        case lexer::TokenKind::KwConst:
            return parse_const_decl(vis);
        case lexer::TokenKind::KwUse:
            return parse_use_decl(vis);
        case lexer::TokenKind::KwMod:
            return parse_mod_decl(vis);
        default:
            return ParseError{
                .message = "Expected declaration",
                .span = peek().span,
                .notes = {}
            };
    }
}

auto Parser::parse_func_decl(Visibility vis, std::vector<Decorator> decorators) -> Result<DeclPtr, ParseError> {
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
    if (is_err(func_result)) return unwrap_err(func_result);

    auto name_result = expect(lexer::TokenKind::Identifier, "Expected function name");
    if (is_err(name_result)) return unwrap_err(name_result);
    auto name = std::string(unwrap(name_result).lexeme);

    // Generic parameters
    std::vector<GenericParam> generics;
    if (check(lexer::TokenKind::LBracket)) {
        auto gen_result = parse_generic_params();
        if (is_err(gen_result)) return unwrap_err(gen_result);
        generics = std::move(unwrap(gen_result));
    }

    // Parameters
    auto lparen = expect(lexer::TokenKind::LParen, "Expected '(' after function name");
    if (is_err(lparen)) return unwrap_err(lparen);

    auto params_result = parse_func_params();
    if (is_err(params_result)) return unwrap_err(params_result);
    auto params = std::move(unwrap(params_result));

    auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after parameters");
    if (is_err(rparen)) return unwrap_err(rparen);

    // Return type
    std::optional<TypePtr> return_type;
    if (match(lexer::TokenKind::Arrow)) {
        auto type_result = parse_type();
        if (is_err(type_result)) return unwrap_err(type_result);
        return_type = std::move(unwrap(type_result));
    }

    // Where clause
    std::optional<WhereClause> where_clause;
    auto where_result = parse_where_clause();
    if (is_err(where_result)) return unwrap_err(where_result);
    where_clause = std::move(unwrap(where_result));

    // Body
    std::optional<BlockExpr> body;
    skip_newlines();
    if (check(lexer::TokenKind::LBrace)) {
        auto body_result = parse_block_expr();
        if (is_err(body_result)) return unwrap_err(body_result);
        auto& block_expr = unwrap(body_result);
        body = std::move(block_expr->as<BlockExpr>());
    }

    auto end_span = previous().span;

    auto func = FuncDecl{
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
        .span = SourceSpan::merge(start_span, end_span)
    };

    return make_box<Decl>(Decl{
        .kind = std::move(func),
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_struct_decl(Visibility vis, std::vector<Decorator> decorators) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    auto type_result = expect(lexer::TokenKind::KwType, "Expected 'type'");
    if (is_err(type_result)) return unwrap_err(type_result);

    auto name_result = expect(lexer::TokenKind::Identifier, "Expected struct name");
    if (is_err(name_result)) return unwrap_err(name_result);
    auto name = std::string(unwrap(name_result).lexeme);

    // Generic parameters
    std::vector<GenericParam> generics;
    if (check(lexer::TokenKind::LBracket)) {
        auto gen_result = parse_generic_params();
        if (is_err(gen_result)) return unwrap_err(gen_result);
        generics = std::move(unwrap(gen_result));
    }

    // Where clause before body
    std::optional<WhereClause> where_clause;
    auto where_result = parse_where_clause();
    if (is_err(where_result)) return unwrap_err(where_result);
    where_clause = std::move(unwrap(where_result));

    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for struct body");
    if (is_err(lbrace)) return unwrap_err(lbrace);

    // Parse fields
    std::vector<StructField> fields;
    skip_newlines();
    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto field_vis = parse_visibility();

        auto field_name_result = expect(lexer::TokenKind::Identifier, "Expected field name");
        if (is_err(field_name_result)) return unwrap_err(field_name_result);
        auto field_name = std::string(unwrap(field_name_result).lexeme);

        auto colon = expect(lexer::TokenKind::Colon, "Expected ':' after field name");
        if (is_err(colon)) return unwrap_err(colon);

        auto field_type_result = parse_type();
        if (is_err(field_type_result)) return unwrap_err(field_type_result);

        fields.push_back(StructField{
            .vis = field_vis,
            .name = std::move(field_name),
            .type = std::move(unwrap(field_type_result)),
            .span = SourceSpan::merge(unwrap(field_name_result).span, previous().span)
        });

        skip_newlines();
        if (!check(lexer::TokenKind::RBrace)) {
            // Optional comma or newline between fields
            match(lexer::TokenKind::Comma);
            skip_newlines();
        }
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after struct fields");
    if (is_err(rbrace)) return unwrap_err(rbrace);

    auto end_span = previous().span;

    auto struct_decl = StructDecl{
        .decorators = std::move(decorators),
        .vis = vis,
        .name = std::move(name),
        .generics = std::move(generics),
        .fields = std::move(fields),
        .where_clause = std::move(where_clause),
        .span = SourceSpan::merge(start_span, end_span)
    };

    return make_box<Decl>(Decl{
        .kind = std::move(struct_decl),
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_enum_decl(Visibility vis, std::vector<Decorator> decorators) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;
    
    // 'type' keyword already consumed by parse_decl
    // We're called when parse_decl determines this is an enum
    
    auto name_result = expect(lexer::TokenKind::Identifier, "Expected enum name");
    if (is_err(name_result)) return unwrap_err(name_result);
    auto name = std::string(unwrap(name_result).lexeme);
    
    // Generic parameters
    std::vector<GenericParam> generics;
    if (check(lexer::TokenKind::LBracket)) {
        auto gen_result = parse_generic_params();
        if (is_err(gen_result)) return unwrap_err(gen_result);
        generics = std::move(unwrap(gen_result));
    }
    
    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for enum body");
    if (is_err(lbrace)) return unwrap_err(lbrace);
    
    std::vector<EnumVariant> variants;
    skip_newlines();
    
    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto variant_name_result = expect(lexer::TokenKind::Identifier, "Expected variant name");
        if (is_err(variant_name_result)) return unwrap_err(variant_name_result);
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
                if (is_err(field_type)) return unwrap_err(field_type);
                fields.push_back(std::move(unwrap(field_type)));
                
                skip_newlines();
                if (!check(lexer::TokenKind::RParen)) {
                    auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between tuple fields");
                    if (is_err(comma)) return unwrap_err(comma);
                    skip_newlines();
                }
            }
            
            auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after tuple fields");
            if (is_err(rparen)) return unwrap_err(rparen);
            
            tuple_fields = std::move(fields);
        }
        // Check for struct variant: Variant { field: Type }
        else if (match(lexer::TokenKind::LBrace)) {
            std::vector<StructField> fields;
            skip_newlines();
            
            while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
                auto field_vis = parse_visibility();
                
                auto field_name_result = expect(lexer::TokenKind::Identifier, "Expected field name");
                if (is_err(field_name_result)) return unwrap_err(field_name_result);
                auto field_name = std::string(unwrap(field_name_result).lexeme);
                
                auto colon = expect(lexer::TokenKind::Colon, "Expected ':' after field name");
                if (is_err(colon)) return unwrap_err(colon);
                
                auto field_type = parse_type();
                if (is_err(field_type)) return unwrap_err(field_type);
                
                fields.push_back(StructField{
                    .vis = field_vis,
                    .name = std::move(field_name),
                    .type = std::move(unwrap(field_type)),
                    .span = SourceSpan::merge(unwrap(field_name_result).span, previous().span)
                });
                
                skip_newlines();
                if (!check(lexer::TokenKind::RBrace)) {
                    match(lexer::TokenKind::Comma);
                    skip_newlines();
                }
            }
            
            auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after struct fields");
            if (is_err(rbrace)) return unwrap_err(rbrace);
            
            struct_fields = std::move(fields);
        }
        
        auto variant_end = previous().span;
        variants.push_back(EnumVariant{
            .name = std::move(variant_name),
            .tuple_fields = std::move(tuple_fields),
            .struct_fields = std::move(struct_fields),
            .span = SourceSpan::merge(variant_start, variant_end)
        });
        
        skip_newlines();
        match(lexer::TokenKind::Comma);
        skip_newlines();
    }
    
    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after enum variants");
    if (is_err(rbrace)) return unwrap_err(rbrace);
    
    auto end_span = previous().span;
    
    auto enum_decl = EnumDecl{
        .decorators = std::move(decorators),
        .vis = vis,
        .name = std::move(name),
        .generics = std::move(generics),
        .variants = std::move(variants),
        .where_clause = std::nullopt,
        .span = SourceSpan::merge(start_span, end_span)
    };
    
    return make_box<Decl>(Decl{
        .kind = std::move(enum_decl),
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_trait_decl(Visibility vis, std::vector<Decorator> decorators) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;
    
    // Consume 'behavior' keyword
    auto behavior_tok = expect(lexer::TokenKind::KwBehavior, "Expected 'behavior'");
    if (is_err(behavior_tok)) return unwrap_err(behavior_tok);
    
    // Parse name
    auto name_result = expect(lexer::TokenKind::Identifier, "Expected behavior name");
    if (is_err(name_result)) return unwrap_err(name_result);
    auto name = std::string(unwrap(name_result).lexeme);
    
    // Generic parameters
    std::vector<GenericParam> generics;
    if (check(lexer::TokenKind::LBracket)) {
        auto gen_result = parse_generic_params();
        if (is_err(gen_result)) return unwrap_err(gen_result);
        generics = std::move(unwrap(gen_result));
    }
    
    // Super traits (behavior Foo: Bar + Baz)
    std::vector<TypePath> super_traits;
    if (match(lexer::TokenKind::Colon)) {
        do {
            skip_newlines();
            auto trait_path = parse_type_path();
            if (is_err(trait_path)) return unwrap_err(trait_path);
            super_traits.push_back(std::move(unwrap(trait_path)));
            skip_newlines();
        } while (match(lexer::TokenKind::Plus));
    }
    
    // Parse body
    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for behavior body");
    if (is_err(lbrace)) return unwrap_err(lbrace);
    
    std::vector<FuncDecl> methods;
    skip_newlines();
    
    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        // Parse method signature or default implementation
        auto method_vis = parse_visibility();
        
        auto func_result = parse_func_decl(method_vis);
        if (is_err(func_result)) return func_result;
        
        auto& func = unwrap(func_result)->as<FuncDecl>();
        methods.push_back(std::move(func));
        
        skip_newlines();
    }
    
    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after behavior body");
    if (is_err(rbrace)) return unwrap_err(rbrace);
    
    auto end_span = previous().span;
    
    auto trait_decl = TraitDecl{
        .decorators = std::move(decorators),
        .vis = vis,
        .name = std::move(name),
        .generics = std::move(generics),
        .super_traits = std::move(super_traits),
        .methods = std::move(methods),
        .where_clause = std::nullopt,
        .span = SourceSpan::merge(start_span, end_span)
    };
    
    return make_box<Decl>(Decl{
        .kind = std::move(trait_decl),
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_impl_decl() -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;
    
    // Consume 'impl' keyword
    auto impl_tok = expect(lexer::TokenKind::KwImpl, "Expected 'impl'");
    if (is_err(impl_tok)) return unwrap_err(impl_tok);
    
    // Generic parameters
    std::vector<GenericParam> generics;
    if (check(lexer::TokenKind::LBracket)) {
        auto gen_result = parse_generic_params();
        if (is_err(gen_result)) return unwrap_err(gen_result);
        generics = std::move(unwrap(gen_result));
    }
    
    // Parse first type (could be trait or self type)
    auto first_type = parse_type();
    if (is_err(first_type)) return unwrap_err(first_type);
    
    std::optional<TypePath> trait_path;
    TypePtr self_type;
    
    skip_newlines();
    
    // Check for 'for' keyword (impl Trait for Type)
    if (match(lexer::TokenKind::KwFor)) {
        // First type was the trait, now parse self type
        // Extract TypePath from the first type if it's a named type
        auto& first = unwrap(first_type);
        if (first->is<NamedType>()) {
            trait_path = std::move(first->as<NamedType>().path);
        }
        
        auto st = parse_type();
        if (is_err(st)) return unwrap_err(st);
        self_type = std::move(unwrap(st));
    } else {
        // No trait, just impl Type
        self_type = std::move(unwrap(first_type));
    }
    
    // Parse body
    skip_newlines();
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{' for impl body");
    if (is_err(lbrace)) return unwrap_err(lbrace);
    
    std::vector<FuncDecl> methods;
    skip_newlines();
    
    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto method_vis = parse_visibility();
        
        auto func_result = parse_func_decl(method_vis);
        if (is_err(func_result)) return func_result;
        
        auto& func = unwrap(func_result)->as<FuncDecl>();
        methods.push_back(std::move(func));
        
        skip_newlines();
    }
    
    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}' after impl body");
    if (is_err(rbrace)) return unwrap_err(rbrace);
    
    auto end_span = previous().span;
    
    auto impl_decl = ImplDecl{
        .generics = std::move(generics),
        .trait_path = std::move(trait_path),
        .self_type = std::move(self_type),
        .methods = std::move(methods),
        .where_clause = std::nullopt,
        .span = SourceSpan::merge(start_span, end_span)
    };
    
    return make_box<Decl>(Decl{
        .kind = std::move(impl_decl),
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_type_alias_decl(Visibility vis) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    auto type_result = expect(lexer::TokenKind::KwType, "Expected 'type'");
    if (is_err(type_result)) return unwrap_err(type_result);

    auto name_result = expect(lexer::TokenKind::Identifier, "Expected type alias name");
    if (is_err(name_result)) return unwrap_err(name_result);
    auto name = std::string(unwrap(name_result).lexeme);

    // Generic parameters
    std::vector<GenericParam> generics;
    if (check(lexer::TokenKind::LBracket)) {
        auto gen_result = parse_generic_params();
        if (is_err(gen_result)) return unwrap_err(gen_result);
        generics = std::move(unwrap(gen_result));
    }

    auto eq = expect(lexer::TokenKind::Assign, "Expected '=' in type alias");
    if (is_err(eq)) return unwrap_err(eq);

    auto aliased_type = parse_type();
    if (is_err(aliased_type)) return unwrap_err(aliased_type);

    auto end_span = previous().span;

    auto alias = TypeAliasDecl{
        .vis = vis,
        .name = std::move(name),
        .generics = std::move(generics),
        .type = std::move(unwrap(aliased_type)),
        .span = SourceSpan::merge(start_span, end_span)
    };

    return make_box<Decl>(Decl{
        .kind = std::move(alias),
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_const_decl(Visibility vis) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // Consume 'const' keyword
    auto const_tok = expect(lexer::TokenKind::KwConst, "Expected 'const'");
    if (is_err(const_tok)) return unwrap_err(const_tok);

    // Parse identifier name
    auto name_tok = expect(lexer::TokenKind::Identifier, "Expected const name");
    if (is_err(name_tok)) return unwrap_err(name_tok);
    std::string name = std::string(unwrap(name_tok).lexeme);

    // Type annotation is required
    auto colon = expect(lexer::TokenKind::Colon, "Expected ':' and type annotation");
    if (is_err(colon)) return unwrap_err(colon);

    auto type = parse_type();
    if (is_err(type)) return unwrap_err(type);

    // Const must have an initializer
    auto assign = expect(lexer::TokenKind::Assign, "Expected '=' for const initializer");
    if (is_err(assign)) return unwrap_err(assign);

    auto value = parse_expr();
    if (is_err(value)) return unwrap_err(value);

    auto end_span = previous().span;

    return make_box<Decl>(Decl{
        .kind = ConstDecl{
            .vis = vis,
            .name = std::move(name),
            .type = std::move(unwrap(type)),
            .value = std::move(unwrap(value)),
            .span = SourceSpan::merge(start_span, end_span)
        },
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_use_decl(Visibility vis) -> Result<DeclPtr, ParseError> {
    auto start_span = peek().span;

    // Consume 'use' keyword
    auto use_result = expect(lexer::TokenKind::KwUse, "Expected 'use'");
    if (is_err(use_result)) return unwrap_err(use_result);

    // Parse path manually to handle grouped imports: std::time::{Instant, Duration}
    TypePath path;

    // First segment
    auto first = expect(lexer::TokenKind::Identifier, "Expected identifier");
    if (is_err(first)) return unwrap_err(first);
    path.segments.push_back(std::string(unwrap(first).lexeme));
    path.span = unwrap(first).span;

    // Continue parsing path segments
    std::optional<std::vector<std::string>> symbols;
    while (match(lexer::TokenKind::ColonColon)) {
        // Check for grouped imports: {Instant, Duration}
        if (check(lexer::TokenKind::LBrace)) {
            advance(); // consume '{'
            std::vector<std::string> names;

            do {
                auto name_result = expect(lexer::TokenKind::Identifier, "Expected identifier in use group");
                if (is_err(name_result)) return unwrap_err(name_result);
                names.push_back(std::string(unwrap(name_result).lexeme));
            } while (match(lexer::TokenKind::Comma));

            auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}'");
            if (is_err(rbrace)) return unwrap_err(rbrace);

            // Store all symbols for grouped imports
            symbols = std::move(names);
            break;
        }

        auto seg = expect(lexer::TokenKind::Identifier, "Expected identifier after '::'");
        if (is_err(seg)) return unwrap_err(seg);
        path.segments.push_back(std::string(unwrap(seg).lexeme));
        path.span = SourceSpan::merge(path.span, unwrap(seg).span);
    }

    // Optional alias: as Alias
    std::optional<std::string> alias;
    if (match(lexer::TokenKind::KwAs)) {
        auto alias_result = expect(lexer::TokenKind::Identifier, "Expected alias name");
        if (is_err(alias_result)) return unwrap_err(alias_result);
        alias = std::string(unwrap(alias_result).lexeme);
    }

    auto end_span = previous().span;

    auto use = UseDecl{
        .vis = vis,
        .path = std::move(path),
        .alias = alias,
        .symbols = std::move(symbols),
        .span = SourceSpan::merge(start_span, end_span)
    };

    return make_box<Decl>(Decl{
        .kind = std::move(use),
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_mod_decl(Visibility vis) -> Result<DeclPtr, ParseError> {
    (void)vis; // TODO: Will be used when implementing mod parsing
    return ParseError{
        .message = "Mod parsing not yet implemented",
        .span = peek().span,
        .notes = {}
    };
}

// ============================================================================
// Generic Parsing
// ============================================================================

auto Parser::parse_generic_params() -> Result<std::vector<GenericParam>, ParseError> {
    auto lbracket = expect(lexer::TokenKind::LBracket, "Expected '['");
    if (is_err(lbracket)) return unwrap_err(lbracket);

    std::vector<GenericParam> params;

    while (!check(lexer::TokenKind::RBracket) && !is_at_end()) {
        auto name_result = expect(lexer::TokenKind::Identifier, "Expected type parameter name");
        if (is_err(name_result)) return unwrap_err(name_result);
        auto name = std::string(unwrap(name_result).lexeme);
        auto param_span = unwrap(name_result).span;

        std::vector<TypePath> bounds;
        if (match(lexer::TokenKind::Colon)) {
            // Parse bounds: T: Trait + OtherTrait
            do {
                auto bound = parse_type_path();
                if (is_err(bound)) return unwrap_err(bound);
                bounds.push_back(std::move(unwrap(bound)));
            } while (match(lexer::TokenKind::Plus));
        }

        params.push_back(GenericParam{
            .name = std::move(name),
            .bounds = std::move(bounds),
            .span = param_span
        });

        if (!check(lexer::TokenKind::RBracket)) {
            auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between type parameters");
            if (is_err(comma)) return unwrap_err(comma);
        }
    }

    auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']' after type parameters");
    if (is_err(rbracket)) return unwrap_err(rbracket);

    return params;
}

auto Parser::parse_where_clause() -> Result<std::optional<WhereClause>, ParseError> {
    // Check for 'where' keyword
    if (!check(lexer::TokenKind::KwWhere)) {
        return std::nullopt;
    }

    auto start_span = peek().span;
    advance(); // consume 'where'

    std::vector<std::pair<TypePtr, std::vector<TypePath>>> constraints;

    // Parse constraints: T: Trait, U: Trait2, ...
    do {
        // Parse type parameter (e.g., T)
        auto type_result = parse_type();
        if (is_err(type_result)) return unwrap_err(type_result);
        auto type_param = std::move(unwrap(type_result));

        // Expect ':'
        auto colon = expect(lexer::TokenKind::Colon, "Expected ':' after type parameter in where clause");
        if (is_err(colon)) return unwrap_err(colon);

        // Parse trait bounds (e.g., Trait1 or Trait1 + Trait2)
        std::vector<TypePath> bounds;

        do {
            auto path_result = parse_type_path();
            if (is_err(path_result)) return unwrap_err(path_result);
            bounds.push_back(std::move(unwrap(path_result)));

            // Check for '+' to continue parsing bounds
            if (!match(lexer::TokenKind::Plus)) {
                break;
            }
        } while (true);

        constraints.push_back({std::move(type_param), std::move(bounds)});

        // Check for ',' to continue parsing constraints
        if (!match(lexer::TokenKind::Comma)) {
            break;
        }

        skip_newlines();
    } while (!check(lexer::TokenKind::LBrace) && !is_at_end());

    auto end_span = previous().span;

    return std::optional<WhereClause>(WhereClause{
        .constraints = std::move(constraints),
        .span = SourceSpan::merge(start_span, end_span)
    });
}

// ============================================================================
// Function Parameter Parsing
// ============================================================================

auto Parser::parse_func_params() -> Result<std::vector<FuncParam>, ParseError> {
    std::vector<FuncParam> params;

    skip_newlines();
    while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
        auto param = parse_func_param();
        if (is_err(param)) return unwrap_err(param);
        params.push_back(std::move(unwrap(param)));

        skip_newlines();
        if (!check(lexer::TokenKind::RParen)) {
            auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between parameters");
            if (is_err(comma)) return unwrap_err(comma);
            skip_newlines();
        }
    }

    return params;
}

auto Parser::parse_func_param() -> Result<FuncParam, ParseError> {
    auto pattern_result = parse_pattern();
    if (is_err(pattern_result)) return unwrap_err(pattern_result);
    auto pattern = std::move(unwrap(pattern_result));

    // Special case: 'this' parameter doesn't require a type annotation
    bool is_this_param = pattern->is<IdentPattern>() &&
                         pattern->as<IdentPattern>().name == "this";

    if (is_this_param && !check(lexer::TokenKind::Colon)) {
        // 'this' without type - use This type implicitly
        auto span = pattern->span;
        auto this_type = make_box<Type>(Type{
            .kind = NamedType{TypePath{{"This"}, span}, {}, span},
            .span = span
        });
        return FuncParam{
            .pattern = std::move(pattern),
            .type = std::move(this_type),
            .span = span
        };
    }

    auto colon = expect(lexer::TokenKind::Colon, "Expected ':' after parameter name");
    if (is_err(colon)) return unwrap_err(colon);

    auto type_result = parse_type();
    if (is_err(type_result)) return unwrap_err(type_result);
    auto type = std::move(unwrap(type_result));

    auto span = SourceSpan::merge(pattern->span, type->span);

    return FuncParam{
        .pattern = std::move(pattern),
        .type = std::move(type),
        .span = span
    };
}

// ============================================================================
// Statement Parsing

} // namespace tml::parser
