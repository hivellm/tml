//! # Borrow Checker Core Implementation
//!
//! This file contains the core borrow checking logic including:
//! - Module-level checking entry point
//! - Type analysis for Copy vs Move semantics
//! - Function and impl block validation
//!
//! ## Architecture
//!
//! The borrow checker operates in a single forward pass over the AST:
//!
//! ```text
//! Module
//!   └─ check_module()
//!        ├─ FuncDecl → check_func_decl()
//!        └─ ImplDecl → check_impl_decl()
//!              └─ methods → check_func_decl() for each
//! ```
//!
//! ## Copy vs Move Types
//!
//! TML uses ownership semantics similar to Rust:
//!
//! | Type Category     | Semantics | Examples                |
//! |-------------------|-----------|-------------------------|
//! | Primitives        | Copy      | `I32`, `Bool`, `F64`    |
//! | References        | Copy      | `ref T`, `mut ref T`    |
//! | Tuples (Copy)     | Copy      | `(I32, Bool)`           |
//! | Arrays (Copy)     | Copy      | `[I32; 5]`              |
//! | Named types       | Move      | `String`, `Vec[T]`      |
//! | Structs           | Move      | User-defined structs    |
//!
//! Copy types are implicitly duplicated on assignment. Move types transfer
//! ownership, leaving the source invalid.

#include "borrow/checker.hpp"
#include "types/env.hpp"

#include <algorithm>
#include <map>
#include <set>

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

BorrowChecker::BorrowChecker(const types::TypeEnv& type_env) : type_env_(&type_env) {}

/// Determines if a type has interior mutability.
///
/// Interior mutable types (Cell, Mutex, Shared, Sync) allow mutation through
/// shared references. This is safe because they provide their own
/// synchronization or single-threaded access patterns.
auto BorrowChecker::is_interior_mutable(const types::TypePtr& type) const -> bool {
    if (!type_env_) {
        return false; // Cannot check without type environment
    }
    return type_env_->is_interior_mutable(type);
}

/// Checks an entire module for borrow violations.
///
/// This is the main entry point for borrow checking. It iterates over all
/// top-level declarations and checks functions and impl blocks for ownership
/// and borrowing rule violations.
///
/// ## Process
///
/// 1. Clear any previous errors and reset the environment
/// 2. For each declaration in the module:
///    - `FuncDecl`: Check the function body
///    - `ImplDecl`: Check all methods in the impl block
///    - Other declarations (types, behaviors): Skip (no ownership rules)
/// 3. Return accumulated errors or success
///
/// ## Example
///
/// ```cpp
/// BorrowChecker checker;
/// auto result = checker.check_module(parsed_module);
/// if (result.is_err()) {
///     for (const auto& error : result.error()) {
///         std::cerr << error.message << std::endl;
///     }
/// }
/// ```
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

/// Determines if a type implements Copy semantics.
///
/// Copy types can be implicitly duplicated when assigned or passed to
/// functions. This is safe because copying doesn't invalidate the source.
///
/// ## Copy Type Rules
///
/// A type is Copy if and only if:
/// - It's a primitive type (`I32`, `Bool`, `F64`, etc.)
/// - It's a reference type (`ref T` or `mut ref T`) - the reference is copied,
///   not the referent
/// - It's a tuple where ALL elements are Copy
/// - It's an array where the element type is Copy
///
/// ## Move Types (non-Copy)
///
/// All other types use move semantics:
/// - Named types (structs, enums)
/// - Function types
/// - Types containing non-Copy fields
///
/// ## Example
///
/// ```tml
/// let x: I32 = 42
/// let y: I32 = x      // Copy: x is still valid
/// println(x)          // OK
///
/// let s: String = "hello"
/// let t: String = s   // Move: s is now invalid
/// println(s)          // ERROR: use of moved value
/// ```
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
            } else if constexpr (std::is_same_v<T, types::NamedType>) {
                // Check if the named type implements the Copy behavior
                if (type_env_ && type_env_->type_implements(t.name, "Copy")) {
                    return true;
                }
                return false;
            } else if constexpr (std::is_same_v<T, types::ClassType>) {
                // Check if the class type implements the Copy behavior
                if (type_env_ && type_env_->type_implements(t.name, "Copy")) {
                    return true;
                }
                return false;
            } else {
                // Function types, etc. are not Copy
                return false;
            }
        },
        type->kind);
}

/// Returns the move semantics (Copy or Move) for a given type.
///
/// This is a convenience wrapper around `is_copy_type()` that returns
/// the appropriate `MoveSemantics` enum value.
auto BorrowChecker::get_move_semantics(const types::TypePtr& type) const -> MoveSemantics {
    return is_copy_type(type) ? MoveSemantics::Copy : MoveSemantics::Move;
}

