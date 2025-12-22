#pragma once
#include <string>

namespace tml::cli {

// Debug commands
int run_lex(const std::string& path, bool verbose);
int run_parse(const std::string& path, bool verbose);
int run_check(const std::string& path, bool verbose);

}
