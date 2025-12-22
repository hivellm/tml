// TML source code formatter - core utilities
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

} // namespace tml::format
