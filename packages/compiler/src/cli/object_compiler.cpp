#include "object_compiler.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>

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
        case 0: return "-O0";
        case 1: return "-O1";
        case 2: return "-O2";
        case 3: return "-O3";
        default: return "-O3";
    }
}

// Helper to convert path to forward slashes for cross-platform compatibility
static std::string to_forward_slashes(const fs::path& path) {
    std::string result = path.string();
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

ObjectCompileResult compile_ll_to_object(
    const fs::path& ll_file,
    const std::optional<fs::path>& output_file,
    const std::string& clang_path,
    const ObjectCompileOptions& options
) {
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
    cmd << clang_path;  // Don't quote clang path (no spaces in standard paths)
    cmd << " -c";  // Compile only, don't link

    // Optimization level
    cmd << " " << get_optimization_flag(options.optimization_level);

    // Platform-specific flags
#ifdef _WIN32
    // Windows: use native object format
    cmd << " -target x86_64-pc-windows-msvc";
#else
    // Unix: use ELF object format
    cmd << " -target x86_64-unknown-linux-gnu";
#endif

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

LinkResult link_objects(
    const std::vector<fs::path>& object_files,
    const fs::path& output_file,
    const std::string& clang_path,
    const LinkOptions& options
) {
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
            cmd << clang_path;  // Don't quote clang path

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
#ifdef _WIN32
            // Windows: use lib.exe or llvm-ar
            cmd << "lib.exe /OUT:\"" << to_forward_slashes(output_file) << "\"";
            for (const auto& obj : object_files) {
                cmd << " \"" << to_forward_slashes(obj) << "\"";
            }
            for (const auto& obj : options.additional_objects) {
                cmd << " \"" << to_forward_slashes(obj) << "\"";
            }
#else
            // Unix: use ar
            cmd << "ar rcs \"" << to_forward_slashes(output_file) << "\"";
            for (const auto& obj : object_files) {
                cmd << " \"" << to_forward_slashes(obj) << "\"";
            }
            for (const auto& obj : options.additional_objects) {
                cmd << " \"" << to_forward_slashes(obj) << "\"";
            }
#endif
            break;
        }

        case LinkOptions::OutputType::DynamicLib: {
            // Shared library using clang
            cmd << clang_path;  // Don't quote clang path
            cmd << " -shared";

#ifndef _WIN32
            cmd << " -fPIC";
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

} // namespace tml::cli
