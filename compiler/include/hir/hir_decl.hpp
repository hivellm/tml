//! # HIR Declarations
//!
//! This module defines top-level declaration types for the HIR: functions, structs,
//! enums, behaviors (traits), and implementation blocks.
//!
//! ## Overview
//!
//! Declarations are the building blocks of TML modules. Each declaration type
//! represents a different kind of top-level item:
//!
//! | Declaration | TML Syntax | Description |
//! |-------------|------------|-------------|
//! | `HirFunction` | `func foo() -> I32` | Function definition |
//! | `HirStruct` | `type Point { x: I32 }` | Struct (product type) |
//! | `HirEnum` | `type Maybe { Just(T) }` | Enum (sum type) |
//! | `HirBehavior` | `behavior Display { ... }` | Trait definition |
//! | `HirImpl` | `impl Display for Point` | Implementation block |
//! | `HirConst` | `const PI: F64 = 3.14` | Compile-time constant |
//!
//! ## Monomorphization
//!
//! All generic types and functions in HIR are fully monomorphized. This means:
//! - No generic type parameters remain
//! - Each instantiation becomes a separate declaration
//! - Declarations have `mangled_name` fields for unique identification
//!
//! For example, `Vec[I32]` and `Vec[Bool]` become two separate `HirStruct`
//! declarations with different mangled names.
//!
//! ## Name Mangling
//!
//! The `mangled_name` field contains the unique name for each monomorphized
//! instance:
//! - `Vec` + `[I32]` → `Vec__I32`
//! - `map` + `[I32, Str]` → `map__I32_Str`
//!
//! For non-generic declarations, `mangled_name` equals `name`.
//!
//! ## See Also
//!
//! - `docs/specs/31-HIR.md` - Complete HIR documentation
//! - `hir_module.hpp` - Module container for declarations
//! - `hir_builder.hpp` - AST to HIR lowering

#pragma once

#include "hir/hir_expr.hpp"
#include "hir/hir_id.hpp"

