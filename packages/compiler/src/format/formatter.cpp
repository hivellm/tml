// TML source code formatter
// Produces consistently formatted TML code from an AST

#include "tml/format/formatter.hpp"

namespace tml::format {

Formatter::Formatter(FormatOptions options)
    : options_(std::move(options)) {}

void Formatter::emit(const std::string& text) {
    output_ << text;
}

void Formatter::emit_line(const std::string& text) {
    emit_indent();
    output_ << text << "\n";
}

void Formatter::emit_newline() {
    output_ << "\n";
}

void Formatter::emit_indent() {
    output_ << indent_str();
}

void Formatter::push_indent() {
    ++indent_level_;
}

void Formatter::pop_indent() {
    if (indent_level_ > 0) --indent_level_;
}

auto Formatter::indent_str() const -> std::string {
    if (options_.use_tabs) {
        return std::string(indent_level_, '\t');
    }
    return std::string(indent_level_ * options_.indent_width, ' ');
}

auto Formatter::format(const parser::Module& module) -> std::string {
    output_.str("");
    indent_level_ = 0;

    for (size_t i = 0; i < module.decls.size(); ++i) {
        format_decl(*module.decls[i]);
        if (i + 1 < module.decls.size()) {
            emit_newline();
        }
    }

    return output_.str();
}

void Formatter::format_decl(const parser::Decl& decl) {
    if (decl.is<parser::FuncDecl>()) {
        format_func_decl(decl.as<parser::FuncDecl>());
    } else if (decl.is<parser::StructDecl>()) {
        format_struct_decl(decl.as<parser::StructDecl>());
    } else if (decl.is<parser::EnumDecl>()) {
        format_enum_decl(decl.as<parser::EnumDecl>());
    } else if (decl.is<parser::TraitDecl>()) {
        format_trait_decl(decl.as<parser::TraitDecl>());
    } else if (decl.is<parser::ImplDecl>()) {
        format_impl_decl(decl.as<parser::ImplDecl>());
    } else if (decl.is<parser::TypeAliasDecl>()) {
        format_type_alias(decl.as<parser::TypeAliasDecl>());
    } else if (decl.is<parser::ConstDecl>()) {
        format_const_decl(decl.as<parser::ConstDecl>());
    } else if (decl.is<parser::UseDecl>()) {
        format_use_decl(decl.as<parser::UseDecl>());
    } else if (decl.is<parser::ModDecl>()) {
        format_mod_decl(decl.as<parser::ModDecl>());
    }
}

void Formatter::format_decorators(const std::vector<parser::Decorator>& decorators) {
    for (const auto& dec : decorators) {
        emit_indent();
        output_ << "@" << dec.name;
        if (!dec.args.empty()) {
            output_ << "(";
            for (size_t i = 0; i < dec.args.size(); ++i) {
                if (i > 0) output_ << ", ";
                output_ << format_expr(*dec.args[i]);
            }
            output_ << ")";
        }
        output_ << "\n";
    }
}

void Formatter::format_visibility(parser::Visibility vis) {
    if (vis == parser::Visibility::Public) {
        output_ << "pub ";
    }
}

void Formatter::format_generics(const std::vector<parser::GenericParam>& generics) {
    if (generics.empty()) return;

    output_ << "[";
    for (size_t i = 0; i < generics.size(); ++i) {
        if (i > 0) output_ << ", ";
        output_ << generics[i].name;
        if (!generics[i].bounds.empty()) {
            output_ << ": ";
            for (size_t j = 0; j < generics[i].bounds.size(); ++j) {
                if (j > 0) output_ << " + ";
                output_ << format_type_path(generics[i].bounds[j]);
            }
        }
    }
    output_ << "]";
}

void Formatter::format_where_clause(const std::optional<parser::WhereClause>& where) {
    if (!where.has_value() || where->constraints.empty()) return;

    output_ << "\n";
    emit_indent();
    output_ << "where ";
    for (size_t i = 0; i < where->constraints.size(); ++i) {
        if (i > 0) output_ << ", ";
        const auto& [type, bounds] = where->constraints[i];
        output_ << format_type_ptr(type) << ": ";
        for (size_t j = 0; j < bounds.size(); ++j) {
            if (j > 0) output_ << " + ";
            output_ << format_type_path(bounds[j]);
        }
    }
}

auto Formatter::format_func_params(const std::vector<parser::FuncParam>& params) -> std::string {
    std::stringstream ss;
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << format_pattern(*params[i].pattern);
        // Special case: 'this' doesn't need type annotation
        if (!(params[i].pattern->is<parser::IdentPattern>() &&
              params[i].pattern->as<parser::IdentPattern>().name == "this")) {
            ss << ":";
            if (options_.space_after_colon) ss << " ";
            ss << format_type_ptr(params[i].type);
        }
    }
    return ss.str();
}

