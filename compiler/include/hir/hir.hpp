//! # High-level Intermediate Representation (HIR)
//!
//! HIR is a type-resolved, desugared representation of TML programs that sits
//! between the parser AST and MIR. It preserves high-level constructs while
//! providing fully resolved types and simplified syntax.
//!
//! ## Design Goals
//!
//! 1. **Type Resolution** - All types are fully resolved (no inference variables)
//! 2. **Desugaring** - Syntactic sugar is expanded (for -> loop, etc.)
//! 3. **Generic Monomorphization** - Generics are instantiated with concrete types
//! 4. **Explicit Ownership** - Ownership and borrowing are explicit
//! 5. **Optimizable** - High-level enough for constant folding, inlining
//!
//! ## Module Structure
//!
//! - `hir.hpp` - Main header (this file), includes all HIR components
//! - `hir_id.hpp` - HIR ID types and generator
//! - `hir_pattern.hpp` - Pattern definitions
//! - `hir_expr.hpp` - Expression definitions
//! - `hir_stmt.hpp` - Statement definitions
//! - `hir_decl.hpp` - Declaration definitions (functions, structs, enums)
//! - `hir_module.hpp` - Module container
//!
//! ## Pipeline Position
//!
//! ```
//! Source -> Lexer -> Parser (AST) -> TypeChecker -> HIR -> MIR -> LLVM IR
//! ```

#pragma once

// Include all HIR components
#include "hir/hir_decl.hpp"
#include "hir/hir_expr.hpp"
#include "hir/hir_id.hpp"
#include "hir/hir_module.hpp"
#include "hir/hir_pattern.hpp"
#include "hir/hir_printer.hpp"
#include "hir/hir_stmt.hpp"
