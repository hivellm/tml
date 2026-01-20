//! # CLI Command Dispatcher
//!
//! This file implements the main entry point for the TML compiler CLI.
//! It parses command-line arguments and routes to the appropriate command handler.
//!
//! ## Architecture
//!
//! ```text
//! tml_main()
//!   ├─ --help, -h     → print_usage()
//!   ├─ --version, -V  → print_version()
//!   ├─ lex            → run_lex()
//!   ├─ parse          → run_parse()
//!   ├─ check          → run_check()
//!   ├─ build          → run_build() / run_build_ex()
//!   ├─ run            → run_run()
//!   ├─ test           → run_test()
//!   ├─ fmt            → run_fmt()
//!   ├─ lint           → run_lint()
//!   ├─ init           → run_init()
//!   ├─ rlib           → run_rlib()
//!   ├─ cache          → run_cache()
//!   ├─ build-all      → run_parallel_build()
//!   ├─ mcp            → cmd_mcp()
//!   └─ add/update/rm  → package management
//! ```
//!
//! ## Command Categories
//!
//! | Category       | Commands                    | Description                    |
//! |----------------|-----------------------------|--------------------------------|
//! | Compilation    | lex, parse, check, build    | Compile TML source code        |
//! | Execution      | run, test                   | Build and run programs         |
//! | Tooling        | fmt, lint                   | Code formatting and linting    |
//! | Project        | init, rlib, cache           | Project and library management |
//! | Dependencies   | add, update, rm, deps       | Package management             |
//!
//! ## Global Flags
//!
//! These flags are available for all commands:
//! - `--verbose` / `-v`: Enable verbose output
//! - `--help` / `-h`: Show usage information
//! - `--version` / `-V`: Show compiler version

#include "builder/build_config.hpp"
#include "builder/parallel_build.hpp"
#include "commands/cmd_build.hpp"
#include "commands/cmd_cache.hpp"
#include "commands/cmd_debug.hpp"
#include "commands/cmd_doc.hpp"
#include "commands/cmd_format.hpp"
#include "commands/cmd_init.hpp"
#include "commands/cmd_lint.hpp"
#include "commands/cmd_mcp.hpp"
#include "commands/cmd_pkg.hpp"
#include "commands/cmd_rlib.hpp"
#include "commands/cmd_test.hpp"
#include "common.hpp"
#include "utils.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace tml::cli {

