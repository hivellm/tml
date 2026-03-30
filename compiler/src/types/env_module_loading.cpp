TML_MODULE("compiler")

//! # Type Environment - Module Loading
//!
//! This file implements native module loading and filesystem resolution.
//!
//! `load_native_module()` resolves module paths (e.g., "core::str", "std::io")
//! to filesystem paths and loads them via `load_module_from_file()`.
//!
//! ## Path Resolution
//!
//! Module paths are resolved relative to:
//! - Current file directory (local modules)
//! - Library search paths (lib/core, lib/std, lib/test, lib/backtrace)
//!
//! ## Caching
//!
//! Resolved filesystem paths are cached to avoid repeated filesystem probing.
//! A global module cache and binary metadata cache (.tml.meta) provide
//! fast loading for library modules across compilation units.

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"
#include "types/env.hpp"
#include "types/module.hpp"
#include "types/module_binary.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <shared_mutex>

namespace tml::types {

// ============================================================================
// Module Path Resolution Cache
// ============================================================================
// Caches resolved filesystem paths for module paths to avoid repeated
// filesystem probing (each module tries 10-12 paths before finding the file).
// This is critical for performance: without caching, importing std::thread
// triggers ~1365 fs::exists() calls across ~40 transitive module dependencies.

#ifdef _WIN32
// On Windows (NTFS/FAT), std::filesystem::exists() is case-insensitive.
// This causes "List.tml" to match "list.tml", making the module resolver
// treat "std::collections::List" as a module file instead of a symbol import
// from "std::collections". That skips loading the parent module and its sibling
// files (e.g., behaviors.tml containing ListIter[T]).
// This helper verifies the filename matches case-sensitively.
static bool exists_case_sensitive(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path))
        return false;
    std::string expected = path.filename().string();
    auto parent = path.parent_path();
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(parent, ec)) {
        if (entry.path().filename().string() == expected) {
            return true;
        }
    }
    return false;
}
#else
static bool exists_case_sensitive(const std::filesystem::path& path) {
    return std::filesystem::exists(path);
}
#endif

static std::shared_mutex s_path_cache_mutex;
static std::unordered_map<std::string, std::string> s_resolved_paths; // module_path -> fs_path
static std::unordered_set<std::string> s_not_found_paths;             // module_path -> not found

// Cached library root directory (determined once, reused for all lookups)
static std::mutex s_lib_root_mutex;
static std::string s_lib_root; // e.g., "F:/Node/hivellm/tml/lib"
static bool s_lib_root_resolved = false;

static std::string find_lib_root() {
    std::lock_guard<std::mutex> lock(s_lib_root_mutex);
    if (s_lib_root_resolved) {
        return s_lib_root;
    }
    s_lib_root_resolved = true;

    auto cwd = std::filesystem::current_path();

    // Try common locations in order of likelihood
    std::vector<std::filesystem::path> candidates = {
        cwd / "lib",                                      // Running from project root
        std::filesystem::path("lib"),                     // Relative to CWD
        std::filesystem::path("F:/Node/hivellm/tml/lib"), // Hardcoded fallback
        cwd.parent_path() / "lib",                        // Running from build/
        cwd.parent_path().parent_path() / "lib",          // Running from build/debug/
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate / "core" / "src") &&
            std::filesystem::exists(candidate / "std" / "src")) {
            s_lib_root = std::filesystem::canonical(candidate).string();
            TML_DEBUG_LN("[MODULE] Library root resolved: " << s_lib_root);
            return s_lib_root;
        }
    }

    // Fallback: empty string means use old search behavior
    TML_DEBUG_LN("[MODULE] WARNING: Could not determine library root");
    return "";
}

// Look up a resolved path from the cache. Returns empty string on miss.
static std::string get_cached_path(const std::string& module_path) {
    std::shared_lock<std::shared_mutex> lock(s_path_cache_mutex);
    auto it = s_resolved_paths.find(module_path);
    if (it != s_resolved_paths.end()) {
        return it->second;
    }
    return "";
}

// Check if a module is known to not exist on disk.
static bool is_known_not_found(const std::string& module_path) {
    std::shared_lock<std::shared_mutex> lock(s_path_cache_mutex);
    return s_not_found_paths.count(module_path) > 0;
}

