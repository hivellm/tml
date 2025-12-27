#ifndef TML_CLI_OBJECT_COMPILER_HPP
#define TML_CLI_OBJECT_COMPILER_HPP

#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace tml::cli {

/**
 * Object file compilation result
 */
struct ObjectCompileResult {
    bool success;
    fs::path object_file;
    std::string error_message;
};

/**
 * Linker result
 */
struct LinkResult {
    bool success;
    fs::path output_file;
    std::string error_message;
};

/**
 * Compilation options for object file generation
 */
struct ObjectCompileOptions {
    int optimization_level = 3;        // -O3 by default
    bool debug_info = false;           // Include debug information
    bool position_independent = false; // -fPIC for shared libraries
    bool verbose = false;              // Print commands
};

/**
 * Linker options
 */
struct LinkOptions {
    enum class OutputType {
        Executable, // .exe
        StaticLib,  // .a/.lib
        DynamicLib  // .so/.dll
    };

    OutputType output_type = OutputType::Executable;
    bool verbose = false;
    std::vector<fs::path> additional_objects; // Runtime libs, etc.
    std::vector<std::string> link_flags;
};

/**
 * Compile LLVM IR (.ll) to object file (.o/.obj)
 *
 * @param ll_file Path to LLVM IR file
 * @param output_file Optional output path (auto-generated if not provided)
 * @param clang_path Path to clang executable
 * @param options Compilation options
 * @return Compilation result with object file path or error
 */
ObjectCompileResult compile_ll_to_object(const fs::path& ll_file,
                                         const std::optional<fs::path>& output_file,
                                         const std::string& clang_path,
                                         const ObjectCompileOptions& options);

/**
 * Batch compilation result
 */
struct BatchCompileResult {
    bool success;
    std::vector<fs::path> object_files;
    std::vector<std::string> errors;
};

/**
 * Compile multiple LLVM IR files in parallel
 *
 * @param ll_files Paths to LLVM IR files
 * @param clang_path Path to clang executable
 * @param options Compilation options
 * @param num_threads Number of parallel threads (0 = auto)
 * @return Batch compilation result
 */
BatchCompileResult compile_ll_batch(const std::vector<fs::path>& ll_files,
                                    const std::string& clang_path,
                                    const ObjectCompileOptions& options, int num_threads = 0);

/**
 * Link object files to create final output
 *
 * @param object_files List of .o/.obj files to link
 * @param output_file Final output path (.exe, .a, .so, .dll)
 * @param clang_path Path to clang/linker executable
 * @param options Link options
 * @return Link result with output file path or error
 */
LinkResult link_objects(const std::vector<fs::path>& object_files, const fs::path& output_file,
                        const std::string& clang_path, const LinkOptions& options);

/**
 * Get default object file extension for current platform
 * Windows: .obj, Unix: .o
 */
std::string get_object_extension();

/**
 * Build optimization flag string from level
 * 0 -> -O0 (no optimization)
 * 1 -> -O1 (basic optimization)
 * 2 -> -O2 (moderate optimization)
 * 3 -> -O3 (aggressive optimization)
 * 4 -> -Os (optimize for size)
 * 5 -> -Oz (optimize for size, aggressive)
 */
std::string get_optimization_flag(int level);

} // namespace tml::cli

#endif // TML_CLI_OBJECT_COMPILER_HPP
