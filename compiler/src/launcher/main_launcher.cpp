TML_MODULE("launcher")

//! # Thin Launcher Entry Point (Modular Build)
//!
//! This is the entry point for the modular build of `tml.exe`.
//! It is intentionally tiny (~500KB without LLVM) and performs:
//!
//! 1. Minimal command parsing (just argv[1])
//! 2. `--help` / `--version` handled locally (no plugins needed)
//! 3. Everything else: load the compiler plugin and delegate
//!
//! The compiler plugin itself loads codegen/tools/test plugins on demand.

#include "plugin/abi.h"
#include "plugin/loader.hpp"

#include <cstring>
#include <iostream>
#include <string>

// Version is generated at build time
#ifndef TML_VERSION
#define TML_VERSION "0.1.6"
#endif

// Function pointer type for the compiler's main entry point
typedef int (*CompilerMainFn)(int argc, char* argv[]);

static void print_usage() {
    std::cout << "TML Compiler " << TML_VERSION << " (modular)\n"
              << "\n"
              << "Usage: tml <command> [options]\n"
              << "\n"
              << "Commands:\n"
              << "  build   <file>    Compile a TML source file\n"
              << "  run     <file>    Build and run immediately\n"
              << "  check   <file>    Type check without codegen\n"
              << "  test              Run tests\n"
              << "  fmt     <file>    Format source code\n"
              << "  lint    <file>    Lint source code\n"
              << "  lex     <file>    Show lexer tokens\n"
              << "  parse   <file>    Show parse tree\n"
              << "  init              Initialize a new project\n"
              << "  mcp               Start MCP server\n"
              << "  explain <code>    Explain an error code\n"
              << "\n"
              << "Flags:\n"
              << "  --help, -h        Show this help\n"
              << "  --version, -V     Show version\n"
              << "  --verbose, -v     Enable verbose output\n";
}

static void print_version() {
    std::cout << "tml " << TML_VERSION << " (modular)\n";
}

int main(int argc, char* argv[]) {
    // No arguments → show help
    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string command = argv[1];

    // Handle --help and --version without loading any plugin
    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }
    if (command == "--version" || command == "-V") {
        print_version();
        return 0;
    }

    // Everything else requires the compiler plugin
    tml::plugin::Loader loader;

    if (!loader.load("tml_compiler")) {
        std::cerr << "error: failed to load compiler plugin (tml_compiler)\n";
        std::cerr << "  Searched: " << loader.plugins_dir().string() << "\n";
        std::cerr << "  Set TML_PLUGIN_DIR to the directory containing plugin DLLs.\n";
        return 1;
    }

    // Get the compiler's main entry point
    auto* plugin = loader.get("tml_compiler");
    if (!plugin || !plugin->handle) {
        std::cerr << "error: compiler plugin loaded but handle is null\n";
        return 1;
    }

    // Look up the exported compiler_main function
    auto compiler_main = reinterpret_cast<CompilerMainFn>(
        tml::plugin::Loader::get_symbol(plugin->handle, "compiler_main"));

    if (!compiler_main) {
        std::cerr << "error: compiler plugin does not export 'compiler_main'\n";
        return 1;
    }

    // Delegate to the compiler
    int result = compiler_main(argc, argv);

    // Cleanup
    loader.unload_all();

    return result;
}
