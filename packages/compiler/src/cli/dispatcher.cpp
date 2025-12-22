#include "utils.hpp"
#include "cmd_debug.hpp"
#include "cmd_build.hpp"
#include "cmd_format.hpp"
#include "cmd_test.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace tml::cli {

// Main dispatcher - routes commands to appropriate handlers
int tml_main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string command = argv[1];
    bool verbose = false;

    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--verbose" || std::string(argv[i]) == "-v") {
            verbose = true;
        }
    }

    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }

    if (command == "--version" || command == "-V") {
        print_version();
        return 0;
    }

    if (command == "lex") {
        if (argc < 3) {
            std::cerr << "Usage: tml lex <file.tml> [--verbose]\n";
            return 1;
        }
        return run_lex(argv[2], verbose);
    }

    if (command == "parse") {
        if (argc < 3) {
            std::cerr << "Usage: tml parse <file.tml> [--verbose]\n";
            return 1;
        }
        return run_parse(argv[2], verbose);
    }

    if (command == "check") {
        if (argc < 3) {
            std::cerr << "Usage: tml check <file.tml> [--verbose]\n";
            return 1;
        }
        return run_check(argv[2], verbose);
    }

    if (command == "build") {
        if (argc < 3) {
            std::cerr << "Usage: tml build <file.tml> [--emit-ir] [--verbose]\n";
            return 1;
        }
        bool emit_ir_only = false;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--emit-ir" || std::string(argv[i]) == "--emit-c") {
                emit_ir_only = true;
            }
        }
        return run_build(argv[2], verbose, emit_ir_only);
    }

    if (command == "fmt") {
        if (argc < 3) {
            std::cerr << "Usage: tml fmt <file.tml> [--check] [--verbose]\n";
            return 1;
        }
        bool check_only = false;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--check") {
                check_only = true;
            }
        }
        return run_fmt(argv[2], check_only, verbose);
    }

    if (command == "run") {
        if (argc < 3) {
            std::cerr << "Usage: tml run <file.tml> [args...] [--verbose]\n";
            return 1;
        }
        std::vector<std::string> program_args;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg != "--verbose" && arg != "-v") {
                program_args.push_back(arg);
            }
        }
        return run_run(argv[2], program_args, verbose);
    }

    if (command == "test") {
        return run_test(argc, argv, verbose);
    }

    if (command == "new") {
        std::cerr << "Error: 'new' command not yet implemented\n";
        std::cerr << "The compiler is still in early development.\n";
        return 1;
    }

    std::cerr << "Error: Unknown command '" << command << "'\n";
    std::cerr << "Run 'tml --help' for usage information.\n";
    return 1;
}

}

// Entry point wrapper (outside namespace)
int tml_main(int argc, char* argv[]) {
    return tml::cli::tml_main(argc, argv);
}
