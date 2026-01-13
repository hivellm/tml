//! # Abstract Syntax Tree (AST)
//!
//! This module defines the abstract syntax tree nodes for TML. The AST is
//! produced by the parser and consumed by semantic analysis and code generation.
//!
//! ## Architecture
//!
//! The AST uses a variant-based design with multiple node categories,
//! organized into themed headers for maintainability:
//!
//! - `ast_common.hpp` - Forward declarations and pointer types
//! - `ast_types.hpp` - Type annotations (`Type`, `RefType`, `ArrayType`, etc.)
//! - `ast_patterns.hpp` - Pattern matching (`Pattern`, `IdentPattern`, etc.)
//! - `ast_exprs.hpp` - Expressions (`Expr`, `BinaryExpr`, `CallExpr`, etc.)
//! - `ast_stmts.hpp` - Statements (`Stmt`, `LetStmt`, `VarStmt`, etc.)
//! - `ast_decls.hpp` - Declarations (`FuncDecl`, `StructDecl`, `TraitDecl`, etc.)
//! - `ast_oop.hpp` - OOP constructs (`ClassDecl`, `InterfaceDecl`, etc.)
//! - `ast.hpp` - Main header with `Decl` variant and `Module` (this file)
//!
//! ## Ownership Model
//!
//! All child nodes are owned via `Box<T>` (unique pointer). This ensures
//! proper memory management and clear ownership semantics. The type aliases
//! `ExprPtr`, `StmtPtr`, `DeclPtr`, `PatternPtr`, and `TypePtr` are provided
//! for convenience.
//!
//! ## Source Spans
//!
//! Every AST node includes a `SourceSpan` for error reporting and debugging.
//! Spans are preserved through all compiler phases.
//!
//! ## TML-Specific Nodes
//!
//! - `WhenExpr` - Pattern matching (instead of `match`)
//! - `ClosureExpr` - Closures using `do(x) expr` syntax
//! - `TraitDecl` - Behaviors (TML's term for traits)
//! - `DynType` - Dynamic trait objects
//! - `LowlevelExpr` - Unsafe blocks (called `lowlevel` in TML)
//! - `ClassDecl` - C#-style classes with inheritance
//! - `InterfaceDecl` - C#-style interfaces

#ifndef TML_PARSER_AST_HPP
#define TML_PARSER_AST_HPP

// Include all themed AST headers
#include "ast_common.hpp"
#include "ast_decls.hpp"
#include "ast_exprs.hpp"
#include "ast_oop.hpp"
#include "ast_patterns.hpp"
#include "ast_stmts.hpp"
#include "ast_types.hpp"

namespace tml::parser {

// ============================================================================
// Declaration Variant
// ============================================================================

/// A top-level or nested declaration.
///
/// Declarations define named items: functions, types, behaviors, constants,
/// modules, imports, classes, interfaces, and namespaces.
///
/// Uses `std::variant` for type-safe sum types with helper methods
/// for type checking and casting.
///
/// ## Declaration Types
///
/// | Declaration | Description |
/// |-------------|-------------|
/// | `FuncDecl` | Function or method |
/// | `StructDecl` | Struct type |
/// | `EnumDecl` | Enum type |
/// | `TraitDecl` | Behavior (trait) |
/// | `ImplDecl` | Implementation block |
/// | `TypeAliasDecl` | Type alias |
/// | `ConstDecl` | Constant |
/// | `UseDecl` | Import |
/// | `ModDecl` | Module |
/// | `ClassDecl` | OOP class |
/// | `InterfaceDecl` | OOP interface |
/// | `NamespaceDecl` | Namespace |
struct Decl {
    std::variant<FuncDecl, StructDecl, EnumDecl, TraitDecl, ImplDecl, TypeAliasDecl, ConstDecl,
                 UseDecl, ModDecl, ClassDecl, InterfaceDecl, NamespaceDecl>
        kind;        ///< The declaration variant.
    SourceSpan span; ///< Source location.

    /// Checks if this declaration is of kind `T`.
    ///
    /// # Example
    /// ```cpp
    /// if (decl.is<FuncDecl>()) { ... }
    /// ```
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Gets this declaration as kind `T`. Throws `std::bad_variant_access` if wrong kind.
    ///
    /// # Example
    /// ```cpp
    /// auto& func = decl.as<FuncDecl>();
    /// ```
    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    /// Gets this declaration as kind `T` (const). Throws `std::bad_variant_access` if wrong kind.
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }
};

