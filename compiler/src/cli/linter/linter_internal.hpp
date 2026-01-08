//! # Linter Internal Interface
//!
//! This header defines internal types and utilities for the linter.
//!
//! ## Components
//!
//! | Type            | Description                              |
//! |-----------------|------------------------------------------|
//! | `LintConfig`    | Style rules configuration                |
//! | `LintResult`    | Collected warnings and errors            |
//! | `LintIssue`     | Single lint issue with location          |
//! | `SemanticLinter`| AST visitor for naming/unused checks     |
//!
//! ## Rule Categories
//!
//! - **S (Style)**: Tabs, trailing whitespace, line length, naming
//! - **W (Warning)**: Unused variables/imports/parameters
//! - **C (Complexity)**: Function length, cyclomatic, nesting

#pragma once

// Internal header for lint command implementation
// Contains shared types, utilities, and declarations

#include "cli/cmd_format.hpp"
#include "cli/cmd_lint.hpp"
#include "cli/utils.hpp"
#include "common.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli::linter {

// ============================================================================
// ANSI Colors
// ============================================================================

extern const char* RED;
extern const char* YELLOW;
extern const char* GREEN;
extern const char* CYAN;
extern const char* DIM;
extern const char* BOLD;
extern const char* RESET;

// ============================================================================
// Lint Rule Codes
// ============================================================================

// Style rules (S)
extern const char* RULE_TAB;
extern const char* RULE_TRAIL;
extern const char* RULE_LINE_LENGTH;
extern const char* RULE_NAMING_FUNC;
extern const char* RULE_NAMING_TYPE;
extern const char* RULE_NAMING_CONST;
extern const char* RULE_NAMING_VAR;

// Semantic rules (W)
extern const char* RULE_UNUSED_VAR;
extern const char* RULE_UNUSED_IMPORT;
extern const char* RULE_UNUSED_FUNC;
extern const char* RULE_UNUSED_PARAM;

// Complexity rules (C)
extern const char* RULE_FUNC_LENGTH;
extern const char* RULE_CYCLOMATIC;
extern const char* RULE_NESTING;

// ============================================================================
// Lint Issue
// ============================================================================

enum class Severity { Error, Warning, Info };

struct LintIssue {
    std::string file;
    int line;
    int column;
    std::string code;
    std::string message;
    Severity severity;
    std::string fix_hint;
};

struct LintResult {
    std::vector<LintIssue> issues;
    int files_checked = 0;
    int errors = 0;
    int warnings = 0;
    int infos = 0;
};

// ============================================================================
// Lint Configuration
// ============================================================================

struct LintConfig {
    // Enabled rules
    bool check_tabs = true;
    bool check_trailing = true;
    bool check_line_length = true;
    bool check_naming = true;
    bool check_unused = true;
    bool check_complexity = true;

    // Thresholds
    int max_line_length = 120;
    int max_function_lines = 50;
    int max_cyclomatic_complexity = 10;
    int max_nesting_depth = 4;

    // Disabled rules (by code)
    std::set<std::string> disabled_rules;

    bool is_rule_enabled(const char* code) const {
        return disabled_rules.find(code) == disabled_rules.end();
    }
};

// ============================================================================
// Helper Functions
// ============================================================================

// Naming convention checks
bool is_snake_case(const std::string& name);
bool is_pascal_case(const std::string& name);
bool is_upper_snake_case(const std::string& name);

// Pattern name extraction
std::string get_pattern_name(const parser::Pattern& pattern);

// ============================================================================
// Config Functions
// ============================================================================

// Load lint configuration from tml.toml
LintConfig load_lint_config(const fs::path& project_root);

// ============================================================================
// Style Linting
// ============================================================================

// Text-based style linting
void lint_style(const fs::path& filepath, const std::string& content, LintResult& result,
                const LintConfig& config, bool fix_mode, std::string& fixed_content);

// ============================================================================
// Semantic Linting
// ============================================================================

// AST-based semantic linter class
class SemanticLinter {
public:
    SemanticLinter(const fs::path& filepath, LintResult& result, const LintConfig& config);
    void lint(const parser::Module& module);

private:
    const fs::path& filepath_;
    LintResult& result_;
    const LintConfig& config_;

    struct VarInfo {
        SourceSpan span;
        bool is_used = false;
        bool is_param = false;
    };

    struct ImportInfo {
        SourceSpan span;
        std::string full_path;
        bool is_used = false;
    };

    std::map<std::string, VarInfo> variables_;
    std::map<std::string, ImportInfo> imports_;
    std::set<std::string> used_identifiers_;

    void collect_declarations(const parser::Module& module);
    void collect_import_info(const parser::UseDecl& use_decl);
    void analyze_function(const parser::FuncDecl& func);
    void check_unused_in_function();
    void collect_block_vars(const parser::BlockExpr& block);
    void collect_stmt_vars(const parser::Stmt& stmt);
    void collect_expr_usages(const parser::Expr& expr);
    void check_unused_items();
    void check_naming_conventions(const parser::Module& module);
    void check_complexity(const parser::Module& module);
    void check_function_complexity(const parser::FuncDecl& func);

    int count_statements(const parser::BlockExpr& block);
    int count_expr_statements(const parser::Expr& expr);
    int calculate_cyclomatic_complexity(const parser::BlockExpr& block);
    int count_decision_points(const parser::Stmt& stmt);
    int count_expr_decision_points(const parser::Expr& expr);
    int calculate_max_nesting(const parser::BlockExpr& block, int current_depth = 0);
    int calculate_expr_nesting(const parser::Expr& expr, int current_depth);

    void add_issue(const SourceSpan& span, const char* code, const std::string& message,
                   Severity severity, const std::string& hint = "");
};

// ============================================================================
// File Operations
// ============================================================================

// Lint a single file
void lint_file(const fs::path& filepath, LintResult& result, const LintConfig& config,
               bool fix_mode, bool semantic);

// Find all .tml files in directory
void find_tml_files(const fs::path& dir, std::vector<fs::path>& files);

// Print help
void print_lint_help();

} // namespace tml::cli::linter
