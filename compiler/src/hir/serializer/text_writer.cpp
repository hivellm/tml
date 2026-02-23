TML_MODULE("compiler")

//! # HIR Text Writer
//!
//! This file produces human-readable text output for HIR modules,
//! primarily for debugging and analysis purposes.
//!
//! ## Output Format
//!
//! The text format resembles TML source syntax with additional annotations:
//!
//! ```text
//! ; HIR Module: main
//! ; Source: src/main.tml
//! ; Hash: 12345678901234567890
//!
//! pub type Point {
//!     x: I32
//!     y: I32
//! }
//!
//! pub func add(a: I32, b: I32) -> I32 {
//!     return (a + b)
//! }
//! ```
//!
//! ## Features
//!
//! - **Struct/Enum Definitions** - Full field/variant information
//! - **Function Signatures** - Parameters, return types, attributes
//! - **Expression Trees** - Nested expressions with operators
//! - **Pattern Matching** - Let patterns and when arms
//!
//! ## Limitations
//!
//! The text format is **not designed for round-trip**:
//! - Simplified expression representation
//! - Some type details may be lost
//! - Use binary format for serialization/deserialization
//!
//! ## Usage
//!
//! ```cpp
//! std::ostringstream oss;
//! HirTextWriter writer(oss);
//! writer.write_module(module);
//! std::cout << oss.str();
//! ```
//!
//! ## See Also
//!
//! - `text_reader.cpp` - Corresponding (partial) reader
//! - `hir_printer.hpp` - Alternative HIR printing utilities

#include "hir/hir_serialize.hpp"
#include "types/type.hpp"

namespace tml::hir {

HirTextWriter::HirTextWriter(std::ostream& out, HirSerializeOptions options)
    : out_(out), options_(options) {}

void HirTextWriter::write_indent() {
    for (int i = 0; i < indent_; ++i) {
        out_ << "  ";
    }
}

void HirTextWriter::write_line(const std::string& line) {
    write_indent();
    out_ << line << "\n";
}

void HirTextWriter::write_type(const HirType& type) {
    if (type) {
        out_ << types::type_to_string(type);
    } else {
        out_ << "<unknown>";
    }
}

void HirTextWriter::write_module(const HirModule& module) {
    out_ << "; HIR Module: " << module.name << "\n";
    out_ << "; Source: " << module.source_path << "\n";
    out_ << "; Hash: " << compute_hir_hash(module) << "\n";
    out_ << "\n";

    // Write imports
    if (!module.imports.empty()) {
        out_ << "; Imports\n";
        for (const auto& imp : module.imports) {
            out_ << "use " << imp << "\n";
        }
        out_ << "\n";
    }

    // Write structs
    if (!module.structs.empty()) {
        out_ << "; Structs\n";
        for (const auto& s : module.structs) {
            write_struct(s);
            out_ << "\n";
        }
    }

    // Write enums
    if (!module.enums.empty()) {
        out_ << "; Enums\n";
        for (const auto& e : module.enums) {
            write_enum(e);
            out_ << "\n";
        }
    }

    // Write behaviors
    if (!module.behaviors.empty()) {
        out_ << "; Behaviors\n";
        for (const auto& b : module.behaviors) {
            write_behavior(b);
            out_ << "\n";
        }
    }

    // Write impls
    if (!module.impls.empty()) {
        out_ << "; Implementations\n";
        for (const auto& impl : module.impls) {
            write_impl(impl);
            out_ << "\n";
        }
    }

    // Write constants
    if (!module.constants.empty()) {
        out_ << "; Constants\n";
        for (const auto& c : module.constants) {
            write_const(c);
            out_ << "\n";
        }
    }

    // Write functions
    if (!module.functions.empty()) {
        out_ << "; Functions\n";
        for (const auto& f : module.functions) {
            write_function(f);
            out_ << "\n";
        }
    }
}

void HirTextWriter::write_struct(const HirStruct& s) {
    out_ << (s.is_public ? "pub " : "") << "type " << s.name;
    if (s.mangled_name != s.name) {
        out_ << " [" << s.mangled_name << "]";
    }
    out_ << " {\n";

    ++indent_;
    for (const auto& f : s.fields) {
        write_indent();
        out_ << (f.is_public ? "pub " : "") << f.name << ": ";
        write_type(f.type);
        out_ << "\n";
    }
    --indent_;

    out_ << "}\n";
}

void HirTextWriter::write_enum(const HirEnum& e) {
    out_ << (e.is_public ? "pub " : "") << "type " << e.name;
    if (e.mangled_name != e.name) {
        out_ << " [" << e.mangled_name << "]";
    }
    out_ << " {\n";

    ++indent_;
    for (const auto& v : e.variants) {
        write_indent();
        out_ << v.name;
        if (!v.payload_types.empty()) {
            out_ << "(";
            for (size_t i = 0; i < v.payload_types.size(); ++i) {
                if (i > 0)
                    out_ << ", ";
                write_type(v.payload_types[i]);
            }
            out_ << ")";
        }
        out_ << " = " << v.index << "\n";
    }
    --indent_;

    out_ << "}\n";
}

void HirTextWriter::write_behavior(const HirBehavior& b) {
    out_ << (b.is_public ? "pub " : "") << "behavior " << b.name;

    if (!b.super_behaviors.empty()) {
        out_ << ": ";
        for (size_t i = 0; i < b.super_behaviors.size(); ++i) {
            if (i > 0)
                out_ << " + ";
            out_ << b.super_behaviors[i];
        }
    }

    out_ << " {\n";

    ++indent_;
    for (const auto& m : b.methods) {
        write_indent();
        out_ << "func " << m.name << "(";
        for (size_t i = 0; i < m.params.size(); ++i) {
            if (i > 0)
                out_ << ", ";
            out_ << (m.params[i].is_mut ? "mut " : "") << m.params[i].name << ": ";
            write_type(m.params[i].type);
        }
        out_ << ") -> ";
        write_type(m.return_type);
        if (m.has_default_impl) {
            out_ << " { ... }";
        }
        out_ << "\n";
    }
    --indent_;

    out_ << "}\n";
}

void HirTextWriter::write_impl(const HirImpl& impl) {
    out_ << "impl ";
    if (impl.behavior_name) {
        out_ << *impl.behavior_name << " for ";
    }
    out_ << impl.type_name << " {\n";

    ++indent_;
    for (const auto& m : impl.methods) {
        write_function(m);
    }
    --indent_;

    out_ << "}\n";
}

void HirTextWriter::write_const(const HirConst& c) {
    out_ << (c.is_public ? "pub " : "") << "const " << c.name << ": ";
    write_type(c.type);
    out_ << " = ";
    if (c.value) {
        write_expr(*c.value);
    } else {
        out_ << "<no value>";
    }
    out_ << "\n";
}

void HirTextWriter::write_function(const HirFunction& func) {
    write_indent();

    if (func.is_public)
        out_ << "pub ";
    if (func.is_async)
        out_ << "async ";
    if (func.is_extern)
        out_ << "extern ";

    out_ << "func " << func.name;
    if (func.mangled_name != func.name) {
        out_ << " [" << func.mangled_name << "]";
    }

    out_ << "(";
    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0)
            out_ << ", ";
        out_ << (func.params[i].is_mut ? "mut " : "") << func.params[i].name << ": ";
        write_type(func.params[i].type);
    }
    out_ << ") -> ";
    write_type(func.return_type);

    if (func.body) {
        out_ << " {\n";
        ++indent_;
        write_expr(**func.body);
        out_ << "\n";
        --indent_;
        write_indent();
        out_ << "}\n";
    } else {
        out_ << "\n";
    }
}

