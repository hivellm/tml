#include "utils.hpp"
#include "cmd_debug.hpp"
#include "cmd_build.hpp"
#include "cmd_format.hpp"
#include "cmd_test.hpp"
#include "cmd_cache.hpp"
#include "tml/common.hpp"
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

    // Set global verbose flag for debug output
    tml::CompilerOptions::verbose = verbose;

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
            std::cerr << "Usage: tml build <file.tml> [--emit-ir] [--emit-header] [--verbose] [--no-cache] [--crate-type=<type>]\n";
            std::cerr << "  Crate types: bin (default), lib, dylib\n";
            return 1;
        }
        bool emit_ir_only = false;
        bool emit_header = false;
        bool no_cache = false;
        BuildOutputType output_type = BuildOutputType::Executable;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--emit-ir" || arg == "--emit-c") {
                emit_ir_only = true;
            } else if (arg == "--emit-header") {
                emit_header = true;
            } else if (arg == "--no-cache") {
                no_cache = true;
            } else if (arg.starts_with("--crate-type=")) {
                std::string crate_type = arg.substr(13);
                if (crate_type == "bin") {
                    output_type = BuildOutputType::Executable;
                } else if (crate_type == "lib" || crate_type == "staticlib") {
                    output_type = BuildOutputType::StaticLib;
                } else if (crate_type == "dylib" || crate_type == "cdylib") {
                    output_type = BuildOutputType::DynamicLib;
                } else {
                    std::cerr << "error: unknown crate type '" << crate_type << "'\n";
                    std::cerr << "  valid types: bin, lib, dylib\n";
                    return 1;
                }
            }
        }
        return run_build(argv[2], verbose, emit_ir_only, no_cache, output_type, emit_header);
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
            std::cerr << "Usage: tml run <file.tml> [args...] [--verbose] [--no-cache]\n";
            return 1;
        }
        std::vector<std::string> program_args;
        bool no_cache = false;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--verbose" || arg == "-v") {
                // Already handled
            } else if (arg == "--no-cache") {
                no_cache = true;
            } else {
                program_args.push_back(arg);
            }
        }
        return run_run(argv[2], program_args, verbose, false, no_cache);
    }

    if (command == "test") {
        return run_test(argc, argv, verbose);
    }

    if (command == "cache") {
        return run_cache(argc, argv);
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
