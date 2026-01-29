//! # Compiler Setup and Toolchain Discovery
//!
//! This file handles discovery and configuration of external build tools.
//!
//! ## Toolchain Components
//!
//! | Tool        | Purpose                         | Search Locations           |
//! |-------------|--------------------------------|----------------------------|
//! | Clang       | LLVM IR to object compilation  | PATH, LLVM install dirs    |
//! | MSVC        | Windows SDK and linker         | Visual Studio paths        |
//! | LLD         | LLVM linker (optional)         | With clang installation    |
//!
//! ## C Runtime Compilation
//!
//! `ensure_c_compiled()` compiles C runtime files with caching:
//! - Checks if .obj exists and is newer than .c source
//! - Uses clang to compile with appropriate flags
//! - Thread-safe to avoid duplicate compilation
//!
//! ## Windows-Specific
//!
//! - `find_msvc()`: Locates Visual Studio installation
//! - Detects VS 2019/2022, Community/Professional/Enterprise editions
//! - Handles x64 vs x86 library paths

#include "compiler_setup.hpp"

#include "cli/utils.hpp"
#include "common.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;

namespace tml::cli {

// Thread-safe compilation mutex (per-file basis)
static std::mutex compilation_mutex;
static std::map<std::string, bool> compilation_in_progress;

/// Quotes a command path if it contains spaces.
static std::string quote_command(const std::string& cmd) {
    if (cmd.find(' ') != std::string::npos) {
        return "\"" + cmd + "\"";
    }
    return cmd;
}

#ifdef _WIN32
MSVCInfo find_msvc() {
    MSVCInfo info;
    std::vector<std::string> vs_bases = {
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC",
        "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC",
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC",
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC",
        "C:/Program Files/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC",
        "C:/Program Files/Microsoft Visual Studio/2019/Professional/VC/Tools/MSVC",
        "C:/Program Files/Microsoft Visual Studio/2019/Enterprise/VC/Tools/MSVC",
        "C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Tools/MSVC",
    };

    std::string msvc_base;
    std::string msvc_ver;
    for (const auto& vs_path : vs_bases) {
        if (fs::exists(vs_path)) {
            for (const auto& entry : fs::directory_iterator(vs_path)) {
                if (entry.is_directory()) {
                    std::string ver = entry.path().filename().string();
                    if (msvc_ver.empty() || ver > msvc_ver) {
                        msvc_ver = ver;
                        msvc_base = vs_path;
                    }
                }
            }
        }
    }

    if (!msvc_ver.empty()) {
        std::string msvc_path = msvc_base + "/" + msvc_ver;
        std::string cl_x64 = msvc_path + "/bin/Hostx64/x64/cl.exe";
        std::string cl_x86 = msvc_path + "/bin/Hostx86/x86/cl.exe";
        if (fs::exists(cl_x64)) {
            info.cl_path = cl_x64;
        } else if (fs::exists(cl_x86)) {
            info.cl_path = cl_x86;
        }

        std::string inc = msvc_path + "/include";
        if (fs::exists(inc)) {
            info.includes.push_back(inc);
        }

        std::string lib_x64 = msvc_path + "/lib/x64";
        std::string lib_x86 = msvc_path + "/lib/x86";
        if (fs::exists(cl_x64) && fs::exists(lib_x64)) {
            info.libs.push_back(lib_x64);
        } else if (fs::exists(lib_x86)) {
            info.libs.push_back(lib_x86);
        }
    }

    std::string sdk_base = "C:/Program Files (x86)/Windows Kits/10";
    if (fs::exists(sdk_base + "/Include")) {
        std::string sdk_ver;
        for (const auto& entry : fs::directory_iterator(sdk_base + "/Include")) {
            if (entry.is_directory()) {
                std::string ver = entry.path().filename().string();
                if (ver.find("10.") == 0 && (sdk_ver.empty() || ver > sdk_ver)) {
                    sdk_ver = ver;
                }
            }
        }
        if (!sdk_ver.empty()) {
            std::string inc_base = sdk_base + "/Include/" + sdk_ver;
            if (fs::exists(inc_base + "/ucrt"))
                info.includes.push_back(inc_base + "/ucrt");
            if (fs::exists(inc_base + "/shared"))
                info.includes.push_back(inc_base + "/shared");
            if (fs::exists(inc_base + "/um"))
                info.includes.push_back(inc_base + "/um");

            std::string lib_base = sdk_base + "/Lib/" + sdk_ver;
            bool use_x64 = info.cl_path.find("x64") != std::string::npos;
            std::string arch = use_x64 ? "x64" : "x86";
            if (fs::exists(lib_base + "/ucrt/" + arch))
                info.libs.push_back(lib_base + "/ucrt/" + arch);
            if (fs::exists(lib_base + "/um/" + arch))
                info.libs.push_back(lib_base + "/um/" + arch);
        }
    }

    return info;
}
#endif

std::string find_clang() {
    std::string clang = "clang";
#ifdef _WIN32
    std::vector<std::string> clang_paths = {
        "F:/LLVM/bin/clang.exe",
        "C:/Program Files/LLVM/bin/clang.exe",
        "C:/LLVM/bin/clang.exe",
    };
    for (const auto& p : clang_paths) {
        if (fs::exists(p)) {
            return p;
        }
    }
#endif
    return clang;
}

std::string find_runtime() {
    std::vector<std::string> runtime_search = {
        "compiler/runtime/essential.c",
        "runtime/essential.c",
        "../runtime/essential.c",
        "../../runtime/essential.c",
        "F:/Node/hivellm/tml/compiler/runtime/essential.c",
    };
    for (const auto& rp : runtime_search) {
        if (fs::exists(rp)) {
            return to_forward_slashes(fs::absolute(rp).string());
        }
    }
    return "";
}

std::string find_runtime_library() {
    // Search for pre-compiled runtime library
    // Priority: same dir as executable > build dir > known locations
#ifdef _WIN32
    std::string lib_name = "tml_runtime.lib";
#else
    std::string lib_name = "libtml_runtime.a";
#endif

    std::vector<std::string> search_paths = {
        // Same directory as the executable (standard distribution)
        ".",
        // Build output directories
        "build/debug",
        "build/release",
        "../build/debug",
        "../build/release",
        // Development paths
        "F:/Node/hivellm/tml/build/debug",
        "F:/Node/hivellm/tml/build/release",
    };

    for (const auto& path : search_paths) {
        fs::path lib_path = fs::path(path) / lib_name;
        if (fs::exists(lib_path)) {
            return to_forward_slashes(fs::absolute(lib_path).string());
        }
    }

    return "";
}

bool is_precompiled_runtime_available() {
    return !find_runtime_library().empty();
}

std::string ensure_runtime_compiled(const std::string& runtime_c_path, const std::string& clang,
                                    bool verbose) {
    fs::path c_path = runtime_c_path;
    fs::path obj_path = c_path.parent_path() / "essential";
#ifdef _WIN32
    obj_path += ".obj";
#else
    obj_path += ".o";
#endif

    bool needs_compile = !fs::exists(obj_path);
    if (!needs_compile) {
        auto c_time = fs::last_write_time(c_path);
        auto obj_time = fs::last_write_time(obj_path);
        needs_compile = (c_time > obj_time);
    }

    if (needs_compile) {
        if (verbose) {
            std::cout << "Pre-compiling runtime: " << c_path << "\n";
        }
        // Build compile command with appropriate flags
        // -fms-extensions: Enable MSVC extensions (SEH __try/__except) on Windows
        std::string compile_cmd = quote_command(clang) +
#ifdef _WIN32
                                  " -c -O3 -fms-extensions -march=native -mtune=native "
                                  "-fomit-frame-pointer -funroll-loops -o \"" +
#else
                                  " -c -O3 -march=native -mtune=native -fomit-frame-pointer "
                                  "-funroll-loops -o \"" +
#endif
                                  to_forward_slashes(obj_path.string()) + "\" \"" +
                                  to_forward_slashes(c_path.string()) + "\"";
        int ret = std::system(compile_cmd.c_str());
        if (ret != 0) {
            return runtime_c_path;
        }
    }

    return to_forward_slashes(obj_path.string());
}

std::string ensure_c_compiled(const std::string& c_path_str, const std::string& cache_dir,
                              const std::string& clang, bool verbose,
                              const std::string& extra_flags) {
    fs::path c_path = c_path_str;

    // Create cache directory if needed
    fs::path cache_path = cache_dir;
    fs::create_directories(cache_path);

    // Generate object file name from source file name
    // Include hash of extra_flags in filename to avoid collisions
    std::string base_name = c_path.stem().string();
    if (!extra_flags.empty()) {
        // Simple hash for flag suffix (use first 8 chars of flags, sanitized)
        std::string flag_suffix;
        for (char c : extra_flags) {
            if (std::isalnum(c) && flag_suffix.size() < 8) {
                flag_suffix += c;
            }
        }
        if (!flag_suffix.empty()) {
            base_name += "_" + flag_suffix;
        }
    }
    fs::path obj_path = cache_path / base_name;
#ifdef _WIN32
    obj_path += ".obj";
#else
    obj_path += ".o";
#endif

    std::string obj_path_str = to_forward_slashes(obj_path.string());

    // Thread-safe check and compilation
    bool should_compile = false;
    {
        std::lock_guard<std::mutex> lock(compilation_mutex);

        // If another thread is compiling, wait by checking if file exists after lock
        bool needs_compile = !fs::exists(obj_path);
        if (!needs_compile) {
            auto c_time = fs::last_write_time(c_path);
            auto obj_time = fs::last_write_time(obj_path);
            needs_compile = (c_time > obj_time);
        }

        if (needs_compile && !compilation_in_progress[obj_path_str]) {
            compilation_in_progress[obj_path_str] = true;
            should_compile = true;
        }
    }

    if (should_compile) {
        if (verbose) {
            std::cout << "Compiling: " << c_path.filename().string() << " -> "
                      << obj_path.filename().string();
            if (!extra_flags.empty()) {
                std::cout << " (flags: " << extra_flags << ")";
            }
            std::cout << "\n";
        }
        // Build compile command with appropriate flags
        // -fms-extensions: Enable MSVC extensions (SEH __try/__except) on Windows
        // -D_CRT_SECURE_NO_WARNINGS: Suppress MSVC CRT deprecation warnings (strncpy, fopen, etc.)
        std::string compile_cmd = quote_command(clang) +
#ifdef _WIN32
                                  " -c -O3 -fms-extensions -D_CRT_SECURE_NO_WARNINGS "
                                  "-march=native -mtune=native "
                                  "-fomit-frame-pointer -funroll-loops " +
#else
                                  " -c -O3 -march=native -mtune=native -fomit-frame-pointer "
                                  "-funroll-loops " +
#endif
                                  extra_flags + " -o \"" + obj_path_str + "\" \"" +
                                  to_forward_slashes(c_path.string()) + "\"";
        int ret = std::system(compile_cmd.c_str());

        // Mark compilation as done
        {
            std::lock_guard<std::mutex> lock(compilation_mutex);
            compilation_in_progress.erase(obj_path_str);
        }

        if (ret != 0) {
            // Fallback to returning .c path on compile failure
            return c_path_str;
        }
    } else {
        // Another thread compiled or is compiling - wait for file to exist
        int retries = 0;
        while (!fs::exists(obj_path) && retries < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            retries++;
        }
    }

    return obj_path_str;
}

// ============================================================================
// LLVM Coverage Tools
// ============================================================================

std::string find_llvm_profdata() {
#ifdef _WIN32
    std::string exe_name = "llvm-profdata.exe";
#else
    std::string exe_name = "llvm-profdata";
#endif

    // Search locations (same as clang)
    std::vector<std::string> search_paths = {
        "F:/LLVM/bin/" + exe_name,
        "C:/Program Files/LLVM/bin/" + exe_name,
        "C:/LLVM/bin/" + exe_name,
    };

    for (const auto& p : search_paths) {
        if (fs::exists(p)) {
            return p;
        }
    }

    // Try to find alongside clang
    std::string clang = find_clang();
    if (!clang.empty()) {
        fs::path clang_dir = fs::path(clang).parent_path();
        fs::path profdata_path = clang_dir / exe_name;
        if (fs::exists(profdata_path)) {
            return profdata_path.string();
        }
    }

#ifndef _WIN32
    // Unix: try system path
    return "llvm-profdata";
#else
    return "";
#endif
}

std::string find_llvm_cov() {
#ifdef _WIN32
    std::string exe_name = "llvm-cov.exe";
#else
    std::string exe_name = "llvm-cov";
#endif

    // Search locations (same as clang)
    std::vector<std::string> search_paths = {
        "F:/LLVM/bin/" + exe_name,
        "C:/Program Files/LLVM/bin/" + exe_name,
        "C:/LLVM/bin/" + exe_name,
    };

    for (const auto& p : search_paths) {
        if (fs::exists(p)) {
            return p;
        }
    }

    // Try to find alongside clang
    std::string clang = find_clang();
    if (!clang.empty()) {
        fs::path clang_dir = fs::path(clang).parent_path();
        fs::path cov_path = clang_dir / exe_name;
        if (fs::exists(cov_path)) {
            return cov_path.string();
        }
    }

#ifndef _WIN32
    // Unix: try system path
    return "llvm-cov";
#else
    return "";
#endif
}

bool is_llvm_coverage_available() {
    std::string profdata = find_llvm_profdata();
    std::string cov = find_llvm_cov();
    return !profdata.empty() && !cov.empty();
}

std::string find_llvm_profile_runtime() {
    // Find clang_rt.profile library for coverage instrumentation
    std::string clang = find_clang();
    if (clang.empty()) {
        return "";
    }

    fs::path clang_dir = fs::path(clang).parent_path();
    fs::path llvm_root = clang_dir.parent_path();

    // Get clang version by running clang --print-resource-dir
    // or by checking lib/clang/*/lib/windows/
#ifdef _WIN32
    // Windows: look for clang_rt.profile-x86_64.lib
    std::vector<fs::path> search_paths;

    // Try lib/clang/*/lib/windows/ pattern
    fs::path clang_lib = llvm_root / "lib" / "clang";
    if (fs::exists(clang_lib)) {
        for (const auto& entry : fs::directory_iterator(clang_lib)) {
            if (entry.is_directory()) {
                fs::path profile_lib =
                    entry.path() / "lib" / "windows" / "clang_rt.profile-x86_64.lib";
                if (fs::exists(profile_lib)) {
                    return profile_lib.string();
                }
            }
        }
    }

    // Fallback: try direct paths
    search_paths = {
        llvm_root / "lib" / "clang" / "21" / "lib" / "windows" / "clang_rt.profile-x86_64.lib",
        llvm_root / "lib" / "clang" / "20" / "lib" / "windows" / "clang_rt.profile-x86_64.lib",
        llvm_root / "lib" / "clang" / "19" / "lib" / "windows" / "clang_rt.profile-x86_64.lib",
        llvm_root / "lib" / "clang" / "18" / "lib" / "windows" / "clang_rt.profile-x86_64.lib",
    };

    for (const auto& path : search_paths) {
        if (fs::exists(path)) {
            return path.string();
        }
    }
#else
    // Unix: look for libclang_rt.profile-*.a
    fs::path clang_lib = llvm_root / "lib" / "clang";
    if (fs::exists(clang_lib)) {
        for (const auto& entry : fs::directory_iterator(clang_lib)) {
            if (entry.is_directory()) {
                fs::path lib_dir = entry.path() / "lib";
                // Try various platform subdirs
                for (const auto& subdir : {"linux", "darwin", ""}) {
                    fs::path search_dir = lib_dir / subdir;
                    if (fs::exists(search_dir)) {
                        for (const auto& lib_entry : fs::directory_iterator(search_dir)) {
                            std::string name = lib_entry.path().filename().string();
                            if (name.find("profile") != std::string::npos &&
                                (name.find("x86_64") != std::string::npos ||
                                 name.find("aarch64") != std::string::npos)) {
                                return lib_entry.path().string();
                            }
                        }
                    }
                }
            }
        }
    }
#endif

    return "";
}

} // namespace tml::cli
