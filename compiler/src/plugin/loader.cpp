//! # Plugin Loader Implementation
//!
//! Cross-platform plugin loading with zstd decompression and disk cache.

#include "plugin/loader.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

// zstd decoder — minimal header from third_party/zstd/
// Implementation in zstddeclib.c, linked as a separate TU.
#include "zstd.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace tml::plugin {

// ============================================================================
// Platform-specific dynamic library operations
// ============================================================================

auto Loader::dl_open(const fs::path& path) -> void* {
#ifdef _WIN32
    return static_cast<void*>(LoadLibraryW(path.wstring().c_str()));
#else
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
}

auto Loader::dl_sym(void* handle, const char* symbol) -> void* {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), symbol));
#else
    return dlsym(handle, symbol);
#endif
}

void Loader::dl_close(void* handle) {
    if (!handle)
        return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

auto Loader::dl_error() -> std::string {
#ifdef _WIN32
    DWORD err = GetLastError();
    if (err == 0)
        return "";
    LPSTR buf = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, 0,
                   reinterpret_cast<LPSTR>(&buf), 0, nullptr);
    std::string msg = buf ? buf : "Unknown error";
    LocalFree(buf);
    return msg;
#else
    const char* err = dlerror();
    return err ? err : "";
#endif
}

auto Loader::get_symbol(void* handle, const char* symbol) -> void* {
    return dl_sym(handle, symbol);
}

auto Loader::exe_dir() -> fs::path {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path();
#else
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return fs::path(buf).parent_path();
    }
    return fs::current_path();
#endif
}

// ============================================================================
// Loader lifecycle
// ============================================================================

Loader::Loader() {
    discover_paths();
}

Loader::~Loader() {
    unload_all();
}

void Loader::discover_paths() {
    auto exe = exe_dir();

    // Primary: plugins/ next to the executable
    plugins_dir_ = exe / "plugins";

    // Override via environment variable
#ifdef _MSC_VER
    char* env = nullptr;
    size_t env_len = 0;
    _dupenv_s(&env, &env_len, "PLUGIN_DIR");
#else
    const char* env = std::getenv("PLUGIN_DIR");
#endif
    if (env && *env) {
        plugins_dir_ = fs::path(env);
    }
#ifdef _MSC_VER
    free(env);
#endif

    // Cache directory: cache/plugins/ next to executable
    cache_dir_ = exe / "cache" / "plugins";

    // Create cache dir if it doesn't exist
    std::error_code ec;
    fs::create_directories(cache_dir_, ec);
}

auto Loader::plugins_dir() const -> const fs::path& {
    return plugins_dir_;
}

auto Loader::cache_dir() const -> const fs::path& {
    return cache_dir_;
}

// ============================================================================
// Loading
// ============================================================================

