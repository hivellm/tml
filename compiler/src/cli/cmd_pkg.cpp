//! # Package Management Commands
//!
//! This file implements package management CLI commands.
//!
//! ## Commands
//!
//! | Command       | Status     | Description                    |
//! |---------------|------------|--------------------------------|
//! | `tml deps`    | Implemented| List project dependencies      |
//! | `tml remove`  | Implemented| Remove dependency from tml.toml|
//! | `tml add`     | Pending    | Add package (no registry yet)  |
//! | `tml update`  | Pending    | Update packages                |
//! | `tml publish` | Pending    | Publish to registry            |
//!
//! ## Dependency Display
//!
//! ```text
//! $ tml deps
//! myproject v1.0.0
//!   core ^0.1.0
//!   utils (path: ../utils)
//!
//! $ tml deps --tree
//! myproject v1.0.0
//!   |-- core v0.1.0
//!   |   |-- alloc v0.1.0
//!   |-- utils v1.0.0
//! ```

#include "cmd_pkg.hpp"

#include "build_config.hpp"
#include "cmd_test.hpp" // For colors namespace
#include "dependency_resolver.hpp"
#include "utils.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace tml::cli {

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Read the entire tml.toml file as a string
 */
static std::string read_manifest_file(const fs::path& path) {
    std::ifstream file(path);
    if (!file) {
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

/**
 * Write content to tml.toml file
 */
static bool write_manifest_file(const fs::path& path, const std::string& content) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }
    file << content;
    return true;
}

/**
 * Remove a dependency from the manifest
 */
static std::string remove_dependency_from_manifest(const std::string& content,
                                                   const std::string& name) {
    std::string result;
    std::istringstream stream(content);
    std::string line;
    bool in_dependencies = false;

    while (std::getline(stream, line)) {
        // Check if we're entering a section
        if (line.starts_with("[")) {
            in_dependencies = (line == "[dependencies]");
        }

        // Skip the dependency line if in dependencies section
        if (in_dependencies && line.starts_with(name + " ")) {
            continue;
        }
        if (in_dependencies && line.starts_with(name + "=")) {
            continue;
        }

        result += line + "\n";
    }

    return result;
}

// ============================================================================
// Command Implementations
// ============================================================================

// NOTE: run_add, run_update, and run_publish are not yet implemented because
// there is no package registry service (like crates.io for Rust) available.
// These commands will be implemented when a TML package registry is created.
//
// For now, use path dependencies in tml.toml:
//   [dependencies]
//   mylib = { path = "../mylib" }

int run_add(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cerr << "error: 'tml add' is not yet implemented\n";
    std::cerr << "\n";
    std::cerr << "There is no TML package registry available yet.\n";
    std::cerr << "Use path dependencies instead by editing tml.toml directly:\n";
    std::cerr << "\n";
    std::cerr << "  [dependencies]\n";
    std::cerr << "  mylib = { path = \"../mylib\" }\n";
    std::cerr << "\n";
    std::cerr << "Or for git dependencies (coming soon):\n";
    std::cerr << "  mylib = { git = \"https://github.com/user/mylib\" }\n";

    return 1;
}

int run_update(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cerr << "error: 'tml update' is not yet implemented\n";
    std::cerr << "\n";
    std::cerr << "There is no TML package registry available yet.\n";
    std::cerr << "For path dependencies, changes are picked up automatically on rebuild.\n";

    return 1;
}

int run_publish(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cerr << "error: 'tml publish' is not yet implemented\n";
    std::cerr << "\n";
    std::cerr << "There is no TML package registry available yet.\n";
    std::cerr << "To share your library, consider:\n";
    std::cerr << "  - Publishing to GitHub/GitLab\n";
    std::cerr << "  - Using git dependencies (coming soon)\n";

    return 1;
}

int run_remove(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: tml remove <package>\n";
        std::cerr << "\n";
        std::cerr << "Remove a dependency from tml.toml\n";
        return 1;
    }

    fs::path manifest_path = fs::current_path() / "tml.toml";
    if (!fs::exists(manifest_path)) {
        std::cerr << "error: No tml.toml found in current directory\n";
        std::cerr << "hint: Run 'tml init' to create a new project\n";
        return 1;
    }

    std::string package_name = argv[2];

    // Read current manifest
    std::string content = read_manifest_file(manifest_path);
    if (content.empty()) {
        std::cerr << "error: Could not read tml.toml\n";
        return 1;
    }

    // Check if dependency exists
    if (content.find(package_name + " =") == std::string::npos &&
        content.find(package_name + "=") == std::string::npos) {
        std::cerr << "error: Dependency '" << package_name << "' not found\n";
        return 1;
    }

    // Remove the dependency
    std::string new_content = remove_dependency_from_manifest(content, package_name);

    // Write updated manifest
    if (!write_manifest_file(manifest_path, new_content)) {
        std::cerr << "error: Could not write tml.toml\n";
        return 1;
    }

    std::cout << colors::red << "-" << colors::reset << " Removed " << package_name << "\n";
    return 0;
}

int run_deps(int argc, char* argv[]) {
    fs::path manifest_path = fs::current_path() / "tml.toml";
    if (!fs::exists(manifest_path)) {
        std::cerr << "error: No tml.toml found in current directory\n";
        std::cerr << "hint: Run 'tml init' to create a new project\n";
        return 1;
    }

    bool show_tree = false;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--tree") {
            show_tree = true;
        }
    }

    // Load manifest
    auto manifest = Manifest::load(manifest_path);
    if (!manifest) {
        std::cerr << "error: Could not parse tml.toml\n";
        return 1;
    }

    if (manifest->dependencies.empty()) {
        std::cout << colors::bold << manifest->package.name << colors::reset << " v"
                  << manifest->package.version << "\n";
        std::cout << "No dependencies.\n";
        return 0;
    }

    std::cout << colors::bold << manifest->package.name << colors::reset << " v"
              << manifest->package.version << "\n";

    if (show_tree) {
        // Resolve dependencies for tree view
        DependencyResolverOptions opts;
        DependencyResolver resolver(opts);
        auto result = resolver.resolve(*manifest, fs::current_path());

        if (!result.success) {
            std::cerr << "error: " << result.error_message << "\n";
            return 1;
        }

        // Print dependency tree
        std::function<void(const std::string&, int, std::set<std::string>&)> print_tree;
        print_tree = [&](const std::string& name, int depth, std::set<std::string>& visited) {
            auto it = result.by_name.find(name);
            if (it == result.by_name.end())
                return;

            std::string indent(depth * 2, ' ');
            std::cout << indent << "|-- " << name;

            if (visited.count(name)) {
                std::cout << " (*)";
            } else {
                std::cout << " v" << it->second.version;
                visited.insert(name);
                for (const auto& child : it->second.dependencies) {
                    print_tree(child, depth + 1, visited);
                }
            }
            std::cout << "\n";
        };

        std::set<std::string> visited;
        for (const auto& [name, dep] : manifest->dependencies) {
            print_tree(name, 0, visited);
        }
    } else {
        // Simple list view
        for (const auto& [name, dep] : manifest->dependencies) {
            std::cout << "  " << name;
            if (!dep.version.empty()) {
                std::cout << " " << colors::green << dep.version << colors::reset;
            } else if (!dep.path.empty()) {
                std::cout << " " << colors::cyan << "(path: " << dep.path << ")" << colors::reset;
            } else if (!dep.git.empty()) {
                std::cout << " " << colors::yellow << "(git: " << dep.git << ")" << colors::reset;
            }
            std::cout << "\n";
        }
    }

    return 0;
}

} // namespace tml::cli
