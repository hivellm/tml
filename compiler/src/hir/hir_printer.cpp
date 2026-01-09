//! # HIR Pretty Printer Implementation
//!
//! This file implements the HIR pretty printer for debugging output.
//!
//! ## Overview
//!
//! The HirPrinter converts HIR structures back into human-readable text.
//! This is primarily used for:
//! - Debugging compiler internals (`--emit-hir` flag)
//! - Test output verification
//! - Error message generation with context
//!
//! ## Output Format
//!
//! The printer produces TML-like syntax with added annotations:
//! - Mangled names shown as `/* comments */`
//! - Type annotations on all bindings
//! - Full parenthesization of expressions
//!
//! ## Color Support
//!
//! When `use_colors` is enabled, ANSI escape codes colorize:
//! - Keywords (magenta bold): `func`, `let`, `if`, etc.
//! - Type names (yellow bold): `I32`, `Bool`, etc.
//! - Literals (cyan): numbers, strings, booleans
//! - Comments (gray): mangled names, annotations
//!
//! ## Usage Example
//!
//! ```cpp
//! HirPrinter printer(/*use_colors=*/true);
//! std::cout << printer.print_module(module);
//! ```
//!
//! ## See Also
//!
//! - `hir_printer.hpp` - Printer class declaration
//! - `hir_module.hpp` - Module structure definitions

#include "hir/hir_printer.hpp"

#include "hir/hir_expr.hpp"
#include "hir/hir_stmt.hpp"
#include "types/type.hpp"

#include <sstream>

namespace tml::hir {

// ============================================================================
// Constructor
// ============================================================================

HirPrinter::HirPrinter(bool use_colors) : use_colors_(use_colors) {}

// ============================================================================
// Color Helpers
// ============================================================================
//
// ANSI escape codes for terminal colorization. Each helper wraps text in
// the appropriate escape sequence when colors are enabled.
//
// Color scheme:
// - Magenta bold (\033[1;35m): Keywords (func, let, if, etc.)
// - Yellow bold (\033[1;33m): Type names
// - Cyan (\033[0;36m): Literals (strings, numbers)
// - Gray (\033[0;90m): Comments and annotations

auto HirPrinter::keyword(const std::string& s) -> std::string {
    if (use_colors_) {
        return "\033[1;35m" + s + "\033[0m"; // Magenta bold
    }
    return s;
}

auto HirPrinter::type_name(const std::string& s) -> std::string {
    if (use_colors_) {
        return "\033[1;33m" + s + "\033[0m"; // Yellow bold
    }
    return s;
}

auto HirPrinter::literal(const std::string& s) -> std::string {
    if (use_colors_) {
        return "\033[0;36m" + s + "\033[0m"; // Cyan
    }
    return s;
}

auto HirPrinter::comment(const std::string& s) -> std::string {
    if (use_colors_) {
        return "\033[0;90m" + s + "\033[0m"; // Gray
    }
    return s;
}

// ============================================================================
// Indentation
// ============================================================================
//
// Indentation management for nested structures. Uses 2-space indentation.
// push_indent/pop_indent adjust the current level for block contents.

auto HirPrinter::indent() -> std::string {
    return std::string(indent_ * 2, ' ');
}

void HirPrinter::push_indent() {
    indent_++;
}

void HirPrinter::pop_indent() {
    if (indent_ > 0) {
        indent_--;
    }
}

// ============================================================================
// Module Printing
// ============================================================================
//
// Prints the complete module with header comment, then sections for:
// structs, enums, and functions. Each section is prefixed with a comment.

auto HirPrinter::print_module(const HirModule& module) -> std::string {
    std::ostringstream out;

    out << comment("// HIR Module: " + module.name) << "\n";
    out << comment("// Source: " + module.source_path) << "\n\n";

    // Print structs
    if (!module.structs.empty()) {
        out << comment("// Structs") << "\n";
        for (const auto& s : module.structs) {
            out << print_struct(s) << "\n";
        }
        out << "\n";
    }

    // Print enums
    if (!module.enums.empty()) {
        out << comment("// Enums") << "\n";
        for (const auto& e : module.enums) {
            out << print_enum(e) << "\n";
        }
        out << "\n";
    }

    // Print functions
    if (!module.functions.empty()) {
        out << comment("// Functions") << "\n";
        for (const auto& f : module.functions) {
            out << print_function(f) << "\n";
        }
    }

    return out.str();
}

// ============================================================================
// Function Printing
// ============================================================================
//
// Prints function signatures with:
// - Attributes (@inline, @extern, etc.)
// - Visibility (pub)
// - Modifiers (async, extern)
// - Parameters with types
// - Return type
// - Body expression (or semicolon for declarations)

auto HirPrinter::print_function(const HirFunction& func) -> std::string {
    std::ostringstream out;

    // Attributes
    for (const auto& attr : func.attributes) {
        out << indent() << "@" << attr << "\n";
    }

    // Signature
    out << indent();
    if (func.is_public) {
        out << keyword("pub") << " ";
    }
    if (func.is_async) {
        out << keyword("async") << " ";
    }
    if (func.is_extern) {
        out << keyword("extern");
        if (func.extern_abi) {
            out << "(" << literal("\"" + *func.extern_abi + "\"") << ")";
        }
        out << " ";
    }

    out << keyword("func") << " " << func.name;

    // Mangled name as comment
    if (func.mangled_name != func.name) {
        out << " " << comment("/* " + func.mangled_name + " */");
    }

    // Parameters
    out << "(";
    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        const auto& param = func.params[i];
        if (param.is_mut) {
            out << keyword("mut") << " ";
        }
        out << param.name << ": " << print_type(param.type);
    }
    out << ")";

