#pragma once
#include <string>
#include <vector>

namespace tml::cli {

// Build commands
int run_build(const std::string& path, bool verbose, bool emit_ir_only);
int run_run(const std::string& path, const std::vector<std::string>& args, bool verbose);

}
