//! # IR Emitter - Statements
//!
//! This file emits IR statements in S-expression format.
//!
//! ## Output Format
//!
//! | Statement   | S-expression                           |
//! |-------------|----------------------------------------|
//! | Let         | `(let (bind x) (type I32) (init ...))`|
//! | Var         | `(var x (type I32) (init ...))`        |
//! | Assign      | `(assign (var x) (lit 42 I32))`        |
//! | Expr stmt   | `(expr (call foo))`                    |
//!
//! ## Patterns
//!
//! | Pattern     | S-expression                           |
//! |-------------|----------------------------------------|
//! | Wildcard    | `_`                                    |
//! | Binding     | `(bind x)` or `(bind mut x)`           |
//! | Tuple       | `(tuple (bind a) (bind b))`            |

#include "ir/ir.hpp"

#include <sstream>

namespace tml::ir {

void IREmitter::emit_stmt(std::ostringstream& out, const IRStmt& stmt) {
    std::visit(
        [this, &out](const auto& s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, IRLet>) {
                out << "(let ";
                emit_pattern(out, *s.pattern);
                if (s.type_annotation) {
                    out << " ";
                    emit_type_expr(out, *s.type_annotation);
                }
                out << " ";
                emit_expr(out, *s.init);
                out << ")";
            } else if constexpr (std::is_same_v<T, IRVarMut>) {
                out << "(var-mut " << s.name;
                if (s.type_annotation) {
                    out << " ";
                    emit_type_expr(out, *s.type_annotation);
                }
                out << " ";
                emit_expr(out, *s.init);
                out << ")";
            } else if constexpr (std::is_same_v<T, IRAssign>) {
                out << "(assign ";
                emit_expr(out, *s.target);
                out << " ";
                emit_expr(out, *s.value);
                out << ")";
            } else if constexpr (std::is_same_v<T, IRExprStmt>) {
                emit_expr(out, *s.expr);
            }
        },
        stmt.kind);
}

void IREmitter::emit_pattern(std::ostringstream& out, const IRPattern& pattern) {
    std::visit(
        [this, &out](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, IRPatternLit>) {
                out << "(pattern-lit " << p.value << ")";
            } else if constexpr (std::is_same_v<T, IRPatternBind>) {
                out << "(pattern-bind " << p.name;
                if (p.is_mut)
                    out << " :mut";
                out << ")";
            } else if constexpr (std::is_same_v<T, IRPatternWild>) {
                out << "(pattern-wild)";
            } else if constexpr (std::is_same_v<T, IRPatternTuple>) {
                out << "(pattern-tuple";
                for (const auto& elem : p.elements) {
                    out << " ";
                    emit_pattern(out, *elem);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, IRPatternStruct>) {
                out << "(pattern-struct " << p.type_name;
                for (const auto& [name, pat] : p.fields) {
                    out << " (" << name << " ";
                    emit_pattern(out, *pat);
                    out << ")";
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, IRPatternVariant>) {
                out << "(pattern-variant " << p.variant_name;
                for (const auto& field : p.fields) {
                    out << " ";
                    emit_pattern(out, *field);
                }
                out << ")";
            }
        },
        pattern.kind);
}

void IREmitter::emit_type_expr(std::ostringstream& out, const IRTypeExpr& type) {
    std::visit(
        [&out](const auto& t) {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, IRTypeRef>) {
                out << t.name;
                if (!t.type_args.empty()) {
                    out << "[";
                    for (size_t i = 0; i < t.type_args.size(); ++i) {
                        if (i > 0)
                            out << ", ";
                        out << t.type_args[i]->name;
                    }
                    out << "]";
                }
            } else if constexpr (std::is_same_v<T, IRRefType>) {
                out << (t.is_mut ? "(mut-ref " : "(ref ");
                out << t.inner->name << ")";
            } else if constexpr (std::is_same_v<T, IRSliceType>) {
                out << "(slice " << t.element->name << ")";
            } else if constexpr (std::is_same_v<T, IRArrayType>) {
                out << "(array " << t.element->name << " " << t.size << ")";
            } else if constexpr (std::is_same_v<T, IRTupleType>) {
                out << "(tuple";
                for (const auto& elem : t.elements) {
                    out << " " << elem->name;
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, IRFuncType>) {
                out << "(func (";
                for (size_t i = 0; i < t.params.size(); ++i) {
                    if (i > 0)
                        out << " ";
                    out << t.params[i]->name;
                }
                out << ") -> " << t.ret->name << ")";
            }
        },
        type.kind);
}

void IREmitter::emit_block(std::ostringstream& out, const IRBlock& block) {
    out << "(block";
    indent_level_++;
    for (const auto& stmt : block.stmts) {
        emit_newline(out);
        emit_indent(out);
        emit_stmt(out, *stmt);
    }
    if (block.expr) {
        emit_newline(out);
        emit_indent(out);
        emit_expr(out, **block.expr);
    }
    indent_level_--;
    out << ")";
}

} // namespace tml::ir
