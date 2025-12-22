#include "tml/ir/ir.hpp"
#include <sstream>

namespace tml::ir {

IREmitter::IREmitter(Options opts) : opts_(opts) {}

auto IREmitter::emit_module(const IRModule& module) -> std::string {
    std::ostringstream out;

    out << "(module " << module.name << " " << module.id;
    indent_level_++;

    // Caps
    if (!module.caps.empty()) {
        emit_newline(out);
        emit_indent(out);
        out << "(caps [";
        for (size_t i = 0; i < module.caps.size(); ++i) {
            if (i > 0) out << " ";
            out << module.caps[i];
        }
        out << "])";
    }

    // Imports
    if (!module.imports.empty()) {
        emit_newline(out);
        emit_indent(out);
        out << "(imports";
        indent_level_++;
        for (const auto& imp : module.imports) {
            emit_newline(out);
            emit_indent(out);
            out << "(import " << imp.path;
            if (imp.alias) {
                out << " :as " << *imp.alias;
            }
            out << ")";
        }
        indent_level_--;
        out << ")";
    }

    // Items
    if (!module.items.empty()) {
        emit_newline(out);
        emit_indent(out);
        out << "(items";
        indent_level_++;
        for (const auto& item : module.items) {
            emit_newline(out);
            emit_item(out, item);
        }
        indent_level_--;
        out << ")";
    }

    indent_level_--;
    out << ")";
    emit_newline(out);

    return out.str();
}

void IREmitter::emit_item(std::ostringstream& out, const IRItem& item) {
    std::visit([this, &out](const auto& i) {
        using T = std::decay_t<decltype(i)>;
        if constexpr (std::is_same_v<T, IRConst>) {
            emit_const(out, i);
        } else if constexpr (std::is_same_v<T, IRType>) {
            emit_type(out, i);
        } else if constexpr (std::is_same_v<T, IRBehavior>) {
            emit_behavior(out, i);
        } else if constexpr (std::is_same_v<T, IRImpl>) {
            emit_impl(out, i);
        } else if constexpr (std::is_same_v<T, IRFunc>) {
            emit_func(out, i);
        }
    }, item);
}


void IREmitter::emit_indent(std::ostringstream& out) {
    if (!opts_.compact) {
        for (int i = 0; i < indent_level_ * opts_.indent_size; ++i) {
            out << ' ';
        }
    }
}

void IREmitter::emit_newline(std::ostringstream& out) {
    if (!opts_.compact) {
        out << '\n';
    } else {
        out << ' ';
    }
}

} // namespace tml::ir
