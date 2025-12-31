#include "object_compiler.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tml::cli {

std::string get_object_extension() {
#ifdef _WIN32
    return ".obj";
#else
    return ".o";
#endif
}

std::string get_optimization_flag(int level) {
    switch (level) {
    case 0:
        return "-O0";
    case 1:
        return "-O1";
    case 2:
        return "-O2";
    case 3:
        return "-O3";
    case 4:
        return "-Os"; // Optimize for size
    case 5:
        return "-Oz"; // Optimize for size (aggressive)
    default:
        return "-O3";
    }
}

// Helper to convert path to forward slashes for cross-platform compatibility
static std::string to_forward_slashes(const fs::path& path) {
    std::string result = path.string();
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

// Helper to quote a command path only if it contains spaces
static std::string quote_command(const std::string& cmd) {
    if (cmd.find(' ') != std::string::npos) {
        return "\"" + cmd + "\"";
    }
    return cmd;
}

ObjectCompileResult compile_ll_to_object(const fs::path& ll_file,
                                         const std::optional<fs::path>& output_file,
                                         const std::string& clang_path,
                                         const ObjectCompileOptions& options) {
    ObjectCompileResult result;
    result.success = false;

    // Verify input file exists
    if (!fs::exists(ll_file)) {
        result.error_message = "LLVM IR file not found: " + ll_file.string();
        return result;
    }

    // Determine output file path
    fs::path obj_file;
    if (output_file.has_value()) {
        obj_file = output_file.value();
    } else {
        // Auto-generate: same name as .ll but with .o/.obj extension
        obj_file = ll_file;
        obj_file.replace_extension(get_object_extension());
    }

    // Build clang command
    std::ostringstream cmd;
    cmd << quote_command(clang_path); // Quote path only if it has spaces
    cmd << " -c";                     // Compile only, don't link

    // Optimization level
    cmd << " " << get_optimization_flag(options.optimization_level);

    // Target triple (use provided or default to host)
    if (!options.target_triple.empty()) {
        cmd << " -target " << options.target_triple;
    } else {
#ifdef _WIN32
        // Windows: use native object format
        cmd << " -target x86_64-pc-windows-msvc";
#else
        // Unix: use ELF object format
        cmd << " -target x86_64-unknown-linux-gnu";
#endif
    }

    // Sysroot for cross-compilation
    if (!options.sysroot.empty()) {
        cmd << " --sysroot=\"" << to_forward_slashes(fs::path(options.sysroot)) << "\"";
    }

    // Position-independent code for shared libraries
    if (options.position_independent) {
        cmd << " -fPIC";
    }

    // Debug information
    if (options.debug_info) {
        cmd << " -g";
    }

    // Additional flags for better codegen
    cmd << " -march=native";
    cmd << " -mtune=native";
    cmd << " -fomit-frame-pointer";
    cmd << " -funroll-loops";

    // Link-Time Optimization
    if (options.lto) {
        if (options.thin_lto) {
            cmd << " -flto=thin";
        } else {
            cmd << " -flto";
        }
    }

    // Suppress warnings
    cmd << " -Wno-override-module";

    // Input and output
    cmd << " -o \"" << to_forward_slashes(obj_file) << "\"";
    cmd << " \"" << to_forward_slashes(ll_file) << "\"";

    std::string command = cmd.str();

    if (options.verbose) {
        std::cout << "[object_compiler] " << command << "\n";
    }

    // Execute compilation
    int ret = std::system(command.c_str());

    if (ret != 0) {
        result.error_message = "Clang compilation failed with exit code " + std::to_string(ret);
        return result;
    }

    // Verify object file was created
    if (!fs::exists(obj_file)) {
        result.error_message = "Object file was not created: " + obj_file.string();
        return result;
    }

    result.success = true;
    result.object_file = obj_file;
    return result;
}

LinkResult link_objects(const std::vector<fs::path>& object_files, const fs::path& output_file,
                        const std::string& clang_path, const LinkOptions& options) {
    LinkResult result;
    result.success = false;

    // Verify at least one object file provided
    if (object_files.empty()) {
        result.error_message = "No object files provided for linking";
        return result;
    }

    // Verify all object files exist
    for (const auto& obj : object_files) {
        if (!fs::exists(obj)) {
            result.error_message = "Object file not found: " + obj.string();
            return result;
        }
    }

    std::ostringstream cmd;

    // Different linking strategies based on output type
    switch (options.output_type) {
    case LinkOptions::OutputType::Executable: {
        // Link executable using clang
        cmd << quote_command(clang_path); // Quote path only if it has spaces

        // Target triple for cross-compilation
        if (!options.target_triple.empty()) {
            cmd << " -target " << options.target_triple;
        }

        // Sysroot for cross-compilation
        if (!options.sysroot.empty()) {
            cmd << " --sysroot=\"" << to_forward_slashes(fs::path(options.sysroot)) << "\"";
        }

        // Link-Time Optimization
        if (options.lto) {
            if (options.thin_lto) {
                cmd << " -flto=thin";
            } else {
                cmd << " -flto";
            }
            // Parallel LTO jobs
            if (options.lto_jobs > 0) {
                cmd << " -flto-jobs=" << options.lto_jobs;
            }
            // Use LLD for faster LTO linking
            cmd << " -fuse-ld=lld";
        }

        // Output file
        cmd << " -o \"" << to_forward_slashes(output_file) << "\"";

        // All object files
        for (const auto& obj : object_files) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }

        // Additional objects (runtime libs)
        for (const auto& obj : options.additional_objects) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }

        // Additional link flags
        for (const auto& flag : options.link_flags) {
            cmd << " " << flag;
        }

        break;
    }

    case LinkOptions::OutputType::StaticLib: {
        // Use llvm-ar for cross-platform static library creation
        // Fall back to system ar on Unix if llvm-ar not available
        fs::path clang_dir = fs::path(clang_path).parent_path();
        fs::path llvm_ar = clang_dir / "llvm-ar";
#ifdef _WIN32
        llvm_ar += ".exe";
#endif

        std::string ar_cmd;
        if (fs::exists(llvm_ar)) {
            ar_cmd = to_forward_slashes(llvm_ar.string());
        } else {
#ifdef _WIN32
            // On Windows, llvm-ar should be available with LLVM
            ar_cmd = to_forward_slashes(llvm_ar.string());
#else
            // On Unix, fall back to system ar
            ar_cmd = "ar";
#endif
        }

        cmd << ar_cmd;
        cmd << " rcs \"" << to_forward_slashes(output_file) << "\"";

        for (const auto& obj : object_files) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }
        for (const auto& obj : options.additional_objects) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }
        break;
    }

    case LinkOptions::OutputType::DynamicLib: {
        // Shared library using clang
        cmd << quote_command(clang_path); // Quote path only if it has spaces
        cmd << " -shared";

        // Target triple for cross-compilation
        if (!options.target_triple.empty()) {
            cmd << " -target " << options.target_triple;
        }

        // Sysroot for cross-compilation
        if (!options.sysroot.empty()) {
            cmd << " --sysroot=\"" << to_forward_slashes(fs::path(options.sysroot)) << "\"";
        }

        // Link-Time Optimization for shared libraries
        if (options.lto) {
            if (options.thin_lto) {
                cmd << " -flto=thin";
            } else {
                cmd << " -flto";
            }
            if (options.lto_jobs > 0) {
                cmd << " -flto-jobs=" << options.lto_jobs;
            }
        }

#ifdef _WIN32
        // Windows: use LLD linker
        cmd << " -fuse-ld=lld";
        // Export all symbols from DLL (MSVC-style flag)
        cmd << " -Wl,-export-all-symbols";
        // Create import library alongside DLL (MSVC-style flag)
        fs::path lib_file = output_file;
        lib_file.replace_extension(".lib");
        cmd << " -Wl,-implib:" << to_forward_slashes(lib_file);
#else
        // Unix: position-independent code required for shared libraries
        cmd << " -fPIC";
        if (options.lto) {
            cmd << " -fuse-ld=lld";
        }
#endif

        // Output file
        cmd << " -o \"" << to_forward_slashes(output_file) << "\"";

        // All object files
        for (const auto& obj : object_files) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }

        // Additional objects
        for (const auto& obj : options.additional_objects) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }

        // Additional link flags
        for (const auto& flag : options.link_flags) {
            cmd << " " << flag;
        }

        break;
    }
    }

    std::string command = cmd.str();

    if (options.verbose) {
        std::cout << "[linker] " << command << "\n";
    }

    // Execute linking
    int ret = std::system(command.c_str());

    if (ret != 0) {
        result.error_message = "Linking failed with exit code " + std::to_string(ret);
        return result;
    }

    // Verify output file was created
    if (!fs::exists(output_file)) {
        result.error_message = "Output file was not created: " + output_file.string();
        return result;
    }

    result.success = true;
    result.output_file = output_file;
    return result;
}

