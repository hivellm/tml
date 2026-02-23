TML_MODULE("compiler")

//! # CLI Utility Functions
//!
//! This file contains shared utility functions used across the CLI.
//!
//! ## Contents
//!
//! - **Path Utilities**: `to_forward_slashes()` for cross-platform paths
//! - **File I/O**: `read_file()` for loading source files
//! - **Help Text**: `print_usage()`, `print_version()`
//! - **Clang Discovery**: `find_clang()` to locate the LLVM toolchain
//! - **Runtime Discovery**: `find_runtime()` for linking C runtime

#include "utils.hpp"

#include "common.hpp"
#include "version_generated.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace tml::cli {

/// Converts backslashes to forward slashes for cross-platform paths.
std::string to_forward_slashes(const std::string& path) {
    std::string result = path;
    for (char& c : result) {
        if (c == '\\')
            c = '/';
    }
    return result;
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void print_usage() {
    std::cerr << "TML Compiler " << VERSION << "\n\n"
              << "Usage: tml <command> [options] [files]\n\n"
              << "Commands:\n"
              << "  build     Compile the project\n"
              << "  run       Build and run the project\n"
              << "  check     Type-check without generating code\n"
              << "  test      Run tests\n"
              << "  fmt       Format source files\n"
              << "  lint      Check files for style issues\n"
              << "  doc       Generate documentation\n"
              << "  cache     Manage build cache\n"
              << "  rlib      Inspect RLIB libraries\n"
              << "  init      Initialize a new project\n"
              << "  deps      List project dependencies\n"
              << "  remove    Remove a dependency from tml.toml\n"
              << "  explain   Show detailed error code explanation\n"
              << "  lex       Tokenize a file (debug)\n"
              << "  parse     Parse a file (debug)\n"
              << "\nOptions:\n"
              << "  --help, -h       Show this help\n"
              << "  --version, -V    Show version\n"
              << "  --release        Build with optimizations (-O3)\n"
              << "  --debug, -g      Include debug info (DWARF)\n"
              << "  -O0...-O3        Set optimization level\n"
              << "  -Os, -Oz         Optimize for size\n"
              << "  --verbose        Show detailed output\n";
}

void print_version() {
    std::cerr << "tml " << VERSION << "\n";
}

} // namespace tml::cli
