//! # Lint Runner Implementation
//!
//! This file implements the main `run_lint()` function that orchestrates
//! style and semantic linting of TML source files.
//!
//! ## Lint Flow
//!
//! ```text
//! run_lint()
//!   ├─ Parse arguments (--fix, --semantic, --quiet)
//!   ├─ Load config from tml.toml
//!   ├─ discover_and_lint_files() or discover_and_lint_semantic()
//!   │     └─ For each file:
//!   │           ├─ lint_style_issues() - Whitespace, formatting
//!   │           └─ lint_semantic()     - AST analysis (if --semantic)
//!   └─ Report totals and exit code
//! ```
//!
//! ## Fix Mode
//!
//! When `--fix` is used, auto-fixable issues are corrected in-place:
//! - Trailing whitespace removal
//! - Tab-to-space conversion
//! - Missing final newline

#include "linter_internal.hpp"

namespace tml::cli {

// Using linter namespace for internal functions
using namespace linter;

// ============================================================================
// Main Entry Point
// ============================================================================

/// Main entry point for the `tml lint` command.
int run_lint(int argc, char* argv[]) {
    bool fix_mode = false;
    bool quiet = false;
    bool verbose = false;
    bool semantic = false;
    std::vector<std::string> paths;

    // Parse arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--fix") {
            fix_mode = true;
        } else if (arg == "--semantic") {
            semantic = true;
        } else if (arg == "--quiet" || arg == "-q") {
            quiet = true;
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            print_lint_help();
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            paths.push_back(arg);
        }
    }

    // Default to current directory if no paths specified
    if (paths.empty()) {
        paths.push_back(".");
    }

    // Load config from tml.toml
    LintConfig config = load_lint_config(fs::current_path());

    std::cout << CYAN << "TML" << RESET << " " << DIM << "v" << VERSION << RESET << "\n\n";

    if (fix_mode) {
        std::cout << YELLOW << "Linting and fixing TML files..." << RESET << "\n";

        // First, run the formatter on all paths
        std::cout << "\n" << YELLOW << "Running formatter..." << RESET << "\n";
        for (const auto& path : paths) {
            run_fmt(path, false /* check_only */, verbose);
        }
        std::cout << "\n";
    } else {
        std::cout << "Linting TML files";
        if (semantic) {
            std::cout << " (with semantic checks)";
        }
        std::cout << "...\n";
    }

    // Collect all .tml files
    std::vector<fs::path> files;
    for (const auto& path : paths) {
        fs::path p(path);
        if (fs::is_directory(p)) {
            find_tml_files(p, files);
        } else if (fs::is_regular_file(p) && p.extension() == ".tml") {
            files.push_back(p);
        } else if (fs::is_regular_file(p)) {
            std::cerr << "Warning: " << path << " is not a .tml file, skipping\n";
        } else {
            std::cerr << "Warning: " << path << " does not exist\n";
        }
    }

    if (files.empty()) {
        std::cout << "No .tml files found\n";
        return 0;
    }

    // Lint all files
    LintResult result;
    for (const auto& file : files) {
        if (verbose) {
            std::cout << "  Checking: " << file.string() << "\n";
        }
        lint_file(file, result, config, fix_mode, semantic);
    }

    // Print issues (if not in fix mode)
    if (!fix_mode) {
        // Sort issues by file, then line
        std::sort(result.issues.begin(), result.issues.end(),
                  [](const LintIssue& a, const LintIssue& b) {
                      if (a.file != b.file)
                          return a.file < b.file;
                      return a.line < b.line;
                  });

        std::string current_file;
        for (const auto& issue : result.issues) {
            if (quiet && issue.severity != Severity::Error)
                continue;

            // Print file header if changed
            if (issue.file != current_file) {
                if (!current_file.empty()) {
                    std::cout << "\n";
                }
                current_file = issue.file;
                std::cout << BOLD << fs::path(issue.file).filename().string() << RESET << "\n";
            }

            const char* color = RED;
            const char* severity_str = "error";
            switch (issue.severity) {
            case Severity::Error:
                color = RED;
                severity_str = "error";
                break;
            case Severity::Warning:
                color = YELLOW;
                severity_str = "warning";
                break;
            case Severity::Info:
                color = CYAN;
                severity_str = "info";
                break;
            }

            std::cout << "  " << DIM << issue.line << ":" << issue.column << RESET << "  " << color
                      << severity_str << RESET << "  " << DIM << "[" << issue.code << "]" << RESET
                      << " " << issue.message;

            // Print fix hint if available
            if (!issue.fix_hint.empty()) {
                std::cout << " " << DIM << "(" << issue.fix_hint << ")" << RESET;
            }
            std::cout << "\n";
        }
    }

    // Print summary
    std::cout << "\n";
    std::cout << "Checked " << result.files_checked << " files\n";

    if (fix_mode) {
        std::cout << GREEN << "Lint fix complete" << RESET << "\n";
        return 0;
    }

    if (result.errors > 0 || result.warnings > 0) {
        if (result.errors > 0) {
            std::cout << RED << result.errors << " error(s)" << RESET;
        }
        if (result.warnings > 0) {
            if (result.errors > 0)
                std::cout << ", ";
            std::cout << YELLOW << result.warnings << " warning(s)" << RESET;
        }
        std::cout << "\n";

        if (result.errors > 0) {
            std::cout << "Run " << CYAN << "tml lint --fix" << RESET
                      << " to auto-fix style errors\n";
            return 1;
        }
    } else {
        std::cout << GREEN << "All files passed lint checks" << RESET << "\n";
    }

    return 0;
}

} // namespace tml::cli
