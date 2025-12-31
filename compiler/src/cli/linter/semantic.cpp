// Lint command - semantic linting (AST-based)

#include "linter_internal.hpp"

namespace tml::cli::linter {

// ============================================================================
// Semantic Linter
// ============================================================================

SemanticLinter::SemanticLinter(const fs::path& filepath, LintResult& result,
                               const LintConfig& config)
    : filepath_(filepath), result_(result), config_(config) {}

void SemanticLinter::lint(const parser::Module& module) {
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

void SemanticLinter::collect_declarations(const parser::Module& module) {
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

void SemanticLinter::collect_import_info(const parser::UseDecl& use_decl) {
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

void SemanticLinter::analyze_function(const parser::FuncDecl& func) {
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

void SemanticLinter::check_unused_in_function() {
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

void SemanticLinter::collect_block_vars(const parser::BlockExpr& block) {
    for (const auto& stmt : block.stmts) {
        collect_stmt_vars(*stmt);
    }
    if (block.expr) {
        collect_expr_usages(*block.expr.value());
    }
}

void SemanticLinter::collect_stmt_vars(const parser::Stmt& stmt) {
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

void SemanticLinter::collect_expr_usages(const parser::Expr& expr) {
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

void SemanticLinter::check_unused_items() {
    // Check unused imports (module-level)
    if (config_.is_rule_enabled(RULE_UNUSED_IMPORT)) {
        for (const auto& [name, info] : imports_) {
            if (!info.is_used) {
                add_issue(info.span, RULE_UNUSED_IMPORT, "unused import '" + name + "'",
                          Severity::Warning, "remove the unused import");
            }
        }
    }
}

void SemanticLinter::check_naming_conventions(const parser::Module& module) {
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
                          "function '" + func.name + "' should use snake_case", Severity::Warning);
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
                add_issue(s.span, RULE_NAMING_TYPE, "struct '" + s.name + "' should use PascalCase",
                          Severity::Warning);
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
                add_issue(e.span, RULE_NAMING_TYPE, "enum '" + e.name + "' should use PascalCase",
                          Severity::Warning);
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

void SemanticLinter::check_complexity(const parser::Module& module) {
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            check_function_complexity(func);
        }
    }
}

void SemanticLinter::check_function_complexity(const parser::FuncDecl& func) {
    if (!func.body)
        return;

    // Count statements (simple line count approximation)
    if (config_.is_rule_enabled(RULE_FUNC_LENGTH)) {
        int stmt_count = count_statements(func.body.value());
        if (stmt_count > config_.max_function_lines) {
            add_issue(func.span, RULE_FUNC_LENGTH,
                      "function '" + func.name + "' has " + std::to_string(stmt_count) +
                          " statements (max " + std::to_string(config_.max_function_lines) + ")",
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

int SemanticLinter::count_statements(const parser::BlockExpr& block) {
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

int SemanticLinter::count_expr_statements(const parser::Expr& expr) {
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

int SemanticLinter::calculate_cyclomatic_complexity(const parser::BlockExpr& block) {
    // Cyclomatic complexity = 1 + number of decision points
    int complexity = 1;

    for (const auto& stmt : block.stmts) {
        complexity += count_decision_points(*stmt);
    }

    return complexity;
}

int SemanticLinter::count_decision_points(const parser::Stmt& stmt) {
    int count = 0;

    if (stmt.is<parser::ExprStmt>()) {
        const auto& expr_stmt = stmt.as<parser::ExprStmt>();
        count += count_expr_decision_points(*expr_stmt.expr);
    }

    return count;
}

int SemanticLinter::count_expr_decision_points(const parser::Expr& expr) {
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
            for (const auto& stmt : if_expr.else_branch.value()->as<parser::BlockExpr>().stmts) {
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

int SemanticLinter::calculate_max_nesting(const parser::BlockExpr& block, int current_depth) {
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

int SemanticLinter::calculate_expr_nesting(const parser::Expr& expr, int current_depth) {
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
            int while_depth =
                calculate_max_nesting(while_expr.body->as<parser::BlockExpr>(), current_depth + 1);
            max_depth = std::max(max_depth, while_depth);
        }
    } else if (expr.is<parser::ForExpr>()) {
        const auto& for_expr = expr.as<parser::ForExpr>();
        if (for_expr.body->is<parser::BlockExpr>()) {
            int for_depth =
                calculate_max_nesting(for_expr.body->as<parser::BlockExpr>(), current_depth + 1);
            max_depth = std::max(max_depth, for_depth);
        }
    } else if (expr.is<parser::BlockExpr>()) {
        int block_depth = calculate_max_nesting(expr.as<parser::BlockExpr>(), current_depth + 1);
        max_depth = std::max(max_depth, block_depth);
    }

    return max_depth;
}

void SemanticLinter::add_issue(const SourceSpan& span, const char* code, const std::string& message,
                               Severity severity, const std::string& hint) {
    result_.issues.push_back({filepath_.string(), static_cast<int>(span.start.line),
                              static_cast<int>(span.start.column), code, message, severity, hint});

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

} // namespace tml::cli::linter
