#pragma once

/// Platform detection for native library resolution.
///
/// Provides compile-time detection of OS and architecture, plus helpers
/// for constructing platform-specific directory names and file extensions.

#include <string>

namespace tml::cli {

/// Platform information for native library resolution.
struct Platform {
    std::string os;   // "windows", "linux", "macos"
    std::string arch; // "x64", "arm64"

    /// Returns the native directory name: "win-x64", "linux-x64", "macos-arm64"
    std::string native_dir_name() const;

    /// Returns shared library extension: ".dll", ".so", ".dylib"
    std::string shared_lib_ext() const;

    /// Returns static library extension: ".lib" (Windows), ".a" (others)
    std::string static_lib_ext() const;

    /// Returns import library extension: ".lib" (Windows only), "" (others)
    std::string import_lib_ext() const;

    /// Detect the current platform at compile time.
    static Platform detect();
};

} // namespace tml::cli
