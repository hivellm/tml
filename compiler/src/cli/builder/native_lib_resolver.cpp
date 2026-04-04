TML_MODULE("compiler")

//! # Native Library Resolver
//!
//! Central resolver for native (C/C++) libraries needed by TML packages.
//! Replaces all ad-hoc DLL discovery with a tiered search:
//!
//! | Tier | Location | Description |
//! |------|----------|-------------|
//! | 0 | Extra paths | From build.tml link-search directives |
//! | 1 | Project-local | `<project>/lib/<pkg>/native/<platform>/` |
//! | 2 | User cache | `~/.tml/native-cache/<lib>/<version>/<platform>/` |
//! | 3 | Compiler-bundled | `<compiler_dir>/lib/native/<platform>/` |
//! | 4 | vcpkg (compat) | `vcpkg_installed/<triplet>/` |

#include "cli/builder/native_lib_resolver.hpp"

#include "log/log.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

#include <cstdlib>

namespace tml::cli {

NativeLibResolver::NativeLibResolver(const Platform& platform) : platform_(platform) {}

void NativeLibResolver::set_project_root(const fs::path& root) {
    project_root_ = root;
}

void NativeLibResolver::add_search_paths(const std::vector<fs::path>& paths) {
    for (const auto& p : paths) {
        extra_search_paths_.push_back(p);
    }
}

// ============================================================================
// Directory helpers
// ============================================================================

fs::path NativeLibResolver::get_tml_home() {
#ifdef _WIN32
    // Use %LOCALAPPDATA%\tml
    const char* local_app_data = std::getenv("LOCALAPPDATA");
    if (local_app_data) {
        return fs::path(local_app_data) / "tml";
    }
    // Fallback: %USERPROFILE%\.tml
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) {
        return fs::path(userprofile) / ".tml";
    }
    return fs::path("C:/Users/Public/.tml");
#else
    // Use $HOME/.tml
    const char* home = std::getenv("HOME");
    if (home) {
        return fs::path(home) / ".tml";
    }
    // Fallback: passwd entry
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return fs::path(pw->pw_dir) / ".tml";
    }
    return fs::path("/tmp/.tml");
#endif
}

fs::path NativeLibResolver::get_native_cache_dir() {
    return get_tml_home() / "native-cache";
}

fs::path NativeLibResolver::get_compiler_native_dir() const {
    // <exe_dir>/../lib/native/<platform>/
    // Get the running executable's directory
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        fs::path exe_dir = fs::path(std::string(buf, len)).parent_path();
        // exe is in bin/ or bin/plugins/, go up to installation root
        fs::path install_root = exe_dir.parent_path(); // bin/ -> root
        if (install_root.filename() == "bin") {
            install_root = install_root.parent_path();
        }
        return install_root / "lib" / "native" / platform_.native_dir_name();
    }
#else
    std::error_code ec;
    auto exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        fs::path exe_dir = exe.parent_path();
        fs::path install_root = exe_dir.parent_path();
        if (install_root.filename() == "bin") {
            install_root = install_root.parent_path();
        }
        return install_root / "lib" / "native" / platform_.native_dir_name();
    }
#endif
    return {};
}

std::pair<fs::path, fs::path> NativeLibResolver::get_vcpkg_dirs() const {
    // Check common vcpkg locations relative to project root
    fs::path vcpkg_root;
    if (!project_root_.empty()) {
        vcpkg_root = project_root_ / "vcpkg_installed";
    } else {
        vcpkg_root = fs::path("vcpkg_installed");
    }

    std::string triplet;
    if (platform_.os == "windows") {
        triplet = "x64-windows";
    } else if (platform_.os == "linux") {
        triplet = "x64-linux";
    } else {
        triplet = "arm64-osx";
    }

    fs::path lib_dir = vcpkg_root / triplet / "lib";
    fs::path bin_dir = vcpkg_root / triplet / "bin";
    return {lib_dir, bin_dir};
}

// ============================================================================
// Runtime lib discovery
// ============================================================================

std::vector<fs::path> NativeLibResolver::find_runtime_libs(const fs::path& dir,
                                                           const std::string& lib_name) const {
    std::vector<fs::path> result;
    if (!fs::exists(dir))
        return result;

    std::string ext = platform_.shared_lib_ext();
    std::error_code ec;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec)
            break;
        if (!entry.is_regular_file())
            continue;
        auto filename = entry.path().filename().string();

        // Match patterns:
        // Windows: libname.dll, liblibname.dll
        // Linux: libname.so, libname.so.1, libname.so.1.2.3
        // macOS: libname.dylib, libname.1.dylib

        // Exact match
        if (filename == lib_name + ext) {
            result.push_back(entry.path());
            continue;
        }
        // lib prefix match
        if (filename == "lib" + lib_name + ext) {
            result.push_back(entry.path());
            continue;
        }
        // Versioned match (Linux .so.N, macOS .dylib)
        if (platform_.os == "linux" || platform_.os == "macos") {
            if (filename.find(lib_name + ".so") == 0 ||
                filename.find("lib" + lib_name + ".so") == 0 ||
                filename.find(lib_name + ".dylib") == 0 ||
                filename.find("lib" + lib_name + ".dylib") == 0) {
                result.push_back(entry.path());
                continue;
            }
        }
    }
    return result;
}

