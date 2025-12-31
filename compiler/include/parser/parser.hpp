#ifndef TML_PARSER_PARSER_HPP
#define TML_PARSER_PARSER_HPP

#include "common.hpp"
#include "lexer/lexer.hpp"
#include "parser/ast.hpp"

#include <functional>
#include <vector>

namespace tml::parser {

// Operator precedence levels (higher = tighter binding)
namespace precedence {
constexpr int NONE = 0;
constexpr int ASSIGN = 1;     // =, +=, etc.
constexpr int TERNARY = 2;    // ? :
constexpr int OR = 3;         // || / or
constexpr int AND = 4;        // && / and
constexpr int COMPARISON = 5; // ==, !=, <, >, <=, >=
constexpr int BITOR = 6;      // |
constexpr int BITXOR = 7;     // ^
constexpr int BITAND = 8;     // &
constexpr int SHIFT = 9;      // <<, >>
constexpr int TERM = 10;      // +, -
constexpr int FACTOR = 11;    // *, /, %
constexpr int CAST = 12;      // as (type casting)
constexpr int UNARY = 13;     // -, !, ~, &, *
constexpr int CALL = 14;      // (), [], .
constexpr int RANGE = 15;     // .., ..=
} // namespace precedence

// Fix-it hint for automatic code correction
struct FixItHint {
    SourceSpan span;         // Where to apply the fix
    std::string replacement; // Text to insert/replace
    std::string description; // Human-readable description of the fix
};

// Parser error
struct ParseError {
    std::string message;
    SourceSpan span;
    std::vector<std::string> notes;
    std::vector<FixItHint> fixes; // Suggested fixes
};

// Parser for TML source code
class Parser {
public:
    explicit Parser(std::vector<lexer::Token> tokens);

    // Parse entire module
    [[nodiscard]] auto parse_module(const std::string& name)
        -> Result<Module, std::vector<ParseError>>;

    // Parse single declaration (for testing)
    [[nodiscard]] auto parse_decl() -> Result<DeclPtr, ParseError>;

    // Parse single expression (for testing)
    [[nodiscard]] auto parse_expr() -> Result<ExprPtr, ParseError>;

    // Parse single statement (for testing)
    [[nodiscard]] auto parse_stmt() -> Result<StmtPtr, ParseError>;

    // Get all errors
    [[nodiscard]] auto errors() const -> const std::vector<ParseError>& {
        return errors_;
    }

    // Check if any errors occurred
    [[nodiscard]] auto has_errors() const -> bool {
        return !errors_.empty();
    }

private:
    std::vector<lexer::Token> tokens_;
    size_t pos_ = 0;
    std::vector<ParseError> errors_;

    // Token access
    [[nodiscard]] auto peek() const -> const lexer::Token&;
    [[nodiscard]] auto peek_next() const -> const lexer::Token&;
    [[nodiscard]] auto previous() const -> const lexer::Token&;
    auto advance() -> const lexer::Token&;
    [[nodiscard]] auto is_at_end() const -> bool;
    [[nodiscard]] auto check(lexer::TokenKind kind) const -> bool;
    [[nodiscard]] auto check_next(lexer::TokenKind kind) const -> bool;
    auto match(lexer::TokenKind kind) -> bool;
    auto expect(lexer::TokenKind kind, const std::string& message)
        -> Result<lexer::Token, ParseError>;

    // Skip newlines (where they're not significant)
    void skip_newlines();

    // Error handling
    void report_error(const std::string& message);
    void report_error(const std::string& message, SourceSpan span);
    void report_error_with_fix(const std::string& message, SourceSpan span,
                               const std::vector<FixItHint>& fixes);
    void synchronize();

    // Enhanced error recovery
    void synchronize_to_stmt();             // Recover to next statement
    void synchronize_to_decl();             // Recover to next declaration
    void synchronize_to_brace();            // Recover to closing brace
    bool try_recover_missing_semi();        // Try to recover from missing semicolon
    bool skip_until(lexer::TokenKind kind); // Skip tokens until finding kind
    bool skip_until_any(std::initializer_list<lexer::TokenKind> kinds);

    // Fix-it hint helpers
    [[nodiscard]] auto make_insertion_fix(const SourceSpan& at, const std::string& text,
                                          const std::string& desc) -> FixItHint;
    [[nodiscard]] auto make_replacement_fix(const SourceSpan& span, const std::string& text,
                                            const std::string& desc) -> FixItHint;
    [[nodiscard]] auto make_deletion_fix(const SourceSpan& span, const std::string& desc)
        -> FixItHint;

