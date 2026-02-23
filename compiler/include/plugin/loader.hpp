//! # Plugin Loader
//!
//! Cross-platform dynamic loading of plugin modules with zstd decompression
//! and disk cache. Plugins are discovered from a `plugins/` directory next to
//! the executable, optionally compressed as `.dll.zst` / `.so.zst`.
//!
//! ## Loading Flow
//!
//! ```text
//! 1. Check cache/plugins/foo.dll — if hash matches, LoadLibrary directly
//! 2. Otherwise decompress plugins/foo.dll.zst → cache/plugins/foo.dll
//! 3. LoadLibrary / dlopen the cached DLL
//! 4. GetProcAddress("plugin_query") → verify ABI version
//! 5. GetProcAddress("plugin_init") → initialize
//! ```
//!
//! ## Search Order
//!
//! 1. `<exe_dir>/plugins/`
//! 2. `PLUGIN_DIR` environment variable
//! 3. `<exe_dir>/../lib/tml/plugins/`

#pragma once

#include "plugin/abi.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace tml::plugin {

/// A loaded plugin module.
struct LoadedPlugin {
    void* handle = nullptr;           // HMODULE (Win) or void* (Linux)
    const PluginInfo* info = nullptr; // Metadata from plugin_query()
    PluginInitFn init = nullptr;
    PluginShutdownFn shutdown = nullptr;
    fs::path dll_path; // Path to the loaded DLL
    bool initialized = false;
};

/// Plugin loader with zstd decompression and disk cache.
class Loader {
public:
    Loader();
    ~Loader();

    // Non-copyable
    Loader(const Loader&) = delete;
    Loader& operator=(const Loader&) = delete;

    /// Load a plugin by name. Returns null if not found or ABI mismatch.
    /// Handles decompression and caching transparently.
    auto load(const std::string& name) -> LoadedPlugin*;

    /// Unload all plugins (calls shutdown on each).
    void unload_all();

    /// Get a previously loaded plugin by name.
    auto get(const std::string& name) -> LoadedPlugin*;

    /// Check if a plugin is loaded.
    auto is_loaded(const std::string& name) const -> bool;

    /// Get the plugins directory path.
    auto plugins_dir() const -> const fs::path&;

    /// Get the cache directory path.
    auto cache_dir() const -> const fs::path&;

    /// Look up a symbol from a loaded plugin handle.
    /// Use this to find exported C functions (e.g., compiler_main).
    static auto get_symbol(void* handle, const char* symbol) -> void*;

private:
    /// Discover plugin directories based on exe location.
    void discover_paths();

    /// Decompress a .zst file to the cache directory.
    auto decompress_to_cache(const fs::path& zst_path, const fs::path& cache_path) -> bool;

    /// Check if cache is valid (hash matches).
    auto is_cache_valid(const fs::path& zst_path, const fs::path& cache_path) -> bool;

    /// Compute CRC32 hash of a file (for cache validation).
    static auto compute_file_hash(const fs::path& path) -> std::string;

    /// Load a DLL/SO from path and resolve plugin entry points.
    auto load_dll(const fs::path& path) -> LoadedPlugin;

    /// Platform-specific dynamic library loading.
    static auto dl_open(const fs::path& path) -> void*;
    static auto dl_sym(void* handle, const char* symbol) -> void*;
    static void dl_close(void* handle);
    static auto dl_error() -> std::string;

    /// Get the directory containing the current executable.
    static auto exe_dir() -> fs::path;

    fs::path plugins_dir_;
    fs::path cache_dir_;
    std::unordered_map<std::string, LoadedPlugin> loaded_;
};

} // namespace tml::plugin
