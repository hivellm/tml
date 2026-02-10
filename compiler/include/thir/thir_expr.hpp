//! # THIR Expressions
//!
//! Typed High-level IR expressions. THIR mirrors HIR but adds:
//! - Explicit coercion nodes (numeric widening, auto-deref, auto-ref)
//! - Resolved method dispatch info on method calls
//! - Operator overloading desugared to method calls
//!
//! ## Differences from HIR
//!
//! | HIR | THIR | Change |
//! |-----|------|--------|
//! | `HirBinaryExpr(I8 + I32)` | `ThirBinaryExpr(ThirCoercionExpr(lhs), rhs)` | Coercion explicit |
//! | `HirMethodCallExpr` | `ThirMethodCallExpr` with `ResolvedMethod` | Dispatch resolved |
//! | `HirBinaryExpr(a + b)` overloaded | `ThirMethodCallExpr(a, Add::add, [b])` | Op desugared |

#pragma once

#include "hir/hir_expr.hpp"
#include "hir/hir_id.hpp"
#include "hir/hir_pattern.hpp"

namespace tml::thir {

// ============================================================================
// Forward Declarations
// ============================================================================

struct ThirExpr;
struct ThirStmt;
struct ThirPattern;

using ThirExprPtr = Box<ThirExpr>;
using ThirStmtPtr = Box<ThirStmt>;
using ThirPatternPtr = Box<ThirPattern>;

/// THIR reuses the same type representation as HIR.
using ThirType = types::TypePtr;

/// THIR reuses HIR IDs.
using ThirId = hir::HirId;
constexpr ThirId INVALID_THIR_ID = hir::INVALID_HIR_ID;

// ============================================================================
// Coercion Kinds
// ============================================================================

/// Coercion kinds materialized in THIR.
///
/// In HIR, implicit coercions (e.g., I8 used where I32 is expected) are not
/// represented. THIR makes them explicit so MIR building can emit the correct
/// instructions without re-deriving coercion rules.
enum class CoercionKind {
    IntWidening,    ///< Signed int widening: I8 -> I32 (sign-extend)
    UintWidening,   ///< Unsigned int widening: U8 -> U32 (zero-extend)
    FloatWidening,  ///< Float widening: F32 -> F64
    IntToFloat,     ///< Integer to float: I32 -> F64
    DerefCoercion,  ///< Auto-deref: ref ref T -> ref T
    RefCoercion,    ///< Auto-ref: T -> ref T (for method receivers)
    MutToShared,    ///< Mutable to shared ref: mut ref T -> ref T
    NeverCoercion,  ///< Never type coercion: Never -> any type
    UnsizeCoercion, ///< Array to slice: [T; N] -> [T]
};

// ============================================================================
// Resolved Method Dispatch
// ============================================================================

/// Fully resolved method dispatch information.
///
/// In HIR, method calls carry the method name and receiver type, but the
/// exact implementation to call is not resolved. THIR resolves this via
/// the trait solver, producing a `ResolvedMethod` that tells codegen exactly
/// which function to call.
struct ResolvedMethod {
    /// Fully qualified function name (e.g., "Point::distance", "Display::display").
    std::string qualified_name;

    /// Behavior name if this is a trait method (None for inherent methods).
    std::optional<std::string> behavior_name;

    /// Monomorphized type arguments for the method.
    std::vector<ThirType> type_args;

