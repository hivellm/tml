#pragma once
#include <string>
#include <vector>

namespace tml::cli {

// Build commands
int run_build(const std::string& path, bool verbose, bool emit_ir_only, bool no_cache = false);
int run_run(const std::string& path, const std::vector<std::string>& args, bool verbose,
            bool coverage = false, bool no_cache = false);

// Run with output capture (for test runner)
// Returns exit code, stores stdout/stderr in output if provided
int run_run_quiet(const std::string& path, const std::vector<std::string>& args,
                  bool verbose, std::string* output = nullptr, bool coverage = false,
                  bool no_cache = false);

}
