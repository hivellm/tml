#include "cmd_lint.hpp"

#include "cmd_format.hpp"
#include "common.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "utils.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

namespace {

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
constexpr const char* RULE_TAB = "S001";          // Tabs instead of spaces
constexpr const char* RULE_TRAIL = "S002";        // Trailing whitespace
constexpr const char* RULE_LINE_LENGTH = "S003";  // Line too long
constexpr const char* RULE_NAMING_FUNC = "S010";  // Function naming (snake_case)
constexpr const char* RULE_NAMING_TYPE = "S011";  // Type naming (PascalCase)
constexpr const char* RULE_NAMING_CONST = "S012"; // Constant naming (UPPER_CASE)
constexpr const char* RULE_NAMING_VAR = "S013";   // Variable naming (snake_case)

// Semantic rules (W)
constexpr const char* RULE_UNUSED_VAR = "W001";                   // Unused variable
constexpr const char* RULE_UNUSED_IMPORT = "W002";                // Unused import
[[maybe_unused]] constexpr const char* RULE_UNUSED_FUNC = "W003"; // Unused private function
constexpr const char* RULE_UNUSED_PARAM = "W004";                 // Unused function parameter

// Complexity rules (C)
constexpr const char* RULE_FUNC_LENGTH = "C001"; // Function too long
constexpr const char* RULE_CYCLOMATIC = "C002";  // High cyclomatic complexity
constexpr const char* RULE_NESTING = "C003";     // Deep nesting

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
    std::string fix_hint; // Optional hint for --fix
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
// Config File Parsing
// ============================================================================

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

// ============================================================================
// Style Linting (text-based)
// ============================================================================

void lint_style(const fs::path& filepath, const std::string& content, LintResult& result,
                const LintConfig& config, bool fix_mode, std::string& fixed_content) {
    std::istringstream stream(content);
    std::string line;
    int line_number = 0;
    bool file_modified = false;
    std::vector<std::string> lines;

    while (std::getline(stream, line)) {
        line_number++;

        // Check for tabs (TML uses spaces)
        if (config.check_tabs && config.is_rule_enabled(RULE_TAB) &&
            line.find('\t') != std::string::npos) {
            if (fix_mode) {
                std::string fixed;
                for (char c : line) {
                    if (c == '\t') {
                        fixed += "    ";
                    } else {
                        fixed += c;
                    }
                }
                line = fixed;
                file_modified = true;
            } else {
                result.issues.push_back({filepath.string(), line_number, 1, RULE_TAB,
                                         "use spaces instead of tabs", Severity::Error, ""});
                result.errors++;
            }
        }

        // Check for trailing whitespace
        if (config.check_trailing && config.is_rule_enabled(RULE_TRAIL) && !line.empty()) {
            size_t last_non_space = line.find_last_not_of(" \t\r");
            if (last_non_space != std::string::npos && last_non_space < line.length() - 1) {
                if (fix_mode) {
                    line = line.substr(0, last_non_space + 1);
                    file_modified = true;
                } else {
                    result.issues.push_back({filepath.string(), line_number,
                                             static_cast<int>(last_non_space + 2), RULE_TRAIL,
                                             "trailing whitespace", Severity::Error, ""});
                    result.errors++;
                }
            }
        }

        // Check for very long lines
        if (config.check_line_length && config.is_rule_enabled(RULE_LINE_LENGTH) &&
            static_cast<int>(line.length()) > config.max_line_length) {
            result.issues.push_back(
                {filepath.string(), line_number, config.max_line_length + 1, RULE_LINE_LENGTH,
                 "line exceeds " + std::to_string(config.max_line_length) + " characters (" +
                     std::to_string(line.length()) + " chars)",
                 Severity::Warning, ""});
            result.warnings++;
        }

        lines.push_back(line);
    }

    // Build fixed content
    if (fix_mode && file_modified) {
        std::ostringstream oss;
        for (size_t i = 0; i < lines.size(); ++i) {
            oss << lines[i];
            if (i < lines.size() - 1) {
                oss << "\n";
            }
        }
        oss << "\n";
        fixed_content = oss.str();
    } else {
        fixed_content = content;
    }
}

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
// Semantic Linting (AST-based)
// ============================================================================

class SemanticLinter {
public:
    SemanticLinter(const fs::path& filepath, LintResult& result, const LintConfig& config)
        : filepath_(filepath), result_(result), config_(config) {}

