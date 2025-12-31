#include "utils.hpp"

#include "common.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace tml::cli {

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
    std::cout << "TML Compiler " << VERSION << "\n\n";
    std::cout << "Usage: tml <command> [options] [files]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  build     Compile the project\n";
    std::cout << "  run       Build and run the project\n";
    std::cout << "  check     Type-check without generating code\n";
    std::cout << "  test      Run tests\n";
    std::cout << "  fmt       Format source files\n";
    std::cout << "  lint      Check files for style issues\n";
    std::cout << "  cache     Manage build cache\n";
    std::cout << "  rlib      Inspect RLIB libraries\n";
    std::cout << "  init      Initialize a new project\n";
    std::cout << "  deps      List project dependencies\n";
    std::cout << "  remove    Remove a dependency from tml.toml\n";
    std::cout << "  lex       Tokenize a file (debug)\n";
    std::cout << "  parse     Parse a file (debug)\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --help, -h       Show this help\n";
    std::cout << "  --version, -V    Show version\n";
    std::cout << "  --release        Build with optimizations (-O3)\n";
    std::cout << "  --debug, -g      Include debug info (DWARF)\n";
    std::cout << "  -O0...-O3        Set optimization level\n";
    std::cout << "  -Os, -Oz         Optimize for size\n";
    std::cout << "  --verbose        Show detailed output\n";
}

void print_version() {
    std::cout << "tml " << VERSION << "\n";
}

} // namespace tml::cli
