//! # Compiler Setup Interface
//!
//! This header defines toolchain discovery and C runtime compilation.
//!
//! ## Toolchain Discovery
//!
//! | Function      | Platform | Description                        |
//! |---------------|----------|-------------------------------------|
//! | `find_clang()`| All      | Locate clang in PATH or known dirs |
//! | `find_msvc()` | Windows  | Find Visual Studio and Windows SDK |
//! | `find_runtime()` | All   | Locate essential.c runtime         |
//!
//! ## C Runtime Compilation
//!
//! - `ensure_runtime_compiled()`: Pre-compile essential.c with caching
//! - `ensure_c_compiled()`: Compile any C file with caching

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
// extra_flags: optional additional compiler flags (e.g., "-DTML_DEBUG_MEMORY")
// Returns: path to compiled object file, or original .c path on failure
std::string ensure_c_compiled(const std::string& c_path, const std::string& cache_dir,
                              const std::string& clang, bool verbose,
                              const std::string& extra_flags = "");

// Find runtime path (source .c file for fallback compilation)
std::string find_runtime();

// Find pre-compiled runtime library (tml_runtime.lib or libtml_runtime.a)
// Returns empty string if not found
std::string find_runtime_library();

// Check if pre-compiled runtime is available
bool is_precompiled_runtime_available();

// ============================================================================
// LLVM Coverage Tools
// ============================================================================

// Find llvm-profdata (for merging profile data)
// Returns empty string if not found
std::string find_llvm_profdata();

// Find llvm-cov (for generating coverage reports)
// Returns empty string if not found
std::string find_llvm_cov();

// Check if LLVM coverage tools are available
bool is_llvm_coverage_available();

// Find LLVM profile runtime library (clang_rt.profile)
// Required for linking coverage-instrumented binaries
// Returns empty string if not found
std::string find_llvm_profile_runtime();

} // namespace tml::cli