void Formatter::format_func_decl(const parser::FuncDecl& func) {
    format_decorators(func.decorators);
    emit_indent();
    format_visibility(func.vis);

    if (func.is_async) output_ << "async ";
    if (func.is_unsafe) output_ << "lowlevel ";

    output_ << "func " << func.name;
    format_generics(func.generics);
    output_ << "(" << format_func_params(func.params) << ")";

    if (func.return_type.has_value()) {
        output_ << " -> " << format_type_ptr(func.return_type.value());
    }

    format_where_clause(func.where_clause);

    if (func.body.has_value()) {
        output_ << " {\n";
        push_indent();

        for (const auto& stmt : func.body->stmts) {
            format_stmt(*stmt);
        }

        if (func.body->expr.has_value()) {
            emit_indent();
            output_ << format_expr(*func.body->expr.value()) << "\n";
        }

        pop_indent();
        emit_line("}");
    } else {
        output_ << "\n";
    }
}

void Formatter::format_struct_decl(const parser::StructDecl& s) {
    format_decorators(s.decorators);
    emit_indent();
    format_visibility(s.vis);
    output_ << "type " << s.name;
    format_generics(s.generics);
    format_where_clause(s.where_clause);
    output_ << " {\n";

    push_indent();
    for (const auto& field : s.fields) {
        emit_indent();
        if (field.vis == parser::Visibility::Public) {
            output_ << "pub ";
        }
        output_ << field.name << ":";
        if (options_.space_after_colon) output_ << " ";
        output_ << format_type_ptr(field.type);
        if (options_.trailing_commas) output_ << ",";
        output_ << "\n";
    }
    pop_indent();

    emit_line("}");
}

void Formatter::format_enum_decl(const parser::EnumDecl& e) {
    format_decorators(e.decorators);
    emit_indent();
    format_visibility(e.vis);
    output_ << "type " << e.name;
    format_generics(e.generics);
    format_where_clause(e.where_clause);
    output_ << " {\n";

    push_indent();
    for (const auto& variant : e.variants) {
        emit_indent();
        output_ << variant.name;

        if (variant.tuple_fields.has_value()) {
            output_ << "(";
            for (size_t i = 0; i < variant.tuple_fields->size(); ++i) {
                if (i > 0) output_ << ", ";
                output_ << format_type_ptr((*variant.tuple_fields)[i]);
            }
            output_ << ")";
        } else if (variant.struct_fields.has_value()) {
            output_ << " {\n";
            push_indent();
            for (const auto& field : *variant.struct_fields) {
                emit_indent();
                output_ << field.name << ":";
                if (options_.space_after_colon) output_ << " ";
                output_ << format_type_ptr(field.type);
                if (options_.trailing_commas) output_ << ",";
                output_ << "\n";
            }
            pop_indent();
            emit_indent();
            output_ << "}";
        }

        if (options_.trailing_commas) output_ << ",";
        output_ << "\n";
    }
    pop_indent();

    emit_line("}");
}

