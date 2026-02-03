//! # TML Parser
//!
//! This module implements the parser for TML. The parser converts a stream
//! of tokens into an abstract syntax tree (AST).
//!
//! ## Architecture
//!
//! The parser uses a Pratt parser (top-down operator precedence) for expressions,
//! which elegantly handles operator precedence and associativity. Declarations
//! and statements use recursive descent.
//!
//! ## Error Recovery
//!
//! The parser implements error recovery to continue parsing after errors,
//! collecting multiple errors in a single pass. Recovery strategies include:
//!
//! - **Synchronization**: Skip tokens until a recovery point (`;`, `}`, etc.)
//! - **Fix-it hints**: Suggest automatic corrections for common mistakes
//! - **Contextual recovery**: Different strategies for statements vs declarations
//!
//! ## Example
//!
//! ```cpp
//! Source source = Source::from_string("func main() { let x = 42; }");
//! Lexer lexer(source);
//! Parser parser(lexer.tokenize());
//!
//! auto result = parser.parse_module("main");
//! if (result.is_err()) {
//!     for (const auto& err : result.error()) {
//!         report(err);
//!     }
//! } else {
//!     Module& module = result.value();
//!     // Process AST...
//! }
//! ```

#ifndef TML_PARSER_PARSER_HPP
#define TML_PARSER_PARSER_HPP

#include "common.hpp"
#include "lexer/lexer.hpp"
#include "parser/ast.hpp"

#include <functional>
#include <vector>

namespace tml::parser {

/// Operator precedence levels (higher = tighter binding).
///
/// Used by the Pratt parser to determine which operators bind more tightly.
namespace precedence {
constexpr int NONE = 0;       ///< No precedence (initial value).
constexpr int ASSIGN = 1;     ///< `=`, `+=`, etc. (right-associative).
constexpr int TERNARY = 2;    ///< `? :` ternary operator.
constexpr int OR = 3;         ///< `or` / `||` logical OR.
constexpr int AND = 4;        ///< `and` / `&&` logical AND.
constexpr int COMPARISON = 5; ///< `==`, `!=`, `<`, `>`, `<=`, `>=`.
constexpr int BITOR = 6;      ///< `|` bitwise OR.
constexpr int BITXOR = 7;     ///< `^` / `xor` bitwise XOR.
constexpr int BITAND = 8;     ///< `&` bitwise AND.
constexpr int SHIFT = 9;      ///< `<<`, `>>` / `shl`, `shr`.
constexpr int TERM = 10;      ///< `+`, `-` addition/subtraction.
constexpr int FACTOR = 11;    ///< `*`, `/`, `%` multiplication/division.
constexpr int CAST = 12;      ///< `as` type casting.
constexpr int UNARY = 13;     ///< `-`, `not`, `~`, `ref`, `*`.
constexpr int CALL = 14;      ///< `()`, `[]`, `.` call and access.
constexpr int RANGE = 15;     ///< `to`, `through` ranges.
} // namespace precedence

/// A suggested fix for a parse error.
///
/// Fix-it hints allow tools to automatically correct common mistakes.
struct FixItHint {
    SourceSpan span;         ///< Where to apply the fix.
    std::string replacement; ///< Text to insert/replace.
    std::string description; ///< Human-readable description.
};

/// A parse error with location, message, and optional fixes.
struct ParseError {
    std::string message;            ///< Error message.
    SourceSpan span;                ///< Error location.
    std::vector<std::string> notes; ///< Additional notes.
    std::vector<FixItHint> fixes;   ///< Suggested fixes.
    std::string code;               ///< Error code (e.g., "P001"). Empty uses default.
};

/// Parser for TML source code.
///
/// Converts a token stream into an AST. Uses Pratt parsing for expressions
/// and recursive descent for declarations and statements.
///
/// # Usage
///
/// ```cpp
/// Parser parser(tokens);
/// auto result = parser.parse_module("main");
/// ```
class Parser {
public:
    /// Constructs a parser from a token stream.
    explicit Parser(std::vector<lexer::Token> tokens);

    /// Parses an entire module, returning all declarations.
    [[nodiscard]] auto parse_module(const std::string& name)
        -> Result<Module, std::vector<ParseError>>;

    /// Parses a single declaration (for testing).
    [[nodiscard]] auto parse_decl() -> Result<DeclPtr, ParseError>;

    /// Parses a single expression (for testing).
    [[nodiscard]] auto parse_expr() -> Result<ExprPtr, ParseError>;