void NativeLibResolver::collect_all_dlls(const fs::path& dir,
                                         std::vector<fs::path>& runtime_libs) const {
    if (platform_.os != "windows")
        return;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec)
            break;
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".dll")
            continue;

        // Add if not already present
        bool already = false;
        for (const auto& existing : runtime_libs) {
            if (existing.filename() == entry.path().filename()) {
                already = true;
                break;
            }
        }
        if (!already) {
            runtime_libs.push_back(entry.path());
        }
    }
}

// ============================================================================
// Tier resolution
// ============================================================================

std::optional<ResolvedNativeLib>
NativeLibResolver::try_project_local(const std::string& lib_name) const {
    if (project_root_.empty())
        return std::nullopt;

    // Check lib/<pkg>/native/<platform>/
    // Scan all directories under lib/ that have native/<platform>/
    fs::path lib_dir = project_root_ / "lib";
    if (!fs::exists(lib_dir))
        return std::nullopt;

    std::error_code ec;
    for (const auto& pkg_entry : fs::directory_iterator(lib_dir, ec)) {
        if (ec)
            break;
        if (!pkg_entry.is_directory())
            continue;
        fs::path native_dir = pkg_entry.path() / "native" / platform_.native_dir_name();
        if (!fs::exists(native_dir))
            continue;

        // Check for the library file (static or import lib)
        std::string link_ext = platform_.static_lib_ext();
        fs::path link_file = native_dir / (lib_name + link_ext);
        fs::path link_file_lib = native_dir / ("lib" + lib_name + link_ext);

        if (fs::exists(link_file) || fs::exists(link_file_lib)) {
            ResolvedNativeLib resolved;
            resolved.name = lib_name;
            resolved.tier = NativeLibTier::ProjectLocal;
            resolved.search_path = native_dir;
            resolved.link_name = lib_name;
            resolved.runtime_libs = find_runtime_libs(native_dir, lib_name);

            // Also collect ALL DLLs in the directory (transitive deps like libiconv, libintl)
            collect_all_dlls(native_dir, resolved.runtime_libs);

            TML_LOG_DEBUG("compiler", "[native] Found " << lib_name << " in project-local: "
                                                        << native_dir.string());
            return resolved;
        }
    }
    return std::nullopt;
}

std::optional<ResolvedNativeLib>
NativeLibResolver::try_user_cache(const std::string& lib_name) const {
    fs::path cache_dir = get_native_cache_dir() / lib_name;
    if (!fs::exists(cache_dir))
        return std::nullopt;

    // Find the latest version directory (lexicographic last)
    fs::path latest_version;
    std::error_code ec;
    for (const auto& ver_entry : fs::directory_iterator(cache_dir, ec)) {
        if (ec)
            break;
        if (!ver_entry.is_directory())
            continue;
        fs::path platform_dir = ver_entry.path() / platform_.native_dir_name();
        if (fs::exists(platform_dir)) {
            latest_version = platform_dir;
            // Keep iterating to find the lexicographic last version
        }
    }

    if (latest_version.empty())
        return std::nullopt;

    ResolvedNativeLib resolved;
    resolved.name = lib_name;
    resolved.tier = NativeLibTier::UserCache;
    resolved.search_path = latest_version;
    resolved.link_name = lib_name;
    resolved.runtime_libs = find_runtime_libs(latest_version, lib_name);

    TML_LOG_DEBUG("compiler",
                  "[native] Found " << lib_name << " in user cache: " << latest_version.string());
    return resolved;
}

std::optional<ResolvedNativeLib>
NativeLibResolver::try_compiler_bundled(const std::string& lib_name) const {
    fs::path bundled_dir = get_compiler_native_dir();
    if (bundled_dir.empty() || !fs::exists(bundled_dir))
        return std::nullopt;

    std::string link_ext = platform_.static_lib_ext();
    fs::path link_file = bundled_dir / (lib_name + link_ext);
    fs::path link_file_lib = bundled_dir / ("lib" + lib_name + link_ext);

    if (!fs::exists(link_file) && !fs::exists(link_file_lib))
        return std::nullopt;

    ResolvedNativeLib resolved;
    resolved.name = lib_name;
    resolved.tier = NativeLibTier::CompilerBundled;
    resolved.search_path = bundled_dir;
    resolved.link_name = lib_name;
    resolved.runtime_libs = find_runtime_libs(bundled_dir, lib_name);

    TML_LOG_DEBUG("compiler", "[native] Found "
                                  << lib_name << " in compiler-bundled: " << bundled_dir.string());
    return resolved;
}

