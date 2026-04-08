//! # PackageRegistry — workspace-driven module namespace resolution.
//!
//! Rust/Cargo-style: the root `tml.toml` declares `[workspace] members = [...]`,
//! each member has its own `tml.toml` with `[package] name = "..."`, and the
//! compiler builds a `PackageRegistry` at first use that maps namespace → source
//! directory. All prefix checks that used to be hardcoded (`"core::"`, `"std::"`,
//! `"test::"`) consult the registry instead.
//!
//! Directory layout is fixed at `<member>/src/` (Rust convention, no override).
//!
//! Usage:
//! ```cpp
//! auto& reg = PackageRegistry::instance();
//! if (auto info = reg.lookup_for_module("compiler::serial::ast")) {
//!     // info->name == "compiler"
//!     // info->src_dir == ".../compiler-tml/src"
//!     // info->rest == "serial::ast"
//! }
//! ```

#ifndef TML_PACKAGE_PACKAGE_REGISTRY_HPP
#define TML_PACKAGE_PACKAGE_REGISTRY_HPP

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace tml::pkg {

/// Information about a single workspace package.
struct PackageInfo {
    std::string name;                      ///< [package].name from tml.toml — TML namespace
    std::filesystem::path root_dir;        ///< Member directory (contains tml.toml)
    std::filesystem::path src_dir;         ///< Source root (<root_dir>/src)
    std::vector<std::string> dependencies; ///< [dependencies] names (path deps only for now)
};

/// Lookup result: the package a module_path resolves to, plus the tail.
struct PackageLookup {
    const PackageInfo* package; ///< Non-owning pointer into registry
    std::string rest;           ///< module_path with `name::` prefix stripped
};

/// Singleton, lazy-initialized. First call walks up the filesystem from CWD
/// to find a `tml.toml` with `[workspace]`, reads member manifests, and
/// populates the map. Subsequent lookups are O(log N) on the package name.
class PackageRegistry {
public:
    static PackageRegistry& instance();

    /// Resolve a fully-qualified module path (e.g. "std::collections::list")
    /// to its owning package and the tail segment. Returns std::nullopt if
    /// the prefix is not a registered package.
    std::optional<PackageLookup> lookup_for_module(const std::string& module_path) const;

    /// True iff `module_path` starts with a registered package name.
    /// Equivalent to `lookup_for_module(...).has_value()`.
    bool is_package_module(const std::string& module_path) const;

    /// Direct package lookup by name. Returns nullptr if not found.
    const PackageInfo* get(const std::string& name) const;

    /// All registered packages (for diagnostics, iteration).
    const std::map<std::string, PackageInfo>& all() const {
        return packages_;
    }

    /// Workspace root directory (the folder containing the root tml.toml
    /// with [workspace]). Empty if no workspace was found.
    const std::filesystem::path& workspace_root() const {
        return workspace_root_;
    }

    /// Force reload — primarily for tests.
    void reload();

    PackageRegistry(const PackageRegistry&) = delete;
    PackageRegistry& operator=(const PackageRegistry&) = delete;

private:
    PackageRegistry() = default;

    void ensure_loaded() const;
    void load_impl();

    mutable std::mutex load_mutex_;
    mutable bool loaded_ = false;
    std::map<std::string, PackageInfo> packages_;
    std::filesystem::path workspace_root_;
};

} // namespace tml::pkg

#endif // TML_PACKAGE_PACKAGE_REGISTRY_HPP
