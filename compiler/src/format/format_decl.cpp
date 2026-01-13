//! # Declaration Formatting
//!
//! This file implements formatting for all declaration types.
//!
//! ## Declaration Types
//!
//! | Declaration | Keyword    | Example                           |
//! |-------------|------------|-----------------------------------|
//! | Function    | `func`     | `func add(a: I32, b: I32) -> I32` |
//! | Struct      | `type`     | `type Point { x: I32, y: I32 }`   |
//! | Enum        | `type`     | `type Color { Red, Green, Blue }` |
//! | Trait       | `behavior` | `behavior Show { func show() }`   |
//! | Impl        | `impl`     | `impl Show for Point { ... }`     |
//! | Const       | `const`    | `const PI: F64 = 3.14159`         |
//! | Use         | `use`      | `use std::io`                     |
//! | Module      | `mod`      | `mod utils { ... }`               |
//!
//! ## Helper Functions
//!
//! - `format_decorators()`: Format @attr annotations
//! - `format_visibility()`: Format pub keyword
//! - `format_generics()`: Format [T, U] parameters
//! - `format_where_clause()`: Format where constraints

#include "format/formatter.hpp"

namespace tml::format {

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
                if (i > 0)
                    output_ << ", ";
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
    if (generics.empty())
        return;

    output_ << "[";
    for (size_t i = 0; i < generics.size(); ++i) {
        if (i > 0)
            output_ << ", ";
        output_ << generics[i].name;
        if (!generics[i].bounds.empty()) {
            output_ << ": ";
            for (size_t j = 0; j < generics[i].bounds.size(); ++j) {
                if (j > 0)
                    output_ << " + ";
                output_ << format_type(*generics[i].bounds[j]);
            }
        }
    }
    output_ << "]";
}

void Formatter::format_where_clause(const std::optional<parser::WhereClause>& where) {
    if (!where.has_value() || (where->constraints.empty() && where->type_equalities.empty()))
        return;

    output_ << "\n";
    emit_indent();
    output_ << "where ";
    bool first = true;

    // Format trait bounds
    for (const auto& [type, bounds] : where->constraints) {
        if (!first)
            output_ << ", ";
        first = false;
        output_ << format_type_ptr(type) << ": ";
        for (size_t j = 0; j < bounds.size(); ++j) {
            if (j > 0)
                output_ << " + ";
            output_ << format_type_ptr(bounds[j]);
        }
    }

    // Format type equalities
    for (const auto& [lhs, rhs] : where->type_equalities) {
        if (!first)
            output_ << ", ";
        first = false;
        output_ << format_type_ptr(lhs) << " = " << format_type_ptr(rhs);
    }
}

auto Formatter::format_func_params(const std::vector<parser::FuncParam>& params) -> std::string {
    std::stringstream ss;
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0)
            ss << ", ";
        ss << format_pattern(*params[i].pattern);
        // Special case: 'this' doesn't need type annotation
        if (!(params[i].pattern->is<parser::IdentPattern>() &&
              params[i].pattern->as<parser::IdentPattern>().name == "this")) {
            ss << ":";
            if (options_.space_after_colon)
                ss << " ";
            ss << format_type_ptr(params[i].type);
        }
    }
    return ss.str();
}

void Formatter::format_func_decl(const parser::FuncDecl& func) {
    format_decorators(func.decorators);
    emit_indent();
    format_visibility(func.vis);

    if (func.is_async)
        output_ << "async ";
    if (func.is_unsafe)
        output_ << "lowlevel ";

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
        if (options_.space_after_colon)
            output_ << " ";
        output_ << format_type_ptr(field.type);
        if (options_.trailing_commas)
            output_ << ",";
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
                if (i > 0)
                    output_ << ", ";
                output_ << format_type_ptr((*variant.tuple_fields)[i]);
            }
            output_ << ")";
        } else if (variant.struct_fields.has_value()) {
            output_ << " {\n";
            push_indent();
            for (const auto& field : *variant.struct_fields) {
                emit_indent();
                output_ << field.name << ":";
                if (options_.space_after_colon)
                    output_ << " ";
                output_ << format_type_ptr(field.type);
                if (options_.trailing_commas)
                    output_ << ",";
                output_ << "\n";
            }
            pop_indent();
            emit_indent();
            output_ << "}";
        }

        if (options_.trailing_commas)
            output_ << ",";
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
            if (i > 0)
                output_ << " + ";
            output_ << format_type_ptr(t.super_traits[i]);
        }
    }

    format_where_clause(t.where_clause);
    output_ << " {\n";

    push_indent();
    for (size_t i = 0; i < t.methods.size(); ++i) {
        format_func_decl(t.methods[i]);
        if (i + 1 < t.methods.size())
            emit_newline();
    }
    pop_indent();

    emit_line("}");
}

void Formatter::format_impl_decl(const parser::ImplDecl& impl) {
    emit_indent();
    output_ << "impl";
    format_generics(impl.generics);
    output_ << " ";

    if (impl.trait_type) {
        output_ << format_type_ptr(impl.trait_type) << " for ";
    }

    output_ << format_type_ptr(impl.self_type);
    format_where_clause(impl.where_clause);
    output_ << " {\n";

    push_indent();
    for (size_t i = 0; i < impl.methods.size(); ++i) {
        format_func_decl(impl.methods[i]);
        if (i + 1 < impl.methods.size())
            emit_newline();
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
    if (options_.space_after_colon)
        output_ << " ";
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

} // namespace tml::format
