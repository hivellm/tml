#include "tml/parser/parser.hpp"
#include <unordered_map>

namespace tml::parser {

// Operator precedence levels (higher = tighter binding)
namespace precedence {
    constexpr int NONE = 0;
    constexpr int ASSIGN = 1;      // =, +=, etc.
    constexpr int OR = 2;          // ||
    constexpr int AND = 3;         // &&
    constexpr int COMPARISON = 4;  // ==, !=, <, >, <=, >=
    constexpr int BITOR = 5;       // |
    constexpr int BITXOR = 6;      // ^
    constexpr int BITAND = 7;      // &
    constexpr int SHIFT = 8;       // <<, >>
    constexpr int TERM = 9;        // +, -
    constexpr int FACTOR = 10;     // *, /, %
    constexpr int UNARY = 11;      // -, !, ~, &, *
    constexpr int CALL = 12;       // (), [], .
    constexpr int RANGE = 13;      // .., ..=
}

Parser::Parser(std::vector<lexer::Token> tokens)
    : tokens_(std::move(tokens)) {}

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
    return ParseError{
        .message = message + ", found '" + std::string(lexer::token_kind_to_string(peek().kind)) + "'",
        .span = peek().span,
        .notes = {}
    };
}

void Parser::skip_newlines() {
    while (check(lexer::TokenKind::Newline)) {
        advance();
    }
}

void Parser::report_error(const std::string& message) {
    report_error(message, peek().span);
}

