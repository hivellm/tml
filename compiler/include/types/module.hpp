//! # Module System
//!
//! This module defines the module representation and registry for TML.
//! Modules are the unit of compilation and namespace organization.
//!
//! ## Module Structure
//!
//! A module contains:
//! - Functions
//! - Structs and enums
//! - Behaviors (traits)
//! - Type aliases
//! - Submodules
//! - Re-exports
//!
//! ## Module Paths
//!
//! Modules are identified by paths like `std::io::File`. The path separator
//! is `::` and maps to the file system directory structure.
//!
//! ## Re-exports
//!
//! Modules can re-export symbols from other modules using `pub use`:
//!
//! ```tml
//! pub use core::iter::Iterator    // Re-export single symbol
//! pub use core::ops::*            // Re-export all public symbols
//! ```

#ifndef TML_TYPES_MODULE_HPP
#define TML_TYPES_MODULE_HPP

#include "parser/ast.hpp"
#include "types/env_stability.hpp"
#include "types/type.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace tml::types {

// Forward declarations
struct FuncSig;
struct StructDef;
struct EnumDef;
struct BehaviorDef;

/// Information about a re-exported symbol.
struct ReExport {
    std::string source_path;          ///< Full source module path.
    bool is_glob = false;             ///< True for glob imports (`pub use foo::*`).
    std::vector<std::string> symbols; ///< Specific symbols (for `pub use foo::{a, b}`).
    std::optional<std::string> alias; ///< Optional alias (`pub use foo as bar`).
};

/// A TML module with its symbols and metadata.
struct Module {
    std::string name;      ///< Module name.
    std::string file_path; ///< Source file location.

    // Symbol tables
    std::unordered_map<std::string, FuncSig> functions;      ///< Function definitions.
    std::unordered_map<std::string, StructDef> structs;      ///< Struct definitions.
    std::unordered_map<std::string, EnumDef> enums;          ///< Enum definitions.
    std::unordered_map<std::string, BehaviorDef> behaviors;  ///< Behavior definitions.
    std::unordered_map<std::string, TypePtr> type_aliases;   ///< Type aliases.
    std::unordered_map<std::string, std::string> submodules; ///< Submodule name -> path.
    std::unordered_map<std::string, std::string> constants;  ///< Constants name -> value.

    std::vector<ReExport> re_exports; ///< Re-exported symbols.

    std::string source_code;             ///< Source for pure TML modules.
    bool has_pure_tml_functions = false; ///< True if module has non-extern functions.

    parser::Visibility default_visibility = parser::Visibility::Private; ///< Default visibility.
};

/// Information about an imported symbol.
struct ImportedSymbol {
    std::string original_name;     ///< Name in source module.
    std::string local_name;        ///< Name in current scope (after `as`).
    std::string module_path;       ///< Full module path.
    parser::Visibility visibility; ///< Import visibility.
};

/// Union type for any symbol that can be imported.
using ModuleSymbol = std::variant<FuncSig, StructDef, EnumDef, BehaviorDef, TypePtr>;

/// Central registry for all modules in the program.
///
/// Manages module registration, lookup, and symbol resolution across modules.
class ModuleRegistry {
public:
    ModuleRegistry() = default;

    /// Registers a module at the given path.
    void register_module(const std::string& path, Module module);

    /// Gets a module by path.
    auto get_module(const std::string& path) const -> std::optional<Module>;

    /// Gets a mutable pointer to a module.
    auto get_module_mut(const std::string& path) -> Module*;

    /// Returns true if the module exists.
    bool has_module(const std::string& path) const;

    /// Lists all registered module paths.
    auto list_modules() const -> std::vector<std::string>;

    /// Returns all registered modules.
    auto get_all_modules() const -> const std::unordered_map<std::string, Module>& {
        return modules_;
    }

    /// Resolves a file path to a module path.
    auto resolve_file_module(const std::string& path) const -> std::optional<std::string>;

    /// Registers a file to module path mapping.
    void register_file_mapping(const std::string& file_path, const std::string& module_path);

    // Symbol lookup across modules

    /// Looks up a function in a module.
    auto lookup_function(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<FuncSig>;

    /// Looks up a struct in a module.
    auto lookup_struct(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<StructDef>;

    /// Looks up an enum in a module.
    auto lookup_enum(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<EnumDef>;

    /// Looks up a behavior in a module.
    auto lookup_behavior(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<BehaviorDef>;

    /// Looks up a type alias in a module.
    auto lookup_type_alias(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<TypePtr>;

    /// Looks up any symbol in a module.
    auto lookup_symbol(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<ModuleSymbol>;

private:
    std::unordered_map<std::string, Module> modules_;             ///< Registered modules.
    std::unordered_map<std::string, std::string> file_to_module_; ///< File to module mapping.
};

} // namespace tml::types

#endif // TML_TYPES_MODULE_HPP
