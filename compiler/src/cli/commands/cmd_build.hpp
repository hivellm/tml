//! # Build Command Interface
//!
//! This header defines the build command API and related types.
//!
//! ## Build Functions
//!
//! | Function          | Description                              |
//! |-------------------|------------------------------------------|
//! | `run_build()`     | Compile source to executable/library     |
//! | `run_build_ex()`  | Extended build with all options          |
//! | `run_run()`       | Build and execute program                |
//! | `run_run_quiet()` | Run with output capture                  |
//! | `run_run_profiled()` | Run with phase timing breakdown       |
//!
//! ## Exit Codes
//!
//! - `EXIT_SUCCESS_CODE (0)`: Success
//! - `EXIT_RUNTIME_ERROR (1)`: Test/program failed
//! - `EXIT_COMPILATION_ERROR (2)`: Compilation failed

#pragma once
#include <map>
#include <string>
#include <vector>

namespace tml::cli {

// Phase timing result for profiling
struct PhaseTimings {
    std::map<std::string, int64_t> timings_us; // Phase name -> microseconds
};

// Exit codes for test/run commands
// These help distinguish compilation errors from runtime errors
constexpr int EXIT_SUCCESS_CODE = 0;
constexpr int EXIT_RUNTIME_ERROR = 1;     // Test executed but failed
constexpr int EXIT_COMPILATION_ERROR = 2; // Code failed to compile (lex/parse/type/codegen)

// Build output types
enum class BuildOutputType {
    Executable,
    StaticLib,
    DynamicLib,
    RlibLib // TML native library format
};

// Extended build options
struct BuildOptions {
    bool verbose = false;
    bool emit_ir_only = false;
    bool emit_hir = false; // Emit HIR (High-level IR)
    bool emit_mir = false;
    bool no_cache = false;
    bool emit_header = false;
    bool show_timings = false;  // Show detailed phase timings
    bool lto = false;           // Link-Time Optimization
    bool use_hir = false;       // Use HIR pipeline (AST -> HIR -> MIR)
    bool debug = false;         // Debug build (sets DEBUG symbol)
    bool release = false;       // Release build (sets RELEASE symbol)
    int optimization_level = 0; // -O0 to -O3
    BuildOutputType output_type = BuildOutputType::Executable;
    std::string output_dir;
    std::string target;               // Target triple (e.g., x86_64-unknown-linux-gnu)
    std::vector<std::string> defines; // -D defines for preprocessor

    // PGO (Profile-Guided Optimization) options
    bool profile_generate = false; // Generate profile data during execution
    std::string profile_use;       // Use profile data from file (empty = disabled)

    // Runtime profiling (generates .cpuprofile for Chrome DevTools)
    bool profile = false;       // Enable runtime profiling instrumentation
    std::string profile_output; // Custom output path (default: profile.cpuprofile)

    // Backend selection ("llvm" or "cranelift")
    std::string backend = "llvm";

    // Use Polonius borrow checker (more permissive than NLL)
    bool polonius = false;
};

// Run options (for run command)
struct RunOptions {
    bool verbose = false;
    bool coverage = false;
    bool no_cache = false;
    bool legacy = false;           // Use legacy sequential pipeline instead of query system
    bool profile = false;          // Enable runtime profiling
    std::string profile_output;    // Custom output path (default: profile.cpuprofile)
    std::vector<std::string> args; // Program arguments
    std::string backend = "llvm";  // Codegen backend ("llvm" or "cranelift")
};

// Build commands
int run_build(const std::string& path, bool verbose, bool emit_ir_only, bool emit_mir = false,
              bool no_cache = false, BuildOutputType output_type = BuildOutputType::Executable,
              bool emit_header = false, const std::string& output_dir = "");

// Extended build command with all options
int run_build_ex(const std::string& path, const BuildOptions& options);

// Query-based build (uses QueryContext for memoized, demand-driven compilation)
int run_build_with_queries(const std::string& path, const BuildOptions& options);

int run_run(const std::string& path, const std::vector<std::string>& args, bool verbose,
            bool coverage = false, bool no_cache = false, const std::string& backend = "llvm");

// Run with extended options
int run_run_ex(const std::string& path, const RunOptions& options);

// Run with output capture (for test runner)
// Returns exit code, stores stdout/stderr in output if provided
int run_run_quiet(const std::string& path, const std::vector<std::string>& args, bool verbose,
                  std::string* output = nullptr, bool coverage = false, bool no_cache = false);

// Run with profiling (returns phase timings)
int run_run_profiled(const std::string& path, const std::vector<std::string>& args, bool verbose,
                     std::string* output, PhaseTimings* timings, bool coverage = false,
                     bool no_cache = false);

} // namespace tml::cli
