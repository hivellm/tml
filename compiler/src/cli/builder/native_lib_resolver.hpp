#pragma once

/// Central native library resolver — replaces all ad-hoc DLL discovery.
///
/// Search order per library:
/// 1. Extra paths (from build.tml link-search directives)
/// 2. Project-local: `<project>/lib/<pkg>/native/<platform>/`
/// 3. User cache: `~/.tml/native-cache/<lib>/<version>/<platform>/`
/// 4. Compiler-bundled: `<compiler_dir>/lib/native/<platform>/`
/// 5. vcpkg (compat): `vcpkg_installed/x64-windows/` etc.

#include "cli/builder/platform.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

/// Which tier resolved a native library.
enum class NativeLibTier {
    ProjectLocal,    // <project>/lib/<pkg>/native/<platform>/ or extra paths
    UserCache,       // ~/.tml/native-cache/<lib>/<version>/<platform>/
    CompilerBundled, // <compiler_dir>/lib/native/<platform>/
    VcpkgCompat,     // vcpkg_installed/<triplet>/ (backward compat)
    NotFound
};

/// A resolved native library with paths for link-time and run-time.
struct ResolvedNativeLib {
    std::string name;
    NativeLibTier tier = NativeLibTier::NotFound;
    fs::path search_path;               // Directory containing the lib (for -L flag)
    std::string link_name;              // Library name (for -l flag)
    std::vector<fs::path> runtime_libs; // .dll/.so/.dylib files to copy at runtime
};

/// Central native library resolver — replaces all ad-hoc DLL discovery.
///
/// Search order per library:
/// 1. Extra paths (from build.tml link-search directives)
/// 2. Project-local: `<project>/lib/<pkg>/native/<platform>/`
/// 3. User cache: `~/.tml/native-cache/<lib>/<version>/<platform>/`
/// 4. Compiler-bundled: `<compiler_dir>/lib/native/<platform>/`
/// 5. vcpkg (compat): `vcpkg_installed/x64-windows/` etc.
class NativeLibResolver {
public:
    explicit NativeLibResolver(const Platform& platform);

    /// Set the project root directory (for project-local resolution).
    void set_project_root(const fs::path& root);

    /// Resolve a single native library by name.
    /// Searches all tiers in order and returns the first match.
    ResolvedNativeLib resolve(const std::string& lib_name) const;

    /// Resolve all native libs for a package that has build.tml output.
    /// Takes the link_libs and link_search_paths from BuildScriptResult.
    std::vector<ResolvedNativeLib>
    resolve_from_build_script(const std::set<std::string>& link_libs,
                              const std::vector<fs::path>& link_search_paths,
                              const fs::path& package_dir) const;

    /// Copy all runtime libraries to a target directory.
    /// Skips files that already exist with same size.
    static void copy_runtime_libs(const std::vector<ResolvedNativeLib>& libs,
                                  const fs::path& target_dir);

    /// Get the TML home directory.
    /// Windows: %LOCALAPPDATA%\tml   Linux/Mac: ~/.tml
    static fs::path get_tml_home();

    /// Get the native cache directory: <tml_home>/native-cache/
    static fs::path get_native_cache_dir();

    /// Get the compiler's native lib directory: <exe_dir>/../lib/native/<platform>/
    fs::path get_compiler_native_dir() const;

    /// Get the vcpkg lib/bin directories (backward compat).
    std::pair<fs::path, fs::path> get_vcpkg_dirs() const;

    /// Add extra search paths (from build.tml link-search directives).
    void add_search_paths(const std::vector<fs::path>& paths);

    /// Get the current platform.
    const Platform& platform() const {
        return platform_;
    }

private:
    Platform platform_;
    fs::path project_root_;
    std::vector<fs::path> extra_search_paths_;

    // Try each tier in order
    std::optional<ResolvedNativeLib> try_project_local(const std::string& lib_name) const;
    std::optional<ResolvedNativeLib> try_user_cache(const std::string& lib_name) const;
    std::optional<ResolvedNativeLib> try_compiler_bundled(const std::string& lib_name) const;
    std::optional<ResolvedNativeLib> try_vcpkg_compat(const std::string& lib_name) const;
    std::optional<ResolvedNativeLib> try_extra_paths(const std::string& lib_name) const;

    // Find all shared libs in a directory matching a library name
    std::vector<fs::path> find_runtime_libs(const fs::path& dir, const std::string& lib_name) const;

    // Collect all DLLs in a directory (for transitive dependency discovery on Windows)
    void collect_all_dlls(const fs::path& dir, std::vector<fs::path>& runtime_libs) const;
};

} // namespace tml::cli
