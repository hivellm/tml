#include "dependency_resolver.hpp"

#include "cmd_build.hpp"
#include "rlib.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stack>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace tml::cli {

// ============================================================================
// Helper Functions
// ============================================================================

fs::path get_default_cache_dir() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        return fs::path(path) / ".tml" / "cache";
    }
    return fs::path("C:") / ".tml" / "cache";
#else
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    return fs::path(home) / ".tml" / "cache";
#endif
}

std::optional<fs::path> build_dependency(const fs::path& source_dir, const fs::path& output_dir,
                                         bool verbose) {
    // Check for tml.toml in source directory
    fs::path manifest_path = source_dir / "tml.toml";
    if (!fs::exists(manifest_path)) {
        return std::nullopt;
    }

    // Load manifest
    auto manifest = Manifest::load(manifest_path);
    if (!manifest) {
        return std::nullopt;
    }

    // Determine source file
    fs::path source_file;
    if (manifest->lib) {
        source_file = source_dir / manifest->lib->path;
    } else {
        // Try default locations
        source_file = source_dir / "src" / "lib.tml";
        if (!fs::exists(source_file)) {
            source_file = source_dir / "lib.tml";
        }
    }

    if (!fs::exists(source_file)) {
        return std::nullopt;
    }

    // Create output directory
    fs::create_directories(output_dir);

    // Build as rlib
    fs::path rlib_output = output_dir / (manifest->package.name + ".rlib");

    // Use the build system to create rlib
    int result = run_build(source_file.string(), verbose, false, false, false,
                           BuildOutputType::RlibLib, false, output_dir.string());

    if (result != 0) {
        return std::nullopt;
    }

    if (fs::exists(rlib_output)) {
        return rlib_output;
    }

    return std::nullopt;
}

// ============================================================================
// DependencyResolver Implementation
// ============================================================================

DependencyResolver::DependencyResolver(const DependencyResolverOptions& options)
    : options_(options) {
    if (options_.cache_dir.empty()) {
        options_.cache_dir = get_default_cache_dir();
    }
}

void DependencyResolver::set_error(const std::string& message) {
    error_message_ = message;
}

bool DependencyResolver::detect_cycle(const std::string& name) {
    for (const auto& dep : resolution_stack_) {
        if (dep == name) {
            // Build cycle description
            std::ostringstream oss;
            oss << "Circular dependency detected: ";
            for (const auto& d : resolution_stack_) {
                oss << d << " -> ";
            }
            oss << name;
            set_error(oss.str());
            return true;
        }
    }
    return false;
}

std::optional<ResolvedDependency>
DependencyResolver::resolve_path_dependency(const Dependency& dep, const fs::path& project_root) {

    fs::path dep_path = dep.path;

    // Handle relative paths
    if (dep_path.is_relative()) {
        dep_path = project_root / dep_path;
    }

    // Normalize path
    dep_path = fs::weakly_canonical(dep_path);

    if (!fs::exists(dep_path)) {
        set_error("Path dependency not found: " + dep_path.string());
        return std::nullopt;
    }

    // Check for tml.toml
    fs::path manifest_path = dep_path / "tml.toml";
    if (!fs::exists(manifest_path)) {
        set_error("No tml.toml found in: " + dep_path.string());
        return std::nullopt;
    }

    // Load manifest
    auto manifest = Manifest::load(manifest_path);
    if (!manifest) {
        set_error("Failed to parse tml.toml in: " + dep_path.string());
        return std::nullopt;
    }

    // Check for existing rlib
    fs::path rlib_path = dep_path / "build" / "debug" / (manifest->package.name + ".rlib");

    // Build if needed
    if (!fs::exists(rlib_path)) {
        if (options_.verbose) {
            std::cout << "Building dependency: " << dep.name << " from " << dep_path << "\n";
        }

        fs::path output_dir = dep_path / "build" / "debug";
        auto built = build_dependency(dep_path, output_dir, options_.verbose);
        if (!built) {
            set_error("Failed to build dependency: " + dep.name);
            return std::nullopt;
        }
        rlib_path = *built;
    }

    // Read rlib metadata
    auto metadata = read_rlib_metadata(rlib_path);
    if (!metadata) {
        // Create minimal metadata if rlib doesn't have it
        RlibMetadata meta;
        meta.format_version = "1.0";
        meta.library.name = manifest->package.name;
        meta.library.version = manifest->package.version;
        metadata = meta;
    }

    ResolvedDependency resolved;
    resolved.name = dep.name;
    resolved.version = manifest->package.version;
    resolved.rlib_path = rlib_path;
    resolved.source_path = dep_path;
    resolved.is_path_dependency = true;
    resolved.metadata = *metadata;

    // Collect transitive dependencies
    for (const auto& [name, _] : manifest->dependencies) {
        resolved.dependencies.push_back(name);
    }

    return resolved;
}

