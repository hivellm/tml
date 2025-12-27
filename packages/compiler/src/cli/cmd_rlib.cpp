#include "cmd_rlib.hpp"

#include "rlib.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>

namespace fs = std::filesystem;

namespace tml::cli {

int run_rlib_info(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: tml rlib info <rlib-file>\n";
        return 1;
    }

    fs::path rlib_file = argv[3];

    if (!fs::exists(rlib_file)) {
        std::cerr << "Error: RLIB file not found: " << rlib_file << "\n";
        return 1;
    }

    // Read metadata
    auto metadata_opt = read_rlib_metadata(rlib_file);
    if (!metadata_opt) {
        std::cerr << "Error: Failed to read RLIB metadata from " << rlib_file << "\n";
        std::cerr << "This may not be a valid TML library file.\n";
        return 1;
    }

    const auto& metadata = *metadata_opt;

    // Display information
    std::cout << "TML Library Information\n";
    std::cout << "=======================\n\n";

    std::cout << "Library: " << metadata.library.name << " v" << metadata.library.version << "\n";
    std::cout << "TML Version: " << metadata.library.tml_version << "\n";
    std::cout << "Format Version: " << metadata.format_version << "\n";
    std::cout << "File: " << rlib_file << "\n";
    std::cout << "Size: " << fs::file_size(rlib_file) << " bytes\n";
    std::cout << "\n";

    // Modules
    std::cout << "Modules: " << metadata.modules.size() << "\n";
    for (const auto& module : metadata.modules) {
        std::cout << "  - " << module.name << "\n";
        std::cout << "    File: " << module.file << "\n";
        std::cout << "    Hash: " << module.hash << "\n";
        std::cout << "    Exports: " << module.exports.size() << " items\n";
    }
    std::cout << "\n";

    // Dependencies
    std::cout << "Dependencies: " << metadata.dependencies.size() << "\n";
    for (const auto& dep : metadata.dependencies) {
        std::cout << "  - " << dep.name << " " << dep.version << "\n";
        std::cout << "    Hash: " << dep.hash << "\n";
    }

    if (metadata.dependencies.empty()) {
        std::cout << "  (none)\n";
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
        std::cerr << "Error: RLIB file not found: " << rlib_file << "\n";
        return 1;
    }

    // Read metadata
    auto metadata_opt = read_rlib_metadata(rlib_file);
    if (!metadata_opt) {
        std::cerr << "Error: Failed to read RLIB metadata from " << rlib_file << "\n";
        return 1;
    }

    const auto& metadata = *metadata_opt;

    // Display exports
    std::cout << "Public exports from " << metadata.library.name << " v" << metadata.library.version
              << ":\n";
    std::cout << std::string(60, '=') << "\n";

    auto exports = metadata.get_all_exports();

    if (exports.empty()) {
        std::cout << "(no public exports)\n";
        return 0;
    }

    for (const auto& exp : exports) {
        if (verbose) {
            std::cout << "\nName: " << exp.name << "\n";
            std::cout << "Symbol: " << exp.symbol << "\n";
            std::cout << "Type: " << exp.type << "\n";
            std::cout << "Public: " << (exp.is_public ? "yes" : "no") << "\n";
        } else {
            // Parse type to make it more readable
            std::string type_str = exp.type;

            // Simple formatting: if it's a function, show it nicely
            if (type_str.find("func") == 0) {
                std::cout << "  func " << exp.name << type_str.substr(4) << "\n";
            } else if (type_str.find("struct") == 0) {
                std::cout << "  struct " << exp.name << " " << type_str.substr(6) << "\n";
            } else {
                std::cout << "  " << exp.name << ": " << type_str << "\n";
            }
        }
    }

    std::cout << "\nTotal: " << exports.size() << " public exports\n";

    return 0;
}

int run_rlib_validate(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: tml rlib validate <rlib-file>\n";
        return 1;
    }

    fs::path rlib_file = argv[3];

    if (!fs::exists(rlib_file)) {
        std::cerr << "Error: RLIB file not found: " << rlib_file << "\n";
        return 1;
    }

    std::cout << "Validating RLIB: " << rlib_file << "\n";

    // Check if it's a valid archive
    auto members = list_rlib_members(rlib_file);
    if (members.empty()) {
        std::cerr << "Error: Not a valid archive file\n";
        return 1;
    }

    std::cout << "✓ Valid archive format\n";
    std::cout << "  Members: " << members.size() << "\n";

    // Check for metadata.json
    bool has_metadata = std::find(members.begin(), members.end(), "metadata.json") != members.end();
    if (!has_metadata) {
        std::cerr << "Error: Missing metadata.json\n";
        std::cerr << "This is not a valid TML library file.\n";
        return 1;
    }

    std::cout << "✓ Found metadata.json\n";

    // Read and validate metadata
    auto metadata_opt = read_rlib_metadata(rlib_file);
    if (!metadata_opt) {
        std::cerr << "Error: Failed to parse metadata.json\n";
        return 1;
    }

    const auto& metadata = *metadata_opt;
    std::cout << "✓ Valid metadata format\n";

    // Check format version
    if (metadata.format_version != "1.0") {
        std::cerr << "Warning: Unexpected format version: " << metadata.format_version << "\n";
        std::cerr << "Expected: 1.0\n";
    } else {
        std::cout << "✓ Format version: " << metadata.format_version << "\n";
    }

    // Check that all modules exist
    std::cout << "Checking modules:\n";
    for (const auto& module : metadata.modules) {
        bool has_module = std::find(members.begin(), members.end(), module.file) != members.end();
        if (has_module) {
            std::cout << "  ✓ " << module.name << " (" << module.file << ")\n";
        } else {
            std::cerr << "  ✗ " << module.name << " (" << module.file << ") - NOT FOUND\n";
            return 1;
        }
    }

    std::cout << "\n";
    std::cout << "✓ RLIB validation passed\n";
    std::cout << "Library: " << metadata.library.name << " v" << metadata.library.version << "\n";

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
        std::cerr << "Unknown rlib subcommand: " << subcommand << "\n";
        std::cerr << "Use 'tml rlib info', 'tml rlib exports', or 'tml rlib validate'\n";
        return 1;
    }
}

} // namespace tml::cli