void Parser::report_error(const std::string& message, SourceSpan span) {
    errors_.push_back(ParseError{
        .message = message,
        .span = span,
        .notes = {}
    });
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

// ============================================================================
// Module Parsing
// ============================================================================

auto Parser::parse_module(const std::string& name) -> Result<Module, std::vector<ParseError>> {
    std::vector<DeclPtr> decls;
    auto start_span = peek().span;

    skip_newlines();

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
    return Module{
        .name = name,
        .decls = std::move(decls),
        .span = SourceSpan::merge(start_span, end_span)
    };
}

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
    (void)vis; // TODO: Will be used when implementing const parsing
    return ParseError{
        .message = "Const parsing not yet implemented",
        .span = peek().span,
        .notes = {}
    };
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

            // For grouped imports, append first name to base path
            // TODO: Return multiple UseDecl for each name
            if (!names.empty()) {
                path.segments.push_back(names[0]);
            }
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
    // TML doesn't use where clauses - bounds are specified inline with generics
    return std::nullopt;
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
            .kind = NamedType{TypePath{{"This"}, span}, {}},
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
// ============================================================================

auto Parser::parse_stmt() -> Result<StmtPtr, ParseError> {
    skip_newlines();

    if (check(lexer::TokenKind::KwLet)) {
        return parse_let_stmt();
    }

    // Check if it's a declaration
    if (check(lexer::TokenKind::KwPub) ||
        check(lexer::TokenKind::KwFunc) ||
        check(lexer::TokenKind::KwType) ||
        check(lexer::TokenKind::KwBehavior) ||
        check(lexer::TokenKind::KwImpl)) {
        auto decl = parse_decl();
        if (is_err(decl)) return unwrap_err(decl);
        auto span = unwrap(decl)->span;
        return make_box<Stmt>(Stmt{
            .kind = std::move(unwrap(decl)),
            .span = span
        });
    }

    return parse_expr_stmt();
}

auto Parser::parse_let_stmt() -> Result<StmtPtr, ParseError> {
    auto start_span = peek().span;

    auto let_tok = expect(lexer::TokenKind::KwLet, "Expected 'let'");
    if (is_err(let_tok)) return unwrap_err(let_tok);

    auto pattern = parse_pattern();
    if (is_err(pattern)) return unwrap_err(pattern);

    // Type annotation is REQUIRED in TML (explicit typing for LLM clarity)
    auto colon = expect(lexer::TokenKind::Colon, "Expected ':' and type annotation after variable name (TML requires explicit types)");
    if (is_err(colon)) return unwrap_err(colon);

    auto type = parse_type();
    if (is_err(type)) return unwrap_err(type);
    std::optional<TypePtr> type_annotation = std::move(unwrap(type));

    std::optional<ExprPtr> init;
    if (match(lexer::TokenKind::Assign)) {
        auto expr = parse_expr();
        if (is_err(expr)) return unwrap_err(expr);
        init = std::move(unwrap(expr));
    }

    auto end_span = previous().span;

    auto let_stmt = LetStmt{
        .pattern = std::move(unwrap(pattern)),
        .type_annotation = std::move(type_annotation),
        .init = std::move(init),
        .span = SourceSpan::merge(start_span, end_span)
    };

    return make_box<Stmt>(Stmt{
        .kind = std::move(let_stmt),
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_var_stmt() -> Result<StmtPtr, ParseError> {
    // TML doesn't have 'var' keyword - use 'let mut' for mutable bindings
    return ParseError{
        .message = "TML doesn't have 'var' keyword - use 'let mut' for mutable bindings",
        .span = peek().span,
        .notes = {}
    };
}

auto Parser::parse_expr_stmt() -> Result<StmtPtr, ParseError> {
    auto expr = parse_expr();
    if (is_err(expr)) return unwrap_err(expr);

    auto span = unwrap(expr)->span;

    return make_box<Stmt>(Stmt{
        .kind = ExprStmt{
            .expr = std::move(unwrap(expr)),
            .span = span
        },
        .span = span
    });
}

// ============================================================================
// Expression Parsing (Pratt Parser)
// ============================================================================

auto Parser::parse_expr() -> Result<ExprPtr, ParseError> {
    return parse_expr_with_precedence(precedence::NONE);
}

auto Parser::parse_expr_with_precedence(int min_precedence) -> Result<ExprPtr, ParseError> {
    auto left = parse_prefix_expr();
    if (is_err(left)) return left;

    while (true) {
        auto prec = get_precedence(peek().kind);
        if (prec <= min_precedence) {
            break;
        }

        // Handle postfix operators first
        if (check(lexer::TokenKind::LParen) ||
            check(lexer::TokenKind::LBracket) ||
            check(lexer::TokenKind::Dot) ||
            check(lexer::TokenKind::Bang) ||
            check(lexer::TokenKind::PlusPlus) ||
            check(lexer::TokenKind::MinusMinus)) {
            left = parse_postfix_expr(std::move(unwrap(left)));
            if (is_err(left)) return left;
            continue;
        }

        // Handle range expressions: x to y, x through y, x..y
        if (check(lexer::TokenKind::KwTo) ||
            check(lexer::TokenKind::KwThrough) ||
            check(lexer::TokenKind::DotDot)) {
            bool inclusive = check(lexer::TokenKind::KwThrough);
            auto op_span = advance().span; // consume 'to', 'through', or '..'

            auto end_expr = parse_expr_with_precedence(prec);
            if (is_err(end_expr)) return end_expr;

            auto span = SourceSpan::merge(unwrap(left)->span, unwrap(end_expr)->span);
            left = make_box<Expr>(Expr{
                .kind = RangeExpr{
                    .start = std::move(unwrap(left)),
                    .end = std::move(unwrap(end_expr)),
                    .inclusive = inclusive,
                    .span = span
                },
                .span = span
            });
            continue;
        }

        // Infix operators
        auto op = token_to_binary_op(peek().kind);
        if (!op) {
            break;
        }

        int actual_prec = is_right_associative(peek().kind) ? prec - 1 : prec;
        left = parse_infix_expr(std::move(unwrap(left)), actual_prec);
        if (is_err(left)) return left;
    }

    return left;
}

auto Parser::parse_prefix_expr() -> Result<ExprPtr, ParseError> {
    // TML syntax: 'mut ref x' for mutable reference
    if (check(lexer::TokenKind::KwMut) && check_next(lexer::TokenKind::KwRef)) {
        auto start_span = peek().span;
        advance(); // consume 'mut'
        advance(); // consume 'ref'

        auto operand = parse_prefix_expr();
        if (is_err(operand)) return operand;

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
        if (is_err(operand)) return operand;

        auto span = SourceSpan::merge(start_span, unwrap(operand)->span);
        return make_unary_expr(*op, std::move(unwrap(operand)), span);
    }

    return parse_primary_expr();
}

auto Parser::parse_postfix_expr(ExprPtr left) -> Result<ExprPtr, ParseError> {
    auto start_span = left->span;

    // Function call
    if (match(lexer::TokenKind::LParen)) {
        auto args = parse_call_args();
        if (is_err(args)) return unwrap_err(args);

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after arguments");
        if (is_err(rparen)) return unwrap_err(rparen);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_call_expr(std::move(left), std::move(unwrap(args)), span);
    }

    // Index access
    if (match(lexer::TokenKind::LBracket)) {
        auto index = parse_expr();
        if (is_err(index)) return index;

        auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']' after index");
        if (is_err(rbracket)) return unwrap_err(rbracket);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{
            .kind = IndexExpr{
                .object = std::move(left),
                .index = std::move(unwrap(index)),
                .span = span
            },
            .span = span
        });
    }

    // Field/method access
    if (match(lexer::TokenKind::Dot)) {
        // Check for await
        if (match(lexer::TokenKind::KwAwait)) {
            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Expr>(Expr{
                .kind = AwaitExpr{
                    .expr = std::move(left),
                    .span = span
                },
                .span = span
            });
        }

        auto name_result = expect(lexer::TokenKind::Identifier, "Expected field or method name");
        if (is_err(name_result)) return unwrap_err(name_result);
        auto name = std::string(unwrap(name_result).lexeme);

        // Check if it's a method call
        if (check(lexer::TokenKind::LParen)) {
            advance();
            auto args = parse_call_args();
            if (is_err(args)) return unwrap_err(args);

            auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after arguments");
            if (is_err(rparen)) return unwrap_err(rparen);

            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Expr>(Expr{
                .kind = MethodCallExpr{
                    .receiver = std::move(left),
                    .method = std::move(name),
                    .args = std::move(unwrap(args)),
                    .span = span
                },
                .span = span
            });
        }

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{
            .kind = FieldExpr{
                .object = std::move(left),
                .field = std::move(name),
                .span = span
            },
            .span = span
        });
    }

    // Try operator (?)
    if (match(lexer::TokenKind::Bang)) {
        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{
            .kind = TryExpr{
                .expr = std::move(left),
                .span = span
            },
            .span = span
        });
    }

    // Postfix increment (++)
    if (match(lexer::TokenKind::PlusPlus)) {
        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{
            .kind = UnaryExpr{
                .op = UnaryOp::Inc,
                .operand = std::move(left),
                .span = span
            },
            .span = span
        });
    }

    // Postfix decrement (--)
    if (match(lexer::TokenKind::MinusMinus)) {
        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{
            .kind = UnaryExpr{
                .op = UnaryOp::Dec,
                .operand = std::move(left),
                .span = span
            },
            .span = span
        });
    }

    return left;
}

