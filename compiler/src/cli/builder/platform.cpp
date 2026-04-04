TML_MODULE("compiler")

//! # Platform Detection
//!
//! Compile-time detection of OS and architecture for native library resolution.
//! Used by NativeLibResolver to locate platform-specific library directories.

#include "cli/builder/platform.hpp"

namespace tml::cli {

std::string Platform::native_dir_name() const {
    if (os == "windows")
        return "win-" + arch;
    if (os == "macos")
        return "macos-" + arch;
    return os + "-" + arch; // "linux-x64"
}

std::string Platform::shared_lib_ext() const {
    if (os == "windows")
        return ".dll";
    if (os == "macos")
        return ".dylib";
    return ".so";
}

std::string Platform::static_lib_ext() const {
    if (os == "windows")
        return ".lib";
    return ".a";
}

std::string Platform::import_lib_ext() const {
    if (os == "windows")
        return ".lib";
    return "";
}

Platform Platform::detect() {
    Platform p;
#ifdef _WIN32
    p.os = "windows";
#elif defined(__APPLE__)
    p.os = "macos";
#else
    p.os = "linux";
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    p.arch = "arm64";
#else
    p.arch = "x64";
#endif
    return p;
}

} // namespace tml::cli
