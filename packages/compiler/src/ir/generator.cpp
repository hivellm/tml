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
                    if (i > 0) out << " ";
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
            if (i > 0) out << " ";
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
    std::visit([this, &out](const auto& k) {
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
                    out << " (vis " << (field.vis == Visibility::Public ? "public" : "private") << "))";
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
                            if (i > 0) out << " ";
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
    }, type.kind);

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

void IREmitter::emit_expr(std::ostringstream& out, const IRExpr& expr) {
    std::visit([this, &out](const auto& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, IRLiteral>) {
            out << "(lit " << e.value << " " << e.type_name << ")";
        }
        else if constexpr (std::is_same_v<T, IRVar>) {
            out << "(var " << e.name << ")";
        }
        else if constexpr (std::is_same_v<T, IRBinaryOp>) {
            out << "(" << e.op << " ";
            emit_expr(out, *e.left);
            out << " ";
            emit_expr(out, *e.right);
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRUnaryOp>) {
            out << "(" << e.op << " ";
            emit_expr(out, *e.operand);
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRCall>) {
            out << "(call " << e.func_name;
            for (const auto& arg : e.args) {
                out << " ";
                emit_expr(out, *arg);
            }
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRMethodCall>) {
            out << "(method-call ";
            emit_expr(out, *e.receiver);
            out << " " << e.method_name;
            for (const auto& arg : e.args) {
                out << " ";
                emit_expr(out, *arg);
            }
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRFieldGet>) {
            out << "(field-get ";
            emit_expr(out, *e.object);
            out << " " << e.field_name << ")";
        }
        else if constexpr (std::is_same_v<T, IRFieldSet>) {
            out << "(field-set ";
            emit_expr(out, *e.object);
            out << " " << e.field_name << " ";
            emit_expr(out, *e.value);
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRIndex>) {
            out << "(index ";
            emit_expr(out, *e.object);
            out << " ";
            emit_expr(out, *e.index);
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRStructExpr>) {
            out << "(struct " << e.type_name;
            for (const auto& [name, val] : e.fields) {
                out << " (" << name << " ";
                emit_expr(out, *val);
                out << ")";
            }
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRVariantExpr>) {
            out << "(variant " << e.variant_name;
            for (const auto& field : e.fields) {
                out << " ";
                emit_expr(out, *field);
            }
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRTupleExpr>) {
            out << "(tuple";
            for (const auto& elem : e.elements) {
                out << " ";
                emit_expr(out, *elem);
            }
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRArrayExpr>) {
            out << "(array";
            for (const auto& elem : e.elements) {
                out << " ";
                emit_expr(out, *elem);
            }
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRArrayRepeat>) {
            out << "(array-repeat ";
            emit_expr(out, *e.value);
            out << " ";
            emit_expr(out, *e.count);
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRIf>) {
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
        }
        else if constexpr (std::is_same_v<T, IRWhen>) {
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
        }
        else if constexpr (std::is_same_v<T, IRLoop>) {
            out << "(loop ";
            emit_expr(out, *e.body);
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRLoopIn>) {
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
        }
        else if constexpr (std::is_same_v<T, IRLoopWhile>) {
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
        }
        else if constexpr (std::is_same_v<T, IRBlock>) {
            emit_block(out, e);
        }
        else if constexpr (std::is_same_v<T, IRClosure>) {
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
        }
        else if constexpr (std::is_same_v<T, IRTry>) {
            out << "(try ";
            emit_expr(out, *e.expr);
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRReturn>) {
            if (e.value) {
                out << "(return ";
                emit_expr(out, **e.value);
                out << ")";
            } else {
                out << "(return)";
            }
        }
        else if constexpr (std::is_same_v<T, IRBreak>) {
            if (e.value) {
                out << "(break ";
                emit_expr(out, **e.value);
                out << ")";
            } else {
                out << "(break)";
            }
        }
        else if constexpr (std::is_same_v<T, IRContinue>) {
            out << "(continue)";
        }
        else if constexpr (std::is_same_v<T, IRRange>) {
            out << "(range ";
            emit_expr(out, *e.start);
            out << " ";
            emit_expr(out, *e.end);
            out << " " << (e.inclusive ? "inclusive" : "exclusive") << ")";
        }
    }, expr.kind);
}

void IREmitter::emit_stmt(std::ostringstream& out, const IRStmt& stmt) {
    std::visit([this, &out](const auto& s) {
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
        }
        else if constexpr (std::is_same_v<T, IRVarMut>) {
            out << "(var-mut " << s.name;
            if (s.type_annotation) {
                out << " ";
                emit_type_expr(out, *s.type_annotation);
            }
            out << " ";
            emit_expr(out, *s.init);
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRAssign>) {
            out << "(assign ";
            emit_expr(out, *s.target);
            out << " ";
            emit_expr(out, *s.value);
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRExprStmt>) {
            emit_expr(out, *s.expr);
        }
    }, stmt.kind);
}

void IREmitter::emit_pattern(std::ostringstream& out, const IRPattern& pattern) {
    std::visit([this, &out](const auto& p) {
        using T = std::decay_t<decltype(p)>;

        if constexpr (std::is_same_v<T, IRPatternLit>) {
            out << "(pattern-lit " << p.value << ")";
        }
        else if constexpr (std::is_same_v<T, IRPatternBind>) {
            out << "(pattern-bind " << p.name;
            if (p.is_mut) out << " :mut";
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRPatternWild>) {
            out << "(pattern-wild)";
        }
        else if constexpr (std::is_same_v<T, IRPatternTuple>) {
            out << "(pattern-tuple";
            for (const auto& elem : p.elements) {
                out << " ";
                emit_pattern(out, *elem);
            }
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRPatternStruct>) {
            out << "(pattern-struct " << p.type_name;
            for (const auto& [name, pat] : p.fields) {
                out << " (" << name << " ";
                emit_pattern(out, *pat);
                out << ")";
            }
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRPatternVariant>) {
            out << "(pattern-variant " << p.variant_name;
            for (const auto& field : p.fields) {
                out << " ";
                emit_pattern(out, *field);
            }
            out << ")";
        }
    }, pattern.kind);
}

void IREmitter::emit_type_expr(std::ostringstream& out, const IRTypeExpr& type) {
    std::visit([this, &out](const auto& t) {
        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, IRTypeRef>) {
            out << t.name;
            if (!t.type_args.empty()) {
                out << "[";
                for (size_t i = 0; i < t.type_args.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << t.type_args[i]->name;
                }
                out << "]";
            }
        }
        else if constexpr (std::is_same_v<T, IRRefType>) {
            out << (t.is_mut ? "(mut-ref " : "(ref ");
            out << t.inner->name << ")";
        }
        else if constexpr (std::is_same_v<T, IRSliceType>) {
            out << "(slice " << t.element->name << ")";
        }
        else if constexpr (std::is_same_v<T, IRArrayType>) {
            out << "(array " << t.element->name << " " << t.size << ")";
        }
        else if constexpr (std::is_same_v<T, IRTupleType>) {
            out << "(tuple";
            for (const auto& elem : t.elements) {
                out << " " << elem->name;
            }
            out << ")";
        }
        else if constexpr (std::is_same_v<T, IRFuncType>) {
            out << "(func (";
            for (size_t i = 0; i < t.params.size(); ++i) {
                if (i > 0) out << " ";
                out << t.params[i]->name;
            }
            out << ") -> " << t.ret->name << ")";
        }
    }, type.kind);
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
