TML_MODULE("tools")

//! # Formatter Core Utilities
//!
//! This file implements core formatting infrastructure.
//!
//! ## Functions
//!
//! | Method          | Description                              |
//! |-----------------|------------------------------------------|
//! | `format()`      | Format complete module to string         |
//! | `emit()`        | Write text to output buffer              |
//! | `emit_line()`   | Write indented line with newline         |
//! | `push_indent()` | Increase indentation level               |
//! | `pop_indent()`  | Decrease indentation level               |
//! | `indent_str()`  | Get current indentation string           |

#include "format/formatter.hpp"

namespace tml::format {

Formatter::Formatter(FormatOptions options) : options_(std::move(options)) {}

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
    if (indent_level_ > 0)
        --indent_level_;
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

} // namespace tml::format
