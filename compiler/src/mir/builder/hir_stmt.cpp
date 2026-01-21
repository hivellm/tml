//! # HIR Statement Lowering to MIR
//!
//! This file implements statement lowering from HIR to MIR.
//! Statements include let bindings and expression statements.

#include "mir/hir_mir_builder.hpp"

namespace tml::mir {

// ============================================================================
// Statement Building
// ============================================================================

auto HirMirBuilder::build_stmt(const hir::HirStmt& stmt) -> bool {
    return std::visit(
        [this](const auto& s) -> bool {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, hir::HirLetStmt>) {
                build_let_stmt(s);
                return false; // Not terminated
            } else if constexpr (std::is_same_v<T, hir::HirExprStmt>) {
                build_expr_stmt(s);
                return is_terminated(); // May terminate if contains return/break
            } else {
                return false;
            }
        },
        stmt.kind);
}

// ============================================================================
// Let Statement
// ============================================================================

void HirMirBuilder::build_let_stmt(const hir::HirLetStmt& let) {
    // Build initializer if present
    Value init_value;
    if (let.init) {
        init_value = build_expr(*let.init);
    } else {
        // Uninitialized variable - use unit as placeholder
        init_value = const_unit();
    }

    // For volatile variables, we need to allocate stack storage and use volatile loads/stores
    if (let.is_volatile) {
        if (auto* binding = std::get_if<hir::HirBindingPattern>(&let.pattern->kind)) {
            // Create volatile alloca for the variable
            AllocaInst alloca;
            alloca.alloc_type = init_value.type;
            alloca.name = binding->name;
            alloca.is_volatile = true;

            auto ptr_type = make_pointer_type(init_value.type, binding->is_mut);
            auto alloca_val = emit(std::move(alloca), ptr_type);

            // Store initial value (volatile store)
            StoreInst store;
            store.ptr = alloca_val;
            store.value = init_value;
            store.value_type = init_value.type;
            store.is_volatile = true;
            emit_void(std::move(store));

            // Map variable to alloca (will be loaded with volatile load when accessed)
            ctx_.variables[binding->name] = alloca_val;
            ctx_.volatile_vars.insert(binding->name);

            // Register for drop if needed
            MirTypePtr var_type = init_value.type;
            std::string type_name = get_type_name(var_type);
            if (!type_name.empty() && !env_.is_trivially_destructible(type_name)) {
                ctx_.register_for_drop(binding->name, alloca_val, type_name, var_type);
            }
            return;
        }
    }

    // Bind pattern to value (non-volatile path)
    build_pattern_binding(let.pattern, init_value);

    // Register for drop if the type needs dropping (not trivially destructible)
    MirTypePtr var_type = init_value.type;
    std::string type_name = get_type_name(var_type);

    // For simple binding patterns, register the variable for drop
    // Only if the type actually needs drop (implements Drop or contains non-trivial fields)
    if (auto* binding = std::get_if<hir::HirBindingPattern>(&let.pattern->kind)) {
        if (!type_name.empty() && !env_.is_trivially_destructible(type_name)) {
            ctx_.register_for_drop(binding->name, init_value, type_name, var_type);
        }
    }
}

// ============================================================================
// Expression Statement
// ============================================================================

void HirMirBuilder::build_expr_stmt(const hir::HirExprStmt& expr) {
    // Build expression, discard result
    (void)build_expr(expr.expr);
}

} // namespace tml::mir