auto Loader::load(const std::string& name) -> LoadedPlugin* {
    // Already loaded?
    if (auto it = loaded_.find(name); it != loaded_.end()) {
        return &it->second;
    }

    // Determine DLL extension
#ifdef _WIN32
    const std::string dll_ext = ".dll";
#else
    const std::string dll_ext = ".so";
#endif
    const std::string zst_ext = dll_ext + ".zst";

    // Search for compressed or uncompressed plugin
    fs::path zst_path = plugins_dir_ / (name + zst_ext);
    fs::path raw_path = plugins_dir_ / (name + dll_ext);
    fs::path cache_path = cache_dir_ / (name + dll_ext);
    fs::path load_path;

    if (fs::exists(zst_path)) {
        // Compressed plugin — decompress to cache if needed
        if (!is_cache_valid(zst_path, cache_path)) {
            if (!decompress_to_cache(zst_path, cache_path)) {
                std::cerr << "plugin: failed to decompress " << zst_path << "\n";
                return nullptr;
            }
        }
        load_path = cache_path;
    } else if (fs::exists(raw_path)) {
        // Uncompressed plugin — load directly
        load_path = raw_path;
    } else if (fs::exists(cache_path)) {
        // Only in cache (dev mode: DLLs placed directly in cache)
        load_path = cache_path;
    } else {
        std::cerr << "error: plugin '" << name << "' not found\n"
                  << "\n"
                  << "  Searched:\n"
                  << "    " << plugins_dir_.string() << "/" << name << dll_ext << "\n"
                  << "    " << plugins_dir_.string() << "/" << name << zst_ext << "\n"
                  << "    " << cache_dir_.string() << "/" << name << dll_ext << "\n"
                  << "\n"
                  << "  To fix:\n"
                  << "    1. Build with: scripts\\build.bat --modular\n"
                  << "    2. Or set PLUGIN_DIR to the plugin directory\n"
                  << "    3. Or use the monolithic build: scripts\\build.bat\n";
        return nullptr;
    }

    // Load the DLL
    auto plugin = load_dll(load_path);
    if (!plugin.handle) {
        return nullptr;
    }

    // Verify ABI
    if (!plugin.info) {
        std::cerr << "error: plugin '" << name << "' does not export plugin_query()\n"
                  << "  The DLL was loaded but has no plugin metadata.\n"
                  << "  Ensure the plugin was built with the correct ABI.\n";
        dl_close(plugin.handle);
        return nullptr;
    }
    if (plugin.info->abi_version != PLUGIN_ABI_VERSION) {
        std::cerr << "error: ABI version mismatch for plugin '" << name << "'\n"
                  << "  Host ABI version:   " << PLUGIN_ABI_VERSION << "\n"
                  << "  Plugin ABI version: " << plugin.info->abi_version << "\n"
                  << "\n"
                  << "  The plugin was built with a different compiler version.\n"
                  << "  Rebuild all plugins: scripts\\build.bat --modular --clean\n";
        dl_close(plugin.handle);
        return nullptr;
    }

    // Load dependencies first
    if (plugin.info->dependencies) {
        for (const char* const* dep = plugin.info->dependencies; *dep; ++dep) {
            if (!is_loaded(*dep)) {
                if (!load(*dep)) {
                    std::cerr << "error: plugin '" << name << "' requires '" << *dep
                              << "' which failed to load\n";
                    dl_close(plugin.handle);
                    return nullptr;
                }
            }
        }
    }

    // Initialize
    if (plugin.init) {
        if (plugin.init(nullptr) != 0) {
            std::cerr << "plugin: init failed for '" << name << "'\n";
            dl_close(plugin.handle);
            return nullptr;
        }
        plugin.initialized = true;
    }

    auto [it, _] = loaded_.emplace(name, std::move(plugin));
    return &it->second;
}

void Loader::unload_all() {
    // Shutdown in reverse load order (approximation: just iterate)
    for (auto& [name, plugin] : loaded_) {
        if (plugin.initialized && plugin.shutdown) {
            plugin.shutdown();
        }
        dl_close(plugin.handle);
        plugin.handle = nullptr;
        plugin.initialized = false;
    }
    loaded_.clear();
}

auto Loader::get(const std::string& name) -> LoadedPlugin* {
    auto it = loaded_.find(name);
    return it != loaded_.end() ? &it->second : nullptr;
}

auto Loader::is_loaded(const std::string& name) const -> bool {
    return loaded_.count(name) > 0;
}

// ============================================================================
// DLL loading
// ============================================================================

auto Loader::load_dll(const fs::path& path) -> LoadedPlugin {
    LoadedPlugin plugin;
    plugin.dll_path = path;

    plugin.handle = dl_open(path);
    if (!plugin.handle) {
        std::cerr << "plugin: failed to load " << path << ": " << dl_error() << "\n";
        return plugin;
    }

    // Resolve entry points
    auto query_fn = reinterpret_cast<PluginQueryFn>(dl_sym(plugin.handle, "plugin_query"));
    if (query_fn) {
        plugin.info = query_fn();
    }

    plugin.init = reinterpret_cast<PluginInitFn>(dl_sym(plugin.handle, "plugin_init"));
    plugin.shutdown = reinterpret_cast<PluginShutdownFn>(dl_sym(plugin.handle, "plugin_shutdown"));

    return plugin;
}