    void lint(const parser::Module& module) {
        // First pass: collect all declarations and usages
        collect_declarations(module);

        // Check naming conventions
        if (config_.check_naming) {
            check_naming_conventions(module);
        }

        // Check for unused items
        if (config_.check_unused) {
            check_unused_items();
        }

        // Check complexity
        if (config_.check_complexity) {
            check_complexity(module);
        }
    }

private:
    const fs::path& filepath_;
    LintResult& result_;
    const LintConfig& config_;

    // Track variables: name -> (span, is_used)
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

    void collect_declarations(const parser::Module& module) {
        // First, collect imports (module-level)
        for (const auto& decl : module.decls) {
            if (decl->is<parser::UseDecl>()) {
                collect_import_info(decl->as<parser::UseDecl>());
            }
        }

        // Then process each function separately for variable tracking
        for (const auto& decl : module.decls) {
            if (decl->is<parser::FuncDecl>()) {
                analyze_function(decl->as<parser::FuncDecl>());
            }
        }
    }

    void collect_import_info(const parser::UseDecl& use_decl) {
        // Get the last segment of the path as the imported name
        if (!use_decl.path.segments.empty()) {
            std::string import_name = use_decl.path.segments.back();
            std::string full_path;
            for (size_t i = 0; i < use_decl.path.segments.size(); ++i) {
                if (i > 0)
                    full_path += "::";
                full_path += use_decl.path.segments[i];
            }

            // Handle alias
            if (use_decl.alias) {
                import_name = use_decl.alias.value();
            }

            // Handle grouped imports: use std::math::{abs, sqrt}
            if (use_decl.symbols) {
                for (const auto& sym : use_decl.symbols.value()) {
                    imports_[sym] = {use_decl.span, full_path + "::" + sym, false};
                }
            } else if (!use_decl.is_glob) {
                imports_[import_name] = {use_decl.span, full_path, false};
            }
        }
    }

    void analyze_function(const parser::FuncDecl& func) {
        // Clear function-local state
        variables_.clear();
        used_identifiers_.clear();

        // Collect parameters
        for (const auto& param : func.params) {
            std::string param_name = get_pattern_name(*param.pattern);
            if (!param_name.empty() && param_name[0] != '_') {
                variables_[param_name] = {param.span, false, true};
            }
        }

        // Collect variables and usages from body
        if (func.body) {
            collect_block_vars(func.body.value());
        }

        // Check for unused variables/params in this function
        check_unused_in_function();
    }

    void check_unused_in_function() {
        // Check unused variables
        if (config_.is_rule_enabled(RULE_UNUSED_VAR)) {
            for (const auto& [name, info] : variables_) {
                if (!info.is_used && !info.is_param) {
                    add_issue(info.span, RULE_UNUSED_VAR, "unused variable '" + name + "'",
                              Severity::Warning, "prefix with underscore to silence: _" + name);
                }
            }
        }

        // Check unused parameters
        if (config_.is_rule_enabled(RULE_UNUSED_PARAM)) {
            for (const auto& [name, info] : variables_) {
                if (!info.is_used && info.is_param) {
                    add_issue(info.span, RULE_UNUSED_PARAM, "unused parameter '" + name + "'",
                              Severity::Warning, "prefix with underscore to silence: _" + name);
                }
            }
        }
    }

    void collect_decl_info([[maybe_unused]] const parser::Decl& decl) {
        // Now handled by collect_declarations separately
    }

    void collect_block_vars(const parser::BlockExpr& block) {
        for (const auto& stmt : block.stmts) {
            collect_stmt_vars(*stmt);
        }
        if (block.expr) {
            collect_expr_usages(*block.expr.value());
        }
    }