void Formatter::format_trait_decl(const parser::TraitDecl& t) {
    format_decorators(t.decorators);
    emit_indent();
    format_visibility(t.vis);
    output_ << "behavior " << t.name;
    format_generics(t.generics);

    if (!t.super_traits.empty()) {
        output_ << ": ";
        for (size_t i = 0; i < t.super_traits.size(); ++i) {
            if (i > 0) output_ << " + ";
            output_ << format_type_path(t.super_traits[i]);
        }
    }

    format_where_clause(t.where_clause);
    output_ << " {\n";

    push_indent();
    for (size_t i = 0; i < t.methods.size(); ++i) {
        format_func_decl(t.methods[i]);
        if (i + 1 < t.methods.size()) emit_newline();
    }
    pop_indent();

    emit_line("}");
}

void Formatter::format_impl_decl(const parser::ImplDecl& impl) {
    emit_indent();
    output_ << "impl";
    format_generics(impl.generics);
    output_ << " ";

    if (impl.trait_path.has_value()) {
        output_ << format_type_path(impl.trait_path.value()) << " for ";
    }

    output_ << format_type_ptr(impl.self_type);
    format_where_clause(impl.where_clause);
    output_ << " {\n";

    push_indent();
    for (size_t i = 0; i < impl.methods.size(); ++i) {
        format_func_decl(impl.methods[i]);
        if (i + 1 < impl.methods.size()) emit_newline();
    }
    pop_indent();

    emit_line("}");
}

void Formatter::format_type_alias(const parser::TypeAliasDecl& alias) {
    emit_indent();
    format_visibility(alias.vis);
    output_ << "type " << alias.name;
    format_generics(alias.generics);
    output_ << " = " << format_type_ptr(alias.type) << "\n";
}

void Formatter::format_const_decl(const parser::ConstDecl& c) {
    emit_indent();
    format_visibility(c.vis);
    output_ << "const " << c.name << ":";
    if (options_.space_after_colon) output_ << " ";
    output_ << format_type_ptr(c.type) << " = " << format_expr(*c.value) << "\n";
}

void Formatter::format_use_decl(const parser::UseDecl& u) {
    emit_indent();
    format_visibility(u.vis);
    output_ << "use " << format_type_path(u.path);
    if (u.alias.has_value()) {
        output_ << " as " << u.alias.value();
    }
    output_ << "\n";
}

void Formatter::format_mod_decl(const parser::ModDecl& m) {
    emit_indent();
    format_visibility(m.vis);
    output_ << "mod " << m.name;

    if (m.items.has_value()) {
        output_ << " {\n";
        push_indent();
        for (const auto& item : m.items.value()) {
            format_decl(*item);
        }
        pop_indent();
        emit_line("}");
    } else {
        output_ << "\n";
    }
}

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
        if (options_.space_after_colon) output_ << " ";
        output_ << format_type_ptr(var.type_annotation.value());
    }

    output_ << " = " << format_expr(*var.init) << "\n";
}

