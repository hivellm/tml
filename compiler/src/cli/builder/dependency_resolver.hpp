//! # Dependency Resolver Interface
//!
//! This header defines the package dependency resolution system.
//!
//! ## Dependency Types
//!
//! | Type    | Source                  | Example                        |
//! |---------|-------------------------|--------------------------------|
//! | Path    | Local filesystem        | `{ path = "../mylib" }`        |
//! | Version | Package registry        | `"^1.2.0"` (future)            |
//! | Git     | Git repository          | `{ git = "..." }` (future)     |
//!
//! ## Resolution Process
//!
//! 1. Parse tml.toml manifest
//! 2. Resolve direct dependencies
//! 3. Resolve transitive dependencies
//! 4. Detect cycles
//! 5. Topological sort for build order
//!
//! ## Lockfile
//!
//! `tml.lock` records exact versions for reproducible builds.

#ifndef TML_CLI_DEPENDENCY_RESOLVER_HPP
#define TML_CLI_DEPENDENCY_RESOLVER_HPP

#include "cli/builder/build_config.hpp"
#include "cli/builder/rlib.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

/**
 * Resolved dependency with path and metadata
 */
struct ResolvedDependency {
    std::string name;
    std::string version;
    fs::path rlib_path;      // Path to .rlib file
    fs::path source_path;    // Path to source directory (for path deps)
    bool is_path_dependency; // true if resolved from path
    RlibMetadata metadata;   // Cached metadata from rlib

    // Transitive dependencies (names)
    std::vector<std::string> dependencies;
};

/**
 * Result of dependency resolution
 */
struct DependencyResolutionResult {
    bool success;
    std::string error_message;

    // All resolved dependencies in topological order (dependencies first)
    std::vector<ResolvedDependency> resolved;

    // Map from name to resolved dependency for quick lookup
    std::map<std::string, ResolvedDependency> by_name;
};

/**
 * Options for dependency resolution
 */
struct DependencyResolverOptions {
    bool verbose = false;
    bool offline = false;  // Don't fetch from registry
    bool update = false;   // Ignore lockfile, get latest
    fs::path cache_dir;    // Local package cache (~/.tml/cache)
    fs::path registry_url; // Package registry URL (future)
};

/**
 * Dependency resolver
 *
 * Resolves dependencies from tml.toml manifest:
 * 1. Path dependencies: Local paths (for development)
 * 2. Version dependencies: From registry (future)
 * 3. Git dependencies: From git repos (future)
 */
class DependencyResolver {
public:
    explicit DependencyResolver(const DependencyResolverOptions& options = {});

    /**
     * Resolve all dependencies for a manifest
     *
     * @param manifest The package manifest
     * @param project_root Root directory of the project
     * @return Resolution result with all resolved dependencies
     */
    DependencyResolutionResult resolve(const Manifest& manifest, const fs::path& project_root);

    /**
     * Resolve a single dependency
     *
     * @param dep Dependency specification
     * @param project_root Root directory for resolving relative paths
     * @return Resolved dependency or nullopt on error
     */
    std::optional<ResolvedDependency> resolve_single(const Dependency& dep,
                                                     const fs::path& project_root);

    /**
     * Get object files for linking from resolved dependencies
     *
     * @param resolved Resolution result
     * @param temp_dir Directory to extract objects to
     * @return List of object file paths
     */
    std::vector<fs::path> get_link_objects(const DependencyResolutionResult& resolved,
                                           const fs::path& temp_dir);

    /**
     * Get error message from last operation
     */
    std::string get_error() const {
        return error_message_;
    }

private:
    DependencyResolverOptions options_;
    std::string error_message_;

    // Resolution state
    std::set<std::string> visited_;
    std::vector<std::string> resolution_stack_;

    // Helper methods
    std::optional<ResolvedDependency> resolve_path_dependency(const Dependency& dep,
                                                              const fs::path& project_root);
    std::optional<ResolvedDependency> resolve_version_dependency(const Dependency& dep);
    std::optional<ResolvedDependency> resolve_git_dependency(const Dependency& dep);

    bool detect_cycle(const std::string& name);
    void set_error(const std::string& message);

    // Topological sort for build order
    std::vector<ResolvedDependency>
    topological_sort(const std::map<std::string, ResolvedDependency>& deps);
};

/**
 * Lockfile entry
 */
struct LockfileEntry {
    std::string name;
    std::string version;
    std::string source;        // "path", "registry", "git"
    std::string source_detail; // path, registry url, or git url
    std::string hash;          // Content hash for verification
    std::vector<std::string> dependencies;
};

/**
 * Lockfile (tml.lock)
 *
 * Records exact versions of all dependencies for reproducible builds.
 */
struct Lockfile {
    std::string version = "1";
    std::vector<LockfileEntry> packages;

    /**
     * Load lockfile from path
     */
    static std::optional<Lockfile> load(const fs::path& path);

    /**
     * Save lockfile to path
     */
    bool save(const fs::path& path) const;

    /**
     * Check if lockfile is up-to-date with manifest
     */
    bool is_compatible(const Manifest& manifest) const;

    /**
     * Find entry by name
     */
    std::optional<LockfileEntry> find(const std::string& name) const;
};

/**
 * Get default cache directory for TML packages
 *
 * @return Path to cache directory (~/.tml/cache or %USERPROFILE%\.tml\cache)
 */
fs::path get_default_cache_dir();

/**
 * Build a dependency from source if needed
 *
 * @param source_dir Source directory with tml.toml
 * @param output_dir Output directory for .rlib
 * @param options Build options
 * @return Path to built .rlib, or nullopt on error
 */
std::optional<fs::path> build_dependency(const fs::path& source_dir, const fs::path& output_dir,
                                         bool verbose = false);

} // namespace tml::cli

#endif // TML_CLI_DEPENDENCY_RESOLVER_HPP