std::optional<ResolvedDependency>
DependencyResolver::resolve_version_dependency(const Dependency& dep) {
    // TODO: Implement registry lookup
    // For now, check local cache
    fs::path cache_path = options_.cache_dir / dep.name / dep.version / (dep.name + ".rlib");

    if (fs::exists(cache_path)) {
        auto metadata = read_rlib_metadata(cache_path);
        if (metadata) {
            ResolvedDependency resolved;
            resolved.name = dep.name;
            resolved.version = dep.version;
            resolved.rlib_path = cache_path;
            resolved.is_path_dependency = false;
            resolved.metadata = *metadata;

            for (const auto& d : metadata->dependencies) {
                resolved.dependencies.push_back(d.name);
            }

            return resolved;
        }
    }

    set_error("Version dependency not implemented yet. Use path dependencies: " + dep.name +
              " = { path = \"...\" }");
    return std::nullopt;
}

std::optional<ResolvedDependency>
DependencyResolver::resolve_git_dependency(const Dependency& dep) {
    // TODO: Implement git clone and build
    set_error("Git dependencies not implemented yet: " + dep.name);
    return std::nullopt;
}

std::optional<ResolvedDependency> DependencyResolver::resolve_single(const Dependency& dep,
                                                                     const fs::path& project_root) {

    if (dep.is_path_dependency()) {
        return resolve_path_dependency(dep, project_root);
    } else if (dep.is_version_dependency()) {
        return resolve_version_dependency(dep);
    } else if (dep.is_git_dependency()) {
        return resolve_git_dependency(dep);
    }

    set_error("Invalid dependency specification for: " + dep.name);
    return std::nullopt;
}

std::vector<ResolvedDependency>
DependencyResolver::topological_sort(const std::map<std::string, ResolvedDependency>& deps) {

    std::vector<ResolvedDependency> result;
    std::set<std::string> visited;
    std::set<std::string> in_stack;

    std::function<bool(const std::string&)> visit = [&](const std::string& name) -> bool {
        if (in_stack.count(name)) {
            // Cycle detected
            return false;
        }
        if (visited.count(name)) {
            return true;
        }

        auto it = deps.find(name);
        if (it == deps.end()) {
            return true; // Unknown dep, skip
        }

        in_stack.insert(name);

        for (const auto& child : it->second.dependencies) {
            if (!visit(child)) {
                return false;
            }
        }

        in_stack.erase(name);
        visited.insert(name);
        result.push_back(it->second);

        return true;
    };

    for (const auto& [name, _] : deps) {
        if (!visit(name)) {
            return {}; // Cycle detected
        }
    }

    return result;
}

DependencyResolutionResult DependencyResolver::resolve(const Manifest& manifest,
                                                       const fs::path& project_root) {
    DependencyResolutionResult result;
    result.success = false;

    visited_.clear();
    resolution_stack_.clear();

    // Queue of dependencies to resolve
    std::vector<std::pair<Dependency, fs::path>> to_resolve;

    // Start with direct dependencies
    for (const auto& [name, dep] : manifest.dependencies) {
        to_resolve.push_back({dep, project_root});
    }

    // Resolve all dependencies (including transitive)
    while (!to_resolve.empty()) {
        auto [dep, root] = to_resolve.back();
        to_resolve.pop_back();

        // Skip if already resolved
        if (visited_.count(dep.name)) {
            continue;
        }

        // Check for cycles
        if (detect_cycle(dep.name)) {
            result.error_message = error_message_;
            return result;
        }

        resolution_stack_.push_back(dep.name);

        // Resolve this dependency
        auto resolved = resolve_single(dep, root);
        if (!resolved) {
            result.error_message = error_message_;
            return result;
        }

        visited_.insert(dep.name);
        result.by_name[dep.name] = *resolved;

        resolution_stack_.pop_back();

        // Queue transitive dependencies
        if (resolved->is_path_dependency && fs::exists(resolved->source_path / "tml.toml")) {
            auto dep_manifest = Manifest::load(resolved->source_path / "tml.toml");
            if (dep_manifest) {
                for (const auto& [name, trans_dep] : dep_manifest->dependencies) {
                    if (!visited_.count(name)) {
                        to_resolve.push_back({trans_dep, resolved->source_path});
                    }
                }
            }
        }
    }

    // Topological sort for correct build/link order
    result.resolved = topological_sort(result.by_name);
    if (result.resolved.empty() && !result.by_name.empty()) {
        result.error_message = "Failed to sort dependencies (circular dependency?)";
        return result;
    }

    result.success = true;
    return result;
}

