//! # Trait Solver
//!
//! Goal-based trait (behavior) solver for TML. Implements candidate assembly,
//! selection, recursive obligation checking, and cycle detection.
//!
//! ## Key Concepts
//!
//! - **TraitGoal**: A proof obligation "does Type implement Behavior?"
//! - **ProjectionGoal**: A type projection "what is T::Output?"
//! - **TraitCandidate**: How a goal was satisfied (impl, builtin, where clause, auto)
//! - **TraitSolver**: Main entry point for solving goals
//! - **AssociatedTypeNormalizer**: Resolves associated type projections to concrete types
//!
//! ## Algorithm
//!
//! 1. Assemble candidates: query impls, builtins, where clauses, auto-derive
//! 2. Select best candidate: impl > where > builtin > auto (error if ambiguous)
//! 3. Recursively check super-behavior obligations
//! 4. Cycle detection via solving stack
//!
//! ## Usage
//!
//! ```cpp
//! traits::TraitSolver solver(type_env);
//! TraitGoal goal{i32_type, "Display", {}, span};
//! auto result = solver.solve(goal);
//! if (auto* candidate = std::get_if<TraitCandidate>(&result)) {
//!     // Goal satisfied
//! }
//! ```

#ifndef TML_TRAITS_SOLVER_HPP
#define TML_TRAITS_SOLVER_HPP

#include "types/env.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tml::traits {

// ============================================================================
// Goal Types
// ============================================================================

/// A proof obligation: "does Type implement Behavior?"
struct TraitGoal {
    types::TypePtr type;                   ///< The type being checked.
    std::string behavior_name;             ///< The behavior required.
    std::vector<types::TypePtr> type_args; ///< Behavior type args (e.g., From[Str]).
    SourceSpan span;                       ///< For error reporting.
};

/// A type projection: "what is T::Output?"
struct ProjectionGoal {
    types::TypePtr type;                   ///< The self type.
    std::string behavior_name;             ///< The behavior containing the associated type.
    std::string assoc_type_name;           ///< The associated type name (e.g., "Output").
    std::vector<types::TypePtr> type_args; ///< GAT args if any.
};

// ============================================================================
// Candidate Types
// ============================================================================

/// How a goal was satisfied.
enum class CandidateKind {
    ImplCandidate,    ///< Explicit `extend Type with Behavior`.
    BuiltinCandidate, ///< Compiler-known impl (e.g., I32: Numeric).
    WhereClause,      ///< From `where T: Behavior` bound.
    AutoCandidate,    ///< Auto-derived (Send, Sync, Sized).
    DefaultImpl,      ///< Default method in behavior definition.
};

/// A candidate that satisfies a trait goal.
struct TraitCandidate {
    CandidateKind kind;
    std::string impl_type;     ///< The implementing type name.
    std::string behavior_name; ///< The behavior being implemented.
    std::unordered_map<std::string, types::TypePtr> substitutions; ///< Type param bindings.
};

/// Result of solving a trait goal: either a candidate or an error message.
using SolveResult = std::variant<TraitCandidate, std::string>;

// ============================================================================
// TraitSolver
// ============================================================================

/// Goal-based trait solver with candidate assembly, selection, and cycle detection.
class TraitSolver {
public:
    explicit TraitSolver(const types::TypeEnv& env);

    /// Solve a trait goal: does `type` implement `behavior`?
    auto solve(const TraitGoal& goal) -> SolveResult;

    /// Normalize an associated type projection to a concrete type.
    auto normalize(const ProjectionGoal& goal) -> std::optional<types::TypePtr>;

    /// Check all super-behavior obligations recursively.
    /// Returns a list of unsatisfied obligations (empty = all satisfied).
    auto check_obligations(const TraitGoal& goal) -> std::vector<std::string>;

    /// Set the current where clause context (for function-level bounds).
    void set_where_clauses(const std::vector<types::WhereConstraint>& clauses);

    /// Clear the where clause context.
    void clear_where_clauses();

    /// Clear the memoization cache (for reuse between functions).
    void clear_cache();

private:
    const types::TypeEnv* env_;

    /// Cycle detection stack.
    std::vector<TraitGoal> solving_stack_;

    /// Memoization cache: goal_key -> result.
    std::unordered_map<std::string, SolveResult> cache_;

    /// Current where clause constraints (set per-function).
    std::vector<types::WhereConstraint> where_clauses_;

    // --- Candidate assembly ---

    /// Assemble all candidates that could satisfy the goal.
    auto assemble_candidates(const TraitGoal& goal) -> std::vector<TraitCandidate>;

    /// Check explicit impl candidates from TypeEnv.
    void assemble_impl_candidates(const TraitGoal& goal, std::vector<TraitCandidate>& candidates);

    /// Check builtin candidates (primitives, etc.).
    void assemble_builtin_candidates(const TraitGoal& goal,
                                     std::vector<TraitCandidate>& candidates);

    /// Check where clause candidates.
    void assemble_where_candidates(const TraitGoal& goal, std::vector<TraitCandidate>& candidates);

    /// Check auto-derive candidates (Send, Sync, Sized).
    void assemble_auto_candidates(const TraitGoal& goal, std::vector<TraitCandidate>& candidates);

    // --- Selection ---

    /// Select the best candidate from the assembled list.
    /// Returns nullopt if no candidates or ambiguous.
    auto select_candidate(const std::vector<TraitCandidate>& candidates)
        -> std::optional<TraitCandidate>;

    // --- Helpers ---

    /// Create a unique key for a goal (for caching).
    auto goal_key(const TraitGoal& goal) const -> std::string;

    /// Check if solving this goal would create a cycle.
    auto is_cycle(const TraitGoal& goal) const -> bool;

    /// Get the type name string for a TypePtr.
    auto type_name(const types::TypePtr& type) const -> std::string;
};

// ============================================================================
// AssociatedTypeNormalizer
// ============================================================================

/// Resolves associated type projections to concrete types.
///
/// Given `T: Iterator`, normalizes `T::Item` to the concrete type
/// specified in the impl block for T.
class AssociatedTypeNormalizer {
public:
    AssociatedTypeNormalizer(const types::TypeEnv& env, TraitSolver& solver);

    /// Normalize a specific associated type projection.
    /// Returns the concrete type, or nullopt if normalization fails.
    auto normalize(types::TypePtr self_type, const std::string& behavior_name,
                   const std::string& assoc_name) -> std::optional<types::TypePtr>;

    /// Walk a type tree and normalize all projections found within.
    auto normalize_deep(types::TypePtr type) -> types::TypePtr;

private:
    const types::TypeEnv* env_;
    TraitSolver* solver_;
};

// ============================================================================
// Builtin Behavior Registry
// ============================================================================

/// Check if a type has a builtin implementation of a behavior.
/// Used by TraitSolver::assemble_builtin_candidates.
auto has_builtin_impl(const types::TypePtr& type, const std::string& behavior_name) -> bool;

/// Get the list of behaviors that a primitive type implements.
auto builtin_behaviors_for_type(const types::TypePtr& type) -> std::vector<std::string>;

} // namespace tml::traits

#endif // TML_TRAITS_SOLVER_HPP
