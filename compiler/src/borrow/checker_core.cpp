TML_MODULE("compiler")

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

#include <cstdlib>
#include <string>

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

/// Reads the staged strict-moves policy gate once from the environment.
/// `TML_STRICT_MOVES=1` (or any non-empty value other than "0"/"false") turns
/// use-after-move into hard errors (phase26f 1.2). Default OFF.
static auto read_strict_moves_env() -> bool {
    const char* v = std::getenv("TML_STRICT_MOVES");
    if (v == nullptr || v[0] == '\0') {
        return false;
    }
    std::string s(v);
    return !(s == "0" || s == "false" || s == "FALSE" || s == "off" || s == "OFF");
}

BorrowChecker::BorrowChecker() : strict_moves_(read_strict_moves_env()) {}

BorrowChecker::BorrowChecker(const types::TypeEnv& type_env)
    : type_env_(&type_env), strict_moves_(read_strict_moves_env()) {}

auto BorrowChecker::ownership_facts() const -> std::vector<PlaceOwnershipFact> {
    // Snapshot every tracked place into a codegen-facing fact keyed by its
    // definition span. `env_.all_places()` retains the end-of-function state
    // for the last checked function (move_value mutates in place; there is no
    // per-point history) — granularity (i), phase26b Step 2.
    std::vector<PlaceOwnershipFact> facts;
    const auto& places = env_.all_places();
    facts.reserve(places.size());
    for (const auto& [id, st] : places) {
        (void)id;
        PlaceOwnershipFact fact;
        fact.def_span = st.definition.span;
        fact.name = st.name;
        // Monotonic "ever fully moved" verdict: OwnershipState::Moved is the
        // terminal move state. Partial moves (moved_projections) are deferred
        // to Step 4 and intentionally not folded into moved_out here.
        fact.moved_out = (st.state == OwnershipState::Moved);
        fact.initialized = st.is_initialized;
        // phase26f 1.5: branch-dependent move → codegen guards the drop with a
        // runtime drop flag instead of static suppression.
        fact.conditionally_moved = st.conditionally_moved;
        facts.push_back(std::move(fact));
    }
    return facts;
}

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
            } else if constexpr (std::is_same_v<T, types::PtrType>) {
                // Raw pointers (`*T` / `*mut T`) are Copy, exactly as Rust
                // treats `*const T` / `*mut T`. Copying a raw pointer duplicates
                // the address only; ownership of the pointee is unaffected. This
                // removes the `ptr`/`p` reuse false-positives across allocator
                // code (blast-radius Gap 1).
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
                // A type that needs drop can never be Copy — Rust makes `Copy`
                // and `Drop` mutually exclusive so that a bit-copy can never
                // duplicate a resource whose destructor would then run twice.
                // Enforce it here even if a (malformed) `impl Copy` exists.
                if (type_env_ && type_env_->type_implements(t.name, "Drop")) {
                    return false;
                }
                // Otherwise the named type is Copy iff it derives the `Copy`
                // marker behavior (Rust-faithful: an explicit derive, not an
                // implicit all-fields-Copy rule).
                if (type_env_ && type_env_->type_implements(t.name, "Copy")) {
                    return true;
                }
                // Transparent type aliases (e.g. `Ptr = *Unit`, `Ptr[U8]`):
                // aliases are transparent in Rust, so an alias to a raw pointer
                // or other Copy type is itself Copy. Resolve the alias and
                // classify by its underlying type. This removes the residual
                // `Ptr`/`RawPtr` reuse false-positives in allocator code that
                // the raw-`PtrType` case cannot reach (the pointer arrives via
                // an alias name, not a `*T` literal type). The alias definition
                // may live in a library module (`core::ptr`) not imported by the
                // current file, so resolution consults the global library cache.
                if (type_env_) {
                    if (auto aliased = resolve_named_alias(t.name)) {
                        // Guard against a self-referential alias to avoid
                        // unbounded recursion (`type X = X`).
                        if (!(aliased->template is<types::NamedType>() &&
                              aliased->template as<types::NamedType>().name == t.name)) {
                            return is_copy_type(aliased);
                        }
                    }
                }
                return false;
            } else if constexpr (std::is_same_v<T, types::ClassType>) {
                // Same Drop/Copy exclusion as for named types.
                if (type_env_ && type_env_->type_implements(t.name, "Drop")) {
                    return false;
                }
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

/// Resolves a named type alias to its underlying type (see header). Consults,
/// in order: the memo table, the local/imported alias table, the module
/// registry, and finally the global library-module cache (which holds
/// `core::*`/`std::*` even when not explicitly imported — this is how `Ptr =
/// *Unit` is reached from a file that only imports `core::alloc`). The result
/// (including "not an alias" = null) is memoized so the global-cache scan runs
/// at most once per distinct name.
auto BorrowChecker::resolve_named_alias(const std::string& name) const -> types::TypePtr {
    auto cached = alias_resolution_cache_.find(name);
    if (cached != alias_resolution_cache_.end()) {
        return cached->second;
    }

    types::TypePtr resolved; // null = not an alias

    if (type_env_) {
        if (auto a = type_env_->lookup_type_alias(name)) {
            resolved = *a;
        }
        if (!resolved) {
            if (auto reg = type_env_->module_registry()) {
                for (const auto& [mod_path, mod] : reg->get_all_modules()) {
                    auto it = mod.type_aliases.find(name);
                    if (it != mod.type_aliases.end()) {
                        resolved = it->second;
                        break;
                    }
                }
            }
        }
        if (!resolved) {
            for (const auto& [mod_path, mod] : types::GlobalModuleCache::instance().get_all()) {
                auto it = mod.type_aliases.find(name);
                if (it != mod.type_aliases.end()) {
                    resolved = it->second;
                    break;
                }
            }
        }
    }

    alias_resolution_cache_.emplace(name, resolved);
    return resolved;
}

/// Returns the move semantics (Copy or Move) for a given type.
///
/// This is a convenience wrapper around `is_copy_type()` that returns
/// the appropriate `MoveSemantics` enum value.
auto BorrowChecker::get_move_semantics(const types::TypePtr& type) const -> MoveSemantics {
    return is_copy_type(type) ? MoveSemantics::Copy : MoveSemantics::Move;
}

/// Determines whether a bare by-value use of a value of this type is a MOVE.
///
/// Under Rust-faithful move semantics, a value moves on by-value assignment /
/// argument passing unless it is trivially copyable. `is_copy_type` already
/// captures the copyable set (primitives — including `Str`, references, and
/// tuples/arrays thereof, and named/class types that derive `Copy`). Everything
/// else — owning collections, smart pointers, and plain user structs without a
/// `Copy` derive — moves. Types that need drop are a subset of the move set.
///
/// A null type means the checker never learned this place's type (e.g. a
/// pattern binding whose resolved type was not threaded). We treat unknown as
/// copy so we never fabricate a move fact from missing information.
auto BorrowChecker::is_move_type(const types::TypePtr& type) const -> bool {
    if (!type) {
        return false;
    }
    return !is_copy_type(type);
}

/// Records a full move of a bare identifier operand when its type moves.
///
/// This is the shared entry point for every full-move site: let-init from an
/// identifier, by-value argument passing, struct-field initialization, enum
/// payload construction, and returning a local. It intentionally does nothing
/// for non-identifier operands, so method-call results (`x.duplicate()`),
/// references (`ref x`), and literals never move their subject. Method
/// receivers are never routed here.
void BorrowChecker::move_if_owned_ident(const parser::Expr& e) {
    if (!e.is<parser::IdentExpr>()) {
        return;
    }
    const auto& ident = e.as<parser::IdentExpr>();
    auto place_id = env_.lookup(ident.name);
    if (!place_id) {
        return; // Not a tracked local (function name, global, etc.)
    }
    const auto& st = env_.get_state(*place_id);
    if (!is_move_type(st.type)) {
        return; // Copy semantics — source stays valid.
    }
    move_value(*place_id, current_location(e.span));
}

/// Records a partial move of `base.field` (or a deeper projection) when the
/// projected leaf field needs drop (phase26f 1.1.3). Only fires when the base
/// is a bare identifier whose struct field type is resolvable and non-copy.
void BorrowChecker::move_if_owned_projection(const parser::Expr& e) {
    if (!e.is<parser::FieldExpr>()) {
        return;
    }
    const auto& fe = e.as<parser::FieldExpr>();
    // Only handle single-level `ident.field` projections; deeper chains are
    // conservatively skipped (recorded as signal only when resolvable).
    if (!fe.object->is<parser::IdentExpr>()) {
        return;
    }
    const auto& base_ident = fe.object->as<parser::IdentExpr>();
    auto place_id = env_.lookup(base_ident.name);
    if (!place_id) {
        return;
    }
    const auto& st = env_.get_state(*place_id);
    if (!st.type || !st.type->is<types::NamedType>()) {
        return;
    }
    if (!type_env_) {
        return;
    }
    // Resolve the field's declared type from the struct definition.
    const auto& named = st.type->as<types::NamedType>();
    auto struct_def = type_env_->lookup_struct(named.name);
    if (!struct_def) {
        return;
    }
    types::TypePtr field_type;
    for (const auto& fld : struct_def->fields) {
        if (fld.name == fe.field) {
            field_type = fld.type;
            break;
        }
    }
    if (!is_move_type(field_type)) {
        return; // Field is copy (or unknown) — no partial move.
    }
    std::vector<Projection> projections;
    projections.push_back(Projection{
        .kind = ProjectionKind::Field,
        .field_name = fe.field,
        .index_value = std::nullopt,
    });
    move_projection(*place_id, projections, current_location(e.span));
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
        // phase26f: resolved parameter type, threaded into the place so that
        // by-value params classify as move sources and ref params do not. The
        // type checker recorded it against the param's IdentPattern node in
        // bind_pattern (types/checker/core.cpp check_func_body).
        types::TypePtr param_type;

        if (param.pattern->template is<parser::IdentPattern>()) {
            const auto& ident = param.pattern->template as<parser::IdentPattern>();
            is_mut = ident.is_mut;
            name = ident.name;
            if (type_env_) {
                param_type = type_env_->get_expr_type(&ident);
            }

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
        env_.define(name, param_type, is_mut, loc, is_mut_ref);
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
            errors_.push_back(
                BorrowError::ambiguous_return_lifetime(func.name, ref_param_names, func.span));
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