    // Declaration parsing
    auto parse_visibility() -> Visibility;
    auto parse_decorators() -> Result<std::vector<Decorator>, ParseError>;
    auto parse_func_decl(Visibility vis, std::vector<Decorator> decorators = {})
        -> Result<DeclPtr, ParseError>;
    auto parse_struct_decl(Visibility vis, std::vector<Decorator> decorators = {})
        -> Result<DeclPtr, ParseError>;
    auto parse_enum_decl(Visibility vis, std::vector<Decorator> decorators = {})
        -> Result<DeclPtr, ParseError>;
    auto parse_trait_decl(Visibility vis, std::vector<Decorator> decorators = {})
        -> Result<DeclPtr, ParseError>;
    auto parse_impl_decl() -> Result<DeclPtr, ParseError>;
    auto parse_type_alias_decl(Visibility vis) -> Result<DeclPtr, ParseError>;
    auto parse_const_decl(Visibility vis) -> Result<DeclPtr, ParseError>;
    auto parse_use_decl(Visibility vis) -> Result<DeclPtr, ParseError>;
    auto parse_mod_decl(Visibility vis) -> Result<DeclPtr, ParseError>;

    // Generic parsing
    auto parse_generic_params() -> Result<std::vector<GenericParam>, ParseError>;
    auto parse_where_clause() -> Result<std::optional<WhereClause>, ParseError>;

    // Function parsing
    auto parse_func_params() -> Result<std::vector<FuncParam>, ParseError>;
    auto parse_func_param() -> Result<FuncParam, ParseError>;

    // Statement parsing
    auto parse_let_stmt() -> Result<StmtPtr, ParseError>;
    auto parse_var_stmt() -> Result<StmtPtr, ParseError>;
    auto parse_expr_stmt() -> Result<StmtPtr, ParseError>;

    // Expression parsing (Pratt parser)
    auto parse_expr_with_precedence(int min_precedence) -> Result<ExprPtr, ParseError>;
    auto parse_primary_expr() -> Result<ExprPtr, ParseError>;
    auto parse_primary_with_postfix() -> Result<ExprPtr, ParseError>;
    auto parse_prefix_expr() -> Result<ExprPtr, ParseError>;
    auto parse_postfix_expr(ExprPtr left) -> Result<ExprPtr, ParseError>;
    auto parse_infix_expr(ExprPtr left, int precedence) -> Result<ExprPtr, ParseError>;

    // Specific expression parsing
    auto parse_literal_expr() -> Result<ExprPtr, ParseError>;
    auto parse_ident_or_path_expr() -> Result<ExprPtr, ParseError>;
    auto parse_paren_or_tuple_expr() -> Result<ExprPtr, ParseError>;
    auto parse_array_expr() -> Result<ExprPtr, ParseError>;
    auto parse_block_expr() -> Result<ExprPtr, ParseError>;
    auto parse_if_expr() -> Result<ExprPtr, ParseError>;
    auto parse_if_let_expr(SourceSpan start_span) -> Result<ExprPtr, ParseError>;
    auto parse_when_expr() -> Result<ExprPtr, ParseError>;
    auto parse_loop_expr() -> Result<ExprPtr, ParseError>;
    auto parse_while_expr() -> Result<ExprPtr, ParseError>;
    auto parse_for_expr() -> Result<ExprPtr, ParseError>;
    auto parse_return_expr() -> Result<ExprPtr, ParseError>;
    auto parse_break_expr() -> Result<ExprPtr, ParseError>;
    auto parse_continue_expr() -> Result<ExprPtr, ParseError>;
    auto parse_closure_expr() -> Result<ExprPtr, ParseError>;
    auto parse_struct_expr(TypePath path, std::optional<GenericArgs> generics = std::nullopt)
        -> Result<ExprPtr, ParseError>;
    auto parse_lowlevel_expr() -> Result<ExprPtr, ParseError>;
    auto parse_interp_string_expr() -> Result<ExprPtr, ParseError>;

    // Call and member access
    auto parse_call_args() -> Result<std::vector<ExprPtr>, ParseError>;

    // Type parsing
    auto parse_type() -> Result<TypePtr, ParseError>;
    auto parse_type_path() -> Result<TypePath, ParseError>;
    auto parse_generic_args() -> Result<std::optional<GenericArgs>, ParseError>;

    // Pattern parsing
    auto parse_pattern() -> Result<PatternPtr, ParseError>;
    auto parse_pattern_no_or() -> Result<PatternPtr, ParseError>;

    // Precedence and associativity
    [[nodiscard]] static auto get_precedence(lexer::TokenKind kind) -> int;
    [[nodiscard]] static auto is_right_associative(lexer::TokenKind kind) -> bool;
    [[nodiscard]] static auto token_to_binary_op(lexer::TokenKind kind) -> std::optional<BinaryOp>;
    [[nodiscard]] static auto token_to_unary_op(lexer::TokenKind kind) -> std::optional<UnaryOp>;
};

} // namespace tml::parser

#endif // TML_PARSER_PARSER_HPP
