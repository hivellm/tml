//! # Lint File Discovery
//!
//! This file implements file discovery and orchestrates linting of individual files.
//!
//! ## Discovery Rules
//!
//! - All `*.tml` files in specified paths
//! - Excludes `build/`, `errors/`, `pending/` directories
//!
//! ## Linting Pipeline
//!
//! ```text
//! lint_file()
//!   ├─ Read file content
//!   ├─ lint_style() - Text-based checks
//!   ├─ lint_semantic() - AST-based checks (if --semantic)
//!   └─ Write fixes (if --fix and modified)
//! ```

#include "linter_internal.hpp"
#include "log/log.hpp"

namespace tml::cli::linter {

// ============================================================================
// File Linting
// ============================================================================

/// Lints a single file for style and optionally semantic issues.
void lint_file(const fs::path& filepath, LintResult& result, const LintConfig& config,
               bool fix_mode, bool semantic) {
    std::ifstream file(filepath);
    if (!file) {
        TML_LOG_ERROR("lint", "Cannot open file: " << filepath);
        return;
    }

    result.files_checked++;

    // Read file content
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Style linting
    std::string fixed_content;
    lint_style(filepath, content, result, config, fix_mode, fixed_content);

    // Semantic linting (requires parsing)
    if (semantic) {
        try {
            // Create Source object from content
            lexer::Source source(filepath.string(), content);
            lexer::Lexer lex(source);
            auto tokens = lex.tokenize();

            if (!lex.has_errors()) {
                parser::Parser p(std::move(tokens));
                auto parse_result = p.parse_module(filepath.stem().string());

                if (is_ok(parse_result)) {
                    auto& module = unwrap(parse_result);
                    SemanticLinter linter(filepath, result, config);
                    linter.lint(module);
                }
            }
        } catch (const std::exception&) {
            // Parse errors are not lint issues - file might have syntax errors
            // Just skip semantic linting for this file
        }
    }

    // Write fixed file if modifications were made
    if (fix_mode && fixed_content != content) {
        std::ofstream out(filepath, std::ios::trunc);
        if (out) {
            out << fixed_content;
            std::cout << "  " << GREEN << "[FIXED]" << RESET << " " << filepath.filename().string()
                      << "\n";
        }
    }
}

// ============================================================================
// File Discovery
// ============================================================================

void find_tml_files(const fs::path& dir, std::vector<fs::path>& files) {
    try {
        if (!fs::exists(dir))
            return;

        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tml") {
                // Skip test files, errors directory, pending directory
                std::string path_str = entry.path().string();
                if (path_str.find("errors") != std::string::npos ||
                    path_str.find("pending") != std::string::npos) {
                    continue;
                }
                files.push_back(entry.path());
            }
        }
    } catch (const fs::filesystem_error& e) {
        TML_LOG_WARN("lint", "Cannot access " << dir << ": " << e.what());
    }
}

} // namespace tml::cli::linter