// Cache a successful resolution.
static void cache_resolved_path(const std::string& module_path, const std::string& fs_path) {
    std::unique_lock<std::shared_mutex> lock(s_path_cache_mutex);
    s_resolved_paths[module_path] = fs_path;
}

// Cache a failed resolution (module doesn't exist on disk).
static void cache_not_found(const std::string& module_path) {
    std::unique_lock<std::shared_mutex> lock(s_path_cache_mutex);
    s_not_found_paths.insert(module_path);
}

// Resolve a module path to a filesystem path using the cached library root.
// Returns the resolved path or empty string if not found.
// lib_subdir is "core", "std", "backtrace", or "test".
static std::string resolve_lib_module_path(const std::string& lib_subdir,
                                           const std::string& src_subdir,
                                           const std::string& fs_module_path) {
    const std::string& lib_root = find_lib_root();
    if (lib_root.empty()) {
        return ""; // Fallback to old behavior
    }

    namespace fs = std::filesystem;
    fs::path base = fs::path(lib_root) / lib_subdir / src_subdir;

    // Try name.tml first, then name/mod.tml
    // Use case-sensitive check to prevent Windows NTFS from matching
    // "List.tml" to "list.tml" (which would skip parent module loading).
    fs::path candidate1 = base / (fs_module_path + ".tml");
    if (exists_case_sensitive(candidate1)) {
        return candidate1.string();
    }

    fs::path candidate2 = base / fs_module_path / "mod.tml";
    if (exists_case_sensitive(candidate2)) {
        return candidate2.string();
    }

    return "";
}

