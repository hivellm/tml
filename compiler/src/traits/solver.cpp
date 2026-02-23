TML_MODULE("compiler")

//! # Trait Solver Implementation
//!
//! Goal-based trait solving with candidate assembly, selection, recursive
//! obligation checking, and cycle detection.

#include "traits/solver.hpp"

namespace tml::traits {

// ============================================================================
// TraitSolver
// ============================================================================

TraitSolver::TraitSolver(const types::TypeEnv& env) : env_(&env) {}

auto TraitSolver::solve(const TraitGoal& goal) -> SolveResult {
    // Check cache first
    auto key = goal_key(goal);
    auto cache_it = cache_.find(key);
    if (cache_it != cache_.end()) {
        return cache_it->second;
    }

    // Cycle detection
    if (is_cycle(goal)) {
        auto err =
            "cycle detected while solving: " + type_name(goal.type) + ": " + goal.behavior_name;
        cache_[key] = err;
        return err;
    }

    // Push onto solving stack
    solving_stack_.push_back(goal);

    // Assemble candidates
    auto candidates = assemble_candidates(goal);

    // Select best candidate
    auto selected = select_candidate(candidates);

    SolveResult result;
    if (selected) {
        // Check super-behavior obligations
        auto unsatisfied = check_obligations(goal);
        if (unsatisfied.empty()) {
            result = *selected;
        } else {
            std::string err = "type '" + type_name(goal.type) + "' does not satisfy all " +
                              "obligations for '" + goal.behavior_name + "': ";
            for (size_t i = 0; i < unsatisfied.size(); ++i) {
                if (i > 0)
                    err += ", ";
                err += unsatisfied[i];
            }
            result = err;
        }
    } else {
        result = "type '" + type_name(goal.type) + "' does not implement behavior '" +
                 goal.behavior_name + "'";
    }

    // Pop from solving stack
    solving_stack_.pop_back();

    // Cache result
    cache_[key] = result;
    return result;
}

auto TraitSolver::normalize(const ProjectionGoal& goal) -> std::optional<types::TypePtr> {
    // First, verify the type implements the behavior
    TraitGoal trait_goal{goal.type, goal.behavior_name, goal.type_args, {}};
    auto solve_result = solve(trait_goal);

    if (std::holds_alternative<std::string>(solve_result)) {
        return std::nullopt; // Can't normalize if behavior isn't implemented
    }

    auto& candidate = std::get<TraitCandidate>(solve_result);

    // Look up the behavior definition
    auto behavior = env_->lookup_behavior(goal.behavior_name);
    if (!behavior) {
        return std::nullopt;
    }

    // Find the associated type in the behavior
    for (const auto& assoc : behavior->associated_types) {
        if (assoc.name == goal.assoc_type_name) {
            // If the candidate has substitutions, apply them
            if (!candidate.substitutions.empty() && assoc.default_type) {
                return types::substitute_type(*assoc.default_type, candidate.substitutions);
            }
            // Return the default type if available
            if (assoc.default_type) {
                return *assoc.default_type;
            }
            // No default — the impl should provide the concrete type
            // For now, we can't resolve further without impl-level associated type bindings
            return std::nullopt;
        }
    }

    return std::nullopt;
}

auto TraitSolver::check_obligations(const TraitGoal& goal) -> std::vector<std::string> {
    std::vector<std::string> unsatisfied;

    auto behavior = env_->lookup_behavior(goal.behavior_name);
    if (!behavior) {
        return unsatisfied; // Unknown behavior — no obligations
    }

    // Check each super-behavior
    for (const auto& super_name : behavior->super_behaviors) {
        TraitGoal super_goal{goal.type, super_name, {}, goal.span};
        auto result = solve(super_goal);
        if (std::holds_alternative<std::string>(result)) {
            unsatisfied.push_back(super_name);
        }
    }

    return unsatisfied;
}

void TraitSolver::set_where_clauses(const std::vector<types::WhereConstraint>& clauses) {
    where_clauses_ = clauses;
}

void TraitSolver::clear_where_clauses() {
    where_clauses_.clear();
}

void TraitSolver::clear_cache() {
    cache_.clear();
}

// ============================================================================
// Candidate Assembly
// ============================================================================

auto TraitSolver::assemble_candidates(const TraitGoal& goal) -> std::vector<TraitCandidate> {
    std::vector<TraitCandidate> candidates;

    assemble_impl_candidates(goal, candidates);
    assemble_builtin_candidates(goal, candidates);
    assemble_where_candidates(goal, candidates);
    assemble_auto_candidates(goal, candidates);

    return candidates;
}

void TraitSolver::assemble_impl_candidates(const TraitGoal& goal,
                                           std::vector<TraitCandidate>& candidates) {
    auto tname = type_name(goal.type);
    if (tname.empty())
        return;

    if (env_->type_implements(tname, goal.behavior_name)) {
        TraitCandidate candidate;
        candidate.kind = CandidateKind::ImplCandidate;
        candidate.impl_type = tname;
        candidate.behavior_name = goal.behavior_name;
        candidates.push_back(std::move(candidate));
    }

    // Also check with TypePtr overload (handles closures implementing Fn, etc.)
    if (candidates.empty() && env_->type_implements(goal.type, goal.behavior_name)) {
        TraitCandidate candidate;
        candidate.kind = CandidateKind::ImplCandidate;
        candidate.impl_type = tname;
        candidate.behavior_name = goal.behavior_name;
        candidates.push_back(std::move(candidate));
    }
}

void TraitSolver::assemble_builtin_candidates(const TraitGoal& goal,
                                              std::vector<TraitCandidate>& candidates) {
    if (has_builtin_impl(goal.type, goal.behavior_name)) {
        TraitCandidate candidate;
        candidate.kind = CandidateKind::BuiltinCandidate;
        candidate.impl_type = type_name(goal.type);
        candidate.behavior_name = goal.behavior_name;
        candidates.push_back(std::move(candidate));
    }
}

void TraitSolver::assemble_where_candidates(const TraitGoal& goal,
                                            std::vector<TraitCandidate>& candidates) {
    // Check if the goal type is a generic parameter with where clause bounds
    if (!goal.type || !goal.type->is<types::GenericType>())
        return;

    const auto& generic = goal.type->as<types::GenericType>();
    for (const auto& clause : where_clauses_) {
        if (clause.type_param != generic.name)
            continue;

        // Check simple bounds
        for (const auto& bound : clause.required_behaviors) {
            if (bound == goal.behavior_name) {
                TraitCandidate candidate;
                candidate.kind = CandidateKind::WhereClause;
                candidate.impl_type = generic.name;
                candidate.behavior_name = goal.behavior_name;
                candidates.push_back(std::move(candidate));
                return;
            }
        }

        // Check parameterized bounds
        for (const auto& pbound : clause.parameterized_bounds) {
            if (pbound.behavior_name == goal.behavior_name) {
                TraitCandidate candidate;
                candidate.kind = CandidateKind::WhereClause;
                candidate.impl_type = generic.name;
                candidate.behavior_name = goal.behavior_name;
                candidates.push_back(std::move(candidate));
                return;
            }
        }

        // Check higher-ranked bounds (e.g., for[T] Fn(T) -> T)
        for (const auto& hrb : clause.higher_ranked_bounds) {
            if (hrb.behavior_name == goal.behavior_name) {
                // Higher-ranked bounds are universally quantified — the bound type params
                // are fresh for each use. We create a candidate and record the bound type
                // params as substitution placeholders. During actual use, the caller will
                // instantiate them with concrete types.
                TraitCandidate candidate;
                candidate.kind = CandidateKind::WhereClause;
                candidate.impl_type = generic.name;
                candidate.behavior_name = goal.behavior_name;

                // Record bound type params so they can be instantiated later
                for (const auto& param : hrb.bound_type_params) {
                    candidate.substitutions[param] = nullptr; // placeholder for fresh var
                }

                candidates.push_back(std::move(candidate));
                return;
            }
        }
    }
}

void TraitSolver::assemble_auto_candidates(const TraitGoal& goal,
                                           std::vector<TraitCandidate>& candidates) {
    // Sized is implemented by all types except dyn Behavior
    if (goal.behavior_name == "Sized") {
        if (goal.type && !goal.type->is<types::DynBehaviorType>()) {
            TraitCandidate candidate;
            candidate.kind = CandidateKind::AutoCandidate;
            candidate.impl_type = type_name(goal.type);
            candidate.behavior_name = "Sized";
            candidates.push_back(std::move(candidate));
        }
        return;
    }

    // Send and Sync are auto-derived for types whose fields are all Send/Sync
    if (goal.behavior_name == "Send" || goal.behavior_name == "Sync") {
        auto tname = type_name(goal.type);

        // Primitives are always Send + Sync
        if (goal.type && goal.type->is<types::PrimitiveType>()) {
            TraitCandidate candidate;
            candidate.kind = CandidateKind::AutoCandidate;
            candidate.impl_type = tname;
            candidate.behavior_name = goal.behavior_name;
            candidates.push_back(std::move(candidate));
            return;
        }

        // Check struct fields
        if (goal.type && goal.type->is<types::NamedType>()) {
            auto struct_def = env_->lookup_struct(tname);
            if (struct_def) {
                bool all_fields_satisfy = true;
                for (const auto& field : struct_def->fields) {
                    TraitGoal field_goal{field.type, goal.behavior_name, {}, goal.span};
                    auto field_result = solve(field_goal);
                    if (std::holds_alternative<std::string>(field_result)) {
                        all_fields_satisfy = false;
                        break;
                    }
                }
                if (all_fields_satisfy) {
                    TraitCandidate candidate;
                    candidate.kind = CandidateKind::AutoCandidate;
                    candidate.impl_type = tname;
                    candidate.behavior_name = goal.behavior_name;
                    candidates.push_back(std::move(candidate));
                }
            }
        }
    }
}

// ============================================================================
// Selection
// ============================================================================

auto TraitSolver::select_candidate(const std::vector<TraitCandidate>& candidates)
    -> std::optional<TraitCandidate> {
    if (candidates.empty())
        return std::nullopt;
    if (candidates.size() == 1)
        return candidates[0];

    // Priority: ImplCandidate > WhereClause > BuiltinCandidate > AutoCandidate > DefaultImpl
    auto priority = [](CandidateKind kind) -> int {
        switch (kind) {
        case CandidateKind::ImplCandidate:
            return 5;
        case CandidateKind::WhereClause:
            return 4;
        case CandidateKind::BuiltinCandidate:
            return 3;
        case CandidateKind::AutoCandidate:
            return 2;
        case CandidateKind::DefaultImpl:
            return 1;
        }
        return 0;
    };

    const TraitCandidate* best = &candidates[0];
    for (size_t i = 1; i < candidates.size(); ++i) {
        if (priority(candidates[i].kind) > priority(best->kind)) {
            best = &candidates[i];
        }
    }

    return *best;
}

// ============================================================================
// Helpers
// ============================================================================

auto TraitSolver::goal_key(const TraitGoal& goal) const -> std::string {
    auto key = type_name(goal.type) + ":" + goal.behavior_name;
    for (const auto& arg : goal.type_args) {
        key += "," + types::type_to_string(arg);
    }
    return key;
}

auto TraitSolver::is_cycle(const TraitGoal& goal) const -> bool {
    auto key = goal_key(goal);
    for (const auto& active : solving_stack_) {
        if (goal_key(active) == key)
            return true;
    }
    return false;
}

auto TraitSolver::type_name(const types::TypePtr& type) const -> std::string {
    if (!type)
        return "<null>";
    return types::type_to_string(type);
}

// ============================================================================
// AssociatedTypeNormalizer
// ============================================================================

AssociatedTypeNormalizer::AssociatedTypeNormalizer(const types::TypeEnv& env, TraitSolver& solver)
    : env_(&env), solver_(&solver) {}

auto AssociatedTypeNormalizer::normalize(types::TypePtr self_type, const std::string& behavior_name,
                                         const std::string& assoc_name)
    -> std::optional<types::TypePtr> {
    ProjectionGoal goal{self_type, behavior_name, assoc_name, {}};
    return solver_->normalize(goal);
}

auto AssociatedTypeNormalizer::normalize_deep(types::TypePtr type) -> types::TypePtr {
    // Walk the type tree and normalize any projection types found.
    // Currently, TML doesn't have a dedicated ProjectionType in the type variant,
    // so this is a no-op placeholder for when projection types are added.
    // The normalization happens at THIR lowering time when encountering T::Output syntax.
    return type;
}

} // namespace tml::traits
