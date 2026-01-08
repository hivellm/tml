//! # Lint Helper Functions
//!
//! This file contains shared constants and utilities for the linter.
//!
//! ## Contents
//!
//! - **ANSI Colors**: Terminal color codes for output
//! - **Rule Codes**: Lint rule identifiers (S001, S002, etc.)
//! - **Naming Checks**: snake_case/PascalCase validation
//! - **Help Text**: `--help` output for `tml lint`

#include "linter_internal.hpp"

namespace tml::cli::linter {

// ============================================================================
// ANSI Colors
// ============================================================================

const char* RED = "\033[31m";
const char* YELLOW = "\033[33m";
const char* GREEN = "\033[32m";
const char* CYAN = "\033[36m";
const char* DIM = "\033[2m";
const char* BOLD = "\033[1m";
const char* RESET = "\033[0m";

// ============================================================================
// Lint Rule Codes
// ============================================================================

// Style rules (S)
const char* RULE_TAB = "S001";
const char* RULE_TRAIL = "S002";
const char* RULE_LINE_LENGTH = "S003";
const char* RULE_NAMING_FUNC = "S010";
const char* RULE_NAMING_TYPE = "S011";
const char* RULE_NAMING_CONST = "S012";
const char* RULE_NAMING_VAR = "S013";

// Semantic rules (W)
const char* RULE_UNUSED_VAR = "W001";
const char* RULE_UNUSED_IMPORT = "W002";
const char* RULE_UNUSED_FUNC = "W003";
const char* RULE_UNUSED_PARAM = "W004";

// Complexity rules (C)
const char* RULE_FUNC_LENGTH = "C001";
const char* RULE_CYCLOMATIC = "C002";
const char* RULE_NESTING = "C003";

// ============================================================================
// Naming Convention Checks
// ============================================================================

bool is_snake_case(const std::string& name) {
    if (name.empty())
        return true;
    // Allow leading underscore for unused params
    size_t start = (name[0] == '_') ? 1 : 0;
    if (start >= name.length())
        return true;

    for (size_t i = start; i < name.length(); ++i) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    // Don't start with number (after optional underscore)
    if (start < name.length() && name[start] >= '0' && name[start] <= '9') {
        return false;
    }
    return true;
}

bool is_pascal_case(const std::string& name) {
    if (name.empty())
        return true;
    if (!(name[0] >= 'A' && name[0] <= 'Z'))
        return false;
    for (size_t i = 1; i < name.length(); ++i) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
            return false;
        }
    }
    return true;
}

bool is_upper_snake_case(const std::string& name) {
    if (name.empty())
        return true;
    for (size_t i = 0; i < name.length(); ++i) {
        char c = name[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

// Helper to extract name from pattern
std::string get_pattern_name(const parser::Pattern& pattern) {
    if (pattern.is<parser::IdentPattern>()) {
        return pattern.as<parser::IdentPattern>().name;
    }
    return "";
}

// ============================================================================
// Help
// ============================================================================

void print_lint_help() {
    std::cout << "Usage: tml lint [options] [paths...]\n\n";
    std::cout << "Lint TML source files for style, naming, and complexity issues.\n\n";
    std::cout << "Options:\n";
    std::cout << "  --fix           Automatically fix style issues\n";
    std::cout << "  --semantic      Enable semantic checks (naming, unused, complexity)\n";
    std::cout << "  --quiet, -q     Only show errors (no warnings)\n";
    std::cout << "  --verbose, -v   Show all files being checked\n";
    std::cout << "  --help, -h      Show this help\n\n";
    std::cout << "If no paths are specified, lints the current directory.\n\n";
    std::cout << "Configuration:\n";
    std::cout << "  Add a [lint] section to tml.toml to customize settings.\n\n";
    std::cout << "Style Rules (S):\n";
    std::cout << "  S001  Tabs instead of spaces (error)\n";
    std::cout << "  S002  Trailing whitespace (error)\n";
    std::cout << "  S003  Line exceeds max length (warning)\n";
    std::cout << "  S010  Function naming (snake_case)\n";
    std::cout << "  S011  Type naming (PascalCase)\n";
    std::cout << "  S012  Constant naming (UPPER_SNAKE_CASE)\n";
    std::cout << "  S013  Variable naming (snake_case)\n\n";
    std::cout << "Semantic Rules (W):\n";
    std::cout << "  W001  Unused variable\n";
    std::cout << "  W002  Unused import\n";
    std::cout << "  W004  Unused parameter\n\n";
    std::cout << "Complexity Rules (C):\n";
    std::cout << "  C001  Function too long\n";
    std::cout << "  C002  High cyclomatic complexity\n";
    std::cout << "  C003  Deep nesting\n";
}

} // namespace tml::cli::linter
