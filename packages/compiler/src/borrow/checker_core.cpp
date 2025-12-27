#include "borrow/checker.hpp"

#include <algorithm>

namespace tml::borrow {

// ============================================================================
// Lifetime Elision Rules
// ============================================================================
//
// TML follows Rust's lifetime elision rules to reduce annotation burden:
//
// Rule 1: Each elided lifetime in input position becomes a distinct lifetime parameter.
//   func foo(x: ref T) -> ...       becomes  func foo['a](x: ref['a] T) -> ...
//   func bar(x: ref T, y: ref U)    becomes  func bar['a, 'b](x: ref['a] T, y: ref['b] U)
//
// Rule 2: If there is exactly one input lifetime position, that lifetime is
//         assigned to all elided output lifetimes.
//   func foo(x: ref T) -> ref U     becomes  func foo['a](x: ref['a] T) -> ref['a] U
//
// Rule 3: If there are multiple input lifetime positions, but one is &self or &mut self,
//         the lifetime of self is assigned to all elided output lifetimes.
//   impl Foo { func bar(self: ref Self, x: ref T) -> ref U }
//   becomes: impl Foo { func bar['a, 'b](self: ref['a] Self, x: ref['b] T) -> ref['a] U }
//
// These rules are applied implicitly during borrow checking.
// ============================================================================

// ============================================================================
// BorrowChecker Implementation
// ============================================================================

BorrowChecker::BorrowChecker() = default;

auto BorrowChecker::check_module(const parser::Module& module)
    -> Result<bool, std::vector<BorrowError>> {
    errors_.clear();
    env_ = BorrowEnv{};

    for (const auto& decl : module.decls) {
        std::visit(
            [this](const auto& d) {
                using T = std::decay_t<decltype(d)>;
                if constexpr (std::is_same_v<T, parser::FuncDecl>) {
                    check_func_decl(d);
                } else if constexpr (std::is_same_v<T, parser::ImplDecl>) {
                    check_impl_decl(d);
                }
                // Other declarations don't need borrow checking
            },
            decl->kind);
    }

    if (has_errors()) {
        return errors_;
    }
    return true;
}

auto BorrowChecker::is_copy_type(const types::TypePtr& type) const -> bool {
    if (!type)
        return true;

    return std::visit(
        [this](const auto& t) -> bool {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, types::PrimitiveType>) {
                // All primitives are Copy
                return true;
            } else if constexpr (std::is_same_v<T, types::RefType>) {
                // References are Copy (the reference, not the data)
                return true;
            } else if constexpr (std::is_same_v<T, types::TupleType>) {
                // Tuple is Copy if all elements are Copy
                for (const auto& elem : t.elements) {
                    if (!is_copy_type(elem))
                        return false;
                }
                return true;
            } else if constexpr (std::is_same_v<T, types::ArrayType>) {
                // Array is Copy if element type is Copy
                return is_copy_type(t.element);
            } else {
                // Named types, functions, etc. are not Copy by default
                return false;
            }
        },
        type->kind);
}

auto BorrowChecker::get_move_semantics(const types::TypePtr& type) const -> MoveSemantics {
    return is_copy_type(type) ? MoveSemantics::Copy : MoveSemantics::Move;
}

void BorrowChecker::check_func_decl(const parser::FuncDecl& func) {
    env_.push_scope();
    current_stmt_ = 0;

    // Register parameters - FuncParam is a simple struct with pattern, type, span
    for (const auto& param : func.params) {
        bool is_mut = false;
        std::string name;

        if (param.pattern->template is<parser::IdentPattern>()) {
            const auto& ident = param.pattern->template as<parser::IdentPattern>();
            is_mut = ident.is_mut;
            name = ident.name;
        } else {
            name = "_param";
        }

        auto loc = current_location(func.span);
        // Note: We'd need the resolved type here - using nullptr for now
        env_.define(name, nullptr, is_mut, loc);
    }

    // Check function body - body is std::optional<BlockExpr>
    if (func.body) {
        check_block(*func.body);
    }

    // Drop all places at end of function
    drop_scope_places();
    env_.pop_scope();
}

void BorrowChecker::check_impl_decl(const parser::ImplDecl& impl) {
    for (const auto& method : impl.methods) {
        check_func_decl(method);
    }
}

} // namespace tml::borrow