    /// Parses a single statement (for testing).
    [[nodiscard]] auto parse_stmt() -> Result<StmtPtr, ParseError>;

    /// Returns all accumulated errors.
    [[nodiscard]] auto errors() const -> const std::vector<ParseError>& {
        return errors_;
    }

    /// Returns true if any errors occurred.
    [[nodiscard]] auto has_errors() const -> bool {
        return !errors_.empty();
    }

private:
    // ========================================================================
    // State
    // ========================================================================

    std::vector<lexer::Token> tokens_; ///< Token stream.
    size_t pos_ = 0;                   ///< Current position.
    std::vector<ParseError> errors_;   ///< Accumulated errors.

    // ========================================================================
    // Token Access
    // ========================================================================

    /// Returns current token without consuming.
    [[nodiscard]] auto peek() const -> const lexer::Token&;

    /// Returns next token without consuming.
    [[nodiscard]] auto peek_next() const -> const lexer::Token&;

    /// Returns previous token.
    [[nodiscard]] auto previous() const -> const lexer::Token&;

    /// Consumes and returns current token.
    auto advance() -> const lexer::Token&;

    /// Returns true if at end of tokens.
    [[nodiscard]] auto is_at_end() const -> bool;

    /// Returns true if current token is of given kind.
    [[nodiscard]] auto check(lexer::TokenKind kind) const -> bool;

    /// Returns true if next token is of given kind.
    [[nodiscard]] auto check_next(lexer::TokenKind kind) const -> bool;

    /// Consumes current token if it matches, returns true if matched.
    auto match(lexer::TokenKind kind) -> bool;

    /// Expects and consumes a token of given kind, or returns error.
    auto expect(lexer::TokenKind kind, const std::string& message)
        -> Result<lexer::Token, ParseError>;

    /// Skips insignificant newlines.
    void skip_newlines();

    /// Collects a doc comment if present (without consuming following newlines).
    /// Returns nullopt if no doc comment is found.
    [[nodiscard]] auto collect_doc_comment() -> std::optional<std::string>;

    // ========================================================================
    // Error Handling
    // ========================================================================

    /// Reports an error at current position.
    void report_error(const std::string& message);

    /// Reports an error at specified span.
    void report_error(const std::string& message, SourceSpan span);

    /// Reports an error with suggested fixes.
    void report_error_with_fix(const std::string& message, SourceSpan span,
                               const std::vector<FixItHint>& fixes);

    /// Synchronizes after an error (skip to recovery point).
    void synchronize();

    // ========================================================================
    // Enhanced Error Recovery
    // ========================================================================

    /// Recovers to next statement boundary.
    void synchronize_to_stmt();

    /// Recovers to next declaration boundary.
    void synchronize_to_decl();

    /// Recovers to closing brace.
    void synchronize_to_brace();

    /// Tries to recover from missing semicolon.
    bool try_recover_missing_semi();

    /// Skips tokens until finding the specified kind.
    bool skip_until(lexer::TokenKind kind);

    /// Skips tokens until finding any of the specified kinds.
    bool skip_until_any(std::initializer_list<lexer::TokenKind> kinds);

    // ========================================================================
    // Fix-it Hint Helpers
    // ========================================================================

    /// Creates an insertion fix.
    [[nodiscard]] auto make_insertion_fix(const SourceSpan& at, const std::string& text,
                                          const std::string& desc) -> FixItHint;

    /// Creates a replacement fix.
    [[nodiscard]] auto make_replacement_fix(const SourceSpan& span, const std::string& text,
                                            const std::string& desc) -> FixItHint;

    /// Creates a deletion fix.
    [[nodiscard]] auto make_deletion_fix(const SourceSpan& span, const std::string& desc)
        -> FixItHint;

    // ========================================================================
    // Declaration Parsing
    // ========================================================================

    /// Parses visibility modifier (`pub`, `pub(crate)`).
    auto parse_visibility() -> Visibility;

    /// Parses decorators (`@name`, `@name(args)`).
    auto parse_decorators() -> Result<std::vector<Decorator>, ParseError>;

    /// Parses function declaration.
    auto parse_func_decl(Visibility vis, std::vector<Decorator> decorators = {},
                         std::optional<std::string> doc = std::nullopt)
        -> Result<DeclPtr, ParseError>;

