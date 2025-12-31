#pragma once
#include <string>
#include <vector>

namespace tml::cli {

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
    bool emit_mir = false;
    bool no_cache = false;
    bool emit_header = false;
    bool show_timings = false; // Show detailed phase timings
    bool lto = false;          // Link-Time Optimization
    BuildOutputType output_type = BuildOutputType::Executable;
    std::string output_dir;
};

// Build commands
int run_build(const std::string& path, bool verbose, bool emit_ir_only, bool emit_mir = false,
              bool no_cache = false, BuildOutputType output_type = BuildOutputType::Executable,
              bool emit_header = false, const std::string& output_dir = "");

// Extended build command with all options
int run_build_ex(const std::string& path, const BuildOptions& options);

int run_run(const std::string& path, const std::vector<std::string>& args, bool verbose,
            bool coverage = false, bool no_cache = false);

// Run with output capture (for test runner)
// Returns exit code, stores stdout/stderr in output if provided
int run_run_quiet(const std::string& path, const std::vector<std::string>& args, bool verbose,
                  std::string* output = nullptr, bool coverage = false, bool no_cache = false);

} // namespace tml::cli