    // Return type
    out << " -> " << print_type(func.return_type);

    // Body
    if (func.body) {
        out << " ";
        out << print_expr(**func.body);
    } else {
        out << ";";
    }

    out << "\n";
    return out.str();
}

// ============================================================================
// Struct Printing
// ============================================================================
//
// Prints struct definitions with visibility, name, mangled name comment,
// and field list with types.

auto HirPrinter::print_struct(const HirStruct& s) -> std::string {
    std::ostringstream out;

    out << indent();
    if (s.is_public) {
        out << keyword("pub") << " ";
    }
    out << keyword("type") << " " << s.name;

    if (s.mangled_name != s.name) {
        out << " " << comment("/* " + s.mangled_name + " */");
    }

    out << " {\n";
    push_indent();

    for (const auto& field : s.fields) {
        out << indent();
        if (field.is_public) {
            out << keyword("pub") << " ";
        }
        out << field.name << ": " << print_type(field.type) << ",\n";
    }

    pop_indent();
    out << indent() << "}\n";

    return out.str();
}

// ============================================================================
// Enum Printing
// ============================================================================
//
// Prints enum definitions with visibility, name, and variants.
// Variants with payloads show their associated types in parentheses.

auto HirPrinter::print_enum(const HirEnum& e) -> std::string {
    std::ostringstream out;

    out << indent();
    if (e.is_public) {
        out << keyword("pub") << " ";
    }
    out << keyword("type") << " " << e.name;

    if (e.mangled_name != e.name) {
        out << " " << comment("/* " + e.mangled_name + " */");
    }

    out << " {\n";
    push_indent();

    for (const auto& variant : e.variants) {
        out << indent() << variant.name;
        if (!variant.payload_types.empty()) {
            out << "(";
            for (size_t i = 0; i < variant.payload_types.size(); ++i) {
                if (i > 0) {
                    out << ", ";
                }
                out << print_type(variant.payload_types[i]);
            }
            out << ")";
        }
        out << ",\n";
    }

    pop_indent();
    out << indent() << "}\n";

    return out.str();
}

// ============================================================================
// Expression Printing
// ============================================================================
//
// Recursively prints expressions using std::visit dispatch. Each expression
// kind has its own formatting logic. Binary expressions are parenthesized
// for clarity. Block expressions use indentation for nested content.