auto Parser::parse_infix_expr(ExprPtr left, int precedence) -> Result<ExprPtr, ParseError> {
    auto op_token = advance();
    auto op = token_to_binary_op(op_token.kind);
    if (!op) {
        return ParseError{
            .message = "Expected binary operator",
            .span = op_token.span,
            .notes = {}
        };
    }

    auto right = parse_expr_with_precedence(precedence);
    if (is_err(right)) return right;

    auto span = SourceSpan::merge(left->span, unwrap(right)->span);
    return make_binary_expr(*op, std::move(left), std::move(unwrap(right)), span);
}

auto Parser::parse_primary_expr() -> Result<ExprPtr, ParseError> {
    // Literals
    if (check(lexer::TokenKind::IntLiteral) ||
        check(lexer::TokenKind::FloatLiteral) ||
        check(lexer::TokenKind::StringLiteral) ||
        check(lexer::TokenKind::CharLiteral) ||
        check(lexer::TokenKind::BoolLiteral)) {
        return parse_literal_expr();
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

    // For
    if (check(lexer::TokenKind::KwFor)) {
        return parse_for_expr();
    }

    // Return
    if (check(lexer::TokenKind::KwReturn)) {
        return parse_return_expr();
    }

    // Break
    if (check(lexer::TokenKind::KwBreak)) {
        return parse_break_expr();
    }

    // Continue
    if (check(lexer::TokenKind::KwContinue)) {
        return parse_continue_expr();
    }

    // Closure: do(x) expr
    if (check(lexer::TokenKind::KwDo)) {
        return parse_closure_expr();
    }

    return ParseError{
        .message = "Expected expression",
        .span = peek().span,
        .notes = {}
    };
}

auto Parser::parse_literal_expr() -> Result<ExprPtr, ParseError> {
    auto token = advance();
    return make_literal_expr(std::move(token));
}

auto Parser::parse_ident_or_path_expr() -> Result<ExprPtr, ParseError> {
    auto path = parse_type_path();
    if (is_err(path)) return unwrap_err(path);

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
            // If followed by : or , or }, it's a struct literal
            is_struct = check(lexer::TokenKind::Colon) ||
                       check(lexer::TokenKind::Comma) ||
                       check(lexer::TokenKind::RBrace);
            pos_ = saved_inner; // restore to before identifier
        }

        pos_ = saved_pos; // restore to before '{'

        if (is_struct) {
            return parse_struct_expr(std::move(unwrap(path)));
        }
        // Otherwise, it's not a struct literal - just return the identifier
    }

    auto span = unwrap(path).span;
    if (unwrap(path).segments.size() == 1) {
        return make_ident_expr(std::move(unwrap(path).segments[0]), span);
    }

    return make_box<Expr>(Expr{
        .kind = PathExpr{
            .path = std::move(unwrap(path)),
            .span = span
        },
        .span = span
    });
}

