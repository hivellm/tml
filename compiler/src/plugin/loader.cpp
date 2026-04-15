//! # Plugin Loader Implementation
//!
//! Cross-platform plugin loading with zstd decompression and disk cache.

#include "plugin/loader.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
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
// Process-level DLL handle cache
//
// Stores loaded handles keyed by canonical DLL path. Each entry includes
// the file mtime at load time so stale entries are evicted after a rebuild.
// dl_open() checks this cache before calling LoadLibrary/dlopen; dl_close()
// skips FreeLibrary for cached handles (the OS cleans up at process exit).
// ============================================================================

struct DllCacheEntry {
    void* handle = nullptr;
    fs::file_time_type mtime;
};

static std::mutex g_dll_cache_mutex;
static std::unordered_map<std::string, DllCacheEntry> g_dll_cache;

// Preload state
static std::atomic<int> g_preload_count{0};

// Daemon mode: when true, unload_all() keeps DLLs resident in g_dll_cache
// instead of calling FreeLibrary. Set by cmd_daemon_server() at startup.
static std::atomic<bool> g_daemon_mode{false};

/// Retrieve mtime for a path; returns default (epoch) on error.
static auto get_path_mtime(const fs::path& p) -> fs::file_time_type {
    std::error_code ec;
    auto t = fs::last_write_time(p, ec);
    return ec ? fs::file_time_type{} : t;
}

/// Core platform DLL load (always does real OS load — no cache check).
static auto platform_dl_open(const fs::path& path) -> void* {
#ifdef _WIN32
    // LOAD_LIBRARY_SEARCH_DEFAULT_DIRS: search app dir, System32, and %PATH%
    // dirs added via AddDllDirectory. Provides deterministic search order and
    // mitigates DLL hijacking. Fall back to plain LoadLibraryW on older Windows.
    HMODULE h = LoadLibraryExW(path.wstring().c_str(), nullptr,
                               LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
                               LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!h) {
        h = LoadLibraryW(path.wstring().c_str());
    }
    return static_cast<void*>(h);
#else
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
}

