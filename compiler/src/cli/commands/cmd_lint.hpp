//! # Lint Command Interface
//!
//! This header defines the linter command API.
//!
//! ## Usage
//!
//! - `tml lint`: Check current directory
//! - `tml lint --fix`: Auto-fix style issues
//! - `tml lint --semantic`: Include naming/unused checks

#pragma once

namespace tml::cli {

// Lint TML source files for style and common issues
// Returns 0 on success, non-zero if lint errors found
int run_lint(int argc, char* argv[]);

} // namespace tml::cli