    void collect_stmt_vars(const parser::Stmt& stmt) {
        if (stmt.is<parser::LetStmt>()) {
            const auto& let_stmt = stmt.as<parser::LetStmt>();
            std::string var_name = get_pattern_name(*let_stmt.pattern);
            if (!var_name.empty() && var_name[0] != '_') {
                variables_[var_name] = {let_stmt.span, false, false};
            }
            if (let_stmt.init) {
                collect_expr_usages(*let_stmt.init.value());
            }
        } else if (stmt.is<parser::VarStmt>()) {
            const auto& var_stmt = stmt.as<parser::VarStmt>();
            if (!var_stmt.name.empty() && var_stmt.name[0] != '_') {
                variables_[var_stmt.name] = {var_stmt.span, false, false};
            }
            collect_expr_usages(*var_stmt.init);
        } else if (stmt.is<parser::ExprStmt>()) {
            const auto& expr_stmt = stmt.as<parser::ExprStmt>();
            collect_expr_usages(*expr_stmt.expr);
        }
    }

    void collect_expr_usages(const parser::Expr& expr) {
        if (expr.is<parser::IdentExpr>()) {
            const auto& ident = expr.as<parser::IdentExpr>();
            used_identifiers_.insert(ident.name);
            // Mark variable as used
            auto it = variables_.find(ident.name);
            if (it != variables_.end()) {
                it->second.is_used = true;
            }
            // Mark import as used
            auto import_it = imports_.find(ident.name);
            if (import_it != imports_.end()) {
                import_it->second.is_used = true;
            }
        } else if (expr.is<parser::PathExpr>()) {
            const auto& path = expr.as<parser::PathExpr>();
            if (!path.path.segments.empty()) {
                // First segment might be an import
                auto import_it = imports_.find(path.path.segments[0]);
                if (import_it != imports_.end()) {
                    import_it->second.is_used = true;
                }
            }
        } else if (expr.is<parser::BinaryExpr>()) {
            const auto& bin = expr.as<parser::BinaryExpr>();
            collect_expr_usages(*bin.left);
            collect_expr_usages(*bin.right);
        } else if (expr.is<parser::UnaryExpr>()) {
            const auto& un = expr.as<parser::UnaryExpr>();
            collect_expr_usages(*un.operand);
        } else if (expr.is<parser::CallExpr>()) {
            const auto& call = expr.as<parser::CallExpr>();
            collect_expr_usages(*call.callee);
            for (const auto& arg : call.args) {
                collect_expr_usages(*arg);
            }
        } else if (expr.is<parser::MethodCallExpr>()) {
            const auto& method = expr.as<parser::MethodCallExpr>();
            collect_expr_usages(*method.receiver);
            for (const auto& arg : method.args) {
                collect_expr_usages(*arg);
            }
        } else if (expr.is<parser::FieldExpr>()) {
            const auto& field = expr.as<parser::FieldExpr>();
            collect_expr_usages(*field.object);
        } else if (expr.is<parser::IndexExpr>()) {
            const auto& idx = expr.as<parser::IndexExpr>();
            collect_expr_usages(*idx.object);
            collect_expr_usages(*idx.index);
        } else if (expr.is<parser::IfExpr>()) {
            const auto& if_expr = expr.as<parser::IfExpr>();
            collect_expr_usages(*if_expr.condition);
            collect_expr_usages(*if_expr.then_branch);
            if (if_expr.else_branch) {
                collect_expr_usages(*if_expr.else_branch.value());
            }
        } else if (expr.is<parser::BlockExpr>()) {
            collect_block_vars(expr.as<parser::BlockExpr>());
        } else if (expr.is<parser::LoopExpr>()) {
            const auto& loop = expr.as<parser::LoopExpr>();
            collect_expr_usages(*loop.body);
        } else if (expr.is<parser::WhileExpr>()) {
            const auto& while_expr = expr.as<parser::WhileExpr>();
            collect_expr_usages(*while_expr.condition);
            collect_expr_usages(*while_expr.body);
        } else if (expr.is<parser::ForExpr>()) {
            const auto& for_expr = expr.as<parser::ForExpr>();
            collect_expr_usages(*for_expr.iter);
            collect_expr_usages(*for_expr.body);
        } else if (expr.is<parser::ReturnExpr>()) {
            const auto& ret = expr.as<parser::ReturnExpr>();
            if (ret.value) {
                collect_expr_usages(*ret.value.value());
            }
        } else if (expr.is<parser::ArrayExpr>()) {
            const auto& arr = expr.as<parser::ArrayExpr>();
            if (std::holds_alternative<std::vector<parser::ExprPtr>>(arr.kind)) {
                const auto& elems = std::get<std::vector<parser::ExprPtr>>(arr.kind);
                for (const auto& elem : elems) {
                    collect_expr_usages(*elem);
                }
            } else {
                const auto& repeat_pair =
                    std::get<std::pair<parser::ExprPtr, parser::ExprPtr>>(arr.kind);
                collect_expr_usages(*repeat_pair.first);
                collect_expr_usages(*repeat_pair.second);
            }
        } else if (expr.is<parser::TupleExpr>()) {
            const auto& tup = expr.as<parser::TupleExpr>();
            for (const auto& elem : tup.elements) {
                collect_expr_usages(*elem);
            }
        } else if (expr.is<parser::StructExpr>()) {
            const auto& s = expr.as<parser::StructExpr>();
            // Check if struct name is an import
            if (!s.path.segments.empty()) {
                auto import_it = imports_.find(s.path.segments[0]);
                if (import_it != imports_.end()) {
                    import_it->second.is_used = true;
                }
            }
            for (const auto& field : s.fields) {
                collect_expr_usages(*field.second);
            }
            if (s.base) {
                collect_expr_usages(*s.base.value());
            }
        } else if (expr.is<parser::WhenExpr>()) {
            const auto& when = expr.as<parser::WhenExpr>();
            collect_expr_usages(*when.scrutinee);
            for (const auto& arm : when.arms) {
                collect_expr_usages(*arm.body);
                if (arm.guard) {
                    collect_expr_usages(*arm.guard.value());
                }
            }
        } else if (expr.is<parser::ClosureExpr>()) {
            const auto& closure = expr.as<parser::ClosureExpr>();
            collect_expr_usages(*closure.body);
        } else if (expr.is<parser::CastExpr>()) {
            const auto& cast = expr.as<parser::CastExpr>();
            collect_expr_usages(*cast.expr);
        } else if (expr.is<parser::TryExpr>()) {
            const auto& try_expr = expr.as<parser::TryExpr>();
            collect_expr_usages(*try_expr.expr);
        }
    }