    /// Parses struct declaration.
    auto parse_struct_decl(Visibility vis, std::vector<Decorator> decorators = {},
                           std::optional<std::string> doc = std::nullopt)
        -> Result<DeclPtr, ParseError>;

    /// Parses enum declaration.
    auto parse_enum_decl(Visibility vis, std::vector<Decorator> decorators = {},
                         std::optional<std::string> doc = std::nullopt)
        -> Result<DeclPtr, ParseError>;

    /// Parses behavior (trait) declaration.
    auto parse_trait_decl(Visibility vis, std::vector<Decorator> decorators = {},
                          std::optional<std::string> doc = std::nullopt)
        -> Result<DeclPtr, ParseError>;

    /// Parses impl block.
    auto parse_impl_decl(std::optional<std::string> doc = std::nullopt)
        -> Result<DeclPtr, ParseError>;

    /// Parses type alias.
    auto parse_type_alias_decl(Visibility vis, std::optional<std::string> doc = std::nullopt)
        -> Result<DeclPtr, ParseError>;

    /// Parses sum type declaration (type Foo = V1 | V2(T) | V3).
    auto parse_sum_type_decl(Visibility vis, std::vector<Decorator> decorators = {},
                             std::optional<std::string> doc = std::nullopt)
        -> Result<DeclPtr, ParseError>;

    /// Parses a single variant in a sum type.
    auto parse_sum_type_variant() -> Result<EnumVariant, ParseError>;

    /// Parses const declaration.
    auto parse_const_decl(Visibility vis, std::optional<std::string> doc = std::nullopt)
        -> Result<DeclPtr, ParseError>;

    /// Parses use declaration.
    auto parse_use_decl(Visibility vis) -> Result<DeclPtr, ParseError>;

    /// Parses module declaration.
    auto parse_mod_decl(Visibility vis) -> Result<DeclPtr, ParseError>;

    // ========================================================================
    // OOP Declarations (C#-style)
    // ========================================================================

    /// Parses class declaration.
    auto parse_class_decl(Visibility vis, std::vector<Decorator> decorators = {},
                          std::optional<std::string> doc = std::nullopt)
        -> Result<DeclPtr, ParseError>;

    /// Parses interface declaration.
    auto parse_interface_decl(Visibility vis, std::vector<Decorator> decorators = {},
                              std::optional<std::string> doc = std::nullopt)
        -> Result<DeclPtr, ParseError>;

    /// Parses namespace declaration.
    auto parse_namespace_decl() -> Result<DeclPtr, ParseError>;

    /// Parses class member (field, method, property, constructor).
    auto parse_class_member(const std::string& class_name)
        -> Result<std::variant<ClassField, ClassMethod, PropertyDecl, ConstructorDecl>, ParseError>;

    /// Parses member visibility (private, protected, pub).
    auto parse_member_visibility() -> MemberVisibility;

    // ========================================================================
    // Generic Parsing
    // ========================================================================

    /// Parses generic parameters (`[T, U: Clone]`).
    auto parse_generic_params() -> Result<std::vector<GenericParam>, ParseError>;

    /// Parses where clause.
    auto parse_where_clause() -> Result<std::optional<WhereClause>, ParseError>;

    // ========================================================================
    // Function Parsing
    // ========================================================================

    /// Parses function parameters.
    auto parse_func_params() -> Result<std::vector<FuncParam>, ParseError>;

    /// Parses a single function parameter.
    auto parse_func_param() -> Result<FuncParam, ParseError>;

    // ========================================================================
    // Statement Parsing
    // ========================================================================

    /// Parses let statement.
    auto parse_let_stmt() -> Result<StmtPtr, ParseError>;

    /// Parses var statement.
    auto parse_var_stmt() -> Result<StmtPtr, ParseError>;

    /// Parses expression statement.
    auto parse_expr_stmt() -> Result<StmtPtr, ParseError>;

    // ========================================================================
    // Expression Parsing (Pratt Parser)
    // ========================================================================

    /// Parses expression with given minimum precedence.
    auto parse_expr_with_precedence(int min_precedence) -> Result<ExprPtr, ParseError>;

    /// Parses primary expression (atoms).
    auto parse_primary_expr() -> Result<ExprPtr, ParseError>;

    /// Parses primary expression with postfix operators.
    auto parse_primary_with_postfix() -> Result<ExprPtr, ParseError>;

    /// Parses prefix expression.
    auto parse_prefix_expr() -> Result<ExprPtr, ParseError>;