auto HirPrinter::print_expr(const HirExpr& expr) -> std::string {
    std::ostringstream out;

    std::visit(
        [&](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirLiteralExpr>) {
                std::visit(
                    [&](const auto& v) {
                        using V = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<V, std::string>) {
                            out << literal("\"" + v + "\"");
                        } else if constexpr (std::is_same_v<V, char>) {
                            out << literal("'" + std::string(1, v) + "'");
                        } else if constexpr (std::is_same_v<V, bool>) {
                            out << literal(v ? "true" : "false");
                        } else if constexpr (std::is_same_v<V, double>) {
                            out << literal(std::to_string(v));
                        } else {
                            out << literal(std::to_string(v));
                        }
                    },
                    e.value);
            } else if constexpr (std::is_same_v<T, HirVarExpr>) {
                out << e.name;
            } else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                out << "(" << print_expr(*e.left) << " ";
                switch (e.op) {
                case HirBinOp::Add:
                    out << "+";
                    break;
                case HirBinOp::Sub:
                    out << "-";
                    break;
                case HirBinOp::Mul:
                    out << "*";
                    break;
                case HirBinOp::Div:
                    out << "/";
                    break;
                case HirBinOp::Mod:
                    out << "%";
                    break;
                case HirBinOp::Eq:
                    out << "==";
                    break;
                case HirBinOp::Ne:
                    out << "!=";
                    break;
                case HirBinOp::Lt:
                    out << "<";
                    break;
                case HirBinOp::Le:
                    out << "<=";
                    break;
                case HirBinOp::Gt:
                    out << ">";
                    break;
                case HirBinOp::Ge:
                    out << ">=";
                    break;
                case HirBinOp::And:
                    out << keyword("and");
                    break;
                case HirBinOp::Or:
                    out << keyword("or");
                    break;
                case HirBinOp::BitAnd:
                    out << "&";
                    break;
                case HirBinOp::BitOr:
                    out << "|";
                    break;
                case HirBinOp::BitXor:
                    out << "^";
                    break;
                case HirBinOp::Shl:
                    out << "<<";
                    break;
                case HirBinOp::Shr:
                    out << ">>";
                    break;
                }
                out << " " << print_expr(*e.right) << ")";
            } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                switch (e.op) {
                case HirUnaryOp::Neg:
                    out << "-";
                    break;
                case HirUnaryOp::Not:
                    out << keyword("not") << " ";
                    break;
                case HirUnaryOp::BitNot:
                    out << "~";
                    break;
                case HirUnaryOp::Ref:
                    out << keyword("ref") << " ";
                    break;
                case HirUnaryOp::RefMut:
                    out << keyword("mut ref") << " ";
                    break;
                case HirUnaryOp::Deref:
                    out << "*";
                    break;
                }
                out << print_expr(*e.operand);
            } else if constexpr (std::is_same_v<T, HirCallExpr>) {
                out << e.func_name << "(";
                for (size_t i = 0; i < e.args.size(); ++i) {
                    if (i > 0) {
                        out << ", ";
                    }
                    out << print_expr(*e.args[i]);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, HirMethodCallExpr>) {
                out << print_expr(*e.receiver) << "." << e.method_name << "(";
                for (size_t i = 0; i < e.args.size(); ++i) {
                    if (i > 0) {
                        out << ", ";
                    }
                    out << print_expr(*e.args[i]);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, HirFieldExpr>) {
                out << print_expr(*e.object) << "." << e.field_name;
            } else if constexpr (std::is_same_v<T, HirIndexExpr>) {
                out << print_expr(*e.object) << "[" << print_expr(*e.index) << "]";
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                out << "{\n";
                push_indent();
                for (const auto& stmt : e.stmts) {
                    out << indent() << print_stmt(*stmt) << "\n";
                }
                if (e.expr) {
                    out << indent() << print_expr(**e.expr) << "\n";
                }
                pop_indent();
                out << indent() << "}";
            } else if constexpr (std::is_same_v<T, HirIfExpr>) {
                out << keyword("if") << " " << print_expr(*e.condition) << " ";
                out << print_expr(*e.then_branch);
                if (e.else_branch) {
                    out << " " << keyword("else") << " " << print_expr(**e.else_branch);
                }
            } else if constexpr (std::is_same_v<T, HirReturnExpr>) {
                out << keyword("return");
                if (e.value) {
                    out << " " << print_expr(**e.value);
                }
            } else if constexpr (std::is_same_v<T, HirBreakExpr>) {
                out << keyword("break");
                if (e.label) {
                    out << " '" << *e.label;
                }
                if (e.value) {
                    out << " " << print_expr(**e.value);
                }
            } else if constexpr (std::is_same_v<T, HirContinueExpr>) {
                out << keyword("continue");
                if (e.label) {
                    out << " '" << *e.label;
                }
            } else {
                out << "<expr>";
            }
        },
        expr.kind);

    return out.str();
}

