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

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
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
struct ClassDef;
struct InterfaceDef;

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
    std::unordered_map<std::string, FuncSig> functions;          ///< Function definitions.
    std::unordered_map<std::string, StructDef> structs;          ///< Public struct definitions.
    std::unordered_map<std::string, StructDef> internal_structs; ///< Internal struct definitions.
    std::unordered_map<std::string, EnumDef> enums;              ///< Enum definitions.
    std::unordered_map<std::string, BehaviorDef> behaviors;      ///< Behavior definitions.
    std::unordered_map<std::string, TypePtr> type_aliases;       ///< Type aliases.
    std::unordered_map<std::string, std::vector<std::string>>
        type_alias_generics;                                 ///< Generic params for type aliases.
    std::unordered_map<std::string, std::string> submodules; ///< Submodule name -> path.
    /// Constant info with value and type.
    struct ConstantInfo {
        std::string value;    ///< The constant value as string.
        std::string tml_type; ///< The TML type name (e.g., "I32", "I64").
    };
    std::unordered_map<std::string, ConstantInfo> constants;  ///< Constants name -> info.
    std::unordered_map<std::string, ClassDef> classes;        ///< Class definitions.
    std::unordered_map<std::string, InterfaceDef> interfaces; ///< Interface definitions.

    std::vector<ReExport> re_exports; ///< Re-exported symbols.

    std::vector<std::string> private_imports; ///< Module paths from private use declarations.

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

// ============================================================================
// Global Module Cache
// ============================================================================
// Thread-safe global cache for pre-parsed library modules (core::*, std::*, test).
// This cache persists across all test file compilations to avoid re-parsing
// the same library modules for every test file.

/// Global cache for pre-parsed library modules.
/// Thread-safe singleton that stores Module structs for library modules.
class GlobalModuleCache {
public:
    /// Get the singleton instance.
    static GlobalModuleCache& instance();

    /// Check if a module is cached.
    bool has(const std::string& module_path) const;

    /// Get a cached module (returns nullopt if not cached).
    std::optional<Module> get(const std::string& module_path) const;

    /// Cache a module (only caches library modules: core::*, std::*, test).
    void put(const std::string& module_path, const Module& module);

    /// Clear the cache (e.g., for --no-cache flag).
    void clear();

    /// Get cache statistics.
    struct Stats {
        size_t total_entries = 0;
        size_t cache_hits = 0;
        size_t cache_misses = 0;
    };
    Stats get_stats() const;

    /// Check if a module path should be cached (library modules only).
    static bool should_cache(const std::string& module_path);

private:
    GlobalModuleCache() = default;
    ~GlobalModuleCache() = default;

    // Non-copyable
    GlobalModuleCache(const GlobalModuleCache&) = delete;
    GlobalModuleCache& operator=(const GlobalModuleCache&) = delete;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Module> cache_;
    mutable std::atomic<size_t> hits_{0};
    mutable std::atomic<size_t> misses_{0};
};

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

    /// Looks up generic parameter names for a type alias in a module.
    auto lookup_type_alias_generics(const std::string& module_path,
                                    const std::string& symbol_name) const
        -> std::optional<std::vector<std::string>>;

    /// Looks up a constant in a module.
    auto lookup_constant(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<std::string>;

    /// Looks up a class in a module.
    auto lookup_class(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<ClassDef>;

    /// Looks up an interface in a module.
    auto lookup_interface(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<InterfaceDef>;

    /// Looks up any symbol in a module.
    auto lookup_symbol(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<ModuleSymbol>;

    /// Creates a deep copy of this registry.
    /// Used to pre-populate registries with commonly-imported modules.
    [[nodiscard]] ModuleRegistry clone() const;

private:
    std::unordered_map<std::string, Module> modules_;             ///< Registered modules.
    std::unordered_map<std::string, std::string> file_to_module_; ///< File to module mapping.

    /// Internal helper for lookup_enum that follows re-exports.
    auto lookup_enum_impl(const std::string& module_path, const std::string& symbol_name,
                          std::unordered_set<std::string>& visited) const -> std::optional<EnumDef>;

    /// Internal helper for lookup_constant that follows re-exports.
    auto lookup_constant_impl(const std::string& module_path, const std::string& symbol_name,
                              std::unordered_set<std::string>& visited) const
        -> std::optional<std::string>;
};

} // namespace tml::types

#endif // TML_TYPES_MODULE_HPP