    /// Parses postfix expression.
    auto parse_postfix_expr(ExprPtr left) -> Result<ExprPtr, ParseError>;

    /// Parses infix expression.
    auto parse_infix_expr(ExprPtr left, int precedence) -> Result<ExprPtr, ParseError>;

    // ========================================================================
    // Specific Expression Parsing
    // ========================================================================

    /// Parses literal expression.
    auto parse_literal_expr() -> Result<ExprPtr, ParseError>;

    /// Parses identifier or path expression.
    auto parse_ident_or_path_expr() -> Result<ExprPtr, ParseError>;

    /// Parses parenthesized or tuple expression.
    auto parse_paren_or_tuple_expr() -> Result<ExprPtr, ParseError>;

    /// Parses array expression.
    auto parse_array_expr() -> Result<ExprPtr, ParseError>;

    /// Parses block expression.
    auto parse_block_expr() -> Result<ExprPtr, ParseError>;

    /// Parses if expression.
    auto parse_if_expr() -> Result<ExprPtr, ParseError>;

    /// Parses if-let expression.
    auto parse_if_let_expr(SourceSpan start_span) -> Result<ExprPtr, ParseError>;

    /// Parses when (match) expression.
    auto parse_when_expr() -> Result<ExprPtr, ParseError>;

    /// Parses loop expression.
    auto parse_loop_expr() -> Result<ExprPtr, ParseError>;

    /// Parses while expression.
    auto parse_while_expr() -> Result<ExprPtr, ParseError>;

    /// Parses for expression.
    auto parse_for_expr() -> Result<ExprPtr, ParseError>;

    /// Parses return expression.
    auto parse_return_expr() -> Result<ExprPtr, ParseError>;

    /// Parses throw expression: `throw expr`
    auto parse_throw_expr() -> Result<ExprPtr, ParseError>;

    /// Parses break expression.
    auto parse_break_expr() -> Result<ExprPtr, ParseError>;

    /// Parses continue expression.
    auto parse_continue_expr() -> Result<ExprPtr, ParseError>;

    /// Parses closure expression.
    auto parse_closure_expr() -> Result<ExprPtr, ParseError>;

    /// Parses struct expression.
    auto parse_struct_expr(TypePath path, std::optional<GenericArgs> generics = std::nullopt)
        -> Result<ExprPtr, ParseError>;

    /// Parses lowlevel block expression.
    auto parse_lowlevel_expr() -> Result<ExprPtr, ParseError>;

    /// Parses base expression for parent class access.
    auto parse_base_expr() -> Result<ExprPtr, ParseError>;

    /// Parses interpolated string expression.
    auto parse_interp_string_expr() -> Result<ExprPtr, ParseError>;

    /// Parses template literal expression (produces Text type).
    auto parse_template_literal_expr() -> Result<ExprPtr, ParseError>;

    // ========================================================================
    // Call and Member Access
    // ========================================================================

    /// Parses call arguments.
    auto parse_call_args() -> Result<std::vector<ExprPtr>, ParseError>;

    // ========================================================================
    // Type Parsing
    // ========================================================================

    /// Parses a type.
    auto parse_type() -> Result<TypePtr, ParseError>;

    /// Parses a type path.
    auto parse_type_path() -> Result<TypePath, ParseError>;

    /// Parses generic arguments (`[T, U]`).
    auto parse_generic_args() -> Result<std::optional<GenericArgs>, ParseError>;

    // ========================================================================
    // Pattern Parsing
    // ========================================================================

    /// Parses a pattern.
    auto parse_pattern() -> Result<PatternPtr, ParseError>;

    /// Parses a pattern without or-patterns.
    auto parse_pattern_no_or() -> Result<PatternPtr, ParseError>;

    // ========================================================================
    // Precedence and Associativity
    // ========================================================================

    /// Gets precedence for a token kind.
    [[nodiscard]] static auto get_precedence(lexer::TokenKind kind) -> int;

    /// Returns true if operator is right-associative.
    [[nodiscard]] static auto is_right_associative(lexer::TokenKind kind) -> bool;

    /// Converts token kind to binary operator.
    [[nodiscard]] static auto token_to_binary_op(lexer::TokenKind kind) -> std::optional<BinaryOp>;

    /// Converts token kind to unary operator.
    [[nodiscard]] static auto token_to_unary_op(lexer::TokenKind kind) -> std::optional<UnaryOp>;
};

} // namespace tml::parser

#endif // TML_PARSER_PARSER_HPP
