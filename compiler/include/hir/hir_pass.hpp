//! # HIR Optimization Passes
//!
//! This module provides optimization passes that operate on HIR.
//!
//! ## Available Passes
//!
//! | Pass | Description |
//! |------|-------------|
//! | `ConstantFolding` | Evaluates constant expressions at compile time |
//! | `DeadCodeElimination` | Removes unreachable code and unused variables |
//! | `Inlining` | Expands small function calls inline |
//! | `ClosureOptimization` | Optimizes closure captures and representations |
//!
//! ## Usage
//!
//! ```cpp
//! HirModule module = builder.lower_module(ast);
//!
//! // Apply individual passes
//! ConstantFolding::run(module);
//! DeadCodeElimination::run(module);
//!
//! // Or use the pass manager
//! HirPassManager pm;
//! pm.add_pass<ConstantFolding>();
//! pm.add_pass<DeadCodeElimination>();
//! pm.run(module);
//! ```

#pragma once

#include "hir/hir_module.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::hir {

// ============================================================================
// Pass Base Class
// ============================================================================

/// Base class for HIR optimization passes.
///
/// Each pass transforms an HirModule in place. Passes should be idempotent
/// and should not depend on execution order unless explicitly documented.
class HirPass {
public:
    virtual ~HirPass() = default;

    /// Returns the name of this pass for debugging/logging.
    [[nodiscard]] virtual auto name() const -> std::string = 0;

    /// Runs the pass on the given module.
    /// @param module The module to transform
    /// @return true if any changes were made
    virtual auto run(HirModule& module) -> bool = 0;
};

// ============================================================================
// Constant Folding Pass
// ============================================================================

/// Evaluates constant expressions at compile time.
///
/// This pass identifies expressions that can be evaluated at compile time
/// and replaces them with literal values.
///
/// ## Optimizations Performed
///
/// - Binary operations on literals: `2 + 3` → `5`
/// - Unary operations on literals: `-42` → literal `-42`
/// - Logical short-circuit: `true or x` → `true`
/// - Comparison of literals: `1 < 2` → `true`
///
/// ## Limitations
///
/// - Does not track variable values across statements
/// - Does not evaluate function calls (even pure ones)
/// - String concatenation is not folded
class ConstantFolding : public HirPass {
public:
    [[nodiscard]] auto name() const -> std::string override { return "constant-folding"; }
    auto run(HirModule& module) -> bool override;

    /// Convenience method to run the pass directly.
    static auto run_pass(HirModule& module) -> bool;

private:
    bool changed_ = false;

    void fold_function(HirFunction& func);
    void fold_stmt(HirStmt& stmt);
    auto fold_expr(HirExprPtr& expr) -> bool;

    auto try_fold_binary(HirBinaryExpr& binary) -> std::optional<HirExprPtr>;
    auto try_fold_unary(HirUnaryExpr& unary) -> std::optional<HirExprPtr>;

    auto eval_int_binary(HirBinOp op, int64_t left, int64_t right, const HirType& type,
                         const SourceSpan& span) -> std::optional<HirExprPtr>;
    auto eval_uint_binary(HirBinOp op, uint64_t left, uint64_t right, const HirType& type,
                          const SourceSpan& span) -> std::optional<HirExprPtr>;
    auto eval_float_binary(HirBinOp op, double left, double right, const HirType& type,
                           const SourceSpan& span) -> std::optional<HirExprPtr>;
    auto eval_bool_binary(HirBinOp op, bool left, bool right, const SourceSpan& span)
        -> std::optional<HirExprPtr>;
};

// ============================================================================
// Dead Code Elimination Pass
// ============================================================================

/// Removes unreachable and unused code.
///
/// This pass identifies and removes code that cannot affect program behavior.
///
/// ## Optimizations Performed
///
/// - Removes statements after unconditional return/break/continue
/// - Removes unused let bindings (if the initializer has no side effects)
/// - Simplifies if expressions with constant conditions
/// - Removes empty blocks
///
/// ## Side Effect Analysis
///
/// The pass conservatively assumes expressions may have side effects unless
/// they are clearly pure (literals, variable references, arithmetic).
class DeadCodeElimination : public HirPass {
public:
    [[nodiscard]] auto name() const -> std::string override { return "dead-code-elimination"; }
    auto run(HirModule& module) -> bool override;

    static auto run_pass(HirModule& module) -> bool;

private:
    bool changed_ = false;

    void eliminate_in_function(HirFunction& func);
    void eliminate_in_block(std::vector<HirStmt>& stmts);
    void eliminate_in_expr_stmt(HirStmt& stmt);
    auto eliminate_in_expr(HirExprPtr& expr) -> bool;

