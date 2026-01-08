//! # CLI Utilities Interface
//!
//! This header defines shared utility functions for the CLI.
//!
//! ## Functions
//!
//! | Function              | Description                        |
//! |-----------------------|------------------------------------|
//! | `to_forward_slashes()`| Convert backslashes to forward     |
//! | `read_file()`         | Read entire file to string         |
//! | `print_usage()`       | Print CLI help text                |
//! | `print_version()`     | Print compiler version             |

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

} // namespace tml::cli
