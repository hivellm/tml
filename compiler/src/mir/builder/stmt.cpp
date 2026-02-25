TML_MODULE("compiler")

//! # MIR Builder - Statements
//!
//! This file converts AST statements and declarations to MIR.
//!
//! ## Declaration Building
//!
//! | Declaration | Handler              | Description              |
//! |-------------|----------------------|--------------------------|
//! | `func`      | `build_func_decl()`  | Create function + blocks |
//! | `type`      | `build_struct_decl()`| Add struct definition    |
//! | `enum`      | `build_enum_decl()`  | Add enum definition      |
//!
//! ## Statement Building
//!
//! | Statement   | Handler              | Description              |
//! |-------------|----------------------|--------------------------|
//! | `let`       | `build_let_stmt()`   | Pattern binding          |
//! | `var`       | `build_var_stmt()`   | Mutable variable (alloca)|
//! | expression  | `build_expr_stmt()`  | Evaluate and discard     |
//!
//! ## Drop Registration
//!
//! Variables are registered for drop when they go out of scope.

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
    ctx_.drop_scopes.clear();
    ctx_.in_async_func = func.is_async;
    ctx_.next_suspension_id = 0;

    // Create entry block
    auto entry = mir_func.create_block("entry");
    ctx_.current_block = entry;

    // Push initial drop scope for function
    ctx_.push_drop_scope();

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
        // Emit drops for function-level scope before implicit return
        emit_all_drops();

        if (mir_func.return_type->is_unit()) {
            emit_return();
        } else {
            emit_return(body_value);
        }
    }

    // Pop function drop scope
    ctx_.pop_drop_scope();

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

    if (let.type_annotation.has_value()) {
        expr_type_hint_ = convert_type(**let.type_annotation);
    }
    auto init_value = build_expr(**let.init);
    expr_type_hint_ = nullptr;
    build_pattern_binding(*let.pattern, init_value);

    // Register for drop if the pattern is a simple identifier
    // Only if the type is NOT trivially destructible
    if (let.pattern->is<parser::IdentPattern>()) {
        const auto& ident = let.pattern->as<parser::IdentPattern>();
        std::string type_name = get_type_name(init_value.type);
        if (!type_name.empty() && !env_.is_trivially_destructible(type_name)) {
            ctx_.register_for_drop(ident.name, init_value, type_name, init_value.type);
        }
    }
}

void MirBuilder::build_var_stmt(const parser::VarStmt& var) {
    // Set the type hint before building the initializer so that array/literal
    // expressions can use the annotated element type (e.g. U8 vs I32).
    if (var.type_annotation.has_value()) {
        expr_type_hint_ = convert_type(**var.type_annotation);
    }
    auto init_value = build_expr(*var.init);
    expr_type_hint_ = nullptr;

    // For mutable variables, allocate stack space
    auto alloca_val =
        emit(AllocaInst{init_value.type, var.name}, make_pointer_type(init_value.type, true));

    // Store initial value
    emit_void(StoreInst{alloca_val, init_value});

    // Map variable to alloca
    ctx_.variables[var.name] = alloca_val;

    // Register for drop - for mutable vars we need to load before dropping
    // Note: we register the alloca pointer; codegen will handle loading
    // Only if the type is NOT trivially destructible
    std::string type_name = get_type_name(init_value.type);
    if (!type_name.empty() && !env_.is_trivially_destructible(type_name)) {
        ctx_.register_for_drop(var.name, alloca_val, type_name, init_value.type);
    }
}

void MirBuilder::build_expr_stmt(const parser::ExprStmt& expr) {
    build_expr(*expr.expr);
}

} // namespace tml::mir
