#pragma once

namespace tml::cli {

// Lint TML source files for style and common issues
// Returns 0 on success, non-zero if lint errors found
int run_lint(int argc, char* argv[]);

} // namespace tml::cli
