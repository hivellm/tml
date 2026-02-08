//! # RLIB Inspection Command
//!
//! This file implements the `tml rlib` command for inspecting TML library files.
//!
//! ## RLIB Format
//!
//! RLIB files are archives containing:
//! - `metadata.json`: Library metadata (name, version, exports)
//! - `*.obj`: Compiled object files for each module
//!
//! ## Subcommands
//!
//! | Command               | Description                      |
//! |-----------------------|----------------------------------|
//! | `rlib info <file>`    | Show library info and structure  |
//! | `rlib exports <file>` | List public function/type exports|
//! | `rlib validate <file>`| Check RLIB format integrity      |
//!
//! ## Example Output
//!
//! ```text
//! Library: my_lib v1.0.0
//! Modules: 1
//!   - my_lib (my_lib.obj)
//! Exports:
//!   func add(a: I32, b: I32) -> I32
//!   struct Point { x: I32, y: I32 }
//! ```

#include "cmd_rlib.hpp"
#include "log/log.hpp"

#include "cli/builder/rlib.hpp"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace tml::cli {

/// Shows detailed information about an RLIB file.
int run_rlib_info(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: tml rlib info <rlib-file>\n";
        return 1;
    }

    fs::path rlib_file = argv[3];

    if (!fs::exists(rlib_file)) {
        TML_LOG_ERROR("rlib", "RLIB file not found: " << rlib_file);
        return 1;
    }

    // Read metadata
    auto metadata_opt = read_rlib_metadata(rlib_file);
    if (!metadata_opt) {
        TML_LOG_ERROR("rlib", "Failed to read RLIB metadata from " << rlib_file << ". This may not be a valid TML library file.");
        return 1;
    }

    const auto& metadata = *metadata_opt;

    // Display information
    TML_LOG_INFO("rlib", "TML Library Information");
    TML_LOG_INFO("rlib", "=======================");
    TML_LOG_INFO("rlib", "Library: " << metadata.library.name << " v" << metadata.library.version);
    TML_LOG_INFO("rlib", "TML Version: " << metadata.library.tml_version);
    TML_LOG_INFO("rlib", "Format Version: " << metadata.format_version);
    TML_LOG_INFO("rlib", "File: " << rlib_file);
    TML_LOG_INFO("rlib", "Size: " << fs::file_size(rlib_file) << " bytes");

    // Modules
    TML_LOG_INFO("rlib", "Modules: " << metadata.modules.size());
    for (const auto& module : metadata.modules) {
        TML_LOG_INFO("rlib", "  - " << module.name);
        TML_LOG_INFO("rlib", "    File: " << module.file);
        TML_LOG_INFO("rlib", "    Hash: " << module.hash);
        TML_LOG_INFO("rlib", "    Exports: " << module.exports.size() << " items");
    }

    // Dependencies
    TML_LOG_INFO("rlib", "Dependencies: " << metadata.dependencies.size());
    for (const auto& dep : metadata.dependencies) {
        TML_LOG_INFO("rlib", "  - " << dep.name << " " << dep.version);
        TML_LOG_INFO("rlib", "    Hash: " << dep.hash);
    }

    if (metadata.dependencies.empty()) {
        TML_LOG_INFO("rlib", "  (none)");
    }

    return 0;
}

int run_rlib_exports(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: tml rlib exports <rlib-file> [--verbose]\n";
        return 1;
    }

    fs::path rlib_file = argv[3];
    bool verbose = false;

    // Parse options
    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        }
    }

    if (!fs::exists(rlib_file)) {
        TML_LOG_ERROR("rlib", "RLIB file not found: " << rlib_file);
        return 1;
    }

    // Read metadata
    auto metadata_opt = read_rlib_metadata(rlib_file);
    if (!metadata_opt) {
        TML_LOG_ERROR("rlib", "Failed to read RLIB metadata from " << rlib_file);
        return 1;
    }

    const auto& metadata = *metadata_opt;

    // Display exports
    TML_LOG_INFO("rlib",
                 "Public exports from " << metadata.library.name << " v" << metadata.library.version);
    TML_LOG_INFO("rlib", std::string(60, '='));

    auto exports = metadata.get_all_exports();

    if (exports.empty()) {
        TML_LOG_INFO("rlib", "(no public exports)");
        return 0;
    }

    for (const auto& exp : exports) {
        if (verbose) {
            TML_LOG_INFO("rlib", "Name: " << exp.name);
            TML_LOG_INFO("rlib", "Symbol: " << exp.symbol);
            TML_LOG_INFO("rlib", "Type: " << exp.type);
            TML_LOG_INFO("rlib", "Public: " << (exp.is_public ? "yes" : "no"));
        } else {
            // Parse type to make it more readable
            std::string type_str = exp.type;

            // Simple formatting: if it's a function, show it nicely
            if (type_str.find("func") == 0) {
                TML_LOG_INFO("rlib", "  func " << exp.name << type_str.substr(4));
            } else if (type_str.find("struct") == 0) {
                TML_LOG_INFO("rlib", "  struct " << exp.name << " " << type_str.substr(6));
            } else {
                TML_LOG_INFO("rlib", "  " << exp.name << ": " << type_str);
            }
        }
    }

    TML_LOG_INFO("rlib", "Total: " << exports.size() << " public exports");

    return 0;
}