void HirTextWriter::write_expr(const HirExpr& expr) {
    std::visit(
        [this](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirLiteralExpr>) {
                std::visit(
                    [this](const auto& val) {
                        using V = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<V, int64_t>) {
                            out_ << val;
                        } else if constexpr (std::is_same_v<V, uint64_t>) {
                            out_ << val << "u";
                        } else if constexpr (std::is_same_v<V, double>) {
                            out_ << val;
                        } else if constexpr (std::is_same_v<V, bool>) {
                            out_ << (val ? "true" : "false");
                        } else if constexpr (std::is_same_v<V, char>) {
                            out_ << "'" << val << "'";
                        } else if constexpr (std::is_same_v<V, std::string>) {
                            out_ << "\"" << val << "\"";
                        }
                    },
                    e.value);
            } else if constexpr (std::is_same_v<T, HirVarExpr>) {
                out_ << e.name;
            } else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                out_ << "(";
                write_expr(*e.left);
                out_ << " ";
                switch (e.op) {
                case HirBinOp::Add:
                    out_ << "+";
                    break;
                case HirBinOp::Sub:
                    out_ << "-";
                    break;
                case HirBinOp::Mul:
                    out_ << "*";
                    break;
                case HirBinOp::Div:
                    out_ << "/";
                    break;
                case HirBinOp::Mod:
                    out_ << "%";
                    break;
                case HirBinOp::Eq:
                    out_ << "==";
                    break;
                case HirBinOp::Ne:
                    out_ << "!=";
                    break;
                case HirBinOp::Lt:
                    out_ << "<";
                    break;
                case HirBinOp::Le:
                    out_ << "<=";
                    break;
                case HirBinOp::Gt:
                    out_ << ">";
                    break;
                case HirBinOp::Ge:
                    out_ << ">=";
                    break;
                case HirBinOp::And:
                    out_ << "and";
                    break;
                case HirBinOp::Or:
                    out_ << "or";
                    break;
                case HirBinOp::BitAnd:
                    out_ << "&";
                    break;
                case HirBinOp::BitOr:
                    out_ << "|";
                    break;
                case HirBinOp::BitXor:
                    out_ << "^";
                    break;
                case HirBinOp::Shl:
                    out_ << "<<";
                    break;
                case HirBinOp::Shr:
                    out_ << ">>";
                    break;
                }
                out_ << " ";
                write_expr(*e.right);
                out_ << ")";
            } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                switch (e.op) {
                case HirUnaryOp::Neg:
                    out_ << "-";
                    break;
                case HirUnaryOp::Not:
                    out_ << "not ";
                    break;
                case HirUnaryOp::BitNot:
                    out_ << "~";
                    break;
                case HirUnaryOp::Ref:
                    out_ << "ref ";
                    break;
                case HirUnaryOp::RefMut:
                    out_ << "mut ref ";
                    break;
                case HirUnaryOp::Deref:
                    out_ << "*";
                    break;
                }
                write_expr(*e.operand);
            } else if constexpr (std::is_same_v<T, HirCallExpr>) {
                out_ << e.func_name << "(";
                for (size_t i = 0; i < e.args.size(); ++i) {
                    if (i > 0)
                        out_ << ", ";
                    write_expr(*e.args[i]);
                }
                out_ << ")";
            } else if constexpr (std::is_same_v<T, HirMethodCallExpr>) {
                write_expr(*e.receiver);
                out_ << "." << e.method_name << "(";
                for (size_t i = 0; i < e.args.size(); ++i) {
                    if (i > 0)
                        out_ << ", ";
                    write_expr(*e.args[i]);
                }
                out_ << ")";
            } else if constexpr (std::is_same_v<T, HirFieldExpr>) {
                write_expr(*e.object);
                out_ << "." << e.field_name;
            } else if constexpr (std::is_same_v<T, HirIndexExpr>) {
                write_expr(*e.object);
                out_ << "[";
                write_expr(*e.index);
                out_ << "]";
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                out_ << "{\n";
                ++indent_;
                for (const auto& s : e.stmts) {
                    write_stmt(*s);
                    out_ << "\n";
                }
                if (e.expr) {
                    write_indent();
                    write_expr(**e.expr);
                    out_ << "\n";
                }
                --indent_;
                write_indent();
                out_ << "}";
            } else if constexpr (std::is_same_v<T, HirIfExpr>) {
                out_ << "if ";
                write_expr(*e.condition);
                out_ << " { ... }";
                if (e.else_branch) {
                    out_ << " else { ... }";
                }
            } else if constexpr (std::is_same_v<T, HirReturnExpr>) {
                out_ << "return";
                if (e.value) {
                    out_ << " ";
                    write_expr(**e.value);
                }
            } else {
                // For other expression types, just show the type
                out_ << "<expr>";
            }
        },
        expr.kind);
}

