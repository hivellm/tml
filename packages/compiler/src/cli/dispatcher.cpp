#include "utils.hpp"
#include "cmd_debug.hpp"
#include "cmd_build.hpp"
#include "cmd_format.hpp"
#include "cmd_test.hpp"
#include "cmd_cache.hpp"
#include "cmd_rlib.hpp"
#include "cmd_init.hpp"
#include "build_config.hpp"
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
            std::cerr << "Usage: tml build <file.tml> [--emit-ir] [--emit-header] [--verbose] [--no-cache] [--crate-type=<type>] [--out-dir=<dir>]\n";
            std::cerr << "  Crate types: bin (default), lib, dylib, rlib\n";
            return 1;
        }

        // Load manifest for default settings
        auto manifest_opt = Manifest::load_from_current_dir();

        // Initialize with defaults (manifest values if available, otherwise hardcoded defaults)
        bool emit_ir_only = manifest_opt ? manifest_opt->build.emit_ir : false;
        bool emit_header = manifest_opt ? manifest_opt->build.emit_header : false;
        bool no_cache = manifest_opt ? !manifest_opt->build.cache : false;
        BuildOutputType output_type = BuildOutputType::Executable;
        std::string output_dir = "";  // Empty means use default (build/debug)

        // Determine output type from manifest if available
        if (manifest_opt && manifest_opt->lib) {
            // If manifest has [lib] section, default to library output
            if (!manifest_opt->lib->crate_types.empty()) {
                const auto& crate_type = manifest_opt->lib->crate_types[0];
                if (crate_type == "rlib") {
                    output_type = BuildOutputType::RlibLib;
                } else if (crate_type == "lib") {
                    output_type = BuildOutputType::StaticLib;
                } else if (crate_type == "dylib") {
                    output_type = BuildOutputType::DynamicLib;
                }
            }
        }

        // Parse command-line arguments (override manifest settings)
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
                } else if (crate_type == "rlib") {
                    output_type = BuildOutputType::RlibLib;
                } else {
                    std::cerr << "error: unknown crate type '" << crate_type << "'\n";
                    std::cerr << "  valid types: bin, lib, dylib, rlib\n";
                    return 1;
                }
            } else if (arg.starts_with("--out-dir=")) {
                output_dir = arg.substr(10);
            }
        }
        return run_build(argv[2], verbose, emit_ir_only, no_cache, output_type, emit_header, output_dir);
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

    if (command == "rlib") {
        return run_rlib(argc, argv);
    }

    if (command == "init") {
        return run_init(argc, argv);
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
