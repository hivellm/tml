#pragma once
#include <string>

namespace tml::cli {

// Path utilities
std::string to_forward_slashes(const std::string& path);

// File I/O
std::string read_file(const std::string& path);

// Help text
void print_usage();
void print_version();

}