void HirTextWriter::write_pattern(const HirPattern& pattern) {
    std::visit(
        [this](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, HirWildcardPattern>) {
                out_ << "_";
            } else if constexpr (std::is_same_v<T, HirBindingPattern>) {
                if (p.is_mut)
                    out_ << "mut ";
                out_ << p.name;
            } else if constexpr (std::is_same_v<T, HirLiteralPattern>) {
                std::visit(
                    [this](const auto& val) {
                        using V = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<V, int64_t>) {
                            out_ << val;
                        } else if constexpr (std::is_same_v<V, uint64_t>) {
                            out_ << val << "u";
                        } else if constexpr (std::is_same_v<V, double>) {
                            out_ << val;
                        } else if constexpr (std::is_same_v<V, bool>) {
                            out_ << (val ? "true" : "false");
                        } else if constexpr (std::is_same_v<V, char>) {
                            out_ << "'" << val << "'";
                        } else if constexpr (std::is_same_v<V, std::string>) {
                            out_ << "\"" << val << "\"";
                        }
                    },
                    p.value);
            } else if constexpr (std::is_same_v<T, HirTuplePattern>) {
                out_ << "(";
                for (size_t i = 0; i < p.elements.size(); ++i) {
                    if (i > 0)
                        out_ << ", ";
                    write_pattern(*p.elements[i]);
                }
                out_ << ")";
            } else {
                out_ << "<pattern>";
            }
        },
        pattern.kind);
}

void HirTextWriter::write_stmt(const HirStmt& stmt) {
    std::visit(
        [this](const auto& s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, HirLetStmt>) {
                write_indent();
                out_ << "let ";
                write_pattern(*s.pattern);
                out_ << ": ";
                write_type(s.type);
                if (s.init) {
                    out_ << " = ";
                    write_expr(**s.init);
                }
            } else if constexpr (std::is_same_v<T, HirExprStmt>) {
                write_indent();
                write_expr(*s.expr);
            }
        },
        stmt.kind);
}

} // namespace tml::hir
