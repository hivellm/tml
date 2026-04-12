//! # Coverage Command Interface
//!
//! `tml cv` — project-scoped test coverage report.
//!
//! Reads tml.toml to scope to a specific project, discovers all source
//! modules and test files, type-checks each, counts @test functions,
//! maps source modules to test files, and prints a coverage report.
//!
//! ## Usage
//!
//! ```
//! tml cv                    # Run in current directory (reads tml.toml)
//! tml cv --path=compiler-tml  # Run for a specific project
//! tml cv --quick            # Type-check only, skip mapping
//! ```

#pragma once

#include <string>

namespace tml::cli {

/// Coverage report options.
struct CoverageOptions {
    std::string project_path = "."; ///< Path to project root (has tml.toml)
    bool quick = false;             ///< Skip module-to-test mapping
    bool verbose = false;           ///< Show per-file results
    bool json = false;              ///< Output JSON instead of table
};

/// Run the coverage command.
/// @return 0 on success, 1 if any type-check fails.
int run_coverage(int argc, char* argv[], bool verbose);

} // namespace tml::cli