    /// True if this requires dynamic dispatch (dyn Behavior).
    bool is_virtual = false;
};

// ============================================================================
// Expression Definitions
// ============================================================================

/// Explicit coercion: wraps an expression with a type conversion.
///
/// This node does not exist in HIR. It is inserted by THIR lowering when
/// the type checker determined an implicit coercion was needed.
struct ThirCoercionExpr {
    ThirId id;
    CoercionKind coercion;
    ThirExprPtr inner;
    ThirType source_type; ///< Type before coercion
    ThirType type;        ///< Type after coercion
    SourceSpan span;
};

/// Literal expression: `42`, `3.14`, `"hello"`, `true`
struct ThirLiteralExpr {
    ThirId id;
    std::variant<int64_t, uint64_t, double, bool, char, std::string> value;
    ThirType type;
    SourceSpan span;
};

/// Variable reference: `x`
struct ThirVarExpr {
    ThirId id;
    std::string name;
    ThirType type;
    SourceSpan span;
};

/// Binary operation: `a + b`, `x == y`
///
/// If operator overloading applies, `operator_method` is populated with the
/// resolved method. The THIR->MIR builder then emits a method call instead
/// of a primitive operation.
struct ThirBinaryExpr {
    ThirId id;
    hir::HirBinOp op;
    ThirExprPtr left;
    ThirExprPtr right;
    ThirType type;
    /// Non-empty if this binary op is desugared to a method call (operator overloading).
    std::optional<ResolvedMethod> operator_method;
    SourceSpan span;
};

/// Unary operation: `-x`, `not x`, `ref x`, `*x`
struct ThirUnaryExpr {
    ThirId id;
    hir::HirUnaryOp op;
    ThirExprPtr operand;
    ThirType type;
    SourceSpan span;
};

/// Function call: `foo(a, b)`
struct ThirCallExpr {
    ThirId id;
    std::string func_name;
    std::vector<ThirType> type_args;
    std::vector<ThirExprPtr> args;
    ThirType type;
    SourceSpan span;
};

/// Method call with fully resolved dispatch: `obj.method(a, b)`
struct ThirMethodCallExpr {
    ThirId id;
    ThirExprPtr receiver;
    ResolvedMethod resolved; ///< Fully resolved dispatch info
    std::vector<ThirExprPtr> args;
    ThirType receiver_type;
    ThirType type;
    SourceSpan span;
};

/// Field access: `obj.field`
struct ThirFieldExpr {
    ThirId id;
    ThirExprPtr object;
    std::string field_name;
    int field_index;
    ThirType type;
    SourceSpan span;
};

/// Index expression: `arr[i]`
struct ThirIndexExpr {
    ThirId id;
    ThirExprPtr object;
    ThirExprPtr index;
    ThirType type;
    SourceSpan span;
};

/// Tuple expression: `(a, b, c)`
struct ThirTupleExpr {
    ThirId id;
    std::vector<ThirExprPtr> elements;
    ThirType type;
    SourceSpan span;
};

/// Array expression: `[1, 2, 3]`
struct ThirArrayExpr {
    ThirId id;
    std::vector<ThirExprPtr> elements;
    ThirType element_type;
    size_t size;
    ThirType type;
    SourceSpan span;
};

/// Array repeat expression: `[0; 10]`
struct ThirArrayRepeatExpr {
    ThirId id;
    ThirExprPtr value;
    size_t count;
    ThirType type;
    SourceSpan span;
};

/// Struct construction: `Point { x: 1, y: 2 }`
struct ThirStructExpr {
    ThirId id;
    std::string struct_name;
    std::vector<ThirType> type_args;
    std::vector<std::pair<std::string, ThirExprPtr>> fields;
    std::optional<ThirExprPtr> base;
    ThirType type;
    SourceSpan span;
};

/// Enum variant construction: `Just(x)`, `Nothing`
struct ThirEnumExpr {
    ThirId id;
    std::string enum_name;
    std::string variant_name;
    int variant_index;
    std::vector<ThirType> type_args;
    std::vector<ThirExprPtr> payload;
    ThirType type;
    SourceSpan span;
};

/// Block expression: `{ stmts; expr }`
struct ThirBlockExpr {
    ThirId id;
    std::vector<ThirStmtPtr> stmts;
    std::optional<ThirExprPtr> expr;
    ThirType type;
    SourceSpan span;
};

/// If expression: `if cond { then } else { else }`
struct ThirIfExpr {
    ThirId id;
    ThirExprPtr condition;
    ThirExprPtr then_branch;
    std::optional<ThirExprPtr> else_branch;
    ThirType type;
    SourceSpan span;
};

/// Match arm for when expression.
struct ThirWhenArm {
    ThirPatternPtr pattern;
    std::optional<ThirExprPtr> guard;
    ThirExprPtr body;
    SourceSpan span;
};

/// When (match) expression: `when x { pat => expr, ... }`
///
/// THIR when expressions have been checked for exhaustiveness.
/// If the checker found missing patterns, diagnostics were emitted
/// during THIR lowering.
struct ThirWhenExpr {
    ThirId id;
    ThirExprPtr scrutinee;
    std::vector<ThirWhenArm> arms;
    ThirType type;
    bool is_exhaustive = true; ///< Set by exhaustiveness checker
    SourceSpan span;
};

/// Loop variable declaration.
struct ThirLoopVarDecl {
    std::string name;
    ThirType type;
    SourceSpan span;
};

/// Loop expression: `loop (condition) { body }`
struct ThirLoopExpr {
    ThirId id;
    std::optional<std::string> label;
    std::optional<ThirLoopVarDecl> loop_var;
    ThirExprPtr condition;
    ThirExprPtr body;
    ThirType type;
    SourceSpan span;
};

/// While loop: `while cond { body }`
struct ThirWhileExpr {
    ThirId id;
    std::optional<std::string> label;
    ThirExprPtr condition;
    ThirExprPtr body;
    ThirType type;
    SourceSpan span;
};

/// For loop: `for x in iter { body }`
struct ThirForExpr {
    ThirId id;
    std::optional<std::string> label;
    ThirPatternPtr pattern;
    ThirExprPtr iter;
    ThirExprPtr body;
    ThirType type;
    SourceSpan span;
};

/// Return expression: `return x`
struct ThirReturnExpr {
    ThirId id;
    std::optional<ThirExprPtr> value;
    SourceSpan span;
};

/// Break expression: `break 'label x`
struct ThirBreakExpr {
    ThirId id;
    std::optional<std::string> label;
    std::optional<ThirExprPtr> value;
    SourceSpan span;
};

/// Continue expression: `continue 'label`
struct ThirContinueExpr {
    ThirId id;
    std::optional<std::string> label;
    SourceSpan span;
};

/// Captured variable in a closure.
struct ThirCapture {
    std::string name;
    ThirType type;
    bool is_mut;
    bool by_move;
};

/// Closure expression: `do(x, y) x + y`
struct ThirClosureExpr {
    ThirId id;
    std::vector<std::pair<std::string, ThirType>> params;
    ThirExprPtr body;
    std::vector<ThirCapture> captures;
    ThirType type;
    SourceSpan span;
};

/// Cast expression: `x as T`
struct ThirCastExpr {
    ThirId id;
    ThirExprPtr expr;
    ThirType target_type;
    ThirType type;
    SourceSpan span;
};

/// Try expression: `expr!`
struct ThirTryExpr {
    ThirId id;
    ThirExprPtr expr;
    ThirType type;
    SourceSpan span;
};

/// Await expression: `expr.await`
struct ThirAwaitExpr {
    ThirId id;
    ThirExprPtr expr;
    ThirType type;
    SourceSpan span;
};

/// Assignment expression: `x = y`
struct ThirAssignExpr {
    ThirId id;
    ThirExprPtr target;
    ThirExprPtr value;
    SourceSpan span;
};

/// Compound assignment: `x += y`
struct ThirCompoundAssignExpr {
    ThirId id;
    hir::HirCompoundOp op;
    ThirExprPtr target;
    ThirExprPtr value;
    /// Non-empty if this compound op is desugared to a method call.
    std::optional<ResolvedMethod> operator_method;
    SourceSpan span;
};

/// Lowlevel (unsafe) block: `lowlevel { ... }`
struct ThirLowlevelExpr {
    ThirId id;
    std::vector<ThirStmtPtr> stmts;
    std::optional<ThirExprPtr> expr;
    ThirType type;
    SourceSpan span;
};

// ============================================================================
// ThirExpr Container
// ============================================================================

/// A THIR expression.
///
/// Same variant pattern as HIR, with the addition of `ThirCoercionExpr`.
struct ThirExpr {
    std::variant<ThirLiteralExpr, ThirVarExpr, ThirBinaryExpr, ThirUnaryExpr, ThirCallExpr,
                 ThirMethodCallExpr, ThirFieldExpr, ThirIndexExpr, ThirTupleExpr, ThirArrayExpr,
                 ThirArrayRepeatExpr, ThirStructExpr, ThirEnumExpr, ThirBlockExpr, ThirIfExpr,
                 ThirWhenExpr, ThirLoopExpr, ThirWhileExpr, ThirForExpr, ThirReturnExpr,
                 ThirBreakExpr, ThirContinueExpr, ThirClosureExpr, ThirCastExpr, ThirTryExpr,
                 ThirAwaitExpr, ThirAssignExpr, ThirCompoundAssignExpr, ThirLowlevelExpr,
                 ThirCoercionExpr>
        kind;