auto Parser::parse_paren_or_tuple_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume '('

    skip_newlines();

    // Empty tuple
    if (check(lexer::TokenKind::RParen)) {
        advance();
        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{
            .kind = TupleExpr{
                .elements = {},
                .span = span
            },
            .span = span
        });
    }

    auto first = parse_expr();
    if (is_err(first)) return first;

    skip_newlines();

    // Check if it's a tuple or just parenthesized
    if (match(lexer::TokenKind::Comma)) {
        // It's a tuple
        std::vector<ExprPtr> elements;
        elements.push_back(std::move(unwrap(first)));

        skip_newlines();
        while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
            auto elem = parse_expr();
            if (is_err(elem)) return elem;
            elements.push_back(std::move(unwrap(elem)));

            skip_newlines();
            if (!check(lexer::TokenKind::RParen)) {
                auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between tuple elements");
                if (is_err(comma)) return unwrap_err(comma);
                skip_newlines();
            }
        }

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after tuple");
        if (is_err(rparen)) return unwrap_err(rparen);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{
            .kind = TupleExpr{
                .elements = std::move(elements),
                .span = span
            },
            .span = span
        });
    }

    // Just a parenthesized expression
    auto rparen = expect(lexer::TokenKind::RParen, "Expected ')'");
    if (is_err(rparen)) return unwrap_err(rparen);

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
        return make_box<Expr>(Expr{
            .kind = ArrayExpr{
                .kind = std::vector<ExprPtr>{},
                .span = span
            },
            .span = span
        });
    }

    auto first = parse_expr();
    if (is_err(first)) return first;

    skip_newlines();

    // Check for repeat syntax: [expr; count]
    if (match(lexer::TokenKind::Semi)) {
        auto count = parse_expr();
        if (is_err(count)) return count;

        auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']'");
        if (is_err(rbracket)) return unwrap_err(rbracket);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Expr>(Expr{
            .kind = ArrayExpr{
                .kind = std::pair{std::move(unwrap(first)), std::move(unwrap(count))},
                .span = span
            },
            .span = span
        });
    }

    // Regular array literal
    std::vector<ExprPtr> elements;
    elements.push_back(std::move(unwrap(first)));

    while (match(lexer::TokenKind::Comma)) {
        skip_newlines();
        if (check(lexer::TokenKind::RBracket)) break;

        auto elem = parse_expr();
        if (is_err(elem)) return elem;
        elements.push_back(std::move(unwrap(elem)));
        skip_newlines();
    }

    auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']'");
    if (is_err(rbracket)) return unwrap_err(rbracket);

    auto span = SourceSpan::merge(start_span, previous().span);
    return make_box<Expr>(Expr{
        .kind = ArrayExpr{
            .kind = std::move(elements),
            .span = span
        },
        .span = span
    });
}

auto Parser::parse_block_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{'");
    if (is_err(lbrace)) return unwrap_err(lbrace);

    std::vector<StmtPtr> stmts;
    std::optional<ExprPtr> expr;

    skip_newlines();

    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto stmt = parse_stmt();
        if (is_err(stmt)) return unwrap_err(stmt);

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
    if (is_err(rbrace)) return unwrap_err(rbrace);

    auto span = SourceSpan::merge(start_span, previous().span);
    return make_block_expr(std::move(stmts), std::move(expr), span);
}