std::vector<fs::path>
DependencyResolver::get_link_objects(const DependencyResolutionResult& resolved,
                                     const fs::path& temp_dir) {

    std::vector<fs::path> objects;

    for (const auto& dep : resolved.resolved) {
        if (fs::exists(dep.rlib_path)) {
            // Extract objects from rlib
            auto extracted = extract_rlib_objects(dep.rlib_path, temp_dir);
            objects.insert(objects.end(), extracted.begin(), extracted.end());
        }
    }

    return objects;
}

// ============================================================================
// Lockfile Implementation
// ============================================================================

std::optional<Lockfile> Lockfile::load(const fs::path& path) {
    if (!fs::exists(path)) {
        return std::nullopt;
    }

    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    Lockfile lockfile;

    std::string line;
    LockfileEntry* current = nullptr;

    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        size_t end = line.find_last_not_of(" \t");
        if (start == std::string::npos) {
            continue;
        }
        line = line.substr(start, end - start + 1);

        // Parse version
        if (line.starts_with("version = ")) {
            lockfile.version = line.substr(11);
            // Remove quotes
            if (lockfile.version.front() == '"') {
                lockfile.version = lockfile.version.substr(1, lockfile.version.size() - 2);
            }
            continue;
        }

        // Parse [[package]] section
        if (line == "[[package]]") {
            lockfile.packages.push_back({});
            current = &lockfile.packages.back();
            continue;
        }

        if (current) {
            size_t eq = line.find(" = ");
            if (eq != std::string::npos) {
                std::string key = line.substr(0, eq);
                std::string value = line.substr(eq + 3);

                // Remove quotes
                if (value.front() == '"') {
                    value = value.substr(1, value.size() - 2);
                }

                if (key == "name") {
                    current->name = value;
                } else if (key == "version") {
                    current->version = value;
                } else if (key == "source") {
                    current->source = value;
                } else if (key == "source_detail") {
                    current->source_detail = value;
                } else if (key == "hash") {
                    current->hash = value;
                } else if (key == "dependencies") {
                    // Parse array
                    if (value.front() == '[') {
                        value = value.substr(1, value.size() - 2);
                        std::istringstream iss(value);
                        std::string dep;
                        while (std::getline(iss, dep, ',')) {
                            // Trim and remove quotes
                            size_t s = dep.find_first_not_of(" \"");
                            size_t e = dep.find_last_not_of(" \"");
                            if (s != std::string::npos) {
                                current->dependencies.push_back(dep.substr(s, e - s + 1));
                            }
                        }
                    }
                }
            }
        }
    }

    return lockfile;
}

bool Lockfile::save(const fs::path& path) const {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << "# This file is auto-generated by TML. Do not edit manually.\n";
    file << "version = \"" << version << "\"\n\n";

    for (const auto& pkg : packages) {
        file << "[[package]]\n";
        file << "name = \"" << pkg.name << "\"\n";
        file << "version = \"" << pkg.version << "\"\n";
        file << "source = \"" << pkg.source << "\"\n";
        if (!pkg.source_detail.empty()) {
            file << "source_detail = \"" << pkg.source_detail << "\"\n";
        }
        if (!pkg.hash.empty()) {
            file << "hash = \"" << pkg.hash << "\"\n";
        }
        if (!pkg.dependencies.empty()) {
            file << "dependencies = [";
            for (size_t i = 0; i < pkg.dependencies.size(); ++i) {
                if (i > 0)
                    file << ", ";
                file << "\"" << pkg.dependencies[i] << "\"";
            }
            file << "]\n";
        }
        file << "\n";
    }

    return true;
}

bool Lockfile::is_compatible(const Manifest& manifest) const {
    // Check that all manifest dependencies are in lockfile
    for (const auto& [name, dep] : manifest.dependencies) {
        auto entry = find(name);
        if (!entry) {
            return false;
        }

        // For path dependencies, check source matches
        if (dep.is_path_dependency() && entry->source != "path") {
            return false;
        }
        if (dep.is_path_dependency() && entry->source_detail != dep.path) {
            return false;
        }
    }

    return true;
}

std::optional<LockfileEntry> Lockfile::find(const std::string& name) const {
    for (const auto& pkg : packages) {
        if (pkg.name == name) {
            return pkg;
        }
    }
    return std::nullopt;
}

} // namespace tml::cli
