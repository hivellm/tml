//! # HIR Module
//!
//! This module defines the `HirModule` container - the top-level compilation unit
//! that holds all declarations after lowering from AST.
//!
//! ## Overview
//!
//! An `HirModule` represents a complete TML source file (or logical module) after
//! it has been type-checked and lowered to HIR. It contains all the declarations
//! organized by category for efficient traversal.
//!
//! ## Module Contents
//!
//! A module contains:
//! - **Type definitions**: Structs and enums (monomorphized)
//! - **Behaviors**: Trait definitions
//! - **Implementations**: Impl blocks for types
//! - **Functions**: Top-level and associated functions (monomorphized)
//! - **Constants**: Compile-time constant values
//! - **Imports**: External module dependencies
//!
//! ## Storage Organization
//!
//! Items are stored in separate vectors by category, enabling efficient
//! iteration over specific kinds without filtering:
//!
//! ```cpp
//! // Iterate only over structs
//! for (const auto& s : module.structs) {
//!     process_struct(s);
//! }
//!
//! // Iterate only over functions
//! for (const auto& f : module.functions) {
//!     codegen_function(f);
//! }
//! ```
//!
//! ## Lookup Operations
//!
//! The module provides O(n) lookup methods for finding declarations by name.
//! For performance-critical code that needs frequent lookups, consider building
//! an index map externally.
//!
//! ## Monomorphization
//!
//! All generic items are monomorphized before storage. A single generic type
//! like `Vec[T]` becomes multiple `HirStruct` entries:
//! - `Vec__I32` (if `Vec[I32]` is used)
//! - `Vec__Str` (if `Vec[Str]` is used)
//! - etc.
//!
//! ## See Also
//!
//! - `docs/specs/31-HIR.md` - Complete HIR documentation
//! - `hir_decl.hpp` - Declaration types stored in modules
//! - `hir_builder.hpp` - Builds HirModule from AST

#pragma once

#include "hir/hir_decl.hpp"
#include "hir/hir_id.hpp"

#include <string>
#include <vector>

namespace tml::hir {

// ============================================================================
// HIR Module
// ============================================================================

/// A complete HIR module (compilation unit).
///
/// Represents a single TML source file after lowering to HIR. Contains all
/// declarations organized by category.
///
/// ## Fields
/// - `name`: Module name (typically derived from filename)
/// - `source_path`: Path to the original source file
/// - `structs`: Struct definitions (monomorphized)
/// - `enums`: Enum definitions (monomorphized)
/// - `behaviors`: Behavior (trait) definitions
/// - `impls`: Implementation blocks
/// - `functions`: Function definitions (monomorphized)
/// - `constants`: Constant definitions
/// - `imports`: Names of imported modules
///
/// ## Example Usage
///
/// ```cpp
/// HirModule module = builder.lower_module(ast_module);
///
/// // Process all structs
/// for (const auto& s : module.structs) {
///     emit_struct_type(s);
/// }
///
/// // Find a specific function
/// if (const auto* main = module.find_function("main")) {
///     codegen_function(*main);
/// }
/// ```
///
/// ## Iteration Order
///
/// Items within each category maintain their source declaration order.
/// This can be important for:
/// - Reproducible output
/// - Dependency ordering (types before functions using them)
/// - Debug information
struct HirModule {
    /// Module name (e.g., "main", "utils")
    std::string name;

    /// Path to the source file this module was built from
    std::string source_path;

    /// Struct definitions (product types)
    ///
    /// Each generic struct instantiation becomes a separate entry:
    /// `Vec[I32]` → `HirStruct { mangled_name: "Vec__I32", ... }`
    std::vector<HirStruct> structs;

    /// Enum definitions (sum types)
    ///
    /// Each generic enum instantiation becomes a separate entry:
    /// `Maybe[I32]` → `HirEnum { mangled_name: "Maybe__I32", ... }`
    std::vector<HirEnum> enums;

    /// Behavior (trait) definitions
    ///
    /// Behaviors are not monomorphized; they define interfaces.
    std::vector<HirBehavior> behaviors;

    /// Implementation blocks
    ///
    /// Includes both inherent impls and trait impls.
    /// Methods within impls are monomorphized.
    std::vector<HirImpl> impls;

    /// Function definitions
    ///
    /// Includes top-level functions. Methods are stored in their
    /// respective `HirImpl` blocks.
    std::vector<HirFunction> functions;

    /// Constant definitions
    ///
    /// Compile-time constant values defined at module scope.
    std::vector<HirConst> constants;

    /// Imported module names
    ///
    /// List of modules this module depends on.
    /// Example: `["std.io", "std.collections"]`
    std::vector<std::string> imports;

    // ========================================================================
    // Lookup Methods
    // ========================================================================

    /// Find a struct by name.
    ///
    /// Searches for a struct with the given name or mangled name.
    ///
    /// @param name The struct name to find
    /// @return Pointer to the struct if found, nullptr otherwise
    ///
    /// ## Example
    /// ```cpp
    /// if (const auto* point = module.find_struct("Point")) {
    ///     for (const auto& field : point->fields) {
    ///         // ...
    ///     }
    /// }
    /// ```
    [[nodiscard]] auto find_struct(const std::string& name) const -> const HirStruct*;

    /// Find an enum by name.
    ///
    /// Searches for an enum with the given name or mangled name.
    ///
    /// @param name The enum name to find
    /// @return Pointer to the enum if found, nullptr otherwise
    [[nodiscard]] auto find_enum(const std::string& name) const -> const HirEnum*;

    /// Find a function by name.
    ///
    /// Searches for a function with the given name or mangled name.
    /// Does not search methods within impl blocks.
    ///
    /// @param name The function name to find
    /// @return Pointer to the function if found, nullptr otherwise
    ///
    /// ## Example
    /// ```cpp
    /// if (const auto* main = module.find_function("main")) {
    ///     if (main->body) {
    ///         codegen_function(*main);
    ///     }
    /// }
    /// ```
    [[nodiscard]] auto find_function(const std::string& name) const -> const HirFunction*;

    /// Find a constant by name.
    ///
    /// Searches for a constant with the given name.
    ///
    /// @param name The constant name to find
    /// @return Pointer to the constant if found, nullptr otherwise
    [[nodiscard]] auto find_const(const std::string& name) const -> const HirConst*;
};

} // namespace tml::hir
