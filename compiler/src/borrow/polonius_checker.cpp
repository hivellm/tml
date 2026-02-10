//! # Polonius Borrow Checker Entry Point
//!
//! Provides `PoloniusChecker`, a drop-in alternative to `BorrowChecker` that
//! uses the Polonius algorithm. Produces the same `BorrowError` format.

#include "borrow/polonius.hpp"
#include "types/env.hpp"

namespace tml::borrow::polonius {

// ============================================================================
// PoloniusChecker implementation
// ============================================================================

PoloniusChecker::PoloniusChecker(const types::TypeEnv& type_env) : type_env_(&type_env) {}

auto PoloniusChecker::check_module(const parser::Module& module)
    -> Result<bool, std::vector<BorrowError>> {

    std::vector<BorrowError> all_errors;

    for (const auto& decl : module.decls) {
        std::visit(
            [this, &all_errors](const auto& d) {
                using T = std::decay_t<decltype(d)>;
                if constexpr (std::is_same_v<T, parser::FuncDecl>) {
                    auto errors = check_function(d);
                    all_errors.insert(all_errors.end(), errors.begin(), errors.end());
                } else if constexpr (std::is_same_v<T, parser::ImplDecl>) {
                    for (const auto& method : d.methods) {
                        auto errors = check_function(method);
                        all_errors.insert(all_errors.end(), errors.begin(), errors.end());
                    }
                }
            },
            decl->kind);
    }

    if (!all_errors.empty()) {
        return all_errors;
    }
    return true;
}

auto PoloniusChecker::check_function(const parser::FuncDecl& func) -> std::vector<BorrowError> {
    // Phase 1: Generate facts from AST
    PoloniusFacts fact_gen(*type_env_);
    fact_gen.generate_function(func);
    auto& facts = fact_gen.facts();

    // Phase 2: Quick check (location-insensitive)
    PoloniusSolver solver(facts);
    if (solver.quick_check()) {
        return {}; // No conflicts possible
    }

    // Phase 3: Full solve
    solver.solve();

    // Phase 4: Convert errors to BorrowError format
    if (!facts.errors.empty()) {
        return convert_errors(facts);
    }

    return {};
}

// ============================================================================
// Error conversion
// ============================================================================

auto PoloniusChecker::convert_errors(const FactTable& facts) -> std::vector<BorrowError> {
    std::vector<BorrowError> result;
    result.reserve(facts.errors.size());

    for (const auto& [loan_id, point_id] : facts.errors) {
        result.push_back(make_error(facts, loan_id, point_id));
    }

    return result;
}

auto PoloniusChecker::make_error(const FactTable& facts, LoanId loan_id, PointId point_id)
    -> BorrowError {
    // Look up loan and point metadata
    auto loan_it = facts.loans.find(loan_id);
    auto point_it = facts.points.find(point_id);

    SourceSpan error_span;
    SourceSpan loan_span;

    if (point_it != facts.points.end()) {
        error_span = point_it->second.span;
    }
    if (loan_it != facts.loans.end()) {
        loan_span = loan_it->second.span;
    }

    // Determine error kind based on loan kind
    if (loan_it != facts.loans.end()) {
        const auto& loan = loan_it->second;

        // Find loan's invalidation context
        // Look for what invalidated it to provide better error messages
        for (const auto& inval : facts.loan_invalidated_at) {
            if (inval.loan == loan_id && inval.point == point_id) {
                // This is an invalidation point
                if (loan.kind == BorrowKind::Mutable) {
                    // Mutable borrow conflict
                    BorrowError err;
                    err.code = BorrowErrorCode::DoubleMutBorrow;
                    err.message = "cannot use value while mutably borrowed";
                    err.span = error_span;
                    err.related_span = loan_span;
                    err.related_message = "mutable borrow created here";
                    err.notes.push_back(
                        "Polonius: loan is still reachable through a live origin at this point");
                    return err;
                } else {
                    // Shared borrow conflict (mutation while shared borrow exists)
                    BorrowError err;
                    err.code = BorrowErrorCode::AssignWhileBorrowed;
                    err.message = "cannot assign to value while it is borrowed";
                    err.span = error_span;
                    err.related_span = loan_span;
                    err.related_message = "borrow created here";
                    err.notes.push_back(
                        "Polonius: loan is still reachable through a live origin at this point");
                    return err;
                }
            }
        }
    }

    // Generic error fallback
    BorrowError err;
    err.code = BorrowErrorCode::Other;
    err.message = "borrow conflict detected by Polonius checker";
    err.span = error_span;
    if (loan_it != facts.loans.end()) {
        err.related_span = loan_span;
        err.related_message = "conflicting borrow created here";
    }
    return err;
}

} // namespace tml::borrow::polonius