void Formatter::format_let_stmt(const parser::LetStmt& let) {
    emit_indent();
    output_ << "let " << format_pattern(*let.pattern);

    if (let.type_annotation.has_value()) {
        output_ << ":";
        if (options_.space_after_colon) output_ << " ";
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

auto Formatter::format_expr(const parser::Expr& expr) -> std::string {
    if (expr.is<parser::LiteralExpr>()) {
        return format_literal(expr.as<parser::LiteralExpr>());
    } else if (expr.is<parser::IdentExpr>()) {
        return format_ident(expr.as<parser::IdentExpr>());
    } else if (expr.is<parser::BinaryExpr>()) {
        return format_binary(expr.as<parser::BinaryExpr>());
    } else if (expr.is<parser::UnaryExpr>()) {
        return format_unary(expr.as<parser::UnaryExpr>());
    } else if (expr.is<parser::CallExpr>()) {
        return format_call(expr.as<parser::CallExpr>());
    } else if (expr.is<parser::MethodCallExpr>()) {
        return format_method_call(expr.as<parser::MethodCallExpr>());
    } else if (expr.is<parser::FieldExpr>()) {
        return format_field(expr.as<parser::FieldExpr>());
    } else if (expr.is<parser::IndexExpr>()) {
        return format_index(expr.as<parser::IndexExpr>());
    } else if (expr.is<parser::IfExpr>()) {
        return format_if(expr.as<parser::IfExpr>());
    } else if (expr.is<parser::BlockExpr>()) {
        return format_block(expr.as<parser::BlockExpr>());
    } else if (expr.is<parser::LoopExpr>()) {
        return format_loop(expr.as<parser::LoopExpr>());
    } else if (expr.is<parser::WhileExpr>()) {
        return format_while(expr.as<parser::WhileExpr>());
    } else if (expr.is<parser::ForExpr>()) {
        return format_for(expr.as<parser::ForExpr>());
    } else if (expr.is<parser::WhenExpr>()) {
        return format_when(expr.as<parser::WhenExpr>());
    } else if (expr.is<parser::ReturnExpr>()) {
        return format_return(expr.as<parser::ReturnExpr>());
    } else if (expr.is<parser::BreakExpr>()) {
        return format_break(expr.as<parser::BreakExpr>());
    } else if (expr.is<parser::ContinueExpr>()) {
        return format_continue(expr.as<parser::ContinueExpr>());
    } else if (expr.is<parser::StructExpr>()) {
        return format_struct_expr(expr.as<parser::StructExpr>());
    } else if (expr.is<parser::TupleExpr>()) {
        return format_tuple(expr.as<parser::TupleExpr>());
    } else if (expr.is<parser::ArrayExpr>()) {
        return format_array(expr.as<parser::ArrayExpr>());
    } else if (expr.is<parser::ClosureExpr>()) {
        return format_closure(expr.as<parser::ClosureExpr>());
    } else if (expr.is<parser::RangeExpr>()) {
        return format_range(expr.as<parser::RangeExpr>());
    } else if (expr.is<parser::CastExpr>()) {
        return format_cast(expr.as<parser::CastExpr>());
    } else if (expr.is<parser::TryExpr>()) {
        return format_try(expr.as<parser::TryExpr>());
    } else if (expr.is<parser::AwaitExpr>()) {
        return format_await(expr.as<parser::AwaitExpr>());
    } else if (expr.is<parser::PathExpr>()) {
        return format_path(expr.as<parser::PathExpr>());
    }

    return "/* unknown expr */";
}

auto Formatter::format_literal(const parser::LiteralExpr& lit) -> std::string {
    return std::string(lit.token.lexeme);
}

auto Formatter::format_ident(const parser::IdentExpr& ident) -> std::string {
    return ident.name;
}

auto Formatter::binary_op_str(parser::BinaryOp op) -> std::string {
    switch (op) {
        case parser::BinaryOp::Add: return "+";
        case parser::BinaryOp::Sub: return "-";
        case parser::BinaryOp::Mul: return "*";
        case parser::BinaryOp::Div: return "/";
        case parser::BinaryOp::Mod: return "%";
        case parser::BinaryOp::Eq: return "==";
        case parser::BinaryOp::Ne: return "!=";
        case parser::BinaryOp::Lt: return "<";
        case parser::BinaryOp::Gt: return ">";
        case parser::BinaryOp::Le: return "<=";
        case parser::BinaryOp::Ge: return ">=";
        case parser::BinaryOp::And: return "and";
        case parser::BinaryOp::Or: return "or";
        case parser::BinaryOp::BitAnd: return "&";
        case parser::BinaryOp::BitOr: return "|";
        case parser::BinaryOp::BitXor: return "^";
        case parser::BinaryOp::Shl: return "<<";
        case parser::BinaryOp::Shr: return ">>";
        case parser::BinaryOp::Assign: return "=";
        case parser::BinaryOp::AddAssign: return "+=";
        case parser::BinaryOp::SubAssign: return "-=";
        case parser::BinaryOp::MulAssign: return "*=";
        case parser::BinaryOp::DivAssign: return "/=";
        case parser::BinaryOp::ModAssign: return "%=";
        case parser::BinaryOp::BitAndAssign: return "&=";
        case parser::BinaryOp::BitOrAssign: return "|=";
        case parser::BinaryOp::BitXorAssign: return "^=";
        case parser::BinaryOp::ShlAssign: return "<<=";
        case parser::BinaryOp::ShrAssign: return ">>=";
        default: return "?";
    }
}

auto Formatter::format_binary(const parser::BinaryExpr& bin) -> std::string {
    std::string left = format_expr(*bin.left);
    std::string right = format_expr(*bin.right);
    std::string op = binary_op_str(bin.op);

    // Add spaces around operators
    return left + " " + op + " " + right;
}

auto Formatter::unary_op_str(parser::UnaryOp op) -> std::string {
    switch (op) {
        case parser::UnaryOp::Neg: return "-";
        case parser::UnaryOp::Not: return "not ";
        case parser::UnaryOp::BitNot: return "~";
        case parser::UnaryOp::Ref: return "ref ";
        case parser::UnaryOp::RefMut: return "mut ref ";
        case parser::UnaryOp::Deref: return "*";
        default: return "";
    }
}

auto Formatter::format_unary(const parser::UnaryExpr& unary) -> std::string {
    return unary_op_str(unary.op) + format_expr(*unary.operand);
}

auto Formatter::format_call(const parser::CallExpr& call) -> std::string {
    std::stringstream ss;
    ss << format_expr(*call.callee) << "(";
    for (size_t i = 0; i < call.args.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << format_expr(*call.args[i]);
    }
    ss << ")";
    return ss.str();
}

auto Formatter::format_method_call(const parser::MethodCallExpr& call) -> std::string {
    std::stringstream ss;
    ss << format_expr(*call.receiver) << "." << call.method << "(";
    for (size_t i = 0; i < call.args.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << format_expr(*call.args[i]);
    }
    ss << ")";
    return ss.str();
}

auto Formatter::format_field(const parser::FieldExpr& field) -> std::string {
    return format_expr(*field.object) + "." + field.field;
}

auto Formatter::format_index(const parser::IndexExpr& index) -> std::string {
    return format_expr(*index.object) + "[" + format_expr(*index.index) + "]";
}

auto Formatter::format_if(const parser::IfExpr& if_expr) -> std::string {
    std::stringstream ss;
    ss << "if " << format_expr(*if_expr.condition) << " ";
    ss << format_block(if_expr.then_branch->as<parser::BlockExpr>());

    if (if_expr.else_branch.has_value()) {
        ss << " else ";
        const auto& else_branch = if_expr.else_branch.value();
        if (else_branch->is<parser::IfExpr>()) {
            ss << format_if(else_branch->as<parser::IfExpr>());
        } else if (else_branch->is<parser::BlockExpr>()) {
            ss << format_block(else_branch->as<parser::BlockExpr>());
        } else {
            ss << "{ " << format_expr(*else_branch) << " }";
        }
    }

    return ss.str();
}

auto Formatter::format_block(const parser::BlockExpr& block, bool inline_single) -> std::string {
    // For simple single-expression blocks, format inline
    if (inline_single && block.stmts.empty() && block.expr.has_value()) {
        return "{ " + format_expr(*block.expr.value()) + " }";
    }

    std::stringstream ss;
    ss << "{\n";

    // We need to track indentation manually here
    std::string indent_add = options_.use_tabs ? "\t" : std::string(options_.indent_width, ' ');
    std::string base_indent = indent_str();
    std::string inner_indent = base_indent + indent_add;

    for (const auto& stmt : block.stmts) {
        ss << inner_indent;
        if (stmt->is<parser::LetStmt>()) {
            const auto& let = stmt->as<parser::LetStmt>();
            ss << "let " << format_pattern(*let.pattern);
            if (let.type_annotation.has_value()) {
                ss << ": " << format_type_ptr(let.type_annotation.value());
            }
            if (let.init.has_value()) {
                ss << " = " << format_expr(*let.init.value());
            }
            ss << "\n";
        } else if (stmt->is<parser::VarStmt>()) {
            const auto& var = stmt->as<parser::VarStmt>();
            ss << "var " << var.name;
            if (var.type_annotation.has_value()) {
                ss << ": " << format_type_ptr(var.type_annotation.value());
            }
            ss << " = " << format_expr(*var.init) << "\n";
        } else if (stmt->is<parser::ExprStmt>()) {
            ss << format_expr(*stmt->as<parser::ExprStmt>().expr) << "\n";
        } else if (stmt->is<parser::DeclPtr>()) {
            // For declarations inside blocks, we need to handle them specially
            // This is a simplified inline format - complex declarations should use format_decl
            ss << "/* nested decl */\n";
        }
    }

    if (block.expr.has_value()) {
        ss << inner_indent << format_expr(*block.expr.value()) << "\n";
    }

    ss << base_indent << "}";
    return ss.str();
}

auto Formatter::format_loop(const parser::LoopExpr& loop) -> std::string {
    std::stringstream ss;
    if (loop.label.has_value()) {
        ss << "'" << loop.label.value() << ": ";
    }
    ss << "loop " << format_expr(*loop.body);
    return ss.str();
}

auto Formatter::format_while(const parser::WhileExpr& while_expr) -> std::string {
    std::stringstream ss;
    if (while_expr.label.has_value()) {
        ss << "'" << while_expr.label.value() << ": ";
    }
    ss << "loop " << format_expr(*while_expr.condition) << " ";
    ss << format_expr(*while_expr.body);
    return ss.str();
}

auto Formatter::format_for(const parser::ForExpr& for_expr) -> std::string {
    std::stringstream ss;
    if (for_expr.label.has_value()) {
        ss << "'" << for_expr.label.value() << ": ";
    }
    ss << "for " << format_pattern(*for_expr.pattern);
    ss << " in " << format_expr(*for_expr.iter) << " ";
    ss << format_expr(*for_expr.body);
    return ss.str();
}

auto Formatter::format_when(const parser::WhenExpr& when) -> std::string {
    std::stringstream ss;
    ss << "when " << format_expr(*when.scrutinee) << " {\n";

    std::string indent_add = options_.use_tabs ? "\t" : std::string(options_.indent_width, ' ');
    std::string inner_indent = indent_str() + indent_add;

    for (const auto& arm : when.arms) {
        ss << inner_indent << format_pattern(*arm.pattern);
        if (arm.guard.has_value()) {
            ss << " if " << format_expr(*arm.guard.value());
        }
        ss << " => " << format_expr(*arm.body);
        if (options_.trailing_commas) ss << ",";
        ss << "\n";
    }

    ss << indent_str() << "}";
    return ss.str();
}

auto Formatter::format_return(const parser::ReturnExpr& ret) -> std::string {
    if (ret.value.has_value()) {
        return "return " + format_expr(*ret.value.value());
    }
    return "return";
}

auto Formatter::format_break(const parser::BreakExpr& brk) -> std::string {
    std::stringstream ss;
    ss << "break";
    if (brk.label.has_value()) {
        ss << " '" << brk.label.value();
    }
    if (brk.value.has_value()) {
        ss << " " << format_expr(*brk.value.value());
    }
    return ss.str();
}

auto Formatter::format_continue(const parser::ContinueExpr& cont) -> std::string {
    if (cont.label.has_value()) {
        return "continue '" + cont.label.value();
    }
    return "continue";
}

auto Formatter::format_struct_expr(const parser::StructExpr& s) -> std::string {
    std::stringstream ss;
    ss << format_type_path(s.path) << " { ";

    for (size_t i = 0; i < s.fields.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << s.fields[i].first << ": " << format_expr(*s.fields[i].second);
    }

    if (s.base.has_value()) {
        if (!s.fields.empty()) ss << ", ";
        ss << ".." << format_expr(*s.base.value());
    }

    ss << " }";
    return ss.str();
}

auto Formatter::format_tuple(const parser::TupleExpr& tuple) -> std::string {
    std::stringstream ss;
    ss << "(";
    for (size_t i = 0; i < tuple.elements.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << format_expr(*tuple.elements[i]);
    }
    if (tuple.elements.size() == 1) ss << ",";  // Single-element tuple needs trailing comma
    ss << ")";
    return ss.str();
}

auto Formatter::format_array(const parser::ArrayExpr& arr) -> std::string {
    std::stringstream ss;
    ss << "[";

    if (std::holds_alternative<std::vector<parser::ExprPtr>>(arr.kind)) {
        const auto& elements = std::get<std::vector<parser::ExprPtr>>(arr.kind);
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << format_expr(*elements[i]);
        }
    } else {
        const auto& [elem, count] = std::get<std::pair<parser::ExprPtr, parser::ExprPtr>>(arr.kind);
        ss << format_expr(*elem) << "; " << format_expr(*count);
    }

    ss << "]";
    return ss.str();
}

auto Formatter::format_closure(const parser::ClosureExpr& closure) -> std::string {
    std::stringstream ss;

    if (closure.is_move) {
        ss << "move ";
    }

    ss << "do(";

    for (size_t i = 0; i < closure.params.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << format_pattern(*closure.params[i].first);
        if (closure.params[i].second.has_value()) {
            ss << ": " << format_type_ptr(closure.params[i].second.value());
        }
    }

    ss << ")";

    if (closure.return_type.has_value()) {
        ss << " -> " << format_type_ptr(closure.return_type.value());
    }

    ss << " " << format_expr(*closure.body);
    return ss.str();
}

auto Formatter::format_range(const parser::RangeExpr& range) -> std::string {
    std::stringstream ss;
    if (range.start.has_value()) {
        ss << format_expr(*range.start.value());
    }
    ss << (range.inclusive ? " through " : " to ");
    if (range.end.has_value()) {
        ss << format_expr(*range.end.value());
    }
    return ss.str();
}

auto Formatter::format_cast(const parser::CastExpr& cast) -> std::string {
    return format_expr(*cast.expr) + " as " + format_type_ptr(cast.target);
}

auto Formatter::format_try(const parser::TryExpr& try_expr) -> std::string {
    return format_expr(*try_expr.expr) + "!";
}

auto Formatter::format_await(const parser::AwaitExpr& await) -> std::string {
    return format_expr(*await.expr) + ".await";
}

auto Formatter::format_path(const parser::PathExpr& path) -> std::string {
    return format_type_path(path.path);
}

auto Formatter::format_type(const parser::Type& type) -> std::string {
    if (type.is<parser::NamedType>()) {
        const auto& named = type.as<parser::NamedType>();
        std::string result = format_type_path(named.path);
        if (named.generics.has_value()) {
            result += "[";
            for (size_t i = 0; i < named.generics->args.size(); ++i) {
                if (i > 0) result += ", ";
                result += format_type_ptr(named.generics->args[i]);
            }
            result += "]";
        }
        return result;
    } else if (type.is<parser::RefType>()) {
        const auto& ref = type.as<parser::RefType>();
        return (ref.is_mut ? "mut ref " : "ref ") + format_type_ptr(ref.inner);
    } else if (type.is<parser::PtrType>()) {
        const auto& ptr = type.as<parser::PtrType>();
        return (ptr.is_mut ? "mut ptr " : "ptr ") + format_type_ptr(ptr.inner);
    } else if (type.is<parser::ArrayType>()) {
        const auto& arr = type.as<parser::ArrayType>();
        return "[" + format_type_ptr(arr.element) + "; " + format_expr(*arr.size) + "]";
    } else if (type.is<parser::SliceType>()) {
        const auto& slice = type.as<parser::SliceType>();
        return "[" + format_type_ptr(slice.element) + "]";
    } else if (type.is<parser::TupleType>()) {
        const auto& tuple = type.as<parser::TupleType>();
        std::string result = "(";
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0) result += ", ";
            result += format_type_ptr(tuple.elements[i]);
        }
        result += ")";
        return result;
    } else if (type.is<parser::FuncType>()) {
        const auto& func = type.as<parser::FuncType>();
        std::string result = "func(";
        for (size_t i = 0; i < func.params.size(); ++i) {
            if (i > 0) result += ", ";
            result += format_type_ptr(func.params[i]);
        }
        result += ")";
        if (func.return_type) {
            result += " -> " + format_type_ptr(func.return_type);
        }
        return result;
    } else if (type.is<parser::InferType>()) {
        return "_";
    }

    return "?";
}