    auto is_terminating(const HirStmt& stmt) -> bool;
    auto is_pure_expr(const HirExpr& expr) -> bool;
    auto has_side_effects(const HirExpr& expr) -> bool;
};

// ============================================================================
// Inlining Pass
// ============================================================================

/// Expands small function calls inline.
///
/// This pass replaces calls to small, non-recursive functions with their
/// bodies, reducing call overhead.
///
/// ## Inlining Criteria
///
/// A function is inlined if:
/// - Body has fewer than `max_statements` statements (default: 5)
/// - Function is not recursive
/// - Function is not marked `@noinline`
/// - Call site is not in a loop (to avoid code bloat)
///
/// ## Limitations
///
/// - Does not inline generic functions (would need monomorphization)
/// - Does not inline closures
/// - Does not track cross-module calls
class Inlining : public HirPass {
public:
    explicit Inlining(size_t max_statements = 5) : max_statements_(max_statements) {}

    [[nodiscard]] auto name() const -> std::string override { return "inlining"; }
    auto run(HirModule& module) -> bool override;

    static auto run_pass(HirModule& module, size_t max_statements = 5) -> bool;

private:
    size_t max_statements_;
    bool changed_ = false;

    auto should_inline(const HirFunction& func) -> bool;
    auto inline_call(HirCallExpr& call, const HirFunction& func) -> std::optional<HirExprPtr>;
    void inline_calls_in_expr(HirExprPtr& expr,
                              const std::unordered_map<std::string, const HirFunction*>& inlinable);
};

// ============================================================================
// Closure Optimization Pass
// ============================================================================

/// Optimizes closure captures and representations.
///
/// This pass analyzes closures and applies optimizations to reduce overhead.
///
/// ## Optimizations Performed
///
/// - Removes unused captures
/// - Converts by-ref captures to by-value when safe
/// - Identifies closures that can be converted to function pointers
///
/// ## Escape Analysis
///
/// The pass performs basic escape analysis to determine if captured
/// references escape the closure's lifetime.
class ClosureOptimization : public HirPass {
public:
    [[nodiscard]] auto name() const -> std::string override { return "closure-optimization"; }
    auto run(HirModule& module) -> bool override;

    static auto run_pass(HirModule& module) -> bool;

private:
    bool changed_ = false;

    void optimize_function(HirFunction& func);
    void optimize_in_expr(HirExprPtr& expr);
    void optimize_closure(HirClosureExpr& closure);
    auto is_capture_used(const HirClosureExpr& closure, const std::string& name) -> bool;
    auto capture_escapes(const HirClosureExpr& closure, const std::string& name) -> bool;
    auto check_var_usage(const HirExpr& expr, const std::string& name) -> bool;
    auto check_var_escapes(const HirExpr& expr, const std::string& name) -> bool;
};

// ============================================================================
// Pass Manager
// ============================================================================

/// Manages and runs a sequence of HIR optimization passes.
///
/// The pass manager provides a convenient way to configure and run
/// multiple optimization passes in sequence.
///
/// ## Usage
///
/// ```cpp
/// HirPassManager pm;
/// pm.add_pass<ConstantFolding>();
/// pm.add_pass<DeadCodeElimination>();
/// pm.run(module);
/// ```
class HirPassManager {
public:
    /// Adds a pass to the pipeline.
    template <typename PassT, typename... Args> void add_pass(Args&&... args) {
        passes_.push_back(std::make_unique<PassT>(std::forward<Args>(args)...));
    }

    /// Runs all passes on the module.
    /// @param module The module to optimize
    /// @return true if any pass made changes
    auto run(HirModule& module) -> bool;

    /// Runs passes until no more changes are made (fixed point).
    /// @param module The module to optimize
    /// @param max_iterations Maximum iterations to prevent infinite loops
    /// @return Number of iterations performed
    auto run_to_fixpoint(HirModule& module, size_t max_iterations = 10) -> size_t;

private:
    std::vector<std::unique_ptr<HirPass>> passes_;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Runs the standard optimization pipeline on a module.
///
/// Applies: ConstantFolding → DeadCodeElimination → ClosureOptimization
///
/// @param module The module to optimize
/// @return true if any optimizations were applied
auto optimize_hir(HirModule& module) -> bool;

/// Runs optimizations at the specified level.
///
/// | Level | Passes |
/// |-------|--------|
/// | 0 | None |
/// | 1 | ConstantFolding |
/// | 2 | ConstantFolding, DeadCodeElimination |
/// | 3 | All passes, run to fixpoint |
///
/// @param module The module to optimize
/// @param level Optimization level (0-3)
/// @return true if any optimizations were applied
auto optimize_hir_level(HirModule& module, int level) -> bool;

} // namespace tml::hir