// ============================================================================
// Module (Top-Level AST)
// ============================================================================

/// A TML module (compilation unit).
///
/// Represents a single source file after parsing. Contains all top-level
/// declarations and module-level documentation.
///
/// # Structure
///
/// ```tml
/// //! Module documentation
/// //! This module provides utilities for...
///
/// use std::io::Read
///
/// pub func main() {
///     // ...
/// }
/// ```
struct Module {
    std::string name;                     ///< Module name (from file name).
    std::vector<std::string> module_docs; ///< Module-level documentation (from `//!`).
    std::vector<DeclPtr> decls;           ///< Top-level declarations.
    SourceSpan span;                      ///< Source location.
};

// ============================================================================
// AST Utilities
// ============================================================================

/// Creates a literal expression from a token.
///
/// # Parameters
/// - `token`: The literal token (int, float, string, char, bool)
///
/// # Returns
/// An owned `ExprPtr` containing a `LiteralExpr`
auto make_literal_expr(lexer::Token token) -> ExprPtr;

/// Creates an identifier expression.
///
/// # Parameters
/// - `name`: The identifier name
/// - `span`: Source location
///
/// # Returns
/// An owned `ExprPtr` containing an `IdentExpr`
auto make_ident_expr(std::string name, SourceSpan span) -> ExprPtr;

/// Creates a binary expression.
///
/// # Parameters
/// - `op`: The binary operator
/// - `left`: Left operand
/// - `right`: Right operand
/// - `span`: Source location
///
/// # Returns
/// An owned `ExprPtr` containing a `BinaryExpr`
auto make_binary_expr(BinaryOp op, ExprPtr left, ExprPtr right, SourceSpan span) -> ExprPtr;

/// Creates a unary expression.
///
/// # Parameters
/// - `op`: The unary operator
/// - `operand`: The operand
/// - `span`: Source location
///
/// # Returns
/// An owned `ExprPtr` containing a `UnaryExpr`
auto make_unary_expr(UnaryOp op, ExprPtr operand, SourceSpan span) -> ExprPtr;

/// Creates a call expression.
///
/// # Parameters
/// - `callee`: The function being called
/// - `args`: Call arguments
/// - `span`: Source location
///
/// # Returns
/// An owned `ExprPtr` containing a `CallExpr`
auto make_call_expr(ExprPtr callee, std::vector<ExprPtr> args, SourceSpan span) -> ExprPtr;

/// Creates a block expression.
///
/// # Parameters
/// - `stmts`: Statements in the block
/// - `expr`: Optional trailing expression
/// - `span`: Source location
///
/// # Returns
/// An owned `ExprPtr` containing a `BlockExpr`
auto make_block_expr(std::vector<StmtPtr> stmts, std::optional<ExprPtr> expr, SourceSpan span)
    -> ExprPtr;

/// Creates a named type.
///
/// # Parameters
/// - `name`: The type name
/// - `span`: Source location
///
/// # Returns
/// An owned `TypePtr` containing a `NamedType`
auto make_named_type(std::string name, SourceSpan span) -> TypePtr;

/// Creates a reference type.
///
/// # Parameters
/// - `is_mut`: True for mutable reference
/// - `inner`: The referenced type
/// - `span`: Source location
///
/// # Returns
/// An owned `TypePtr` containing a `RefType`
auto make_ref_type(bool is_mut, TypePtr inner, SourceSpan span) -> TypePtr;

/// Creates an identifier pattern.
///
/// # Parameters
/// - `name`: The identifier name
/// - `is_mut`: True for mutable binding
/// - `span`: Source location
///
/// # Returns
/// An owned `PatternPtr` containing an `IdentPattern`
auto make_ident_pattern(std::string name, bool is_mut, SourceSpan span) -> PatternPtr;

/// Creates a wildcard pattern.
///
/// # Parameters
/// - `span`: Source location
///
/// # Returns
/// An owned `PatternPtr` containing a `WildcardPattern`
auto make_wildcard_pattern(SourceSpan span) -> PatternPtr;

} // namespace tml::parser

#endif // TML_PARSER_AST_HPP
