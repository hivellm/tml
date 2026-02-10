//! # Project Initialization Command
//!
//! This file implements the `tml init` command for creating new TML projects.
//!
//! ## Usage
//!
//! ```bash
//! tml init                    # Create binary project
//! tml init --lib              # Create library project
//! tml init --name my_project  # Custom project name
//! ```
//!
//! ## Generated Structure
//!
//! ```text
//! project/
//!   ├─ tml.toml              # Project manifest
//!   ├─ src/
//!   │    └─ main.tml         # (binary) Entry point
//!   │    └─ lib.tml          # (library) Library root
//!   └─ build/                # Output directory
//! ```
//!
//! ## Manifest Format
//!
//! The generated `tml.toml` includes:
//! - `[package]`: name, version, authors, edition
//! - `[[bin]]` or `[lib]`: target configuration
//! - `[dependencies]`: empty, ready for deps
//! - `[build]`: default build options

#include "cmd_init.hpp"

#include "cli/utils.hpp"
#include "log/log.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace tml::cli {

namespace {

/// Derives a project name from the current directory name.
std::string get_default_project_name() {
    fs::path current = fs::current_path();
    std::string name = current.filename().string();

    // Convert to lowercase and replace spaces with underscores
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::replace(name.begin(), name.end(), ' ', '_');

    // Remove invalid characters
    std::string result;
    for (char c : name) {
        if (std::isalnum(c) || c == '_' || c == '-') {
            result += c;
        }
    }

    if (result.empty()) {
        result = "my_project";
    }

    return result;
}

/**
 * Generate default tml.toml content
 */
std::string generate_manifest(const std::string& name, bool is_lib, const std::string& bin_path) {
    std::ostringstream oss;

    // [package] section
    oss << "[package]\n";
    oss << "name = \"" << name << "\"\n";
    oss << "version = \"0.1.0\"\n";
    oss << "authors = []\n";
    oss << "edition = \"2024\"\n";
    oss << "\n";

    if (is_lib) {
        // [lib] section
        oss << "[lib]\n";
        oss << "path = \"src/lib.tml\"\n";
        oss << "crate-type = [\"rlib\"]\n";
        oss << "\n";
    } else {
        // [[bin]] section
        oss << "[[bin]]\n";
        oss << "name = \"" << name << "\"\n";
        oss << "path = \"" << (bin_path.empty() ? "src/main.tml" : bin_path) << "\"\n";
        oss << "\n";
    }

    // [dependencies] section (empty)
    oss << "[dependencies]\n";
    oss << "\n";

    // [build] section
    oss << "[build]\n";
    oss << "optimization-level = 0\n";
    oss << "emit-ir = false\n";
    oss << "verbose = false\n";
    oss << "\n";

    // [profile.release] section
    oss << "[profile.release]\n";
    oss << "optimization-level = 2\n";

    return oss.str();
}

/**
 * Create basic source file
 */
bool create_source_file(const fs::path& path, bool is_lib) {
    fs::create_directories(path.parent_path());

    std::ofstream file(path);
    if (!file) {
        return false;
    }

    if (is_lib) {
        file << "// " << path.filename().string() << "\n";
        file << "\n";
        file << "pub func add(a: I32, b: I32) -> I32 {\n";
        file << "    return a + b\n";
        file << "}\n";
        file << "\n";
        file << "pub func subtract(a: I32, b: I32) -> I32 {\n";
        file << "    return a - b\n";
        file << "}\n";
    } else {
        file << "// " << path.filename().string() << "\n";
        file << "\n";
        file << "func main() {\n";
        file << "    println(\"Hello, TML!\")\n";
        file << "}\n";
    }

    file.close();
    return true;
}

} // anonymous namespace

