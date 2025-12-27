#pragma once
#include <string>
#include <vector>

namespace tml::cli {

#ifdef _WIN32
// MSVC compiler information
struct MSVCInfo {
    std::string cl_path;
    std::vector<std::string> includes;
    std::vector<std::string> libs;
};

// Find Visual Studio and Windows SDK paths
MSVCInfo find_msvc();
#endif

// Find clang compiler (cross-platform)
std::string find_clang();

// Ensure runtime is compiled (pre-compile .c to .o/.obj for faster linking)
std::string ensure_runtime_compiled(const std::string& runtime_c_path, const std::string& clang,
                                    bool verbose);

// Ensure any C file is compiled with caching
// cache_dir: where to store the .o/.obj file
// Returns: path to compiled object file, or original .c path on failure
std::string ensure_c_compiled(const std::string& c_path, const std::string& cache_dir,
                              const std::string& clang, bool verbose);

// Find runtime path
std::string find_runtime();

} // namespace tml::cli
