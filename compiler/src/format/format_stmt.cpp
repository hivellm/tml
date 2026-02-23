TML_MODULE("tools")

//! # Statement Formatting
//!
//! This file implements formatting for statement types.
//!
//! ## Statement Types
//!
//! | Statement   | Example                     |
//! |-------------|-----------------------------|
//! | `let`       | `let x: I32 = 42`           |
//! | `var`       | `var count: I32 = 0`        |
//! | Expression  | `foo()` or `x + y`          |

#include "format/formatter.hpp"

namespace tml::format {

void Formatter::format_stmt(const parser::Stmt& stmt) {
    if (stmt.is<parser::LetStmt>()) {
        format_let_stmt(stmt.as<parser::LetStmt>());
    } else if (stmt.is<parser::VarStmt>()) {
        format_var_stmt(stmt.as<parser::VarStmt>());
    } else if (stmt.is<parser::ExprStmt>()) {
        format_expr_stmt(stmt.as<parser::ExprStmt>());
    } else if (stmt.is<parser::DeclPtr>()) {
        format_decl(*stmt.as<parser::DeclPtr>());
    }
}

void Formatter::format_var_stmt(const parser::VarStmt& var) {
    emit_indent();
    output_ << "var " << var.name;

    if (var.type_annotation.has_value()) {
        output_ << ":";
        if (options_.space_after_colon)
            output_ << " ";
        output_ << format_type_ptr(var.type_annotation.value());
    }

    output_ << " = " << format_expr(*var.init) << "\n";
}

void Formatter::format_let_stmt(const parser::LetStmt& let) {
    emit_indent();
    output_ << "let " << format_pattern(*let.pattern);

    if (let.type_annotation.has_value()) {
        output_ << ":";
        if (options_.space_after_colon)
            output_ << " ";
        output_ << format_type_ptr(let.type_annotation.value());
    }

    if (let.init.has_value()) {
        output_ << " = " << format_expr(*let.init.value());
    }

    output_ << "\n";
}

void Formatter::format_expr_stmt(const parser::ExprStmt& expr) {
    emit_indent();
    output_ << format_expr(*expr.expr) << "\n";
}

} // namespace tml::format