int run_init(int argc, char* argv[]) {
    std::string project_name;
    bool is_lib = false;
    std::string bin_path;
    bool create_src = true;

    // Parse arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--lib") {
            is_lib = true;
        } else if (arg == "--bin") {
            is_lib = false;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                bin_path = argv[++i];
            }
        } else if (arg == "--name") {
            if (i + 1 < argc) {
                project_name = argv[++i];
            } else {
                TML_LOG_ERROR("init", "--name requires a value");
                return 1;
            }
        } else if (arg == "--no-src") {
            create_src = false;
        } else if (arg == "--help" || arg == "-h") {
            std::cerr << "Usage: tml init [options]\n"
                      << "\n"
                      << "Initialize a new TML project in the current directory.\n"
                      << "\n"
                      << "Options:\n"
                      << "  --lib              Create a library project (default: binary)\n"
                      << "  --bin [path]       Create a binary project with optional path\n"
                      << "  --name <name>      Set project name (default: directory name)\n"
                      << "  --no-src           Don't create src/ directory or source files\n"
                      << "  --help, -h         Show this help message\n"
                      << "\n"
                      << "Examples:\n"
                      << "  tml init                    # Create binary project\n"
                      << "  tml init --lib              # Create library project\n"
                      << "  tml init --name my_app      # Set custom name\n"
                      << "  tml init --bin src/app.tml  # Custom binary path\n";
            return 0;
        } else {
            TML_LOG_ERROR("init", "Unknown argument: "
                                      << arg << ". Use 'tml init --help' for usage information");
            return 1;
        }
    }

    // Get project name
    if (project_name.empty()) {
        project_name = get_default_project_name();
    }

    // Check if tml.toml already exists
    fs::path manifest_path = fs::current_path() / "tml.toml";
    if (fs::exists(manifest_path)) {
        TML_LOG_ERROR("init", "tml.toml already exists in current directory. Remove it or run 'tml "
                              "init' in a different directory");
        return 1;
    }

    // Generate and write tml.toml
    std::string manifest_content = generate_manifest(project_name, is_lib, bin_path);

    std::ofstream manifest_file(manifest_path);
    if (!manifest_file) {
        TML_LOG_ERROR("init", "Cannot create tml.toml");
        return 1;
    }
    manifest_file << manifest_content;
    manifest_file.close();

    TML_LOG_INFO("init", "Created tml.toml");

    // Create source directory and files if requested
    if (create_src) {
        fs::path src_dir = fs::current_path() / "src";

        if (is_lib) {
            fs::path lib_file = src_dir / "lib.tml";
            if (create_source_file(lib_file, true)) {
                TML_LOG_INFO("init", "Created " << to_forward_slashes(lib_file.string()));
            } else {
                TML_LOG_WARN("init", "Could not create " << lib_file);
            }
        } else {
            fs::path main_file = bin_path.empty() ? src_dir / "main.tml" : fs::path(bin_path);
            if (create_source_file(main_file, false)) {
                TML_LOG_INFO("init", "Created " << to_forward_slashes(main_file.string()));
            } else {
                TML_LOG_WARN("init", "Could not create " << main_file);
            }
        }

        // Create build directory
        fs::path build_dir = fs::current_path() / "build";
        fs::create_directories(build_dir);
        TML_LOG_INFO("init", "Created build/");
    }

    TML_LOG_INFO("init", "Initialized TML project: " << project_name);

    if (is_lib) {
        TML_LOG_INFO("init", "Next steps:");
        TML_LOG_INFO("init", "  1. Edit src/lib.tml");
        TML_LOG_INFO("init", "  2. Build: tml build");
        TML_LOG_INFO("init", "  3. Run tests: tml test");
    } else {
        TML_LOG_INFO("init", "Next steps:");
        TML_LOG_INFO("init", "  1. Edit " << (bin_path.empty() ? "src/main.tml" : bin_path));
        TML_LOG_INFO("init", "  2. Build and run: tml run");
        TML_LOG_INFO("init", "  3. Build only: tml build");
    }

    return 0;
}

} // namespace tml::cli