bool TypeEnv::load_native_module(const std::string& module_path, bool silent) {
    if (!module_registry_) {
        return false;
    }

    // Check if module is already registered
    if (module_registry_->has_module(module_path)) {
        return true; // Already loaded
    }

    // Check global module cache for library modules (core::*, std::*, test)
    // This avoids re-parsing library modules that have already been loaded
    // by other compilation units (e.g., other test files)
    if (GlobalModuleCache::should_cache(module_path)) {
        auto& cache = GlobalModuleCache::instance();
        if (auto cached_module = cache.get(module_path)) {
            // Validate cached module freshness using source content hash.
            // The GlobalModuleCache persists for the lifetime of the compiler process.
            // If library source files change between builds (or within the same build
            // when meta files are regenerated), the in-memory cache may hold stale modules.
            // We compare the current source hash against the meta file's stored hash.
            bool cache_valid = true;
            {
                std::string lib_subdir, rest;
                if (module_path.substr(0, 6) == "core::") {
                    lib_subdir = "core";
                    rest = module_path.substr(6);
                } else if (module_path.substr(0, 5) == "std::") {
                    lib_subdir = "std";
                    rest = module_path.substr(5);
                } else if (module_path.substr(0, 6) == "test::") {
                    lib_subdir = "test";
                    rest = module_path.substr(6);
                }
                if (!lib_subdir.empty() && !rest.empty()) {
                    std::string fs_rest = rest;
                    size_t p = 0;
                    while ((p = fs_rest.find("::", p)) != std::string::npos) {
                        fs_rest.replace(p, 2, "/");
                        p += 1;
                    }
                    std::string src = resolve_lib_module_path(lib_subdir, "src", fs_rest);
                    if (!src.empty()) {
                        // Compute hash of current source and compare against meta file
                        uint64_t current_hash = compute_module_source_hash(src);
                        if (current_hash != 0) {
                            auto build_root = find_build_root();
                            auto cache_path = get_module_cache_path(module_path, build_root);
                            if (!cache_path.empty() && std::filesystem::exists(cache_path)) {
                                std::ifstream meta_in(cache_path, std::ios::binary);
                                if (meta_in) {
                                    ModuleBinaryReader header_reader(meta_in);
                                    uint64_t cached_hash = header_reader.read_header_hash();
                                    if (!header_reader.has_error() && cached_hash != current_hash) {
                                        TML_LOG_DEBUG("types", "[MODULE] GlobalCache stale for: "
                                                                   << module_path << " (hash "
                                                                   << current_hash << " vs "
                                                                   << cached_hash << ")");
                                        cache_valid = false;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (!cache_valid) {
                // Fall through to binary meta cache / file loading below
                // (which will re-validate hash and reload from source)
            } else {
                // Found in cache and valid
                TML_DEBUG_LN("[MODULE] Cache hit for: " << module_path);

                // Copy re-export source paths before any operations that might invalidate iterators
                std::vector<std::string> re_export_sources;
                re_export_sources.reserve(cached_module->re_exports.size());
                for (const auto& re_export : cached_module->re_exports) {
                    re_export_sources.push_back(re_export.source_path);
                }

                // Copy private import paths (for glob imports like `use std::zlib::constants::*`)
                std::vector<std::string> private_import_sources = cached_module->private_imports;

                // Register behavior impls from cached module (e.g., Drop for MutexGuard)
                // Each TypeEnv has its own behavior_impls_, so we must re-register
                for (const auto& [type_name, behaviors] : cached_module->behavior_impls) {
                    for (const auto& behavior_name : behaviors) {
                        register_impl(type_name, behavior_name);
                    }
                }

                // Fallback: if behavior_impls is empty (old cache format), infer Drop impls
                // from function names like "MutexGuard::drop" in the module's functions map
                if (cached_module->behavior_impls.empty()) {
                    for (const auto& [func_name, _sig] : cached_module->functions) {
                        if (func_name.size() > 6 &&
                            func_name.substr(func_name.size() - 6) == "::drop") {
                            std::string type_name = func_name.substr(0, func_name.size() - 6);
                            register_impl(type_name, "Drop");
                        }
                    }
                }

                module_registry_->register_module(module_path, *cached_module);

                // Load re-export source modules to ensure they're in the current registry
                // This is needed because each TypeEnv has its own registry but the cache is global
                for (const auto& source_path : re_export_sources) {
                    load_native_module(source_path, /*silent=*/true);
                }

                // Load private import modules to ensure transitive dependencies are available
                // This handles cases like `use std::zlib::constants::*` in options.tml
                // Private imports may be stored as full paths including symbol names
                // (e.g., "core::option::Maybe"), so we also try the base module path
                for (const auto& import_path : private_import_sources) {
                    bool loaded = load_native_module(import_path, /*silent=*/true);
                    if (!loaded) {
                        // Strip last segment (symbol name) and try as module path
                        auto last_sep = import_path.rfind("::");
                        if (last_sep != std::string::npos) {
                            load_native_module(import_path.substr(0, last_sep), /*silent=*/true);
                        }
                    }
                }

                return true;
            } // else (cache_valid)
        }
    }

    // Check binary metadata cache (.tml.meta) for library modules
    // Loads pre-serialized Module structs without file resolution or parsing.
    // We first resolve the source path to validate the hash against the current source.
    // This catches stale meta caches when library source files change.
    //
    // IMPORTANT: We must validate BEFORE checking GlobalModuleCache, because the
    // in-memory cache may hold a stale module from a previous meta load in this process.
    // For directory modules (mod.tml), the hash covers ALL .tml files in the directory.
    if (GlobalModuleCache::should_cache(module_path)) {
        // Resolve source path for hash validation
        std::string meta_source_path;
        {
            // Determine lib subdir from module path prefix
            std::string lib_subdir;
            std::string rest;
            if (module_path.substr(0, 6) == "core::") {
                lib_subdir = "core";
                rest = module_path.substr(6);
            } else if (module_path.substr(0, 5) == "std::") {
                lib_subdir = "std";
                rest = module_path.substr(5);
            } else if (module_path.substr(0, 6) == "test::") {
                lib_subdir = "test";
                rest = module_path.substr(6);
            }
            if (!lib_subdir.empty() && !rest.empty()) {
                std::string fs_rest = rest;
                size_t p = 0;
                while ((p = fs_rest.find("::", p)) != std::string::npos) {
                    fs_rest.replace(p, 2, "/");
                    p += 1;
                }
                meta_source_path = resolve_lib_module_path(lib_subdir, "src", fs_rest);
            }
        }
        auto cached = meta_source_path.empty()
                          ? load_module_from_cache(module_path)
                          : load_module_from_cache(module_path, meta_source_path);
        if (cached) {
            TML_DEBUG_LN("[MODULE] Binary meta cache hit for: " << module_path);

            // Copy re-export and private import paths before moving the cached module
            std::vector<std::string> re_export_sources;
            re_export_sources.reserve(cached->re_exports.size());
            for (const auto& re_export : cached->re_exports) {
                re_export_sources.push_back(re_export.source_path);
            }
            std::vector<std::string> private_import_sources = cached->private_imports;

            // Register behavior impls from binary-cached module
            for (const auto& [type_name, behaviors] : cached->behavior_impls) {
                for (const auto& behavior_name : behaviors) {
                    register_impl(type_name, behavior_name);
                }
            }

            // Fallback: if behavior_impls is empty (old cache format), infer Drop impls
            if (cached->behavior_impls.empty()) {
                for (const auto& [func_name, _sig] : cached->functions) {
                    if (func_name.size() > 6 &&
                        func_name.substr(func_name.size() - 6) == "::drop") {
                        std::string type_name = func_name.substr(0, func_name.size() - 6);
                        register_impl(type_name, "Drop");
                    }
                }
            }

            GlobalModuleCache::instance().put(module_path, *cached);
            module_registry_->register_module(module_path, std::move(*cached));

            // Load re-export source modules to ensure transitive dependencies are available
            for (const auto& source_path : re_export_sources) {
                load_native_module(source_path, /*silent=*/true);
            }

            // Load private import modules for transitive dependencies
            for (const auto& import_path : private_import_sources) {
                bool loaded = load_native_module(import_path, /*silent=*/true);
                if (!loaded) {
                    // Strip last segment (symbol name) and try as module path
                    auto last_sep = import_path.rfind("::");
                    if (last_sep != std::string::npos) {
                        load_native_module(import_path.substr(0, last_sep), /*silent=*/true);
                    }
                }
            }

            return true;
        }
    }

    // Test module - load from lib/test/
    // Note: We prioritize assertions/mod.tml since mod.tml uses `pub use` re-exports
    // which aren't fully supported yet
    if (module_path == "test") {
        // Check path resolution cache first
        std::string cached = get_cached_path(module_path);
        if (!cached.empty()) {
            TML_DEBUG_LN("[MODULE] Path cache hit for: " << module_path << " -> " << cached);
            return load_module_from_file(module_path, cached);
        }

        // Try cached library root first (2 probes instead of 5)
        const std::string& lib_root = find_lib_root();
        if (!lib_root.empty()) {
            namespace fs = std::filesystem;
            fs::path assertions_mod =
                fs::path(lib_root) / "test" / "src" / "assertions" / "mod.tml";
            if (fs::exists(assertions_mod)) {
                std::string resolved = assertions_mod.string();
                cache_resolved_path(module_path, resolved);
                TML_DEBUG_LN("[MODULE] Found test module at: " << resolved);
                return load_module_from_file(module_path, resolved);
            }
            fs::path test_mod = fs::path(lib_root) / "test" / "src" / "mod.tml";
            if (fs::exists(test_mod)) {
                std::string resolved = test_mod.string();
                cache_resolved_path(module_path, resolved);
                TML_DEBUG_LN("[MODULE] Found test module at: " << resolved);
                return load_module_from_file(module_path, resolved);
            }
        }

        // Fallback: try all relative paths
        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::path("lib") / "test" / "src" / "assertions" / "mod.tml",
            std::filesystem::path("lib") / "test" / "src" / "mod.tml",
            std::filesystem::path("..") / ".." / "lib" / "test" / "src" / "assertions" / "mod.tml",
            std::filesystem::path("..") / "lib" / "test" / "src" / "assertions" / "mod.tml",
            std::filesystem::path("F:/Node/hivellm/tml/lib/test/src/assertions/mod.tml"),
        };

        for (const auto& module_file : search_paths) {
            if (exists_case_sensitive(module_file)) {
                std::string resolved = module_file.string();
                cache_resolved_path(module_path, resolved);
                TML_DEBUG_LN("[MODULE] Found test module at: " << resolved);
                return load_module_from_file(module_path, resolved);
            }
        }

        TML_LOG_ERROR("types", "Test module file not found");
        return false;
    }

    // Test library submodules - load from lib/test/src/
    if (module_path.substr(0, 6) == "test::") {
        std::string cached = get_cached_path(module_path);
        if (!cached.empty()) {
            TML_DEBUG_LN("[MODULE] Path cache hit: " << module_path);
            return load_module_from_file(module_path, cached);
        }
        if (is_known_not_found(module_path)) {
            if (!silent) {
                TML_LOG_ERROR("types", "test module not found: " << module_path);
            }
            return false;
        }

        std::string module_name = module_path.substr(6);
        std::string fs_module_path = module_name;
        size_t pos = 0;
        while ((pos = fs_module_path.find("::", pos)) != std::string::npos) {
            fs_module_path.replace(pos, 2, "/");
            pos += 1;
        }

        // Try cached library root first
        std::string resolved = resolve_lib_module_path("test", "src", fs_module_path);
        if (!resolved.empty()) {
            cache_resolved_path(module_path, resolved);
            TML_DEBUG_LN("[MODULE] Resolved test module: " << module_path << " -> " << resolved);
            return load_module_from_file(module_path, resolved);
        }

        // Fallback: try all relative paths
        auto cwd = std::filesystem::current_path();
        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::path("lib") / "test" / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("lib") / "test" / "src" / fs_module_path / "mod.tml",
            std::filesystem::path("..") / ".." / "lib" / "test" / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("..") / ".." / "lib" / "test" / "src" / fs_module_path /
                "mod.tml",
            std::filesystem::path("..") / "lib" / "test" / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("..") / "lib" / "test" / "src" / fs_module_path / "mod.tml",
            cwd / "lib" / "test" / "src" / (fs_module_path + ".tml"),
            cwd / "lib" / "test" / "src" / fs_module_path / "mod.tml",
        };

        TML_DEBUG_LN("[MODULE] Looking for test module: " << module_path << " (fs_path: "
                                                          << fs_module_path << ")");
        for (const auto& module_file : search_paths) {
            TML_DEBUG_LN("[MODULE]   Checking: " << module_file);
            if (exists_case_sensitive(module_file)) {
                TML_DEBUG_LN("[MODULE]   FOUND!");
                std::string res = module_file.string();
                cache_resolved_path(module_path, res);
                return load_module_from_file(module_path, res);
            }
        }

        if (!silent) {
            TML_LOG_ERROR("types", "test module not found: " << module_path);
        }
        cache_not_found(module_path);
        return false;
    }

    // Backtrace module - load from lib/backtrace/
    if (module_path == "backtrace") {
        std::string cached = get_cached_path(module_path);
        if (!cached.empty()) {
            return load_module_from_file(module_path, cached);
        }

        // Try cached library root first
        std::string resolved = resolve_lib_module_path("backtrace", "src", "mod");
        if (!resolved.empty()) {
            cache_resolved_path(module_path, resolved);
            return load_module_from_file(module_path, resolved);
        }

        // Fallback: try all relative paths
        auto cwd = std::filesystem::current_path();
        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::path("lib") / "backtrace" / "src" / "mod.tml",
            std::filesystem::path("..") / ".." / "lib" / "backtrace" / "src" / "mod.tml",
            std::filesystem::path("..") / "lib" / "backtrace" / "src" / "mod.tml",
            cwd / "lib" / "backtrace" / "src" / "mod.tml",
            std::filesystem::path("F:/Node/hivellm/tml/lib/backtrace/src/mod.tml"),
        };

        for (const auto& module_file : search_paths) {
            if (exists_case_sensitive(module_file)) {
                std::string res = module_file.string();
                cache_resolved_path(module_path, res);
                return load_module_from_file(module_path, res);
            }
        }

        if (!silent) {
            TML_LOG_ERROR("types", "Backtrace module file not found");
        }
        return false;
    }

    // Backtrace submodules - load from lib/backtrace/src/
    if (module_path.substr(0, 11) == "backtrace::") {
        std::string cached = get_cached_path(module_path);
        if (!cached.empty()) {
            return load_module_from_file(module_path, cached);
        }
        if (is_known_not_found(module_path)) {
            return false;
        }

        std::string module_name = module_path.substr(11);
        std::string fs_module_path = module_name;
        size_t pos = 0;
        while ((pos = fs_module_path.find("::", pos)) != std::string::npos) {
            fs_module_path.replace(pos, 2, "/");
            pos += 1;
        }

        // Try cached library root first (2 probes instead of 10)
        std::string resolved = resolve_lib_module_path("backtrace", "src", fs_module_path);
        if (!resolved.empty()) {
            cache_resolved_path(module_path, resolved);
            return load_module_from_file(module_path, resolved);
        }

        // Fallback: try all relative paths
        auto cwd = std::filesystem::current_path();
        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::path("lib") / "backtrace" / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("lib") / "backtrace" / "src" / fs_module_path / "mod.tml",
            std::filesystem::path("..") / ".." / "lib" / "backtrace" / "src" /
                (fs_module_path + ".tml"),
            std::filesystem::path("..") / ".." / "lib" / "backtrace" / "src" / fs_module_path /
                "mod.tml",
            std::filesystem::path("..") / "lib" / "backtrace" / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("..") / "lib" / "backtrace" / "src" / fs_module_path / "mod.tml",
            cwd / "lib" / "backtrace" / "src" / (fs_module_path + ".tml"),
            cwd / "lib" / "backtrace" / "src" / fs_module_path / "mod.tml",
            std::filesystem::path("F:/Node/hivellm/tml/lib/backtrace/src") /
                (fs_module_path + ".tml"),
            std::filesystem::path("F:/Node/hivellm/tml/lib/backtrace/src") / fs_module_path /
                "mod.tml",
        };

        TML_DEBUG_LN("[MODULE] Looking for backtrace module: " << module_path << " (fs_path: "
                                                               << fs_module_path << ")");
        for (const auto& module_file : search_paths) {
            TML_DEBUG_LN("[MODULE]   Checking: " << module_file);
            if (exists_case_sensitive(module_file)) {
                TML_DEBUG_LN("[MODULE]   FOUND!");
                std::string res = module_file.string();
                cache_resolved_path(module_path, res);
                return load_module_from_file(module_path, res);
            }
        }

        if (!silent) {
            TML_LOG_ERROR("types", "backtrace module file not found: " << module_path);
        }
        cache_not_found(module_path);
        return false;
    }

    // Core library modules - load from filesystem
    if (module_path.substr(0, 6) == "core::") {
        // Check path resolution cache first
        std::string cached = get_cached_path(module_path);
        if (!cached.empty()) {
            TML_DEBUG_LN("[MODULE] Path cache hit: " << module_path);
            return load_module_from_file(module_path, cached);
        }
        if (is_known_not_found(module_path)) {
            if (!silent) {
                TML_LOG_ERROR("types", "core module file not found: " << module_path);
            }
            return false;
        }

        std::string module_name = module_path.substr(6);
        std::string fs_module_path = module_name;
        size_t pos = 0;
        while ((pos = fs_module_path.find("::", pos)) != std::string::npos) {
            fs_module_path.replace(pos, 2, "/");
            pos += 1;
        }

        // Try cached library root first (2 probes instead of 12)
        std::string resolved = resolve_lib_module_path("core", "src", fs_module_path);
        if (!resolved.empty()) {
            cache_resolved_path(module_path, resolved);
            TML_DEBUG_LN("[MODULE] Resolved core module: " << module_path << " -> " << resolved);
            return load_module_from_file(module_path, resolved);
        }

        // Fallback: try all relative paths
        auto cwd = std::filesystem::current_path();
        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::path("lib") / "core" / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("lib") / "core" / "src" / fs_module_path / "mod.tml",
            std::filesystem::path("..") / ".." / "lib" / "core" / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("..") / ".." / "lib" / "core" / "src" / fs_module_path /
                "mod.tml",
            std::filesystem::path("..") / "lib" / "core" / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("..") / "lib" / "core" / "src" / fs_module_path / "mod.tml",
            std::filesystem::path("core") / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("core") / "src" / fs_module_path / "mod.tml",
            cwd / "lib" / "core" / "src" / (fs_module_path + ".tml"),
            cwd / "lib" / "core" / "src" / fs_module_path / "mod.tml",
            std::filesystem::path("F:/Node/hivellm/tml/lib/core/src") / (fs_module_path + ".tml"),
            std::filesystem::path("F:/Node/hivellm/tml/lib/core/src") / fs_module_path / "mod.tml",
        };

        TML_DEBUG_LN("[MODULE] Looking for core module: " << module_path << " (fs_path: "
                                                          << fs_module_path << ")");
        for (const auto& module_file : search_paths) {
            TML_DEBUG_LN("[MODULE]   Checking: " << module_file);
            if (exists_case_sensitive(module_file)) {
                TML_DEBUG_LN("[MODULE]   FOUND!");
                std::string res = module_file.string();
                cache_resolved_path(module_path, res);
                return load_module_from_file(module_path, res);
            }
        }

        if (!silent) {
            TML_LOG_ERROR("types", "core module file not found: " << module_path);
        }
        cache_not_found(module_path);
        return false;
    }

    // Standard library modules - load from filesystem
    if (module_path.substr(0, 5) == "std::") {
        // Check path resolution cache first
        std::string cached = get_cached_path(module_path);
        if (!cached.empty()) {
            TML_DEBUG_LN("[MODULE] Path cache hit: " << module_path);
            return load_module_from_file(module_path, cached);
        }
        if (is_known_not_found(module_path)) {
            if (!silent) {
                TML_LOG_ERROR("types", "std module file not found: " << module_path);
            }
            return false;
        }

        std::string module_name = module_path.substr(5);
        std::string fs_module_path = module_name;
        size_t pos = 0;
        while ((pos = fs_module_path.find("::", pos)) != std::string::npos) {
            fs_module_path.replace(pos, 2, "/");
            pos += 1;
        }

        // Try cached library root first (2 probes instead of 12)
        std::string resolved = resolve_lib_module_path("std", "src", fs_module_path);
        if (!resolved.empty()) {
            cache_resolved_path(module_path, resolved);
            TML_DEBUG_LN("[MODULE] Resolved std module: " << module_path << " -> " << resolved);
            return load_module_from_file(module_path, resolved);
        }

        // Fallback: try all relative paths
        auto cwd = std::filesystem::current_path();
        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::path("lib") / "std" / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("lib") / "std" / "src" / fs_module_path / "mod.tml",
            std::filesystem::path("..") / ".." / "lib" / "std" / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("..") / ".." / "lib" / "std" / "src" / fs_module_path / "mod.tml",
            std::filesystem::path("..") / "lib" / "std" / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("..") / "lib" / "std" / "src" / fs_module_path / "mod.tml",
            std::filesystem::path("std") / "src" / (fs_module_path + ".tml"),
            std::filesystem::path("std") / "src" / fs_module_path / "mod.tml",
            cwd / "lib" / "std" / "src" / (fs_module_path + ".tml"),
            cwd / "lib" / "std" / "src" / fs_module_path / "mod.tml",
            std::filesystem::path("F:/Node/hivellm/tml/lib/std/src") / (fs_module_path + ".tml"),
            std::filesystem::path("F:/Node/hivellm/tml/lib/std/src") / fs_module_path / "mod.tml",
        };

        TML_DEBUG_LN("[MODULE] Looking for std module: " << module_path
                                                         << " (fs_path: " << fs_module_path << ")");
        for (const auto& module_file : search_paths) {
            TML_DEBUG_LN("[MODULE]   Checking: " << module_file);
            if (exists_case_sensitive(module_file)) {
                TML_DEBUG_LN("[MODULE]   FOUND!");
                std::string res = module_file.string();
                cache_resolved_path(module_path, res);
                return load_module_from_file(module_path, res);
            }
        }

        if (!silent) {
            TML_LOG_ERROR("types", "std module file not found: " << module_path);
        }
        cache_not_found(module_path);
        return false;
    }

    // Local module - try to load from source directory
    // This supports "use algorithms" to load "algorithms.tml" from the same directory
    // Also supports nested modules like "utils::helpers" -> "utils/helpers.tml"

    // Convert module path to filesystem path: "utils::helpers" -> "utils/helpers"
    std::string fs_module_path = module_path;
    size_t pos = 0;
    while ((pos = fs_module_path.find("::", pos)) != std::string::npos) {
        fs_module_path.replace(pos, 2, "/");
        pos += 1;
    }

    if (!source_directory_.empty()) {
        std::filesystem::path source_dir(source_directory_);

        // Try multiple patterns for local modules
        std::vector<std::filesystem::path> search_paths = {
            source_dir / (fs_module_path + ".tml"),  // algorithms.tml or utils/helpers.tml
            source_dir / fs_module_path / "mod.tml", // algorithms/mod.tml or utils/helpers/mod.tml
        };

        for (const auto& module_file : search_paths) {
            if (exists_case_sensitive(module_file)) {
                TML_DEBUG_LN("[MODULE] Found local module at: " << module_file);
                return load_module_from_file(module_path, module_file.string());
            }
        }
    }

    // Also try current working directory
    auto cwd = std::filesystem::current_path();
    std::vector<std::filesystem::path> cwd_paths = {
        cwd / (fs_module_path + ".tml"),
        cwd / fs_module_path / "mod.tml",
    };

    for (const auto& module_file : cwd_paths) {
        if (exists_case_sensitive(module_file)) {
            TML_DEBUG_LN("[MODULE] Found local module at: " << module_file);
            return load_module_from_file(module_path, module_file.string());
        }
    }

    // Module not found
    return false;
}

} // namespace tml::types