// ============================================================================
// Statement Printing
// ============================================================================
//
// Prints let statements with pattern, type, and optional initializer.
// Expression statements print their expression followed by semicolon.

auto HirPrinter::print_stmt(const HirStmt& stmt) -> std::string {
    std::ostringstream out;

    std::visit(
        [&](const auto& s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, HirLetStmt>) {
                out << keyword("let") << " " << print_pattern(*s.pattern);
                out << ": " << print_type(s.type);
                if (s.init) {
                    out << " = " << print_expr(**s.init);
                }
                out << ";";
            } else if constexpr (std::is_same_v<T, HirExprStmt>) {
                out << print_expr(*s.expr) << ";";
            }
        },
        stmt.kind);

    return out.str();
}

// ============================================================================
// Pattern Printing
// ============================================================================
//
// Prints patterns for destructuring: wildcards, bindings, literals,
// tuples, structs, and enums. Recursive for nested patterns.

auto HirPrinter::print_pattern(const HirPattern& pattern) -> std::string {
    std::ostringstream out;

    std::visit(
        [&](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, HirWildcardPattern>) {
                out << "_";
            } else if constexpr (std::is_same_v<T, HirBindingPattern>) {
                if (p.is_mut) {
                    out << keyword("mut") << " ";
                }
                out << p.name;
            } else if constexpr (std::is_same_v<T, HirLiteralPattern>) {
                std::visit(
                    [&](const auto& v) {
                        using V = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<V, std::string>) {
                            out << literal("\"" + v + "\"");
                        } else if constexpr (std::is_same_v<V, bool>) {
                            out << literal(v ? "true" : "false");
                        } else {
                            out << literal(std::to_string(v));
                        }
                    },
                    p.value);
            } else if constexpr (std::is_same_v<T, HirTuplePattern>) {
                out << "(";
                for (size_t i = 0; i < p.elements.size(); ++i) {
                    if (i > 0) {
                        out << ", ";
                    }
                    out << print_pattern(*p.elements[i]);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, HirStructPattern>) {
                out << type_name(p.struct_name) << " { ";
                for (size_t i = 0; i < p.fields.size(); ++i) {
                    if (i > 0) {
                        out << ", ";
                    }
                    out << p.fields[i].first << ": " << print_pattern(*p.fields[i].second);
                }
                if (p.has_rest) {
                    if (!p.fields.empty()) {
                        out << ", ";
                    }
                    out << "..";
                }
                out << " }";
            } else if constexpr (std::is_same_v<T, HirEnumPattern>) {
                out << type_name(p.enum_name + "::" + p.variant_name);
                if (p.payload && !p.payload->empty()) {
                    out << "(";
                    for (size_t i = 0; i < p.payload->size(); ++i) {
                        if (i > 0) {
                            out << ", ";
                        }
                        out << print_pattern(*(*p.payload)[i]);
                    }
                    out << ")";
                }
            } else {
                out << "<pattern>";
            }
        },
        pattern.kind);

    return out.str();
}

// ============================================================================
// Type Printing
// ============================================================================
//
// Delegates to types::type_to_string for the actual formatting.
// Null types display as underscore (_) for inference placeholders.

auto HirPrinter::print_type(const HirType& type) -> std::string {
    if (!type) {
        return type_name("_");
    }
    return type_name(types::type_to_string(type));
}

} // namespace tml::hir
