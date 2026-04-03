#pragma once

//! # MIR Validation Pass
//!
//! A lightweight diagnostic pass that runs after MIR building and before
//! codegen. It checks that all MIR instructions have proper types, that all
//! basic blocks have terminators, and that no blocks are empty.
//!
//! The pass is non-transforming: it never modifies MIR. On finding an issue
//! it records a warning and continues so that the full scope of problems is
//! visible in one run.
//!
//! ## Usage
//!
//! ```cpp
//! auto result = mir::validate_module(mir_module);
//! for (const auto& w : result.warnings) {
//!     TML_LOG_WARN("mir", w);
//! }
//! ```

#include "mir/mir.hpp"

#include <string>
#include <vector>

namespace tml::mir {

/// Aggregated result of a full module validation run.
struct ValidationResult {
    /// Number of instructions whose `type` field is null.
    int null_type_count = 0;
    /// Number of basic blocks that have no terminator.
    int missing_terminator_count = 0;
    /// Number of basic blocks that contain no instructions and no terminator.
    int empty_block_count = 0;
    /// Human-readable warning messages (one per issue found).
    std::vector<std::string> warnings;

    /// Returns true when no issues were found.
    [[nodiscard]] auto is_clean() const -> bool {
        return null_type_count == 0 && missing_terminator_count == 0 && empty_block_count == 0;
    }
};

/// Validate all functions in a module.
///
/// Runs three sub-checks in order:
///   1. `validate_types`      — null `inst.type` fields
///   2. `validate_terminators` — blocks missing a terminator
///   3. `validate_blocks`     — entry block existence, empty blocks
///
/// @param module  The MIR module to inspect (read-only).
/// @return        A `ValidationResult` summarising all issues found.
auto validate_module(const Module& module) -> ValidationResult;

} // namespace tml::mir