    void check_unused_items() {
        // Check unused imports (module-level)
        if (config_.is_rule_enabled(RULE_UNUSED_IMPORT)) {
            for (const auto& [name, info] : imports_) {
                if (!info.is_used) {
                    add_issue(info.span, RULE_UNUSED_IMPORT, "unused import '" + name + "'",
                              Severity::Warning, "remove the unused import");
                }
            }
        }
        // Note: Unused variables and params are now checked per-function in
        // check_unused_in_function()
    }

    void check_naming_conventions(const parser::Module& module) {
        for (const auto& decl : module.decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();
                // Skip test functions (may have different naming)
                bool is_test = false;
                for (const auto& decorator : func.decorators) {
                    if (decorator.name == "test" || decorator.name == "bench") {
                        is_test = true;
                        break;
                    }
                }

                if (!is_test && config_.is_rule_enabled(RULE_NAMING_FUNC) &&
                    !is_snake_case(func.name)) {
                    add_issue(func.span, RULE_NAMING_FUNC,
                              "function '" + func.name + "' should use snake_case",
                              Severity::Warning);
                }

                // Check parameter naming
                if (config_.is_rule_enabled(RULE_NAMING_VAR)) {
                    for (const auto& param : func.params) {
                        std::string param_name = get_pattern_name(*param.pattern);
                        if (!param_name.empty() && !is_snake_case(param_name)) {
                            add_issue(param.span, RULE_NAMING_VAR,
                                      "parameter '" + param_name + "' should use snake_case",
                                      Severity::Warning);
                        }
                    }
                }
            } else if (decl->is<parser::StructDecl>()) {
                const auto& s = decl->as<parser::StructDecl>();
                if (config_.is_rule_enabled(RULE_NAMING_TYPE) && !is_pascal_case(s.name)) {
                    add_issue(s.span, RULE_NAMING_TYPE,
                              "struct '" + s.name + "' should use PascalCase", Severity::Warning);
                }
                // Check field naming
                if (config_.is_rule_enabled(RULE_NAMING_VAR)) {
                    for (const auto& field : s.fields) {
                        if (!is_snake_case(field.name)) {
                            add_issue(field.span, RULE_NAMING_VAR,
                                      "field '" + field.name + "' should use snake_case",
                                      Severity::Warning);
                        }
                    }
                }
            } else if (decl->is<parser::EnumDecl>()) {
                const auto& e = decl->as<parser::EnumDecl>();
                if (config_.is_rule_enabled(RULE_NAMING_TYPE) && !is_pascal_case(e.name)) {
                    add_issue(e.span, RULE_NAMING_TYPE,
                              "enum '" + e.name + "' should use PascalCase", Severity::Warning);
                }
                // Variants should be PascalCase
                if (config_.is_rule_enabled(RULE_NAMING_TYPE)) {
                    for (const auto& variant : e.variants) {
                        if (!is_pascal_case(variant.name)) {
                            add_issue(variant.span, RULE_NAMING_TYPE,
                                      "variant '" + variant.name + "' should use PascalCase",
                                      Severity::Warning);
                        }
                    }
                }
            } else if (decl->is<parser::ConstDecl>()) {
                const auto& c = decl->as<parser::ConstDecl>();
                if (config_.is_rule_enabled(RULE_NAMING_CONST) && !is_upper_snake_case(c.name)) {
                    add_issue(c.span, RULE_NAMING_CONST,
                              "constant '" + c.name + "' should use UPPER_SNAKE_CASE",
                              Severity::Warning);
                }
            } else if (decl->is<parser::TraitDecl>()) {
                const auto& t = decl->as<parser::TraitDecl>();
                if (config_.is_rule_enabled(RULE_NAMING_TYPE) && !is_pascal_case(t.name)) {
                    add_issue(t.span, RULE_NAMING_TYPE,
                              "behavior '" + t.name + "' should use PascalCase", Severity::Warning);
                }
            }
        }
    }