    /// Get the THIR ID for this expression.
    [[nodiscard]] auto id() const -> ThirId;

    /// Get the type of this expression.
    [[nodiscard]] auto type() const -> ThirType;

    /// Get the source span.
    [[nodiscard]] auto span() const -> SourceSpan;

    /// Check if this expression is of kind `T`.
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Get this expression as kind `T` (mutable).
    template <typename T> [[nodiscard]] auto as() -> T& {
        return std::get<T>(kind);
    }

    /// Get this expression as kind `T` (const).
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        return std::get<T>(kind);
    }
};

// ============================================================================
// ThirPattern — Reuses HIR patterns with ThirType
// ============================================================================

/// THIR patterns mirror HIR patterns exactly.
///
/// Pattern exhaustiveness checking happens in THIR lowering, but the pattern
/// data structure itself doesn't need additional information beyond what HIR
/// provides. We reuse HirPattern types under a THIR alias.

/// Wildcard pattern: `_`
struct ThirWildcardPattern {
    ThirId id;
    SourceSpan span;
};

/// Binding pattern: `x` or `mut x`
struct ThirBindingPattern {
    ThirId id;
    std::string name;
    bool is_mut;
    ThirType type;
    SourceSpan span;
};

/// Literal pattern: `42`, `true`, `"hello"`
struct ThirLiteralPattern {
    ThirId id;
    std::variant<int64_t, uint64_t, double, bool, char, std::string> value;
    ThirType type;
    SourceSpan span;
};