auto Parser::parse_if_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'if'

    auto cond = parse_expr();
    if (is_err(cond)) return cond;

    auto then_branch = parse_block_expr();
    if (is_err(then_branch)) return then_branch;

    std::optional<ExprPtr> else_branch;
    skip_newlines();
    if (match(lexer::TokenKind::KwElse)) {
        skip_newlines();
        if (check(lexer::TokenKind::KwIf)) {
            auto else_if = parse_if_expr();
            if (is_err(else_if)) return else_if;
            else_branch = std::move(unwrap(else_if));
        } else {
            auto else_block = parse_block_expr();
            if (is_err(else_block)) return else_block;
            else_branch = std::move(unwrap(else_block));
        }
    }

    auto end_span = previous().span;
    return make_box<Expr>(Expr{
        .kind = IfExpr{
            .condition = std::move(unwrap(cond)),
            .then_branch = std::move(unwrap(then_branch)),
            .else_branch = std::move(else_branch),
            .span = SourceSpan::merge(start_span, end_span)
        },
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_when_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'when'

    auto scrutinee = parse_expr();
    if (is_err(scrutinee)) return scrutinee;

    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{'");
    if (is_err(lbrace)) return unwrap_err(lbrace);

    std::vector<WhenArm> arms;
    skip_newlines();

    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        auto pattern = parse_pattern();
        if (is_err(pattern)) return unwrap_err(pattern);

        std::optional<ExprPtr> guard;
        if (match(lexer::TokenKind::KwIf)) {
            auto g = parse_expr();
            if (is_err(g)) return g;
            guard = std::move(unwrap(g));
        }

        auto arrow = expect(lexer::TokenKind::FatArrow, "Expected '=>'");
        if (is_err(arrow)) return unwrap_err(arrow);

        auto body = parse_expr();
        if (is_err(body)) return body;

        auto arm_span = SourceSpan::merge(unwrap(pattern)->span, unwrap(body)->span);
        arms.push_back(WhenArm{
            .pattern = std::move(unwrap(pattern)),
            .guard = std::move(guard),
            .body = std::move(unwrap(body)),
            .span = arm_span
        });

        skip_newlines();
        match(lexer::TokenKind::Comma);
        skip_newlines();
    }

    auto rbrace = expect(lexer::TokenKind::RBrace, "Expected '}'");
    if (is_err(rbrace)) return unwrap_err(rbrace);

    auto end_span = previous().span;
    return make_box<Expr>(Expr{
        .kind = WhenExpr{
            .scrutinee = std::move(unwrap(scrutinee)),
            .arms = std::move(arms),
            .span = SourceSpan::merge(start_span, end_span)
        },
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_loop_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'loop'

    auto body = parse_block_expr();
    if (is_err(body)) return body;

    auto end_span = previous().span;
    return make_box<Expr>(Expr{
        .kind = LoopExpr{
            .label = std::nullopt,
            .body = std::move(unwrap(body)),
            .span = SourceSpan::merge(start_span, end_span)
        },
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_while_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'while'

    auto cond = parse_expr();
    if (is_err(cond)) return cond;

    auto body = parse_block_expr();
    if (is_err(body)) return body;

    auto end_span = previous().span;
    return make_box<Expr>(Expr{
        .kind = WhileExpr{
            .label = std::nullopt,
            .condition = std::move(unwrap(cond)),
            .body = std::move(unwrap(body)),
            .span = SourceSpan::merge(start_span, end_span)
        },
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_for_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'for'

    auto pattern = parse_pattern();
    if (is_err(pattern)) return unwrap_err(pattern);

    auto in_tok = expect(lexer::TokenKind::KwIn, "Expected 'in'");
    if (is_err(in_tok)) return unwrap_err(in_tok);

    auto iter = parse_expr();
    if (is_err(iter)) return iter;

    auto body = parse_block_expr();
    if (is_err(body)) return body;

    auto end_span = previous().span;
    return make_box<Expr>(Expr{
        .kind = ForExpr{
            .label = std::nullopt,
            .pattern = std::move(unwrap(pattern)),
            .iter = std::move(unwrap(iter)),
            .body = std::move(unwrap(body)),
            .span = SourceSpan::merge(start_span, end_span)
        },
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_return_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'return'

    std::optional<ExprPtr> value;
    if (!check(lexer::TokenKind::Semi) &&
        !check(lexer::TokenKind::Newline) &&
        !check(lexer::TokenKind::RBrace) &&
        !is_at_end()) {
        auto v = parse_expr();
        if (is_err(v)) return v;
        value = std::move(unwrap(v));
    }

    auto end_span = previous().span;
    return make_box<Expr>(Expr{
        .kind = ReturnExpr{
            .value = std::move(value),
            .span = SourceSpan::merge(start_span, end_span)
        },
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_break_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    advance(); // consume 'break'

    std::optional<ExprPtr> value;
    if (!check(lexer::TokenKind::Semi) &&
        !check(lexer::TokenKind::Newline) &&
        !check(lexer::TokenKind::RBrace) &&
        !is_at_end()) {
        auto v = parse_expr();
        if (is_err(v)) return v;
        value = std::move(unwrap(v));
    }

    auto end_span = previous().span;
    return make_box<Expr>(Expr{
        .kind = BreakExpr{
            .label = std::nullopt,
            .value = std::move(value),
            .span = SourceSpan::merge(start_span, end_span)
        },
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_continue_expr() -> Result<ExprPtr, ParseError> {
    auto span = peek().span;
    advance(); // consume 'continue'

    return make_box<Expr>(Expr{
        .kind = ContinueExpr{
            .label = std::nullopt,
            .span = span
        },
        .span = span
    });
}

auto Parser::parse_closure_expr() -> Result<ExprPtr, ParseError> {
    auto start_span = peek().span;
    
    // Consume 'do' keyword
    auto do_tok = expect(lexer::TokenKind::KwDo, "Expected 'do'");
    if (is_err(do_tok)) return unwrap_err(do_tok);
    
    // Parse parameters in parentheses
    auto lparen = expect(lexer::TokenKind::LParen, "Expected '(' after 'do'");
    if (is_err(lparen)) return unwrap_err(lparen);
    
    std::vector<std::pair<PatternPtr, std::optional<TypePtr>>> params;
    skip_newlines();
    
    while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
        // Parse pattern
        auto pattern = parse_pattern();
        if (is_err(pattern)) return unwrap_err(pattern);
        
        // Optional type annotation
        std::optional<TypePtr> type;
        if (match(lexer::TokenKind::Colon)) {
            auto t = parse_type();
            if (is_err(t)) return unwrap_err(t);
            type = std::move(unwrap(t));
        }
        
        params.emplace_back(std::move(unwrap(pattern)), std::move(type));
        
        skip_newlines();
        if (!check(lexer::TokenKind::RParen)) {
            auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between closure parameters");
            if (is_err(comma)) return unwrap_err(comma);
            skip_newlines();
        }
    }
    
    auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after closure parameters");
    if (is_err(rparen)) return unwrap_err(rparen);
    
    // Optional return type
    std::optional<TypePtr> return_type;
    if (match(lexer::TokenKind::Arrow)) {
        auto t = parse_type();
        if (is_err(t)) return unwrap_err(t);
        return_type = std::move(unwrap(t));
    }
    
    // Parse body (either block or expression)
    skip_newlines();
    ExprPtr body;
    if (check(lexer::TokenKind::LBrace)) {
        auto b = parse_block_expr();
        if (is_err(b)) return b;
        body = std::move(unwrap(b));
    } else {
        auto b = parse_expr();
        if (is_err(b)) return b;
        body = std::move(unwrap(b));
    }
    
    auto end_span = previous().span;
    
    return make_box<Expr>(Expr{
        .kind = ClosureExpr{
            .params = std::move(params),
            .return_type = std::move(return_type),
            .body = std::move(body),
            .is_move = false,  // TML doesn't have move closures in this syntax
            .span = SourceSpan::merge(start_span, end_span)
        },
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_struct_expr(TypePath path) -> Result<ExprPtr, ParseError> {
    auto start_span = path.span;

    auto lbrace = expect(lexer::TokenKind::LBrace, "Expected '{'");
    if (is_err(lbrace)) return unwrap_err(lbrace);

    std::vector<std::pair<std::string, ExprPtr>> fields;
    std::optional<ExprPtr> base;

    skip_newlines();
    while (!check(lexer::TokenKind::RBrace) && !is_at_end()) {
        // Check for ..base
        if (match(lexer::TokenKind::DotDot)) {
            auto base_expr = parse_expr();
            if (is_err(base_expr)) return base_expr;
            base = std::move(unwrap(base_expr));
            skip_newlines();
            break;
        }

        auto field_name_result = expect(lexer::TokenKind::Identifier, "Expected field name");
        if (is_err(field_name_result)) return unwrap_err(field_name_result);
        auto field_name = std::string(unwrap(field_name_result).lexeme);

        ExprPtr value;
        if (match(lexer::TokenKind::Colon)) {
            auto v = parse_expr();
            if (is_err(v)) return v;
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
    if (is_err(rbrace)) return unwrap_err(rbrace);

    auto end_span = previous().span;
    return make_box<Expr>(Expr{
        .kind = StructExpr{
            .path = std::move(path),
            .fields = std::move(fields),
            .base = std::move(base),
            .span = SourceSpan::merge(start_span, end_span)
        },
        .span = SourceSpan::merge(start_span, end_span)
    });
}

auto Parser::parse_call_args() -> Result<std::vector<ExprPtr>, ParseError> {
    std::vector<ExprPtr> args;

    skip_newlines();
    while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
        auto arg = parse_expr();
        if (is_err(arg)) return unwrap_err(arg);
        args.push_back(std::move(unwrap(arg)));

        skip_newlines();
        if (!check(lexer::TokenKind::RParen)) {
            auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between arguments");
            if (is_err(comma)) return unwrap_err(comma);
            skip_newlines();
        }
    }

    return args;
}

// ============================================================================
// Type Parsing
// ============================================================================

auto Parser::parse_type() -> Result<TypePtr, ParseError> {
    auto start_span = peek().span;

    // TML Reference: ref T, mut ref T
    if (match(lexer::TokenKind::KwMut)) {
        // Must be followed by 'ref'
        if (!match(lexer::TokenKind::KwRef)) {
            return ParseError{"Expected 'ref' after 'mut' in type", peek().span};
        }
        auto inner = parse_type();
        if (is_err(inner)) return inner;
        return make_ref_type(true, std::move(unwrap(inner)),
                            SourceSpan::merge(start_span, unwrap(inner)->span));
    }
    if (match(lexer::TokenKind::KwRef)) {
        auto inner = parse_type();
        if (is_err(inner)) return inner;
        return make_ref_type(false, std::move(unwrap(inner)),
                            SourceSpan::merge(start_span, unwrap(inner)->span));
    }

    // Legacy Reference: &T, &mut T
    if (match(lexer::TokenKind::BitAnd)) {
        bool is_mut = match(lexer::TokenKind::KwMut);
        auto inner = parse_type();
        if (is_err(inner)) return inner;
        return make_ref_type(is_mut, std::move(unwrap(inner)),
                            SourceSpan::merge(start_span, unwrap(inner)->span));
    }

    // Pointer: *const T, *mut T
    if (match(lexer::TokenKind::Star)) {
        bool is_mut = false;
        if (match(lexer::TokenKind::KwMut)) {
            is_mut = true;
        } else if (match(lexer::TokenKind::KwConst)) {
            is_mut = false;
        }
        auto inner = parse_type();
        if (is_err(inner)) return inner;
        auto span = SourceSpan::merge(start_span, unwrap(inner)->span);
        return make_box<Type>(Type{
            .kind = PtrType{
                .is_mut = is_mut,
                .inner = std::move(unwrap(inner)),
                .span = span
            },
            .span = span
        });
    }

    // Tuple or function: (T, U) or func(T) -> R
    if (check(lexer::TokenKind::LParen)) {
        return parse_type(); // TODO: implement tuple types
    }

    // Array or slice: [T; N] or [T]
    if (match(lexer::TokenKind::LBracket)) {
        auto elem = parse_type();
        if (is_err(elem)) return elem;

        if (match(lexer::TokenKind::Semi)) {
            // Array: [T; N]
            auto size = parse_expr();
            if (is_err(size)) return unwrap_err(size);

            auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']'");
            if (is_err(rbracket)) return unwrap_err(rbracket);

            auto span = SourceSpan::merge(start_span, previous().span);
            return make_box<Type>(Type{
                .kind = ArrayType{
                    .element = std::move(unwrap(elem)),
                    .size = std::move(unwrap(size)),
                    .span = span
                },
                .span = span
            });
        }

        // Slice: [T]
        auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']'");
        if (is_err(rbracket)) return unwrap_err(rbracket);

        auto span = SourceSpan::merge(start_span, previous().span);
        return make_box<Type>(Type{
            .kind = SliceType{
                .element = std::move(unwrap(elem)),
                .span = span
            },
            .span = span
        });
    }

    // Infer: _
    if (check(lexer::TokenKind::Identifier) && peek().lexeme == "_") {
        advance();
        return make_box<Type>(Type{
            .kind = InferType{.span = start_span},
            .span = start_span
        });
    }

    // Function type: Fn(Params) -> RetType
    if (check(lexer::TokenKind::Identifier) && peek().lexeme == "Fn") {
        advance(); // consume 'Fn'

        auto lparen = expect(lexer::TokenKind::LParen, "Expected '(' after 'Fn'");
        if (is_err(lparen)) return unwrap_err(lparen);

        // Parse parameter types
        std::vector<TypePtr> param_types;
        while (!check(lexer::TokenKind::RParen) && !is_at_end()) {
            auto param_type = parse_type();
            if (is_err(param_type)) return param_type;
            param_types.push_back(std::move(unwrap(param_type)));

            if (!check(lexer::TokenKind::RParen)) {
                auto comma = expect(lexer::TokenKind::Comma, "Expected ',' or ')' in function type");
                if (is_err(comma)) return unwrap_err(comma);
            }
        }

        auto rparen = expect(lexer::TokenKind::RParen, "Expected ')' after function parameters");
        if (is_err(rparen)) return unwrap_err(rparen);

        // Parse return type (optional, defaults to Unit)
        TypePtr return_type = nullptr;
        if (match(lexer::TokenKind::Arrow)) {
            auto ret = parse_type();
            if (is_err(ret)) return ret;
            return_type = std::move(unwrap(ret));
        }

        auto end_span = previous().span;
        auto span = SourceSpan::merge(start_span, end_span);
        return make_box<Type>(Type{
            .kind = FuncType{
                .params = std::move(param_types),
                .return_type = std::move(return_type),
                .span = span
            },
            .span = span
        });
    }

    // Named type: Ident or Path::To::Type
    auto path = parse_type_path();
    if (is_err(path)) return unwrap_err(path);

    auto generics = parse_generic_args();
    if (is_err(generics)) return unwrap_err(generics);

    auto span = unwrap(path).span;
    return make_box<Type>(Type{
        .kind = NamedType{
            .path = std::move(unwrap(path)),
            .generics = std::move(unwrap(generics)),
            .span = span
        },
        .span = span
    });
}

auto Parser::parse_type_path() -> Result<TypePath, ParseError> {
    std::vector<std::string> segments;
    auto start_span = peek().span;

    auto first = expect(lexer::TokenKind::Identifier, "Expected type name");
    if (is_err(first)) return unwrap_err(first);
    segments.push_back(std::string(unwrap(first).lexeme));

    while (match(lexer::TokenKind::ColonColon)) {
        auto segment = expect(lexer::TokenKind::Identifier, "Expected identifier after '::'");
        if (is_err(segment)) return unwrap_err(segment);
        segments.push_back(std::string(unwrap(segment).lexeme));
    }

    auto end_span = previous().span;
    return TypePath{
        .segments = std::move(segments),
        .span = SourceSpan::merge(start_span, end_span)
    };
}

auto Parser::parse_generic_args() -> Result<std::optional<GenericArgs>, ParseError> {
    if (!check(lexer::TokenKind::LBracket)) {
        return std::nullopt;
    }

    auto start_span = peek().span;
    advance(); // consume '['

    std::vector<TypePtr> args;
    while (!check(lexer::TokenKind::RBracket) && !is_at_end()) {
        auto arg = parse_type();
        if (is_err(arg)) return unwrap_err(arg);
        args.push_back(std::move(unwrap(arg)));

        if (!check(lexer::TokenKind::RBracket)) {
            auto comma = expect(lexer::TokenKind::Comma, "Expected ',' between type arguments");
            if (is_err(comma)) return unwrap_err(comma);
        }
    }

    auto rbracket = expect(lexer::TokenKind::RBracket, "Expected ']'");
    if (is_err(rbracket)) return unwrap_err(rbracket);

    return GenericArgs{
        .args = std::move(args),
        .span = SourceSpan::merge(start_span, previous().span)
    };
}

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

    // Mutable binding: mut x
    if (match(lexer::TokenKind::KwMut)) {
        auto name_result = expect(lexer::TokenKind::Identifier, "Expected identifier after 'mut'");
        if (is_err(name_result)) return unwrap_err(name_result);
        auto span = SourceSpan::merge(start_span, unwrap(name_result).span);
        return make_ident_pattern(std::string(unwrap(name_result).lexeme), true, span);
    }

    // this pattern (for method receivers)
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
            auto elem = parse_pattern();
            if (is_err(elem)) return elem;
            elements.push_back(std::move(unwrap(elem)));

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

        case lexer::TokenKind::KwOr:
            return precedence::OR;

        case lexer::TokenKind::KwAnd:
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
            return precedence::BITXOR;

        case lexer::TokenKind::BitAnd:
            return precedence::BITAND;

        case lexer::TokenKind::Shl:
        case lexer::TokenKind::Shr:
            return precedence::SHIFT;

        case lexer::TokenKind::Plus:
        case lexer::TokenKind::Minus:
            return precedence::TERM;

        case lexer::TokenKind::Star:
        case lexer::TokenKind::Slash:
        case lexer::TokenKind::Percent:
            return precedence::FACTOR;

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
    // Assignment is right-associative
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
            return true;
        default:
            return false;
    }
}

auto Parser::token_to_binary_op(lexer::TokenKind kind) -> std::optional<BinaryOp> {
    switch (kind) {
        case lexer::TokenKind::Plus: return BinaryOp::Add;
        case lexer::TokenKind::Minus: return BinaryOp::Sub;
        case lexer::TokenKind::Star: return BinaryOp::Mul;
        case lexer::TokenKind::Slash: return BinaryOp::Div;
        case lexer::TokenKind::Percent: return BinaryOp::Mod;

        case lexer::TokenKind::Eq: return BinaryOp::Eq;
        case lexer::TokenKind::Ne: return BinaryOp::Ne;
        case lexer::TokenKind::Lt: return BinaryOp::Lt;
        case lexer::TokenKind::Gt: return BinaryOp::Gt;
        case lexer::TokenKind::Le: return BinaryOp::Le;
        case lexer::TokenKind::Ge: return BinaryOp::Ge;

        case lexer::TokenKind::KwAnd: return BinaryOp::And;
        case lexer::TokenKind::KwOr: return BinaryOp::Or;

        case lexer::TokenKind::BitAnd: return BinaryOp::BitAnd;
        case lexer::TokenKind::BitOr: return BinaryOp::BitOr;
        case lexer::TokenKind::BitXor: return BinaryOp::BitXor;
        case lexer::TokenKind::Shl: return BinaryOp::Shl;
        case lexer::TokenKind::Shr: return BinaryOp::Shr;

        case lexer::TokenKind::Assign: return BinaryOp::Assign;
        case lexer::TokenKind::PlusAssign: return BinaryOp::AddAssign;
        case lexer::TokenKind::MinusAssign: return BinaryOp::SubAssign;
        case lexer::TokenKind::StarAssign: return BinaryOp::MulAssign;
        case lexer::TokenKind::SlashAssign: return BinaryOp::DivAssign;
        case lexer::TokenKind::PercentAssign: return BinaryOp::ModAssign;
        case lexer::TokenKind::BitAndAssign: return BinaryOp::BitAndAssign;
        case lexer::TokenKind::BitOrAssign: return BinaryOp::BitOrAssign;
        case lexer::TokenKind::BitXorAssign: return BinaryOp::BitXorAssign;
        case lexer::TokenKind::ShlAssign: return BinaryOp::ShlAssign;
        case lexer::TokenKind::ShrAssign: return BinaryOp::ShrAssign;

        default: return std::nullopt;
    }
}

auto Parser::token_to_unary_op(lexer::TokenKind kind) -> std::optional<UnaryOp> {
    switch (kind) {
        case lexer::TokenKind::Minus: return UnaryOp::Neg;
        case lexer::TokenKind::KwNot: return UnaryOp::Not;
        case lexer::TokenKind::BitNot: return UnaryOp::BitNot;
        case lexer::TokenKind::BitAnd: return UnaryOp::Ref;
        case lexer::TokenKind::KwRef: return UnaryOp::Ref;  // TML uses 'ref x' syntax
        case lexer::TokenKind::Star: return UnaryOp::Deref;
        default: return std::nullopt;
    }
}

} // namespace tml::parser