    void check_complexity(const parser::Module& module) {
        for (const auto& decl : module.decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();
                check_function_complexity(func);
            }
        }
    }

    void check_function_complexity(const parser::FuncDecl& func) {
        if (!func.body)
            return;

        // Count statements (simple line count approximation)
        if (config_.is_rule_enabled(RULE_FUNC_LENGTH)) {
            int stmt_count = count_statements(func.body.value());
            if (stmt_count > config_.max_function_lines) {
                add_issue(func.span, RULE_FUNC_LENGTH,
                          "function '" + func.name + "' has " + std::to_string(stmt_count) +
                              " statements (max " + std::to_string(config_.max_function_lines) +
                              ")",
                          Severity::Warning);
            }
        }

        // Calculate cyclomatic complexity
        if (config_.is_rule_enabled(RULE_CYCLOMATIC)) {
            int complexity = calculate_cyclomatic_complexity(func.body.value());
            if (complexity > config_.max_cyclomatic_complexity) {
                add_issue(func.span, RULE_CYCLOMATIC,
                          "function '" + func.name + "' has cyclomatic complexity " +
                              std::to_string(complexity) + " (max " +
                              std::to_string(config_.max_cyclomatic_complexity) + ")",
                          Severity::Warning);
            }
        }

        // Check nesting depth
        if (config_.is_rule_enabled(RULE_NESTING)) {
            int max_depth = calculate_max_nesting(func.body.value());
            if (max_depth > config_.max_nesting_depth) {
                add_issue(func.span, RULE_NESTING,
                          "function '" + func.name + "' has nesting depth " +
                              std::to_string(max_depth) + " (max " +
                              std::to_string(config_.max_nesting_depth) + ")",
                          Severity::Warning);
            }
        }
    }

    int count_statements(const parser::BlockExpr& block) {
        int count = 0;
        for (const auto& stmt : block.stmts) {
            count++;
            // Recursively count nested blocks
            if (stmt->is<parser::ExprStmt>()) {
                const auto& expr_stmt = stmt->as<parser::ExprStmt>();
                count += count_expr_statements(*expr_stmt.expr);
            }
        }
        return count;
    }

    int count_expr_statements(const parser::Expr& expr) {
        int count = 0;
        if (expr.is<parser::IfExpr>()) {
            const auto& if_expr = expr.as<parser::IfExpr>();
            if (if_expr.then_branch->is<parser::BlockExpr>()) {
                count += count_statements(if_expr.then_branch->as<parser::BlockExpr>());
            }
            if (if_expr.else_branch && if_expr.else_branch.value()->is<parser::BlockExpr>()) {
                count += count_statements(if_expr.else_branch.value()->as<parser::BlockExpr>());
            }
        } else if (expr.is<parser::LoopExpr>()) {
            const auto& loop = expr.as<parser::LoopExpr>();
            if (loop.body->is<parser::BlockExpr>()) {
                count += count_statements(loop.body->as<parser::BlockExpr>());
            }
        } else if (expr.is<parser::WhileExpr>()) {
            const auto& while_expr = expr.as<parser::WhileExpr>();
            if (while_expr.body->is<parser::BlockExpr>()) {
                count += count_statements(while_expr.body->as<parser::BlockExpr>());
            }
        } else if (expr.is<parser::ForExpr>()) {
            const auto& for_expr = expr.as<parser::ForExpr>();
            if (for_expr.body->is<parser::BlockExpr>()) {
                count += count_statements(for_expr.body->as<parser::BlockExpr>());
            }
        } else if (expr.is<parser::BlockExpr>()) {
            count += count_statements(expr.as<parser::BlockExpr>());
        }
        return count;
    }

    int calculate_cyclomatic_complexity(const parser::BlockExpr& block) {
        // Cyclomatic complexity = 1 + number of decision points
        int complexity = 1;

        for (const auto& stmt : block.stmts) {
            complexity += count_decision_points(*stmt);
        }

        return complexity;
    }

    int count_decision_points(const parser::Stmt& stmt) {
        int count = 0;

        if (stmt.is<parser::ExprStmt>()) {
            const auto& expr_stmt = stmt.as<parser::ExprStmt>();
            count += count_expr_decision_points(*expr_stmt.expr);
        }

        return count;
    }

    int count_expr_decision_points(const parser::Expr& expr) {
        int count = 0;

        if (expr.is<parser::IfExpr>()) {
            count++; // if itself is a decision point
            const auto& if_expr = expr.as<parser::IfExpr>();
            if (if_expr.then_branch->is<parser::BlockExpr>()) {
                for (const auto& stmt : if_expr.then_branch->as<parser::BlockExpr>().stmts) {
                    count += count_decision_points(*stmt);
                }
            }
            if (if_expr.else_branch && if_expr.else_branch.value()->is<parser::BlockExpr>()) {
                for (const auto& stmt :
                     if_expr.else_branch.value()->as<parser::BlockExpr>().stmts) {
                    count += count_decision_points(*stmt);
                }
            }
        } else if (expr.is<parser::LoopExpr>()) {
            count++; // loop is a decision point
            const auto& loop = expr.as<parser::LoopExpr>();
            if (loop.body->is<parser::BlockExpr>()) {
                for (const auto& stmt : loop.body->as<parser::BlockExpr>().stmts) {
                    count += count_decision_points(*stmt);
                }
            }
        } else if (expr.is<parser::WhileExpr>()) {
            count++; // while is a decision point
            const auto& while_expr = expr.as<parser::WhileExpr>();
            if (while_expr.body->is<parser::BlockExpr>()) {
                for (const auto& stmt : while_expr.body->as<parser::BlockExpr>().stmts) {
                    count += count_decision_points(*stmt);
                }
            }
        } else if (expr.is<parser::ForExpr>()) {
            count++; // for is a decision point
            const auto& for_expr = expr.as<parser::ForExpr>();
            if (for_expr.body->is<parser::BlockExpr>()) {
                for (const auto& stmt : for_expr.body->as<parser::BlockExpr>().stmts) {
                    count += count_decision_points(*stmt);
                }
            }
        } else if (expr.is<parser::WhenExpr>()) {
            const auto& when_expr = expr.as<parser::WhenExpr>();
            count += static_cast<int>(when_expr.arms.size()); // Each arm is a decision point
        } else if (expr.is<parser::BinaryExpr>()) {
            const auto& bin = expr.as<parser::BinaryExpr>();
            if (bin.op == parser::BinaryOp::And || bin.op == parser::BinaryOp::Or) {
                count++; // Short-circuit operators are decision points
            }
            count += count_expr_decision_points(*bin.left);
            count += count_expr_decision_points(*bin.right);
        }

        return count;
    }

    int calculate_max_nesting(const parser::BlockExpr& block, int current_depth = 0) {
        int max_depth = current_depth;

        for (const auto& stmt : block.stmts) {
            if (stmt->is<parser::ExprStmt>()) {
                const auto& expr_stmt = stmt->as<parser::ExprStmt>();
                int depth = calculate_expr_nesting(*expr_stmt.expr, current_depth);
                max_depth = std::max(max_depth, depth);
            }
        }

        return max_depth;
    }

    int calculate_expr_nesting(const parser::Expr& expr, int current_depth) {
        int max_depth = current_depth;

        if (expr.is<parser::IfExpr>()) {
            const auto& if_expr = expr.as<parser::IfExpr>();
            if (if_expr.then_branch->is<parser::BlockExpr>()) {
                int then_depth = calculate_max_nesting(if_expr.then_branch->as<parser::BlockExpr>(),
                                                       current_depth + 1);
                max_depth = std::max(max_depth, then_depth);
            }
            if (if_expr.else_branch && if_expr.else_branch.value()->is<parser::BlockExpr>()) {
                int else_depth = calculate_max_nesting(
                    if_expr.else_branch.value()->as<parser::BlockExpr>(), current_depth + 1);
                max_depth = std::max(max_depth, else_depth);
            }
        } else if (expr.is<parser::LoopExpr>()) {
            const auto& loop = expr.as<parser::LoopExpr>();
            if (loop.body->is<parser::BlockExpr>()) {
                int loop_depth =
                    calculate_max_nesting(loop.body->as<parser::BlockExpr>(), current_depth + 1);
                max_depth = std::max(max_depth, loop_depth);
            }
        } else if (expr.is<parser::WhileExpr>()) {
            const auto& while_expr = expr.as<parser::WhileExpr>();
            if (while_expr.body->is<parser::BlockExpr>()) {
                int while_depth = calculate_max_nesting(while_expr.body->as<parser::BlockExpr>(),
                                                        current_depth + 1);
                max_depth = std::max(max_depth, while_depth);
            }
        } else if (expr.is<parser::ForExpr>()) {
            const auto& for_expr = expr.as<parser::ForExpr>();
            if (for_expr.body->is<parser::BlockExpr>()) {
                int for_depth = calculate_max_nesting(for_expr.body->as<parser::BlockExpr>(),
                                                      current_depth + 1);
                max_depth = std::max(max_depth, for_depth);
            }
        } else if (expr.is<parser::BlockExpr>()) {
            int block_depth =
                calculate_max_nesting(expr.as<parser::BlockExpr>(), current_depth + 1);
            max_depth = std::max(max_depth, block_depth);
        }

        return max_depth;
    }

    void add_issue(const SourceSpan& span, const char* code, const std::string& message,
                   Severity severity, const std::string& hint = "") {
        result_.issues.push_back({filepath_.string(), static_cast<int>(span.start.line),
                                  static_cast<int>(span.start.column), code, message, severity,
                                  hint});

        switch (severity) {
        case Severity::Error:
            result_.errors++;
            break;
        case Severity::Warning:
            result_.warnings++;
            break;
        case Severity::Info:
            result_.infos++;
            break;
        }
    }
};

// ============================================================================
// File Linting
// ============================================================================

void lint_file(const fs::path& filepath, LintResult& result, const LintConfig& config,
               bool fix_mode, bool semantic) {
    std::ifstream file(filepath);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << filepath << "\n";
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
        std::cerr << "Warning: Cannot access " << dir << ": " << e.what() << "\n";
    }
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

} // anonymous namespace

// ============================================================================
// Main Entry Point
// ============================================================================

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