// ============================================================================
// Compression / Cache
// ============================================================================

auto Loader::is_cache_valid(const fs::path& zst_path, const fs::path& cache_path) -> bool {
    if (!fs::exists(cache_path))
        return false;

    // Check hash file: cache_path + ".hash" stores the CRC32 of the .zst file
    fs::path hash_path = cache_path;
    hash_path += ".hash";

    if (!fs::exists(hash_path)) {
        // No hash file → fall back to mtime comparison
        std::error_code ec;
        auto zst_time = fs::last_write_time(zst_path, ec);
        if (ec)
            return false;
        auto cache_time = fs::last_write_time(cache_path, ec);
        if (ec)
            return false;
        return cache_time >= zst_time;
    }

    // Read stored hash
    std::ifstream hf(hash_path);
    if (!hf)
        return false;
    std::string stored_hash;
    std::getline(hf, stored_hash);
    hf.close();

    // Compute current hash of compressed file
    std::string current_hash = compute_file_hash(zst_path);
    return !current_hash.empty() && current_hash == stored_hash;
}

auto Loader::compute_file_hash(const fs::path& path) -> std::string {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        return "";

    auto size = static_cast<size_t>(in.tellg());
    in.seekg(0);

    // Simple CRC32 hash (same algorithm as the compiler's fingerprinting)
    uint32_t crc = 0xFFFFFFFF;
    constexpr size_t BUF_SIZE = 65536;
    char buf[BUF_SIZE];
    while (size > 0) {
        size_t to_read_size = (size < BUF_SIZE) ? size : BUF_SIZE;
        auto to_read = static_cast<std::streamsize>(to_read_size);
        in.read(buf, to_read);
        auto got = static_cast<size_t>(in.gcount());
        for (size_t i = 0; i < got; ++i) {
            crc ^= static_cast<uint8_t>(buf[i]);
            for (int j = 0; j < 8; ++j)
                crc = (crc >> 1) ^ (0x82F63B78 & static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1))));
        }
        size -= got;
    }
    crc ^= 0xFFFFFFFF;

    // Convert to hex string
    char hex[9];
    snprintf(hex, sizeof(hex), "%08x", crc);
    return hex;
}

auto Loader::decompress_to_cache(const fs::path& zst_path, const fs::path& cache_path) -> bool {
    // Read compressed file
    std::ifstream in(zst_path, std::ios::binary | std::ios::ate);
    if (!in)
        return false;

    auto comp_size = static_cast<size_t>(in.tellg());
    in.seekg(0);
    std::vector<char> compressed(comp_size);
    in.read(compressed.data(), static_cast<std::streamsize>(comp_size));
    in.close();

    // Get decompressed size
    auto decomp_size = ZSTD_getFrameContentSize(compressed.data(), comp_size);
    if (decomp_size == ZSTD_CONTENTSIZE_UNKNOWN || decomp_size == ZSTD_CONTENTSIZE_ERROR) {
        // Fallback: estimate at 4x compressed
        decomp_size = comp_size * 4;
    }

    std::vector<char> decompressed(decomp_size);

    // Decompress
    size_t result = ZSTD_decompress(decompressed.data(), decomp_size, compressed.data(), comp_size);
    if (ZSTD_isError(result)) {
        std::cerr << "plugin: zstd decompression failed: " << ZSTD_getErrorName(result) << "\n";
        return false;
    }

    // Create cache directory
    std::error_code ec;
    fs::create_directories(cache_path.parent_path(), ec);

    // Write decompressed DLL to cache
    std::ofstream out(cache_path, std::ios::binary);
    if (!out)
        return false;
    out.write(decompressed.data(), static_cast<std::streamsize>(result));
    out.close();

    // Write hash of the .zst file for future cache validation
    std::string hash = compute_file_hash(zst_path);
    if (!hash.empty()) {
        fs::path hash_path = cache_path;
        hash_path += ".hash";
        std::ofstream hf(hash_path);
        if (hf) {
            hf << hash << "\n";
            hf.close();
        }
    }

    return true;
}

} // namespace tml::plugin