// ============================================================================
// Batch Compilation
// ============================================================================

BatchCompileResult compile_ll_batch(const std::vector<fs::path>& ll_files,
                                    const std::string& clang_path,
                                    const ObjectCompileOptions& options, int num_threads) {
    BatchCompileResult result;
    result.success = true;

    if (ll_files.empty()) {
        return result;
    }

    // Determine number of threads
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0)
            num_threads = 4;
    }

    // Limit threads to number of files
    if (ll_files.size() < static_cast<size_t>(num_threads)) {
        num_threads = static_cast<int>(ll_files.size());
    }

    // Thread-safe result collection
    std::mutex result_mutex;
    std::atomic<size_t> current_index{0};

    // Worker function
    auto worker = [&]() {
        while (true) {
            size_t index = current_index.fetch_add(1);
            if (index >= ll_files.size()) {
                break;
            }

            const auto& ll_file = ll_files[index];

            // Compile this file
            auto compile_result = compile_ll_to_object(ll_file, std::nullopt, clang_path, options);

            // Store result
            std::lock_guard<std::mutex> lock(result_mutex);
            if (compile_result.success) {
                result.object_files.push_back(compile_result.object_file);
            } else {
                result.success = false;
                result.errors.push_back(compile_result.error_message);
            }
        }
    };

    // Launch worker threads
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back(worker);
    }

    // Wait for completion
    for (auto& thread : workers) {
        thread.join();
    }

    return result;
}

} // namespace tml::cli