/// Tuple pattern: `(a, b, c)`
struct ThirTuplePattern {
    ThirId id;
    std::vector<ThirPatternPtr> elements;
    ThirType type;
    SourceSpan span;
};

/// Struct pattern: `Point { x, y }`
struct ThirStructPattern {
    ThirId id;
    std::string struct_name;
    std::vector<std::pair<std::string, ThirPatternPtr>> fields;
    bool has_rest;
    ThirType type;
    SourceSpan span;
};

/// Enum variant pattern: `Just(x)`, `Nothing`
struct ThirEnumPattern {
    ThirId id;
    std::string enum_name;
    std::string variant_name;
    int variant_index;
    std::optional<std::vector<ThirPatternPtr>> payload;
    ThirType type;
    SourceSpan span;
};

/// Or pattern: `a | b | c`
struct ThirOrPattern {
    ThirId id;
    std::vector<ThirPatternPtr> alternatives;
    ThirType type;
    SourceSpan span;
};

/// Range pattern: `0 to 10`, `'a' through 'z'`
struct ThirRangePattern {
    ThirId id;
    std::optional<int64_t> start;
    std::optional<int64_t> end;
    bool inclusive;
    ThirType type;
    SourceSpan span;
};

/// Array pattern: `[a, b, ..rest]`
struct ThirArrayPattern {
    ThirId id;
    std::vector<ThirPatternPtr> elements;
    std::optional<ThirPatternPtr> rest;
    ThirType type;
    SourceSpan span;
};

/// A THIR pattern container.
struct ThirPattern {
    std::variant<ThirWildcardPattern, ThirBindingPattern, ThirLiteralPattern, ThirTuplePattern,
                 ThirStructPattern, ThirEnumPattern, ThirOrPattern, ThirRangePattern,
                 ThirArrayPattern>
        kind;

    /// Get the THIR ID for this pattern.
    [[nodiscard]] auto id() const -> ThirId;

    /// Get the type of this pattern.
    [[nodiscard]] auto type() const -> ThirType;

    /// Get the source span.
    [[nodiscard]] auto span() const -> SourceSpan;

    /// Check if this pattern is of kind `T`.
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Get this pattern as kind `T` (mutable).
    template <typename T> [[nodiscard]] auto as() -> T& {
        return std::get<T>(kind);
    }

    /// Get this pattern as kind `T` (const).
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        return std::get<T>(kind);
    }
};

} // namespace tml::thir
