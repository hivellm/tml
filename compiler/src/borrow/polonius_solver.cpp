//! # Polonius Constraint Solver
//!
//! Implements the fixed-point Datalog solver for Polonius borrow checking.
//! Propagates loans through CFG edges and subset constraints, filtered by
//! origin liveness, then detects errors where loans are invalidated while
//! still reachable through a live origin.

#include "borrow/polonius.hpp"

#include <algorithm>

namespace tml::borrow::polonius {

// ============================================================================
// PoloniusSolver implementation
// ============================================================================

PoloniusSolver::PoloniusSolver(FactTable& facts) : facts_(facts) {}

void PoloniusSolver::build_indices() {
    // Build CFG successor index
    cfg_successors_.clear();
    for (const auto& edge : facts_.cfg_edges) {
        cfg_successors_[edge.from].push_back(edge.to);
    }

    // Build subset constraint index (by sub origin)
    subset_by_sub_.clear();
    for (const auto& sub : facts_.subset_constraints) {
        subset_by_sub_[sub.sub].push_back({sub.sup, sub.at_point});
    }

    // Build liveness set for O(1) lookup
    liveness_set_.clear();
    for (const auto& live : facts_.origin_live_at) {
        liveness_set_.insert(encode_pair(live.origin, live.point));
    }
}

auto PoloniusSolver::is_origin_live(OriginId origin, PointId point) const -> bool {
    return liveness_set_.count(encode_pair(origin, point)) > 0;
}

// ============================================================================
// Location-insensitive pre-check (10.6)
// ============================================================================

auto PoloniusSolver::quick_check() -> bool {
    // Fast pre-check: ignore CFG edges and points entirely.
    // If no origin could ever contain a loan that gets invalidated,
    // then no errors are possible.

    if (facts_.loan_invalidated_at.empty()) {
        return true; // No invalidations → no possible errors
    }

    if (facts_.loan_issued_at.empty()) {
        return true; // No loans → no possible errors
    }

    // Compute origin_contains_loan (ignoring points)
    std::unordered_map<OriginId, std::unordered_set<LoanId>> origin_contains;

    for (const auto& issued : facts_.loan_issued_at) {
        origin_contains[issued.origin].insert(issued.loan);
    }

    // Propagate through subset constraints (ignoring points)
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& sub : facts_.subset_constraints) {
            auto it = origin_contains.find(sub.sub);
            if (it == origin_contains.end())
                continue;
            for (auto loan : it->second) {
                if (origin_contains[sub.sup].insert(loan).second) {
                    changed = true;
                }
            }
        }
    }

    // Check if any invalidated loan is in any origin that is ever live
    std::unordered_set<OriginId> ever_live;
    for (const auto& live : facts_.origin_live_at) {
        ever_live.insert(live.origin);
    }

    for (const auto& inval : facts_.loan_invalidated_at) {
        for (const auto& [origin, loans] : origin_contains) {
            if (loans.count(inval.loan) && ever_live.count(origin)) {
                return false; // Potential conflict found
            }
        }
    }

    return true; // No potential conflicts
}

// ============================================================================
// Full location-sensitive solver
// ============================================================================

void PoloniusSolver::solve() {
    build_indices();

    // Initialize origin_contains_loan_at from loan_issued_at
    for (const auto& issued : facts_.loan_issued_at) {
        facts_.origin_contains_loan_at[issued.origin][issued.point].insert(issued.loan);
    }

    // Worklist: (origin, point) pairs that changed
    std::queue<std::pair<OriginId, PointId>> worklist;

    // Seed worklist with all initial facts
    for (const auto& issued : facts_.loan_issued_at) {
        worklist.push({issued.origin, issued.point});
    }

    while (!worklist.empty()) {
        auto [origin, point] = worklist.front();
        worklist.pop();

        auto it = facts_.origin_contains_loan_at.find(origin);
        if (it == facts_.origin_contains_loan_at.end())
            continue;

        auto pt_it = it->second.find(point);
        if (pt_it == it->second.end())
            continue;

        // Copy the loans set (it may be modified during iteration)
        auto loans = pt_it->second;

        // Rule 1: CFG propagation
        auto succ_it = cfg_successors_.find(point);
        if (succ_it != cfg_successors_.end()) {
            for (PointId succ : succ_it->second) {
                if (is_origin_live(origin, succ)) {
                    for (LoanId loan : loans) {
                        if (facts_.origin_contains_loan_at[origin][succ].insert(loan).second) {
                            worklist.push({origin, succ});
                        }
                    }
                }
            }
        }

        // Rule 2: Subset propagation
        auto sub_it = subset_by_sub_.find(origin);
        if (sub_it != subset_by_sub_.end()) {
            for (const auto& [sup_origin, sub_point] : sub_it->second) {
                // Subset constraints are point-specific, but we propagate
                // at the point where the constraint is defined
                for (LoanId loan : loans) {
                    if (facts_.origin_contains_loan_at[sup_origin][point].insert(loan).second) {
                        worklist.push({sup_origin, point});
                    }
                }
            }
        }
    }

    // Rule 3: Error detection
    check_invalidations();
}

void PoloniusSolver::check_invalidations() {
    facts_.errors.clear();

    for (const auto& inval : facts_.loan_invalidated_at) {
        // Check if any live origin contains this loan at the invalidation point
        for (const auto& [origin, point_map] : facts_.origin_contains_loan_at) {
            auto pt_it = point_map.find(inval.point);
            if (pt_it != point_map.end()) {
                if (pt_it->second.count(inval.loan)) {
                    if (is_origin_live(origin, inval.point)) {
                        facts_.errors.push_back({inval.loan, inval.point});
                        break; // One error per (loan, point) is enough
                    }
                }
            }
        }
    }
}

} // namespace tml::borrow::polonius
