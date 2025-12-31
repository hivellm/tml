// MIR Builder - Statement Building Implementation
//
// This file contains functions for building MIR from statement and
// declaration AST nodes.

#include "mir/mir_builder.hpp"

namespace tml::mir {

// ============================================================================
// Declaration Building
// ============================================================================

void MirBuilder::build_decl(const parser::Decl& decl) {
    std::visit(
        [this](const auto& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, parser::FuncDecl>) {
                build_func_decl(d);
            } else if constexpr (std::is_same_v<T, parser::StructDecl>) {
                build_struct_decl(d);
            } else if constexpr (std::is_same_v<T, parser::EnumDecl>) {
                build_enum_decl(d);
            }
            // Other declarations (trait, impl, use, etc.) handled separately
        },
        decl.kind);
}

void MirBuilder::build_func_decl(const parser::FuncDecl& func) {
    // Skip generic functions (they are instantiated on demand)
    if (!func.generics.empty())
        return;

    // Skip extern functions (no body)
    if (!func.body.has_value())
        return;

    Function mir_func;
    mir_func.name = func.name;
    mir_func.is_public = (func.vis == parser::Visibility::Public);
    mir_func.is_async = func.is_async;

    // Extract decorator attributes (@inline, @noinline, etc.)
    for (const auto& decorator : func.decorators) {
        mir_func.attributes.push_back(decorator.name);
    }

    // Convert return type
    mir_func.return_type = func.return_type ? convert_type(**func.return_type) : make_unit_type();

    // Set up context
    ctx_.current_func = &mir_func;
    ctx_.variables.clear();
    ctx_.in_async_func = func.is_async;
    ctx_.next_suspension_id = 0;

    // Create entry block
    auto entry = mir_func.create_block("entry");
    ctx_.current_block = entry;

    // Add parameters
    for (const auto& param : func.params) {
        // Get parameter name from pattern
        std::string param_name;
        if (param.pattern->is<parser::IdentPattern>()) {
            param_name = param.pattern->as<parser::IdentPattern>().name;
        } else {
            param_name = "_param" + std::to_string(mir_func.params.size());
        }

        auto param_type = convert_type(*param.type);
        auto value_id = mir_func.fresh_value();

        FunctionParam mir_param;
        mir_param.name = param_name;
        mir_param.type = param_type;
        mir_param.value_id = value_id;
        mir_func.params.push_back(mir_param);

        // Add to variable map
        ctx_.variables[param_name] = Value{value_id, param_type};
    }

    // Build function body
    auto body_value = build_block(*func.body);

    // Add implicit return if not terminated
    if (!is_terminated()) {
        if (mir_func.return_type->is_unit()) {
            emit_return();
        } else {
            emit_return(body_value);
        }
    }

    module_.functions.push_back(std::move(mir_func));
    ctx_.current_func = nullptr;
}

void MirBuilder::build_struct_decl(const parser::StructDecl& s) {
    // Skip generic structs
    if (!s.generics.empty())
        return;

    StructDef def;
    def.name = s.name;

    for (const auto& field : s.fields) {
        StructField f;
        f.name = field.name;
        f.type = convert_type(*field.type);
        def.fields.push_back(std::move(f));
    }

    module_.structs.push_back(std::move(def));
}

void MirBuilder::build_enum_decl(const parser::EnumDecl& e) {
    // Skip generic enums
    if (!e.generics.empty())
        return;

    EnumDef def;
    def.name = e.name;

    for (const auto& variant : e.variants) {
        EnumVariant v;
        v.name = variant.name;

        if (variant.tuple_fields.has_value()) {
            for (const auto& field : *variant.tuple_fields) {
                v.payload_types.push_back(convert_type(*field));
            }
        }

        def.variants.push_back(std::move(v));
    }

    module_.enums.push_back(std::move(def));
}

// ============================================================================
// Statement Building
// ============================================================================

void MirBuilder::build_stmt(const parser::Stmt& stmt) {
    std::visit(
        [this](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, parser::LetStmt>) {
                build_let_stmt(s);
            } else if constexpr (std::is_same_v<T, parser::VarStmt>) {
                build_var_stmt(s);
            } else if constexpr (std::is_same_v<T, parser::ExprStmt>) {
                build_expr_stmt(s);
            } else if constexpr (std::is_same_v<T, parser::DeclPtr>) {
                build_decl(*s);
            }
        },
        stmt.kind);
}

void MirBuilder::build_let_stmt(const parser::LetStmt& let) {
    if (!let.init.has_value())
        return;

    auto init_value = build_expr(**let.init);
    build_pattern_binding(*let.pattern, init_value);
}

void MirBuilder::build_var_stmt(const parser::VarStmt& var) {
    auto init_value = build_expr(*var.init);

    // For mutable variables, allocate stack space
    auto alloca_val =
        emit(AllocaInst{init_value.type, var.name}, make_pointer_type(init_value.type, true));

    // Store initial value
    emit_void(StoreInst{alloca_val, init_value});

    // Map variable to alloca
    ctx_.variables[var.name] = alloca_val;
}

void MirBuilder::build_expr_stmt(const parser::ExprStmt& expr) {
    build_expr(*expr.expr);
}

} // namespace tml::mir