int run_rlib_validate(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: tml rlib validate <rlib-file>\n";
        return 1;
    }

    fs::path rlib_file = argv[3];

    if (!fs::exists(rlib_file)) {
        TML_LOG_ERROR("rlib", "RLIB file not found: " << rlib_file);
        return 1;
    }

    TML_LOG_INFO("rlib", "Validating RLIB: " << rlib_file);

    // Check if it's a valid archive
    auto members = list_rlib_members(rlib_file);
    if (members.empty()) {
        TML_LOG_ERROR("rlib", "Not a valid archive file");
        return 1;
    }

    TML_LOG_INFO("rlib", "Valid archive format");
    TML_LOG_INFO("rlib", "  Members: " << members.size());

    // Check for metadata.json
    bool has_metadata = std::find(members.begin(), members.end(), "metadata.json") != members.end();
    if (!has_metadata) {
        TML_LOG_ERROR("rlib", "Missing metadata.json. This is not a valid TML library file.");
        return 1;
    }

    TML_LOG_INFO("rlib", "Found metadata.json");

    // Read and validate metadata
    auto metadata_opt = read_rlib_metadata(rlib_file);
    if (!metadata_opt) {
        TML_LOG_ERROR("rlib", "Failed to parse metadata.json");
        return 1;
    }

    const auto& metadata = *metadata_opt;
    TML_LOG_INFO("rlib", "Valid metadata format");

    // Check format version
    if (metadata.format_version != "1.0") {
        TML_LOG_WARN("rlib", "Unexpected format version: " << metadata.format_version << ". Expected: 1.0");
    } else {
        TML_LOG_INFO("rlib", "Format version: " << metadata.format_version);
    }

    // Check that all modules exist
    TML_LOG_INFO("rlib", "Checking modules:");
    for (const auto& module : metadata.modules) {
        bool has_module = std::find(members.begin(), members.end(), module.file) != members.end();
        if (has_module) {
            TML_LOG_INFO("rlib", "  " << module.name << " (" << module.file << ")");
        } else {
            TML_LOG_ERROR("rlib", "Module " << module.name << " (" << module.file << ") not found in archive");
            return 1;
        }
    }

    TML_LOG_INFO("rlib", "RLIB validation passed");
    TML_LOG_INFO("rlib", "Library: " << metadata.library.name << " v" << metadata.library.version);

    return 0;
}

int run_rlib(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: tml rlib <subcommand> [options]\n";
        std::cerr << "\n";
        std::cerr << "Subcommands:\n";
        std::cerr << "  info <rlib-file>          Show library information\n";
        std::cerr << "  exports <rlib-file>       List public exports\n";
        std::cerr << "  validate <rlib-file>      Validate RLIB format\n";
        std::cerr << "\n";
        std::cerr << "Options:\n";
        std::cerr << "  --verbose, -v             Show detailed information\n";
        return 1;
    }

    std::string subcommand = argv[2];

    if (subcommand == "info") {
        return run_rlib_info(argc, argv);
    } else if (subcommand == "exports") {
        return run_rlib_exports(argc, argv);
    } else if (subcommand == "validate") {
        return run_rlib_validate(argc, argv);
    } else {
        TML_LOG_ERROR("rlib", "Unknown rlib subcommand: " << subcommand << ". Use 'tml rlib info', 'tml rlib exports', or 'tml rlib validate'");
        return 1;
    }
}

} // namespace tml::cli
