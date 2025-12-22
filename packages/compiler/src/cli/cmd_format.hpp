#pragma once
#include <string>

namespace tml::cli {

// Format command
int run_fmt(const std::string& path, bool check_only, bool verbose);

}
