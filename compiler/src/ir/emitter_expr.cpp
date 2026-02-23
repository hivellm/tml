TML_MODULE("compiler")

//! # IR Emitter - Expressions
//!
//! This file emits IR expressions in S-expression format.
//!
//! ## Output Format
//!
//! | Expression  | S-expression                           |
//! |-------------|----------------------------------------|
//! | Literal     | `(lit 42 I32)`                         |
//! | Variable    | `(var x)`                              |
//! | Binary op   | `(+ (var x) (lit 1 I32))`              |
//! | Call        | `(call foo (var a) (var b))`           |
//! | Field       | `(field (var p) x)`                    |
//! | If          | `(if (cond) (then) (else))`            |
//! | Block       | `(block (stmt1) (stmt2) (expr))`       |

#include "ir/ir.hpp"

#include <sstream>

namespace tml::ir {

void IREmitter::emit_expr(std::ostringstream& out, const IRExpr& expr) {
    std::visit(
        [this, &out](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, IRLiteral>) {
                out << "(lit " << e.value << " " << e.type_name << ")";
            } else if constexpr (std::is_same_v<T, IRVar>) {
                out << "(var " << e.name << ")";
            } else if constexpr (std::is_same_v<T, IRBinaryOp>) {
                out << "(" << e.op << " ";
                emit_expr(out, *e.left);
                out << " ";
                emit_expr(out, *e.right);
                out << ")";
            } else if constexpr (std::is_same_v<T, IRUnaryOp>) {
                out << "(" << e.op << " ";
                emit_expr(out, *e.operand);
                out << ")";
            } else if constexpr (std::is_same_v<T, IRCall>) {
                out << "(call " << e.func_name;
                for (const auto& arg : e.args) {
                    out << " ";
                    emit_expr(out, *arg);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, IRMethodCall>) {
                out << "(method-call ";
                emit_expr(out, *e.receiver);
                out << " " << e.method_name;
                for (const auto& arg : e.args) {
                    out << " ";
                    emit_expr(out, *arg);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, IRFieldGet>) {
                out << "(field-get ";
                emit_expr(out, *e.object);
                out << " " << e.field_name << ")";
            } else if constexpr (std::is_same_v<T, IRFieldSet>) {
                out << "(field-set ";
                emit_expr(out, *e.object);
                out << " " << e.field_name << " ";
                emit_expr(out, *e.value);
                out << ")";
            } else if constexpr (std::is_same_v<T, IRIndex>) {
                out << "(index ";
                emit_expr(out, *e.object);
                out << " ";
                emit_expr(out, *e.index);
                out << ")";
            } else if constexpr (std::is_same_v<T, IRStructExpr>) {
                out << "(struct " << e.type_name;
                for (const auto& [name, val] : e.fields) {
                    out << " (" << name << " ";
                    emit_expr(out, *val);
                    out << ")";
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, IRVariantExpr>) {
                out << "(variant " << e.variant_name;
                for (const auto& field : e.fields) {
                    out << " ";
                    emit_expr(out, *field);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, IRTupleExpr>) {
                out << "(tuple";
                for (const auto& elem : e.elements) {
                    out << " ";
                    emit_expr(out, *elem);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, IRArrayExpr>) {
                out << "(array";
                for (const auto& elem : e.elements) {
                    out << " ";
                    emit_expr(out, *elem);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, IRArrayRepeat>) {
                out << "(array-repeat ";
                emit_expr(out, *e.value);
                out << " ";
                emit_expr(out, *e.count);
                out << ")";
            } else if constexpr (std::is_same_v<T, IRIf>) {
                out << "(if ";
                emit_expr(out, *e.condition);
                emit_newline(out);
                indent_level_++;
                emit_indent(out);
                out << "(then ";
                emit_expr(out, *e.then_branch);
                out << ")";
                if (e.else_branch) {
                    emit_newline(out);
                    emit_indent(out);
                    out << "(else ";
                    emit_expr(out, **e.else_branch);
                    out << ")";
                }
                indent_level_--;
                out << ")";
            } else if constexpr (std::is_same_v<T, IRWhen>) {
                out << "(when ";
                emit_expr(out, *e.scrutinee);
                indent_level_++;
                for (const auto& arm : e.arms) {
                    emit_newline(out);
                    emit_indent(out);
                    out << "(arm ";
                    emit_pattern(out, *arm.pattern);
                    if (arm.guard) {
                        out << " :when ";
                        emit_expr(out, **arm.guard);
                    }
                    out << " ";
                    emit_expr(out, *arm.body);
                    out << ")";
                }
                indent_level_--;
                out << ")";
            } else if constexpr (std::is_same_v<T, IRLoop>) {
                out << "(loop ";
                emit_expr(out, *e.body);
                out << ")";
            } else if constexpr (std::is_same_v<T, IRLoopIn>) {
                out << "(loop-in " << e.binding << " ";
                emit_expr(out, *e.iter);
                emit_newline(out);
                indent_level_++;
                emit_indent(out);
                out << "(body ";
                emit_expr(out, *e.body);
                out << ")";
                indent_level_--;
                out << ")";
            } else if constexpr (std::is_same_v<T, IRLoopWhile>) {
                out << "(loop-while ";
                emit_expr(out, *e.condition);
                emit_newline(out);
                indent_level_++;
                emit_indent(out);
                out << "(body ";
                emit_expr(out, *e.body);
                out << ")";
                indent_level_--;
                out << ")";
            } else if constexpr (std::is_same_v<T, IRBlock>) {
                emit_block(out, e);
            } else if constexpr (std::is_same_v<T, IRClosure>) {
                out << "(closure";
                indent_level_++;
                emit_newline(out);
                emit_indent(out);
                out << "(params";
                for (const auto& [name, type] : e.params) {
                    out << " (param " << name;
                    if (type) {
                        out << " ";
                        emit_type_expr(out, *type);
                    }
                    out << ")";
                }
                out << ")";
                emit_newline(out);
                emit_indent(out);
                out << "(body ";
                emit_expr(out, *e.body);
                out << ")";
                indent_level_--;
                out << ")";
            } else if constexpr (std::is_same_v<T, IRTry>) {
                out << "(try ";
                emit_expr(out, *e.expr);
                out << ")";
            } else if constexpr (std::is_same_v<T, IRReturn>) {
                if (e.value) {
                    out << "(return ";
                    emit_expr(out, **e.value);
                    out << ")";
                } else {
                    out << "(return)";
                }
            } else if constexpr (std::is_same_v<T, IRBreak>) {
                if (e.value) {
                    out << "(break ";
                    emit_expr(out, **e.value);
                    out << ")";
                } else {
                    out << "(break)";
                }
            } else if constexpr (std::is_same_v<T, IRContinue>) {
                out << "(continue)";
            } else if constexpr (std::is_same_v<T, IRRange>) {
                out << "(range ";
                emit_expr(out, *e.start);
                out << " ";
                emit_expr(out, *e.end);
                out << " " << (e.inclusive ? "inclusive" : "exclusive") << ")";
            }
        },
        expr.kind);
}

} // namespace tml::ir
