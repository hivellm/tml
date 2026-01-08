//! # HIR Identifiers
//!
//! This module defines the unique identifier system for HIR nodes. Every node
//! in the HIR has a unique `HirId` that identifies it within a compilation session.
//!
//! ## Overview
//!
//! The HIR uses a simple incrementing integer scheme for node IDs. Each `HirId`:
//! - Is unique within a compilation session
//! - Starts from 1 (0 is reserved as `INVALID_HIR_ID`)
//! - Enables efficient node lookup in maps and sets
//! - Provides stable references for error reporting and debugging
//!
//! ## HIR Types
//!
//! HIR does not define its own type system. Instead, it reuses the semantic
//! type system from the type checker via `HirType = types::TypePtr`. This means
//! all expressions and declarations carry fully-resolved types that include:
//! - Concrete types (primitives, structs, enums)
//! - Generic instantiations with resolved type arguments
//! - Reference types with mutability information
//! - Function and closure types
//!
//! ## Example
//!
//! ```cpp
//! HirIdGenerator id_gen;
//!
//! HirId first = id_gen.next();   // 1
//! HirId second = id_gen.next();  // 2
//!
//! assert(first != second);
//! assert(first != INVALID_HIR_ID);
//! ```
//!
//! ## See Also
//!
//! - `docs/specs/31-HIR.md` - Complete HIR documentation
//! - `hir_expr.hpp` - Expression nodes that use HirId
//! - `hir_pattern.hpp` - Pattern nodes that use HirId

#pragma once

#include "common.hpp"
#include "types/type.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace tml::hir {

// ============================================================================
// Forward Declarations
// ============================================================================

struct HirExpr;
struct HirStmt;
struct HirPattern;

/// Owned pointer to an HIR expression.
/// Expressions are always heap-allocated and owned by their parent node.
using HirExprPtr = Box<HirExpr>;

/// Owned pointer to an HIR statement.
/// Statements are always heap-allocated and owned by their containing block or function.
using HirStmtPtr = Box<HirStmt>;

/// Owned pointer to an HIR pattern.
/// Patterns are always heap-allocated and owned by their containing let/when/for.
using HirPatternPtr = Box<HirPattern>;

// ============================================================================
// HIR ID Types
// ============================================================================

/// Unique identifier for HIR nodes.
///
/// Every node in the HIR (expressions, statements, patterns, declarations) has
/// a unique HirId assigned during lowering. IDs are simple incrementing integers
/// starting from 1.
///
/// ## Invariants
///
/// - Valid IDs are always >= 1
/// - ID 0 is reserved as INVALID_HIR_ID
/// - IDs are unique within a single compilation session
/// - IDs are NOT stable across compilations
///
/// ## Usage
///
/// ```cpp
/// auto expr = make_hir_literal(id_gen.next(), 42, types::make_i64(), span);
/// assert(expr->id() != INVALID_HIR_ID);
/// ```
using HirId = uint64_t;

/// Invalid HIR ID sentinel value.
///
/// This value indicates an uninitialized or invalid HIR ID. Well-formed HIR
/// should never contain nodes with this ID value.
constexpr HirId INVALID_HIR_ID = 0;

// ============================================================================
// HIR Types (using semantic types from types/type.hpp)
// ============================================================================

/// HIR uses the fully resolved semantic type system.
///
/// Unlike AST types (which may contain unresolved names or inference variables),
/// HirType is always fully resolved after type checking. This includes:
///
/// - **Primitive types**: `I32`, `Bool`, `Str`, etc.
/// - **Named types**: `Point`, `Vec[I32]` (with resolved type arguments)
/// - **Reference types**: `ref T`, `mut ref T`
/// - **Array/slice types**: `[I32; 10]`, `[T]`
/// - **Tuple types**: `(I32, Bool, Str)`
/// - **Function types**: `func(I32, I32) -> I32`
/// - **Closure types**: With capture information
///
/// ## Nullability
///
/// HirType may be null in error cases, but well-formed HIR should always have
/// non-null types on all expressions and patterns. Use defensive checks when
/// operating on potentially malformed HIR.
using HirType = types::TypePtr;

// ============================================================================
// HIR ID Generator
// ============================================================================

/// Generates unique HIR IDs for a compilation session.
///
/// The generator maintains a simple counter that starts at 1 (since 0 is
/// INVALID_HIR_ID) and increments with each call to `next()`.
///
/// ## Thread Safety
///
/// HirIdGenerator is NOT thread-safe. Each thread should have its own generator,
/// or access should be synchronized externally.
///
/// ## Example
///
/// ```cpp
/// HirIdGenerator gen;
///
/// // Generate IDs for nodes
/// HirId id1 = gen.next();  // 1
/// HirId id2 = gen.next();  // 2
/// HirId id3 = gen.next();  // 3
///
/// // Check how many IDs have been generated
/// assert(gen.count() == 3);
///
/// // Reset for a new compilation (e.g., in tests)
/// gen.reset();
/// assert(gen.next() == 1);
/// ```
class HirIdGenerator {
public:
    /// Construct a new generator.
    /// The first ID generated will be 1.
    HirIdGenerator() : next_id_(1) {} // 0 is INVALID_HIR_ID

    /// Generate a new unique HIR ID.
    ///
    /// Each call returns a new ID that has not been returned before
    /// (within this generator instance).
    ///
    /// @return A fresh, unique HirId
    auto next() -> HirId {
        return next_id_++;
    }

    /// Get the number of IDs that have been generated.
    ///
    /// This is useful for statistics and debugging to understand
    /// the size of the HIR being built.
    ///
    /// @return The count of IDs generated so far
    [[nodiscard]] auto count() const -> HirId {
        return next_id_ - 1;
    }

    /// Reset the generator to its initial state.
    ///
    /// After reset, the next call to `next()` will return 1 again.
    /// This is primarily useful for testing.
    ///
    /// @warning Do not call this in production code while HIR nodes
    ///          with existing IDs are still in use.
    void reset() {
        next_id_ = 1;
    }

private:
    HirId next_id_;
};

} // namespace tml::hir
