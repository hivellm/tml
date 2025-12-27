#include "tml/ir/ir.hpp"

#include <sstream>

namespace tml::ir {

void IREmitter::emit_func(std::ostringstream& out, const IRFunc& func) {
    emit_indent(out);
    out << "(func " << func.name << " " << func.id;
    indent_level_++;

    // Visibility
    emit_newline(out);
    emit_indent(out);
    out << "(vis " << (func.vis == Visibility::Public ? "public" : "private") << ")";

    // Generics
    if (!func.generics.empty()) {
        emit_newline(out);
        emit_indent(out);
        out << "(generics";
        indent_level_++;
        for (const auto& gen : func.generics) {
            emit_newline(out);
            emit_indent(out);
            out << "(param " << gen.name;
            if (!gen.bounds.empty()) {
                out << " (bounds [";
                for (size_t i = 0; i < gen.bounds.size(); ++i) {
                    if (i > 0)
                        out << " ";
                    out << gen.bounds[i];
                }
                out << "])";
            }
            out << ")";
        }
        indent_level_--;
        out << ")";
    }

    // Params
    if (!func.params.empty()) {
        emit_newline(out);
        emit_indent(out);
        out << "(params";
        indent_level_++;
        for (const auto& param : func.params) {
            emit_newline(out);
            emit_indent(out);
            out << "(param " << param.name << " ";
            emit_type_expr(out, param.type);
            out << ")";
        }
        indent_level_--;
        out << ")";
    }

    // Return type
    if (func.return_type) {
        emit_newline(out);
        emit_indent(out);
        out << "(return ";
        emit_type_expr(out, *func.return_type);
        out << ")";
    }

    // Effects
    if (!func.effects.empty()) {
        emit_newline(out);
        emit_indent(out);
        out << "(effects [";
        for (size_t i = 0; i < func.effects.size(); ++i) {
            if (i > 0)
                out << " ";
            out << func.effects[i];
        }
        out << "])";
    }

    // AI context
    if (func.ai_context) {
        emit_newline(out);
        emit_indent(out);
        out << "(ai-context \"" << *func.ai_context << "\")";
    }

    // Body
    if (func.body) {
        emit_newline(out);
        emit_indent(out);
        out << "(body";
        indent_level_++;
        emit_block(out, *func.body);
        indent_level_--;
        out << ")";
    }

    indent_level_--;
    out << ")";
}

void IREmitter::emit_type(std::ostringstream& out, const IRType& type) {
    emit_indent(out);
    out << "(type " << type.name << " " << type.id;
    indent_level_++;

    // Visibility
    emit_newline(out);
    emit_indent(out);
    out << "(vis " << (type.vis == Visibility::Public ? "public" : "private") << ")";

    // Generics
    if (!type.generics.empty()) {
        emit_newline(out);
        emit_indent(out);
        out << "(generics";
        for (const auto& gen : type.generics) {
            out << " (param " << gen.name << ")";
        }
        out << ")";
    }

    // Kind-specific content
    std::visit(
        [this, &out](const auto& k) {
            using T = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<T, IRStructType>) {
                emit_newline(out);
                emit_indent(out);
                out << "(kind struct)";
                if (!k.fields.empty()) {
                    emit_newline(out);
                    emit_indent(out);
                    out << "(fields";
                    indent_level_++;
                    for (const auto& field : k.fields) {
                        emit_newline(out);
                        emit_indent(out);
                        out << "(field " << field.name << " ";
                        emit_type_expr(out, field.type);
                        out << " (vis " << (field.vis == Visibility::Public ? "public" : "private")
                            << "))";
                    }
                    indent_level_--;
                    out << ")";
                }
            } else if constexpr (std::is_same_v<T, IREnumType>) {
                emit_newline(out);
                emit_indent(out);
                out << "(kind enum)";
                if (!k.variants.empty()) {
                    emit_newline(out);
                    emit_indent(out);
                    out << "(variants";
                    indent_level_++;
                    for (const auto& variant : k.variants) {
                        emit_newline(out);
                        emit_indent(out);
                        out << "(variant " << variant.name;
                        if (!variant.fields.empty()) {
                            out << " (";
                            for (size_t i = 0; i < variant.fields.size(); ++i) {
                                if (i > 0)
                                    out << " ";
                                emit_type_expr(out, variant.fields[i]);
                            }
                            out << ")";
                        }
                        out << ")";
                    }
                    indent_level_--;
                    out << ")";
                }
            } else if constexpr (std::is_same_v<T, IRAliasType>) {
                emit_newline(out);
                emit_indent(out);
                out << "(kind alias)";
                emit_newline(out);
                emit_indent(out);
                out << "(target ";
                emit_type_expr(out, k.target);
                out << ")";
            }
        },
        type.kind);