auto Formatter::format_type_ptr(const parser::TypePtr& type) -> std::string {
    if (!type) return "_";
    return format_type(*type);
}

auto Formatter::format_type_path(const parser::TypePath& path) -> std::string {
    std::string result;
    for (size_t i = 0; i < path.segments.size(); ++i) {
        if (i > 0) result += "::";
        result += path.segments[i];
    }
    return result;
}

auto Formatter::format_pattern(const parser::Pattern& pattern) -> std::string {
    if (pattern.is<parser::WildcardPattern>()) {
        return "_";
    } else if (pattern.is<parser::IdentPattern>()) {
        const auto& ident = pattern.as<parser::IdentPattern>();
        std::string result;
        if (ident.is_mut) result = "mut ";
        result += ident.name;
        if (ident.type_annotation.has_value()) {
            result += ": " + format_type_ptr(ident.type_annotation.value());
        }
        return result;
    } else if (pattern.is<parser::LiteralPattern>()) {
        return std::string(pattern.as<parser::LiteralPattern>().literal.lexeme);
    } else if (pattern.is<parser::TuplePattern>()) {
        const auto& tuple = pattern.as<parser::TuplePattern>();
        std::string result = "(";
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0) result += ", ";
            result += format_pattern(*tuple.elements[i]);
        }
        result += ")";
        return result;
    } else if (pattern.is<parser::StructPattern>()) {
        const auto& s = pattern.as<parser::StructPattern>();
        std::string result = format_type_path(s.path) + " { ";
        for (size_t i = 0; i < s.fields.size(); ++i) {
            if (i > 0) result += ", ";
            result += s.fields[i].first + ": " + format_pattern(*s.fields[i].second);
        }
        if (s.has_rest) {
            if (!s.fields.empty()) result += ", ";
            result += "..";
        }
        result += " }";
        return result;
    } else if (pattern.is<parser::EnumPattern>()) {
        const auto& e = pattern.as<parser::EnumPattern>();
        std::string result = format_type_path(e.path);
        if (e.payload.has_value()) {
            result += "(";
            for (size_t i = 0; i < e.payload->size(); ++i) {
                if (i > 0) result += ", ";
                result += format_pattern(*(*e.payload)[i]);
            }
            result += ")";
        }
        return result;
    } else if (pattern.is<parser::OrPattern>()) {
        const auto& or_pat = pattern.as<parser::OrPattern>();
        std::string result;
        for (size_t i = 0; i < or_pat.patterns.size(); ++i) {
            if (i > 0) result += " | ";
            result += format_pattern(*or_pat.patterns[i]);
        }
        return result;
    } else if (pattern.is<parser::RangePattern>()) {
        const auto& range = pattern.as<parser::RangePattern>();
        std::string result;
        if (range.start.has_value()) {
            result += format_expr(*range.start.value());
        }
        result += range.inclusive ? " through " : " to ";
        if (range.end.has_value()) {
            result += format_expr(*range.end.value());
        }
        return result;
    }

    return "?";
}

} // namespace tml::format
