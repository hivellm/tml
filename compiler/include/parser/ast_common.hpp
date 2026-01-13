//! # AST Common Types
//!
//! This module contains forward declarations, pointer type aliases, and shared
//! utilities used throughout the AST module hierarchy.
//!
//! ## Architecture
//!
//! The AST is split into multiple themed headers for maintainability:
//!
//! - `ast_common.hpp` - Forward declarations and pointer types (this file)
//! - `ast_types.hpp` - Type annotations (`Type`, `RefType`, `ArrayType`, etc.)
//! - `ast_patterns.hpp` - Pattern matching (`Pattern`, `IdentPattern`, etc.)
//! - `ast_exprs.hpp` - Expressions (`Expr`, `BinaryExpr`, `CallExpr`, etc.)
//! - `ast_stmts.hpp` - Statements (`Stmt`, `LetStmt`, `VarStmt`, etc.)
//! - `ast_decls.hpp` - Declarations (`Decl`, `FuncDecl`, `StructDecl`, etc.)
//! - `ast_oop.hpp` - OOP constructs (`ClassDecl`, `InterfaceDecl`, etc.)
//! - `ast.hpp` - Main header that includes all of the above
//!
//! ## Ownership Model
//!
//! All child nodes are owned via `Box<T>` (unique pointer). This ensures
//! proper memory management and clear ownership semantics. The type aliases
//! `ExprPtr`, `StmtPtr`, `DeclPtr`, `PatternPtr`, and `TypePtr` are provided
//! for convenience.

#ifndef TML_PARSER_AST_COMMON_HPP
#define TML_PARSER_AST_COMMON_HPP

#include "common.hpp"
#include "lexer/token.hpp"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace tml::parser {

// ============================================================================
// Forward Declarations
// ============================================================================

/// Forward declaration for expression nodes.
struct Expr;

/// Forward declaration for statement nodes.
struct Stmt;

/// Forward declaration for declaration nodes.
struct Decl;

/// Forward declaration for pattern nodes.
struct Pattern;

/// Forward declaration for type nodes.
struct Type;

// ============================================================================
// Pointer Type Aliases
// ============================================================================

/// Owned pointer to an expression node.
///
/// Uses `Box<T>` (unique_ptr) for clear ownership semantics.
/// All expression children in the AST are owned through this type.
using ExprPtr = Box<Expr>;

/// Owned pointer to a statement node.
///
/// Uses `Box<T>` (unique_ptr) for clear ownership semantics.
/// All statement children in the AST are owned through this type.
using StmtPtr = Box<Stmt>;

/// Owned pointer to a declaration node.
///
/// Uses `Box<T>` (unique_ptr) for clear ownership semantics.
/// All declaration children in the AST are owned through this type.
using DeclPtr = Box<Decl>;

/// Owned pointer to a pattern node.
///
/// Uses `Box<T>` (unique_ptr) for clear ownership semantics.
/// All pattern children in the AST are owned through this type.
using PatternPtr = Box<Pattern>;

/// Owned pointer to a type node.
///
/// Uses `Box<T>` (unique_ptr) for clear ownership semantics.
/// All type children in the AST are owned through this type.
using TypePtr = Box<Type>;

} // namespace tml::parser

#endif // TML_PARSER_AST_COMMON_HPP