std::optional<ResolvedNativeLib>
NativeLibResolver::try_vcpkg_compat(const std::string& lib_name) const {
    auto [lib_dir, bin_dir] = get_vcpkg_dirs();
    if (!fs::exists(lib_dir))
        return std::nullopt;

    std::string link_ext = platform_.static_lib_ext();
    fs::path link_file = lib_dir / (lib_name + link_ext);
    fs::path link_file_lib = lib_dir / ("lib" + lib_name + link_ext);

    if (!fs::exists(link_file) && !fs::exists(link_file_lib))
        return std::nullopt;

    ResolvedNativeLib resolved;
    resolved.name = lib_name;
    resolved.tier = NativeLibTier::VcpkgCompat;
    resolved.search_path = lib_dir;
    resolved.link_name = lib_name;
    // Runtime libs are in bin/ on Windows, lib/ on Linux/Mac
    if (platform_.os == "windows" && fs::exists(bin_dir)) {
        resolved.runtime_libs = find_runtime_libs(bin_dir, lib_name);
    } else {
        resolved.runtime_libs = find_runtime_libs(lib_dir, lib_name);
    }

    TML_LOG_DEBUG("compiler", "[native] Found " << lib_name << " in vcpkg: " << lib_dir.string());
    return resolved;
}

std::optional<ResolvedNativeLib>
NativeLibResolver::try_extra_paths(const std::string& lib_name) const {
    for (const auto& dir : extra_search_paths_) {
        if (!fs::exists(dir))
            continue;

        std::string link_ext = platform_.static_lib_ext();
        fs::path link_file = dir / (lib_name + link_ext);
        fs::path link_file_lib = dir / ("lib" + lib_name + link_ext);

        if (fs::exists(link_file) || fs::exists(link_file_lib)) {
            ResolvedNativeLib resolved;
            resolved.name = lib_name;
            resolved.tier = NativeLibTier::ProjectLocal; // Extra paths are project-level
            resolved.search_path = dir;
            resolved.link_name = lib_name;
            resolved.runtime_libs = find_runtime_libs(dir, lib_name);

            // Collect ALL DLLs in the directory (transitive deps)
            collect_all_dlls(dir, resolved.runtime_libs);

            TML_LOG_DEBUG("compiler",
                          "[native] Found " << lib_name << " in extra path: " << dir.string());
            return resolved;
        }
    }
    return std::nullopt;
}

// ============================================================================
// Public API
// ============================================================================

ResolvedNativeLib NativeLibResolver::resolve(const std::string& lib_name) const {
    // Try each tier in order
    if (auto r = try_extra_paths(lib_name))
        return *r;
    if (auto r = try_project_local(lib_name))
        return *r;
    if (auto r = try_user_cache(lib_name))
        return *r;
    if (auto r = try_compiler_bundled(lib_name))
        return *r;
    if (auto r = try_vcpkg_compat(lib_name))
        return *r;

    // Not found
    ResolvedNativeLib not_found;
    not_found.name = lib_name;
    not_found.tier = NativeLibTier::NotFound;
    not_found.link_name = lib_name;
    TML_LOG_DEBUG("compiler", "[native] " << lib_name << " not found in any tier");
    return not_found;
}

std::vector<ResolvedNativeLib>
NativeLibResolver::resolve_from_build_script(const std::set<std::string>& link_libs,
                                             const std::vector<fs::path>& link_search_paths,
                                             const fs::path& package_dir) const {
    // Create a temporary resolver with the build script's search paths added
    NativeLibResolver temp_resolver(platform_);
    temp_resolver.project_root_ = project_root_;
    temp_resolver.extra_search_paths_ = extra_search_paths_;

    // Add build script search paths (resolved relative to package dir)
    for (const auto& sp : link_search_paths) {
        fs::path abs_path = sp.is_absolute() ? sp : package_dir / sp;
        temp_resolver.extra_search_paths_.push_back(abs_path);
    }

    std::vector<ResolvedNativeLib> results;
    for (const auto& lib : link_libs) {
        results.push_back(temp_resolver.resolve(lib));
    }
    return results;
}

void NativeLibResolver::copy_runtime_libs(const std::vector<ResolvedNativeLib>& libs,
                                          const fs::path& target_dir) {
    std::error_code ec;
    fs::create_directories(target_dir, ec);

    std::set<std::string> copied; // Avoid duplicate copies

    for (const auto& lib : libs) {
        for (const auto& runtime_lib : lib.runtime_libs) {
            auto filename = runtime_lib.filename().string();
            if (copied.count(filename))
                continue;
            copied.insert(filename);

            fs::path dest = target_dir / runtime_lib.filename();
            if (fs::exists(dest)) {
                // Skip if same size (likely same file)
                auto src_size = fs::file_size(runtime_lib, ec);
                if (ec)
                    continue;
                auto dst_size = fs::file_size(dest, ec);
                if (!ec && src_size == dst_size)
                    continue;
                ec.clear();
            }

            fs::copy_file(runtime_lib, dest, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                TML_LOG_DEBUG("compiler",
                              "[native] Copied " << filename << " -> " << target_dir.string());
            }
        }
    }
}

} // namespace tml::cli