namespace tml::hir {

// ============================================================================
// Function Declarations
// ============================================================================

/// A function parameter.
///
/// Represents a single parameter in a function signature.
///
/// ## Fields
/// - `name`: Parameter name (may be `_` for unused parameters)
/// - `type`: Parameter type (fully resolved)
/// - `is_mut`: Whether the parameter is mutable within the function body
/// - `span`: Source location
///
/// ## The `this` Parameter
///
/// For methods, the first parameter is typically `this` (self reference):
/// ```tml
/// impl Point {
///     func distance(this) -> F64 { ... }      // Immutable self
///     func move_by(mut this, dx: I32) { ... } // Mutable self
/// }
/// ```
struct HirParam {
    std::string name;
    HirType type;
    bool is_mut;
    SourceSpan span;
};

/// A function declaration in HIR.
///
/// Represents a function definition, including both regular functions and
/// methods within impl blocks. Extern functions have no body.
///
/// ## Fields
/// - `id`: Unique identifier for this declaration
/// - `name`: Original function name
/// - `mangled_name`: Unique name after monomorphization
/// - `params`: Function parameters
/// - `return_type`: Return type (unit `()` if not specified)
/// - `body`: Function body expression (None for extern functions)
/// - `is_public`: Whether the function is exported from the module
/// - `is_async`: Whether the function is asynchronous
/// - `is_extern`: Whether this is an external function (FFI)
/// - `extern_abi`: ABI for extern functions (e.g., "C")
/// - `attributes`: Compiler attributes (@inline, @noinline, etc.)
/// - `span`: Source location
///
/// ## Function Kinds
///
/// | Kind | `body` | `is_extern` | Description |
/// |------|--------|-------------|-------------|
/// | Regular | Some | false | Normal function with implementation |
/// | Extern | None | true | External function (FFI) |
/// | Abstract | None | false | Behavior method without default impl |
///
/// ## Example
/// ```tml
/// @inline
/// pub func add(a: I32, b: I32) -> I32 {
///     return a + b
/// }
/// ```
/// Becomes:
/// ```cpp
/// HirFunction {
///     name: "add",
///     mangled_name: "add",
///     params: [{a, I32}, {b, I32}],
///     return_type: I32,
///     body: HirReturnExpr(...),
///     is_public: true,
///     attributes: ["inline"]
/// }
/// ```
struct HirFunction {
    HirId id;
    std::string name;
    std::string mangled_name;
    std::vector<HirParam> params;
    HirType return_type;
    std::optional<HirExprPtr> body;
    bool is_public;
    bool is_async;
    bool is_extern;
    std::optional<std::string> extern_abi;
    std::vector<std::string> attributes;
    SourceSpan span;
};

// ============================================================================
// Type Declarations
// ============================================================================

/// A struct field.
///
/// Represents a single field in a struct definition.
///
/// ## Fields
/// - `name`: Field name
/// - `type`: Field type (fully resolved)
/// - `is_public`: Whether the field is accessible outside the module
/// - `span`: Source location
///
/// ## Field Visibility
///
/// Fields can have different visibility than the struct itself:
/// ```tml
/// pub type User {
///     pub name: Str,    // Public field
///     password: Str,    // Private field (default)
/// }
/// ```
struct HirField {
    std::string name;
    HirType type;
    bool is_public;
    SourceSpan span;
};

/// A struct declaration in HIR.
///
/// Represents a struct (product type) definition. Structs contain named fields
/// and are always stored by value.
///
/// ## Fields
/// - `id`: Unique identifier for this declaration
/// - `name`: Original struct name
/// - `mangled_name`: Unique name after monomorphization
/// - `fields`: List of struct fields (in declaration order)
/// - `is_public`: Whether the struct is exported from the module
/// - `span`: Source location
///
/// ## Field Order
///
/// Fields maintain their declaration order. The `field_index` in
/// `HirFieldExpr` corresponds to the position in this list.
///
/// ## Example
/// ```tml
/// pub type Point {
///     x: I32,
///     y: I32
/// }
/// ```
/// Becomes:
/// ```cpp
/// HirStruct {
///     name: "Point",
///     mangled_name: "Point",
///     fields: [{x, I32}, {y, I32}],
///     is_public: true
/// }
/// ```
struct HirStruct {
    HirId id;
    std::string name;
    std::string mangled_name;
    std::vector<HirField> fields;
    bool is_public;
    SourceSpan span;
};

/// An enum variant.
///
/// Represents a single variant in an enum definition. Variants may have
/// associated data (payload).
///
/// ## Fields
/// - `name`: Variant name
/// - `index`: Zero-based variant index (discriminant)
/// - `payload_types`: Types of associated data (empty for unit variants)
/// - `span`: Source location
///
/// ## Variant Kinds
///
/// | Kind | payload_types | Example |
/// |------|---------------|---------|
/// | Unit | [] | `Nothing` |
/// | Tuple | [T, U, ...] | `Just(I32)`, `Pair(I32, Str)` |
///
/// ## Index Assignment
///
/// Indices are assigned in declaration order:
/// ```tml
/// type Color { Red, Green, Blue }  // Red=0, Green=1, Blue=2
/// ```
struct HirVariant {
    std::string name;
    int index;
    std::vector<HirType> payload_types;
    SourceSpan span;
};

/// An enum declaration in HIR.
///
/// Represents an enum (sum type) definition. Enums define a closed set of
/// variants, each potentially with associated data.
///
/// ## Fields
/// - `id`: Unique identifier for this declaration
/// - `name`: Original enum name
/// - `mangled_name`: Unique name after monomorphization
/// - `variants`: List of enum variants (in declaration order)
/// - `is_public`: Whether the enum is exported from the module
/// - `span`: Source location
///
/// ## Variant Order
///
/// Variants maintain their declaration order. The `variant_index` in
/// `HirEnumExpr` and `HirEnumPattern` corresponds to the position in this list.
///
/// ## Example
/// ```tml
/// pub type Maybe[T] {
///     Just(T),
///     Nothing
/// }
/// ```
/// After monomorphization with `I32`:
/// ```cpp
/// HirEnum {
///     name: "Maybe",
///     mangled_name: "Maybe__I32",
///     variants: [{Just, 0, [I32]}, {Nothing, 1, []}],
///     is_public: true
/// }
/// ```
struct HirEnum {
    HirId id;
    std::string name;
    std::string mangled_name;
    std::vector<HirVariant> variants;
    bool is_public;
    SourceSpan span;
};

// ============================================================================
// Behavior Declarations
// ============================================================================

/// A behavior (trait) method signature.
///
/// Represents a method declared in a behavior, optionally with a default
/// implementation.
///
/// ## Fields
/// - `name`: Method name
/// - `params`: Method parameters (including `this`)
/// - `return_type`: Return type
/// - `has_default_impl`: Whether a default implementation is provided
/// - `default_body`: The default implementation (if any)
/// - `span`: Source location
///
/// ## Required vs Provided Methods
///
/// - **Required** (`has_default_impl = false`): Implementors must provide
/// - **Provided** (`has_default_impl = true`): Has default, can be overridden
///
/// ## Example
/// ```tml
/// behavior Display {
///     func display(this) -> Str          // Required
///     func debug(this) -> Str {          // Provided (default)
///         return this.display()
///     }
/// }
/// ```
struct HirBehaviorMethod {
    std::string name;
    std::vector<HirParam> params;
    HirType return_type;
    bool has_default_impl;
    std::optional<HirExprPtr> default_body;
    SourceSpan span;
};

/// A behavior (trait) declaration in HIR.
///
/// Represents a behavior (TML's term for traits) that defines a set of
/// methods types can implement.
///
/// ## Fields
/// - `id`: Unique identifier for this declaration
/// - `name`: Behavior name
/// - `methods`: Method signatures (required and provided)
/// - `super_behaviors`: Parent behaviors this behavior extends
/// - `is_public`: Whether the behavior is exported
/// - `span`: Source location
///
/// ## Behavior Inheritance
///
/// Behaviors can extend other behaviors:
/// ```tml
/// behavior Ordered: Comparable {
///     func compare(this, other: ref This) -> Ordering
/// }
/// ```
/// Here, `Comparable` is in `super_behaviors`.
///
/// ## Example
/// ```tml
/// pub behavior Hashable {
///     func hash(this) -> U64
/// }
/// ```
/// Becomes:
/// ```cpp
/// HirBehavior {
///     name: "Hashable",
///     methods: [{hash, [this], U64, false, None}],
///     super_behaviors: [],
///     is_public: true
/// }
/// ```
struct HirBehavior {
    HirId id;
    std::string name;
    std::vector<HirBehaviorMethod> methods;
    std::vector<std::string> super_behaviors;
    bool is_public;
    SourceSpan span;
};

// ============================================================================
// Impl Blocks
// ============================================================================

/// An impl block in HIR.
///
/// Represents an implementation block that provides methods for a type,
/// either as inherent methods or as a behavior implementation.
///
/// ## Fields
/// - `id`: Unique identifier for this declaration
/// - `behavior_name`: Behavior being implemented (None for inherent impls)
/// - `type_name`: Name of the implementing type
/// - `self_type`: Full type being implemented (including type args)
/// - `methods`: Method implementations
/// - `span`: Source location
///
/// ## Impl Kinds
///
/// | Kind | `behavior_name` | Description |
/// |------|-----------------|-------------|
/// | Inherent | None | Methods directly on a type |
/// | Trait | Some("Display") | Implementing a behavior |
///
/// ## Example: Inherent Impl
/// ```tml
/// impl Point {
///     func new(x: I32, y: I32) -> Point { ... }
///     func distance(this, other: Point) -> F64 { ... }
/// }
/// ```
///
/// ## Example: Trait Impl
/// ```tml
/// impl Display for Point {
///     func display(this) -> Str { ... }
/// }
/// ```
struct HirImpl {
    HirId id;
    std::optional<std::string> behavior_name;
    std::string type_name;
    HirType self_type;
    std::vector<HirFunction> methods;
    SourceSpan span;
};

// ============================================================================
// Constants
// ============================================================================

/// A constant declaration in HIR.
///
/// Represents a compile-time constant value. Constants are evaluated at
/// compile time and inlined at use sites.
///
/// ## Fields
/// - `id`: Unique identifier for this declaration
/// - `name`: Constant name (conventionally SCREAMING_SNAKE_CASE)
/// - `type`: Constant type
/// - `value`: Constant value expression (must be compile-time evaluable)
/// - `is_public`: Whether the constant is exported
/// - `span`: Source location
///
/// ## Compile-Time Evaluation
///
/// The `value` expression must be evaluable at compile time:
/// - Literals
/// - Const function calls
/// - Arithmetic on other constants
///
/// ## Example
/// ```tml
/// pub const MAX_SIZE: U64 = 1024 * 1024
/// const PI: F64 = 3.14159265358979
/// ```
struct HirConst {
    HirId id;
    std::string name;
    HirType type;
    HirExprPtr value;
    bool is_public;
    SourceSpan span;
};

} // namespace tml::hir
