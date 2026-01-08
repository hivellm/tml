//! # Expression Formatting
//!
//! This file implements formatting for all expression types.
//!
//! ## Expression Types
//!
//! | Category     | Expressions                                    |
//! |--------------|------------------------------------------------|
//! | Literals     | Integer, float, string, char, bool             |
//! | Operators    | Binary (+, -, and, or), Unary (-, not, ref)    |
//! | Access       | Field (x.y), Index (x[i]), Method (x.foo())    |
//! | Control      | if, when, loop, for, return, break, continue   |
//! | Constructors | Struct { }, Tuple ( ), Array [ ]               |
//! | Other        | Closure (do), Range (to), Cast (as), Try (!)   |

#include "format/formatter.hpp"

namespace tml::format {

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
    case parser::BinaryOp::Add:
        return "+";
    case parser::BinaryOp::Sub:
        return "-";
    case parser::BinaryOp::Mul:
        return "*";
    case parser::BinaryOp::Div:
        return "/";
    case parser::BinaryOp::Mod:
        return "%";
    case parser::BinaryOp::Eq:
        return "==";
    case parser::BinaryOp::Ne:
        return "!=";
    case parser::BinaryOp::Lt:
        return "<";
    case parser::BinaryOp::Gt:
        return ">";
    case parser::BinaryOp::Le:
        return "<=";
    case parser::BinaryOp::Ge:
        return ">=";
    case parser::BinaryOp::And:
        return "and";
    case parser::BinaryOp::Or:
        return "or";
    case parser::BinaryOp::BitAnd:
        return "&";
    case parser::BinaryOp::BitOr:
        return "|";
    case parser::BinaryOp::BitXor:
        return "^";
    case parser::BinaryOp::Shl:
        return "<<";
    case parser::BinaryOp::Shr:
        return ">>";
    case parser::BinaryOp::Assign:
        return "=";
    case parser::BinaryOp::AddAssign:
        return "+=";
    case parser::BinaryOp::SubAssign:
        return "-=";
    case parser::BinaryOp::MulAssign:
        return "*=";
    case parser::BinaryOp::DivAssign:
        return "/=";
    case parser::BinaryOp::ModAssign:
        return "%=";
    case parser::BinaryOp::BitAndAssign:
        return "&=";
    case parser::BinaryOp::BitOrAssign:
        return "|=";
    case parser::BinaryOp::BitXorAssign:
        return "^=";
    case parser::BinaryOp::ShlAssign:
        return "<<=";
    case parser::BinaryOp::ShrAssign:
        return ">>=";
    default:
        return "?";
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
    case parser::UnaryOp::Neg:
        return "-";
    case parser::UnaryOp::Not:
        return "not ";
    case parser::UnaryOp::BitNot:
        return "~";
    case parser::UnaryOp::Ref:
        return "ref ";
    case parser::UnaryOp::RefMut:
        return "mut ref ";
    case parser::UnaryOp::Deref:
        return "*";
    default:
        return "";
    }
}

auto Formatter::format_unary(const parser::UnaryExpr& unary) -> std::string {
    return unary_op_str(unary.op) + format_expr(*unary.operand);
}

auto Formatter::format_call(const parser::CallExpr& call) -> std::string {
    std::stringstream ss;
    ss << format_expr(*call.callee) << "(";
    for (size_t i = 0; i < call.args.size(); ++i) {
        if (i > 0)
            ss << ", ";
        ss << format_expr(*call.args[i]);
    }
    ss << ")";
    return ss.str();
}

auto Formatter::format_method_call(const parser::MethodCallExpr& call) -> std::string {
    std::stringstream ss;
    ss << format_expr(*call.receiver) << "." << call.method << "(";
    for (size_t i = 0; i < call.args.size(); ++i) {
        if (i > 0)
            ss << ", ";
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
        if (options_.trailing_commas)
            ss << ",";
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
        if (i > 0)
            ss << ", ";
        ss << s.fields[i].first << ": " << format_expr(*s.fields[i].second);
    }

    if (s.base.has_value()) {
        if (!s.fields.empty())
            ss << ", ";
        ss << ".." << format_expr(*s.base.value());
    }

    ss << " }";
    return ss.str();
}

auto Formatter::format_tuple(const parser::TupleExpr& tuple) -> std::string {
    std::stringstream ss;
    ss << "(";
    for (size_t i = 0; i < tuple.elements.size(); ++i) {
        if (i > 0)
            ss << ", ";
        ss << format_expr(*tuple.elements[i]);
    }
    if (tuple.elements.size() == 1)
        ss << ","; // Single-element tuple needs trailing comma
    ss << ")";
    return ss.str();
}

auto Formatter::format_array(const parser::ArrayExpr& arr) -> std::string {
    std::stringstream ss;
    ss << "[";

    if (std::holds_alternative<std::vector<parser::ExprPtr>>(arr.kind)) {
        const auto& elements = std::get<std::vector<parser::ExprPtr>>(arr.kind);
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0)
                ss << ", ";
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
        if (i > 0)
            ss << ", ";
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

} // namespace tml::format