/// Checks a function declaration for borrow violations.
///
/// This method sets up the borrow checking context for a function, processes
/// parameters, checks the body, and cleans up when done.
///
/// ## Process
///
/// 1. **Push scope**: Create a new scope for the function body
/// 2. **Register parameters**: Each parameter becomes a place in the environment
///    - Extract mutability from the pattern (`mut` keyword)
///    - Parameters start in `Owned` state
/// 3. **Check body**: Recursively check the function body (if present)
/// 4. **Cleanup**: Drop all places and pop the scope
///
/// ## Parameter Handling
///
/// Parameters are registered as places that can be borrowed or moved:
///
/// ```tml
/// func example(x: I32, mut y: String) {
///     // x: immutable I32 (Copy type)
///     // y: mutable String (Move type)
///     y.push_str("!")  // OK: y is mutable
///     x = 10           // ERROR: x is not mutable
/// }
/// ```
void BorrowChecker::check_func_decl(const parser::FuncDecl& func) {
    env_.push_scope();
    current_stmt_ = 0;

    // ========================================================================
    // Explicit Lifetime Parameters (Task 8.4)
    // ========================================================================
    // Extract named lifetime parameters from generics (e.g., `life a` in `[life a, T]`)
    // These can be used to explicitly annotate reference lifetimes.

    // Clear any previous function's lifetime context
    lifetime_ctx_.clear();

    std::set<std::string> lifetime_params;
    for (const auto& generic : func.generics) {
        if (generic.is_lifetime) {
            lifetime_params.insert(generic.name);
            lifetime_ctx_.lifetime_params.insert(generic.name);
        }
    }

    // ========================================================================
    // Lifetime Elision Analysis
    // ========================================================================
    // TML infers lifetimes automatically using these rules:
    // 1. Each ref T parameter gets a separate lifetime
    // 2. If there's a this/mut this, return has same lifetime as this
    // 3. If there's exactly one ref parameter, return uses that lifetime
    //
    // If multiple ref parameters exist and no this, the return lifetime is
    // ambiguous and we emit E031 - UNLESS explicit lifetimes are provided.

    bool has_this_param = false;
    std::vector<std::string> ref_param_names;
    std::map<std::string, std::string> param_lifetimes; // param name -> lifetime name

    // Register parameters - FuncParam is a simple struct with pattern, type, span
    for (const auto& param : func.params) {
        bool is_mut = false;
        bool is_mut_ref = false;
        std::string name;

        if (param.pattern->template is<parser::IdentPattern>()) {
            const auto& ident = param.pattern->template as<parser::IdentPattern>();
            is_mut = ident.is_mut;
            name = ident.name;

            // Check for this parameter (self in method)
            if (name == "this") {
                has_this_param = true;
            }
        } else {
            name = "_param";
        }

        // Check if the parameter type is a reference (ref T or mut ref T)
        if (param.type && param.type->template is<parser::RefType>()) {
            const auto& ref_type = param.type->template as<parser::RefType>();
            is_mut_ref = ref_type.is_mut;
            // Track reference parameters for lifetime elision
            if (name != "this") {
                ref_param_names.push_back(name);
                // Track explicit lifetime annotation if present (Task 8.4)
                if (ref_type.lifetime.has_value()) {
                    param_lifetimes[name] = ref_type.lifetime.value();
                    lifetime_ctx_.param_lifetimes[name] = ref_type.lifetime.value();
                }
            }
        }

        auto loc = current_location(func.span);
        // Note: We'd need the resolved type here - using nullptr for now
        env_.define(name, nullptr, is_mut, loc, is_mut_ref);
    }

    // Check if return type is a reference and extract its lifetime
    bool returns_ref = false;
    std::optional<std::string> return_lifetime;
    if (func.return_type) {
        const auto& ret_type = *func.return_type;
        if (ret_type->template is<parser::RefType>()) {
            returns_ref = true;
            const auto& ref_type = ret_type->template as<parser::RefType>();
            return_lifetime = ref_type.lifetime;
            // Store in lifetime context for return validation (Task 8.5)
            lifetime_ctx_.return_lifetime = return_lifetime;
        }
    }

    // Apply lifetime elision rules
    if (returns_ref) {
        // Rule 2: If there's this/mut this, return uses this's lifetime - OK
        // Rule 3: If exactly one ref parameter, return uses it - OK
        // Otherwise: Ambiguous - emit E031 UNLESS explicit lifetimes resolve it

        bool lifetime_resolved = false;

        // Task 8.4: Check if explicit lifetimes resolve the ambiguity
        if (return_lifetime.has_value()) {
            const std::string& ret_lt = return_lifetime.value();
            // Check if return lifetime is a declared lifetime parameter
            if (lifetime_params.count(ret_lt) > 0) {
                // Check if at least one input ref has the same lifetime
                for (const auto& [param_name, param_lt] : param_lifetimes) {
                    if (param_lt == ret_lt) {
                        lifetime_resolved = true;
                        break;
                    }
                }
            }
        }

        if (!lifetime_resolved && !has_this_param && ref_param_names.size() > 1) {
            // Ambiguous: multiple ref params, no this, no explicit lifetime resolution
            errors_.push_back(BorrowError::ambiguous_return_lifetime(
                func.name, ref_param_names, func.span));
        }
        // Note: If no ref params at all and returns ref, the return must
        // reference a static or the function body will error on returning
        // a reference to a local. That's handled by check_return_borrows.
    }

    // Check function body - body is std::optional<BlockExpr>
    if (func.body) {
        check_block(*func.body);
    }

    // Drop all places at end of function
    drop_scope_places();
    env_.pop_scope();
}

/// Checks an impl block for borrow violations.
///
/// An impl block contains methods that operate on a type. Each method is
/// checked independently as if it were a standalone function.
///
/// ## Self Parameter Handling
///
/// Methods receive `self` (or `this` in TML) as an implicit first parameter:
///
/// ```tml
/// impl MyStruct {
///     func method(this) {        // this: ref Self (immutable borrow)
///         // ...
///     }
///
///     func mut_method(mut this) { // this: mut ref Self (mutable borrow)
///         // ...
///     }
///
///     func consume(this) {       // this: Self (takes ownership)
///         // ...
///     }
/// }
/// ```
void BorrowChecker::check_impl_decl(const parser::ImplDecl& impl) {
    for (const auto& method : impl.methods) {
        check_func_decl(method);
    }
}

} // namespace tml::borrow