    indent_level_--;
    out << ")";
}

void IREmitter::emit_behavior(std::ostringstream& out, const IRBehavior& behavior) {
    emit_indent(out);
    out << "(behavior " << behavior.name << " " << behavior.id;
    indent_level_++;

    // Visibility
    emit_newline(out);
    emit_indent(out);
    out << "(vis " << (behavior.vis == Visibility::Public ? "public" : "private") << ")";

    // Methods
    if (!behavior.methods.empty()) {
        emit_newline(out);
        emit_indent(out);
        out << "(methods";
        indent_level_++;
        for (const auto& method : behavior.methods) {
            emit_newline(out);
            emit_indent(out);
            out << "(method " << method.name;
            indent_level_++;
            if (!method.params.empty()) {
                emit_newline(out);
                emit_indent(out);
                out << "(params";
                for (const auto& param : method.params) {
                    out << " (param " << param.name << " ";
                    emit_type_expr(out, param.type);
                    out << ")";
                }
                out << ")";
            }
            if (method.return_type) {
                emit_newline(out);
                emit_indent(out);
                out << "(return ";
                emit_type_expr(out, *method.return_type);
                out << ")";
            }
            if (method.default_impl) {
                emit_newline(out);
                emit_indent(out);
                out << "(default";
                emit_block(out, *method.default_impl);
                out << ")";
            } else {
                emit_newline(out);
                emit_indent(out);
                out << "(default nil)";
            }
            indent_level_--;
            out << ")";
        }
        indent_level_--;
        out << ")";
    }

    indent_level_--;
    out << ")";
}

void IREmitter::emit_impl(std::ostringstream& out, const IRImpl& impl) {
    emit_indent(out);
    out << "(extend " << impl.id;
    indent_level_++;

    emit_newline(out);
    emit_indent(out);
    out << "(target " << impl.target_type << ")";

    if (impl.behavior) {
        emit_newline(out);
        emit_indent(out);
        out << "(behavior " << *impl.behavior << ")";
    }

    if (!impl.methods.empty()) {
        emit_newline(out);
        emit_indent(out);
        out << "(methods";
        indent_level_++;
        for (const auto& method : impl.methods) {
            emit_newline(out);
            emit_indent(out);
            out << "(method " << method.name << " " << method.id;
            indent_level_++;
            if (!method.params.empty()) {
                emit_newline(out);
                emit_indent(out);
                out << "(params";
                for (const auto& param : method.params) {
                    out << " (param " << param.name << " ";
                    emit_type_expr(out, param.type);
                    out << ")";
                }
                out << ")";
            }
            if (method.return_type) {
                emit_newline(out);
                emit_indent(out);
                out << "(return ";
                emit_type_expr(out, *method.return_type);
                out << ")";
            }
            emit_newline(out);
            emit_indent(out);
            out << "(body";
            emit_block(out, method.body);
            out << ")";
            indent_level_--;
            out << ")";
        }
        indent_level_--;
        out << ")";
    }

    indent_level_--;
    out << ")";
}

void IREmitter::emit_const(std::ostringstream& out, const IRConst& cst) {
    emit_indent(out);
    out << "(const " << cst.name << " " << cst.id;
    indent_level_++;

    emit_newline(out);
    emit_indent(out);
    out << "(vis " << (cst.vis == Visibility::Public ? "public" : "private") << ")";

    emit_newline(out);
    emit_indent(out);
    out << "(type ";
    emit_type_expr(out, cst.type);
    out << ")";

    emit_newline(out);
    emit_indent(out);
    out << "(value ";
    emit_expr(out, *cst.value);
    out << ")";

    indent_level_--;
    out << ")";
}

} // namespace tml::ir
