//! # Lint Configuration
//!
//! This file implements lint configuration loading from `tml.toml`.
//!
//! ## Configuration Section
//!
//! ```toml
//! [lint]
//! max_line_length = 100
//! check_tabs = true
//! check_trailing_whitespace = true
//! check_naming = true
//! check_unused = true
//!
//! [lint.rules]
//! S001 = false   # Disable tab checking
//! S003 = "warn"  # Line length as warning
//! ```
//!
//! ## Default Settings
//!
//! All checks are enabled by default with sensible thresholds.

#include "linter_internal.hpp"

namespace tml::cli::linter {

// ============================================================================
// Config File Parsing
// ============================================================================

/// Loads lint configuration from tml.toml in the project root.
LintConfig load_lint_config(const fs::path& project_root) {
    LintConfig config;

    // Look for tml.toml in project root
    fs::path config_path = project_root / "tml.toml";
    if (!fs::exists(config_path)) {
        return config;
    }

    std::ifstream file(config_path);
    if (!file) {
        return config;
    }

    std::string line;
    bool in_lint_section = false;
    bool in_lint_rules_section = false;

    while (std::getline(file, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos)
            continue;
        line = line.substr(start);

        // Skip comments
        if (line[0] == '#')
            continue;

        // Check for section headers
        if (line[0] == '[') {
            in_lint_section = (line.find("[lint]") != std::string::npos);
            in_lint_rules_section = (line.find("[lint.rules]") != std::string::npos);
            continue;
        }

        // Parse lint section options
        if (in_lint_section && !in_lint_rules_section) {
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos)
                continue;

            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);

            // Trim
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));

            // Remove quotes from string values
            if (!value.empty() && value[0] == '"') {
                value = value.substr(1);
                if (!value.empty() && value.back() == '"') {
                    value.pop_back();
                }
            }

            if (key == "max-line-length") {
                try {
                    config.max_line_length = std::stoi(value);
                } catch (...) {}
            } else if (key == "max-function-lines") {
                try {
                    config.max_function_lines = std::stoi(value);
                } catch (...) {}
            } else if (key == "max-cyclomatic-complexity") {
                try {
                    config.max_cyclomatic_complexity = std::stoi(value);
                } catch (...) {}
            } else if (key == "max-nesting-depth") {
                try {
                    config.max_nesting_depth = std::stoi(value);
                } catch (...) {}
            } else if (key == "check-tabs") {
                config.check_tabs = (value == "true");
            } else if (key == "check-trailing") {
                config.check_trailing = (value == "true");
            } else if (key == "check-naming") {
                config.check_naming = (value == "true");
            } else if (key == "check-unused") {
                config.check_unused = (value == "true");
            } else if (key == "check-complexity") {
                config.check_complexity = (value == "true");
            }
        }

        // Parse lint.rules section (disabled rules)
        if (in_lint_rules_section) {
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos)
                continue;

            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);

            // Trim
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));

            // If value is "false" or "off", disable the rule
            if (value == "false" || value == "off" || value == "\"off\"") {
                config.disabled_rules.insert(key);
            }
        }
    }

    return config;
}

} // namespace tml::cli::linter