/// Platform DLL unload (unconditional).
static void platform_dl_close(void* handle) {
    if (!handle) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

// ============================================================================
// Platform-specific dynamic library operations
// ============================================================================

auto Loader::dl_open(const fs::path& path) -> void* {
    std::string key = path.string();
    fs::file_time_type current_mtime = get_path_mtime(path);

    {
        std::lock_guard<std::mutex> lock(g_dll_cache_mutex);
        auto it = g_dll_cache.find(key);
        if (it != g_dll_cache.end()) {
            // Cache hit: validate mtime — evict only if the DLL was rebuilt.
            if (it->second.mtime == current_mtime) {
                return it->second.handle; // O(1) — no OS call needed
            }
            // Mtime changed (rebuild): close the stale handle and reload.
            platform_dl_close(it->second.handle);
            g_dll_cache.erase(it);
        }
    }

    // Cache miss: do the real OS load.
    void* handle = platform_dl_open(path);
    if (handle) {
        std::lock_guard<std::mutex> lock(g_dll_cache_mutex);
        // Another thread might have loaded it concurrently.
        auto [it, inserted] = g_dll_cache.emplace(key, DllCacheEntry{handle, current_mtime});
        if (!inserted) {
            // Duplicate load: close the handle we just opened and use the cached one.
            platform_dl_close(handle);
            return it->second.handle;
        }
    }
    return handle;
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
    // Handles that live in the static cache are kept resident until process
    // exit for performance — calling FreeLibrary would evict the DLL from the
    // process's module list and force a fresh load on the next invocation.
    // The OS reclaims all module mappings at process exit automatically.
    {
        std::lock_guard<std::mutex> lock(g_dll_cache_mutex);
        for (const auto& [path, entry] : g_dll_cache) {
            if (entry.handle == handle) {
                return; // Cached handle — skip FreeLibrary.
            }
        }
    }
    platform_dl_close(handle);
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
    // Join any outstanding preload threads so the cache is warm by the
    // time the first load() call is made.
    wait_preload_done();
    discover_paths();
}

Loader::~Loader() {
    unload_all();
}

void Loader::discover_paths() {
    auto exe = exe_dir();

    // Cache directory: always adjacent to the debug exe for consistency.
    cache_dir_ = exe / "cache" / "plugins";
    std::error_code ec;
    fs::create_directories(cache_dir_, ec);

    // Priority 1: explicit override via TML_PLUGIN_DIR env var.
    const char* plugin_dir_env = std::getenv("TML_PLUGIN_DIR");
    if (plugin_dir_env && plugin_dir_env[0] != '\0') {
        plugins_dir_ = fs::path(plugin_dir_env);
        return;
    }

    // Priority 2: TML_COMPILER_BUILD=release — load DLLs from the release
    // build directory so the debug tml.exe can use the optimized compiler
    // without changing PATH.  Layout: build/<type>/bin/ (exe lives here),
    // so release plugins are at build/release/bin/plugins/ or build/release/plugins/.
    const char* build_override = std::getenv("TML_COMPILER_BUILD");
    if (build_override && std::string(build_override) == "release") {
        auto build_dir = exe.parent_path().parent_path(); // build/debug → build/
        auto release_bin_plugins = build_dir / "release" / "bin" / "plugins";
        auto release_plugins = build_dir / "release" / "plugins";
        if (fs::exists(release_bin_plugins)) {
            plugins_dir_ = release_bin_plugins;
            return;
        }
        if (fs::exists(release_plugins)) {
            plugins_dir_ = release_plugins;
            return;
        }
        // Release build not found — warn and fall through to debug.
        std::cerr << "warning: TML_COMPILER_BUILD=release set but no release plugins found at:\n"
                  << "  " << release_bin_plugins.string() << "\n"
                  << "  " << release_plugins.string() << "\n"
                  << "  Run: scripts\\build.bat release\n"
                  << "  Falling back to debug build.\n";
    }

    // Default: plugins next to the exe (build/debug/bin/plugins/).
    plugins_dir_ = exe / "plugins";
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
    // Support both MSVC naming (tml_test.dll) and MinGW/Zig naming (libtml_test.dll)
    fs::path zst_path = plugins_dir_ / (name + zst_ext);
    fs::path raw_path = plugins_dir_ / (name + dll_ext);
    fs::path lib_path = plugins_dir_ / ("lib" + name + dll_ext);
    fs::path cache_path = cache_dir_ / (name + dll_ext);
    fs::path load_path;

    // Also check if plugins are in a sibling 'plugins' dir (Zig CC build layout)
    auto sibling_plugins = plugins_dir_.parent_path().parent_path() / "plugins";

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
    } else if (fs::exists(lib_path)) {
        // Zig CC / MinGW naming: libtml_test.dll
        load_path = lib_path;
    } else if (fs::exists(sibling_plugins / ("lib" + name + dll_ext))) {
        // Zig CC build: plugins in build/debug/plugins/ (not bin/plugins/)
        load_path = sibling_plugins / ("lib" + name + dll_ext);
    } else if (fs::exists(sibling_plugins / (name + dll_ext))) {
        // MSVC build: plugins in build/debug/plugins/
        load_path = sibling_plugins / (name + dll_ext);
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

void Loader::set_daemon_mode(bool enabled) {
    g_daemon_mode.store(enabled, std::memory_order_relaxed);
}

void Loader::unload_all() {
    bool daemon = g_daemon_mode.load(std::memory_order_relaxed);
    // Shutdown in reverse load order (approximation: just iterate)
    for (auto& [name, plugin] : loaded_) {
        if (plugin.initialized && plugin.shutdown) {
            plugin.shutdown();
        }
        if (plugin.handle) {
            if (daemon) {
                // Daemon mode: keep DLL resident in g_dll_cache so the next
                // request reuses the already-loaded handle. DLLs are only
                // truly unloaded when the daemon process exits.
                // (No FreeLibrary — the OS cleans up at process exit.)
            } else {
                // Normal mode: evict from process-level cache and release
                // the OS handle so DllMain(DLL_PROCESS_DETACH) fires now,
                // under controlled teardown. This avoids static destructor
                // ordering issues at process exit (LLVM global registry
                // destructors crash if the DLL is unloaded by ExitProcess
                // after the C runtime has already cleaned up).
                std::string key = plugin.dll_path.string();
                {
                    std::lock_guard<std::mutex> lock(g_dll_cache_mutex);
                    g_dll_cache.erase(key);
                }
                platform_dl_close(plugin.handle);
            }
        }
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
                crc = (crc >> 1) ^
                      (0x82F63B78 & static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1))));
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

// ============================================================================
// Background preloading
// ============================================================================

void Loader::wait_preload_done() {
    // Spin until all background preload threads have decremented the counter.
    // Each thread is detached so the process can exit cleanly without waiting
    // on a joinable thread. The spin is bounded by DLL load time (~100ms warm).
    while (g_preload_count.load(std::memory_order_acquire) > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

void Loader::preload_async(const std::vector<std::string>& plugin_names) {
    // Discover the plugins directory the same way discover_paths() does.
    auto exe = exe_dir();
    auto primary_dir = exe / "plugins";
    auto sibling_dir = primary_dir.parent_path().parent_path() / "plugins";

#ifdef _WIN32
    const std::string dll_ext = ".dll";
#else
    const std::string dll_ext = ".so";
#endif

    for (const auto& name : plugin_names) {
        // Resolve the DLL path: try the same search order as Loader::load().
        fs::path load_path;
        for (const auto& dir : {primary_dir, sibling_dir}) {
            // MSVC layout: tml_foo.dll
            fs::path p = dir / (name + dll_ext);
            if (fs::exists(p)) { load_path = p; break; }
            // Zig CC / MinGW layout: libtml_foo.dll
            p = dir / ("lib" + name + dll_ext);
            if (fs::exists(p)) { load_path = p; break; }
        }

        if (load_path.empty()) {
            continue; // Plugin not found — nothing to preload.
        }

        // Skip if already in the cache.
        {
            std::lock_guard<std::mutex> lock(g_dll_cache_mutex);
            if (g_dll_cache.count(load_path.string())) {
                continue;
            }
        }

        // Increment counter before spawning so wait_preload_done() sees it.
        g_preload_count.fetch_add(1, std::memory_order_relaxed);

        // Detach the thread so process exit is not blocked waiting on it.
        // g_preload_count tracks completion; wait_preload_done() spins on it.
        fs::path dll_path = load_path;
        std::thread([dll_path]() {
            std::string key = dll_path.string();
            fs::file_time_type mtime = get_path_mtime(dll_path);

            // Check cache again inside the thread — another thread may have beaten us.
            {
                std::lock_guard<std::mutex> lk(g_dll_cache_mutex);
                if (g_dll_cache.count(key)) {
                    g_preload_count.fetch_sub(1, std::memory_order_release);
                    return;
                }
            }

            void* handle = platform_dl_open(dll_path);
            if (handle) {
                std::lock_guard<std::mutex> lk(g_dll_cache_mutex);
                auto [it, inserted] = g_dll_cache.emplace(key, DllCacheEntry{handle, mtime});
                if (!inserted) {
                    platform_dl_close(handle); // Lost race — close our duplicate.
                }
            }
            g_preload_count.fetch_sub(1, std::memory_order_release);
        }).detach();
    }
}

} // namespace tml::plugin