/// Main entry point for the TML compiler CLI.
///
/// Parses command-line arguments and dispatches to the appropriate
/// command handler based on the first argument.
///
/// ## Return Codes
///
/// | Code | Meaning                              |
/// |------|--------------------------------------|
/// | 0    | Success                              |
/// | 1    | Error (compilation, runtime, etc.)   |
///
/// ## Examples
///
/// ```bash
/// tml build main.tml              # Compile to executable
/// tml build main.tml --release    # Compile with optimizations
/// tml run main.tml                # Build and run
/// tml test                        # Run all tests
/// tml fmt src/*.tml               # Format source files
/// ```
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
            std::cerr << "Usage: tml build <file.tml> [options]\n";
            std::cerr << "Options:\n";
            std::cerr << "  --emit-ir           Emit LLVM IR instead of executable\n";
            std::cerr << "  --emit-mir          Emit MIR (Mid-level IR) for debugging\n";
            std::cerr << "  --emit-header       Generate C header for FFI\n";
            std::cerr << "  --verbose, -v       Show detailed output\n";
            std::cerr << "  --no-cache          Disable build cache\n";
            std::cerr << "  --release           Build with optimizations (-O3)\n";
            std::cerr << "  --debug, -g         Include debug info (DWARF, equivalent to -g2)\n";
            std::cerr << "  -g0, -g1, -g2, -g3  Set debug info level (0=none, 1=minimal, "
                         "2=standard, 3=full)\n";
            std::cerr << "  --time              Show detailed compiler phase timings\n";
            std::cerr << "  --lto               Enable Link-Time Optimization\n";
            std::cerr << "  -O0...-O3           Set optimization level\n";
            std::cerr << "  -Os, -Oz            Optimize for size\n";
            std::cerr << "  --crate-type=<type> Output type: bin, lib, dylib, rlib\n";
            std::cerr << "  --target=<triple>   Target triple (e.g., x86_64-unknown-linux-gnu)\n";
            std::cerr << "  --sysroot=<path>    Sysroot path for cross-compilation\n";
            std::cerr << "  --out-dir=<dir>     Output directory\n";
            std::cerr << "  -Wnone              Disable all warnings\n";
            std::cerr << "  -Wextra             Enable extra warnings\n";
            std::cerr << "  -Wall               Enable all warnings\n";
            std::cerr << "  -Wpedantic          Enable pedantic warnings\n";
            std::cerr << "  -Werror             Treat warnings as errors\n";
            std::cerr << "  --error-format=json Output diagnostics as JSON\n";
            std::cerr
                << "  --no-check-leaks    Disable memory leak detection (enabled by default)\n";
            std::cerr << "  --profile-generate  Generate profile data for PGO\n";
            std::cerr << "  --profile-use=<file> Use profile data for PGO\n";
            std::cerr << "  --use-external-tools Force use of clang/system linker\n";
            return 1;
        }

        // Load manifest for default settings
        auto manifest_opt = Manifest::load_from_current_dir();

        // Initialize with defaults (manifest values if available, otherwise hardcoded defaults)
        bool emit_ir_only = manifest_opt ? manifest_opt->build.emit_ir : false;
        bool emit_mir = false;
        bool emit_header = manifest_opt ? manifest_opt->build.emit_header : false;
        bool no_cache = manifest_opt ? !manifest_opt->build.cache : false;
        bool debug_info = false;
        int debug_level = 0;
        int opt_level = manifest_opt ? manifest_opt->build.optimization_level : 0;
        BuildOutputType output_type = BuildOutputType::Executable;
        std::string output_dir = "";    // Empty means use default (build/debug)
        std::string target_triple = ""; // Empty means use host target
        std::string sysroot = "";       // Empty means use system default

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

        // Additional options for extended build
        bool show_timings = false;
        bool lto = false;
        std::vector<std::string> defines; // Preprocessor defines (-D)

        // PGO options
        bool profile_generate = false;
        std::string profile_use;

        // Parse command-line arguments (override manifest settings)
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--emit-ir" || arg == "--emit-c") {
                emit_ir_only = true;
            } else if (arg == "--emit-mir") {
                emit_mir = true;
            } else if (arg == "--emit-header") {
                emit_header = true;
            } else if (arg == "--no-cache") {
                no_cache = true;
            } else if (arg == "--release") {
                opt_level = 3;
                // Disable leak checking in release mode for performance
                tml::CompilerOptions::check_leaks = false;
            } else if (arg == "--debug" || arg == "-g") {
                debug_info = true;
                debug_level = 2; // -g is equivalent to -g2
            } else if (arg == "-g0") {
                debug_info = false;
                debug_level = 0;
            } else if (arg == "-g1") {
                debug_info = true;
                debug_level = 1; // Minimal: function names and line numbers only
            } else if (arg == "-g2") {
                debug_info = true;
                debug_level = 2; // Standard: includes local variables
            } else if (arg == "-g3") {
                debug_info = true;
                debug_level = 3; // Full: includes all debug info
            } else if (arg == "--time") {
                show_timings = true;
            } else if (arg == "--lto") {
                lto = true;
            } else if (arg == "-O0") {
                opt_level = 0;
            } else if (arg == "-O1") {
                opt_level = 1;
            } else if (arg == "-O2") {
                opt_level = 2;
            } else if (arg == "-O3") {
                opt_level = 3;
            } else if (arg == "-Os") {
                opt_level = 4; // Optimize for size
            } else if (arg == "-Oz") {
                opt_level = 5; // Optimize for size (aggressive)
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
            } else if (arg.starts_with("--target=")) {
                target_triple = arg.substr(9);
            } else if (arg.starts_with("--sysroot=")) {
                sysroot = arg.substr(10);
            } else if (arg == "-Wnone") {
                tml::CompilerOptions::warning_level = tml::WarningLevel::None;
            } else if (arg == "-Wextra") {
                tml::CompilerOptions::warning_level = tml::WarningLevel::Extra;
            } else if (arg == "-Wall") {
                tml::CompilerOptions::warning_level = tml::WarningLevel::All;
            } else if (arg == "-Wpedantic") {
                tml::CompilerOptions::warning_level = tml::WarningLevel::Pedantic;
            } else if (arg == "-Werror") {
                tml::CompilerOptions::warnings_as_errors = true;
            } else if (arg == "--error-format=json") {
                tml::CompilerOptions::diagnostic_format = tml::DiagnosticFormat::JSON;
            } else if (arg == "--coverage") {
                tml::CompilerOptions::coverage = true;
            } else if (arg.starts_with("--coverage-output=")) {
                tml::CompilerOptions::coverage_output = arg.substr(18);
                tml::CompilerOptions::coverage = true; // Implicitly enable coverage
            } else if (arg == "--check-leaks") {
                tml::CompilerOptions::check_leaks = true;
            } else if (arg == "--no-check-leaks") {
                tml::CompilerOptions::check_leaks = false;
            } else if (arg.starts_with("-D")) {
                // Preprocessor define: -DSYMBOL or -DSYMBOL=VALUE
                if (arg.length() > 2) {
                    defines.push_back(arg.substr(2));
                }
            } else if (arg.starts_with("--define=")) {
                // Preprocessor define: --define=SYMBOL or --define=SYMBOL=VALUE
                defines.push_back(arg.substr(9));
            } else if (arg == "--profile-generate") {
                profile_generate = true;
            } else if (arg.starts_with("--profile-use=")) {
                profile_use = arg.substr(14);
            } else if (arg == "--use-external-tools") {
                tml::CompilerOptions::use_external_tools = true;
            }
        }

        // Set default coverage output if not specified
        if (tml::CompilerOptions::coverage && tml::CompilerOptions::coverage_output.empty()) {
            tml::CompilerOptions::coverage_output = "coverage.html";
        }

        // Store optimization settings in global options for use by build
        tml::CompilerOptions::optimization_level = opt_level;
        tml::CompilerOptions::debug_info = debug_info;
        tml::CompilerOptions::debug_level = debug_level;
        tml::CompilerOptions::target_triple = target_triple;
        tml::CompilerOptions::sysroot = sysroot;

        // Use extended build if new features are requested or if defines are present
        if (show_timings || lto || !defines.empty() || profile_generate || !profile_use.empty()) {
            BuildOptions opts;
            opts.verbose = verbose;
            opts.emit_ir_only = emit_ir_only;
            opts.emit_mir = emit_mir;
            opts.no_cache = no_cache;
            opts.emit_header = emit_header;
            opts.show_timings = show_timings;
            opts.lto = lto;
            opts.output_type = output_type;
            opts.output_dir = output_dir;
            opts.debug = debug_info;
            opts.release = (opt_level >= 2);
            opts.optimization_level = opt_level;
            opts.target = target_triple;
            opts.defines = std::move(defines);
            opts.profile_generate = profile_generate;
            opts.profile_use = std::move(profile_use);
            return run_build_ex(argv[2], opts);
        }

        return run_build(argv[2], verbose, emit_ir_only, emit_mir, no_cache, output_type,
                         emit_header, output_dir);
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
            std::cerr
                << "Usage: tml run <file.tml> [args...] [--verbose] [--no-cache] [--coverage] "
                   "[--coverage-output=<file>] [--profile[=<file>]]\n";
            std::cerr << "\nProfiling options:\n";
            std::cerr << "  --profile           Enable runtime profiling (output: profile.cpuprofile)\n";
            std::cerr << "  --profile=<file>    Enable profiling with custom output path\n";
            std::cerr << "\nThe .cpuprofile file can be loaded in Chrome DevTools or VS Code.\n";
            return 1;
        }
        RunOptions opts;
        opts.verbose = verbose;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--verbose" || arg == "-v") {
                // Already handled
            } else if (arg == "--release") {
                // Enable release mode optimizations
                CompilerOptions::optimization_level = 3;
                CompilerOptions::check_leaks = false;
            } else if (arg == "--no-cache") {
                opts.no_cache = true;
            } else if (arg == "--coverage") {
                opts.coverage = true;
            } else if (arg.starts_with("--coverage-output=")) {
                CompilerOptions::coverage_output = arg.substr(18);
                opts.coverage = true; // Implicitly enable coverage
            } else if (arg == "--profile") {
                opts.profile = true;
                opts.profile_output = "profile.cpuprofile";
            } else if (arg.starts_with("--profile=")) {
                opts.profile = true;
                opts.profile_output = arg.substr(10);
                if (opts.profile_output.empty()) {
                    opts.profile_output = "profile.cpuprofile";
                }
            } else {
                opts.args.push_back(arg);
            }
        }
        // Set default coverage output if not specified
        if (opts.coverage && CompilerOptions::coverage_output.empty()) {
            CompilerOptions::coverage_output = "coverage.html";
        }
        // Set global coverage flag for runtime linking
        CompilerOptions::coverage = opts.coverage;
        // Set global profiling flag
        CompilerOptions::profile = opts.profile;
        CompilerOptions::profile_output = opts.profile_output;
        return run_run_ex(argv[2], opts);
    }

    if (command == "test") {
        return run_test(argc, argv, verbose);
    }

    if (command == "cache") {
        return run_cache(argc, argv);
    }

    if (command == "build-all") {
        // Parallel build of all .tml files in current directory
        std::vector<std::string> args;
        for (int i = 2; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        return run_parallel_build(args, verbose);
    }

    if (command == "rlib") {
        return run_rlib(argc, argv);
    }

    if (command == "init") {
        return run_init(argc, argv);
    }

    if (command == "lint") {
        return run_lint(argc, argv);
    }

    if (command == "doc") {
        auto options = parse_doc_args(argc, argv);
        options.verbose = verbose;
        return run_doc(options);
    }

    if (command == "add") {
        return run_add(argc, argv);
    }

    if (command == "update") {
        return run_update(argc, argv);
    }

    if (command == "remove" || command == "rm") {
        return run_remove(argc, argv);
    }

    if (command == "deps") {
        return run_deps(argc, argv);
    }

    if (command == "publish") {
        return run_publish(argc, argv);
    }

    if (command == "mcp") {
        std::vector<std::string> args;
        for (int i = 2; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        return cmd_mcp(args);
    }

    std::cerr << "Error: Unknown command '" << command << "'\n";
    std::cerr << "Run 'tml --help' for usage information.\n";
    return 1;
}

} // namespace tml::cli

// Entry point wrapper (outside namespace)
int tml_main(int argc, char* argv[]) {
    return tml::cli::tml_main(argc, argv);
}
