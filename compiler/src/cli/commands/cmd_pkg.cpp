TML_MODULE("compiler")

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
//! | `tml add`     | Implemented| Add path/git dependency        |
//! | `tml update`  | Implemented| Check/validate dependencies    |
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

#include "cli/builder/build_config.hpp"
#include "cli/builder/dependency_resolver.hpp"
#include "cli/commands/cmd_test.hpp" // For colors namespace
#include "cli/utils.hpp"
#include "log/log.hpp"

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

/**
 * Add a dependency entry to the manifest
 */
static std::string add_dependency_to_manifest(const std::string& content, const std::string& name,
                                              const std::string& dep_spec) {
    std::string result;
    std::istringstream stream(content);
    std::string line;
    bool found_dependencies = false;
    bool in_dependencies = false;
    bool added = false;

    while (std::getline(stream, line)) {
        result += line + "\n";

        // Check if we're entering the dependencies section
        if (line == "[dependencies]") {
            found_dependencies = true;
            in_dependencies = true;
        } else if (line.starts_with("[") && in_dependencies) {
            // Leaving dependencies section without adding - add before this line
            if (!added) {
                result.insert(result.length() - line.length() - 1, name + " = " + dep_spec + "\n");
                added = true;
            }
            in_dependencies = false;
        }
    }

    // If we found dependencies section but haven't added yet
    if (found_dependencies && !added) {
        result += name + " = " + dep_spec + "\n";
        added = true;
    }

    // If no dependencies section exists, create one
    if (!found_dependencies) {
        result += "\n[dependencies]\n";
        result += name + " = " + dep_spec + "\n";
    }

    return result;
}

int run_add(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: tml add <package> [options]\n";
        std::cerr << "\n";
        std::cerr << "Add a dependency to tml.toml\n";
        std::cerr << "\n";
        std::cerr << "Options:\n";
        std::cerr << "  --path <dir>    Add a path dependency\n";
        std::cerr << "  --git <url>     Add a git dependency\n";
        std::cerr << "  --version <ver> Specify version (requires registry)\n";
        std::cerr << "\n";
        std::cerr << "Examples:\n";
        std::cerr << "  tml add mylib --path ../mylib\n";
        std::cerr << "  tml add mylib --git https://github.com/user/mylib\n";
        return 1;
    }

    fs::path manifest_path = fs::current_path() / "tml.toml";
    if (!fs::exists(manifest_path)) {
        TML_LOG_ERROR(
            "pkg",
            "No tml.toml found in current directory. Run 'tml init' to create a new project");
        return 1;
    }

    std::string package_name = argv[2];
    std::string path_dep;
    std::string git_dep;
    std::string version_dep;

    // Parse options
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--path" && i + 1 < argc) {
            path_dep = argv[++i];
        } else if (arg == "--git" && i + 1 < argc) {
            git_dep = argv[++i];
        } else if (arg == "--version" && i + 1 < argc) {
            version_dep = argv[++i];
        }
    }

    // Determine dependency specification
    std::string dep_spec;
    if (!path_dep.empty()) {
        dep_spec = "{ path = \"" + path_dep + "\" }";
    } else if (!git_dep.empty()) {
        dep_spec = "{ git = \"" + git_dep + "\" }";
    } else if (!version_dep.empty()) {
        // Version dependencies require registry support
        TML_LOG_ERROR(
            "pkg", "Version dependencies require a package registry. Use --path or --git instead");
        return 1;
    } else {
        TML_LOG_ERROR("pkg", "Must specify --path, --git, or --version. Use 'tml add <package> "
                             "--path <dir>' for local dependencies");
        return 1;
    }

    // Read current manifest
    std::string content = read_manifest_file(manifest_path);
    if (content.empty()) {
        TML_LOG_ERROR("pkg", "Could not read tml.toml");
        return 1;
    }

    // Check if dependency already exists
    if (content.find(package_name + " =") != std::string::npos ||
        content.find(package_name + "=") != std::string::npos) {
        TML_LOG_ERROR("pkg", "Dependency '" << package_name << "' already exists. Use 'tml remove "
                                            << package_name << "' first to replace it");
        return 1;
    }

    // Add the dependency
    std::string new_content = add_dependency_to_manifest(content, package_name, dep_spec);

    // Write updated manifest
    if (!write_manifest_file(manifest_path, new_content)) {
        TML_LOG_ERROR("pkg", "Could not write tml.toml");
        return 1;
    }

    TML_LOG_INFO("pkg", "+ Added " << package_name << " " << dep_spec);
    return 0;
}

int run_update(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    fs::path manifest_path = fs::current_path() / "tml.toml";
    if (!fs::exists(manifest_path)) {
        TML_LOG_ERROR(
            "pkg",
            "No tml.toml found in current directory. Run 'tml init' to create a new project");
        return 1;
    }

    // Load manifest
    auto manifest = Manifest::load(manifest_path);
    if (!manifest) {
        TML_LOG_ERROR("pkg", "Could not parse tml.toml");
        return 1;
    }

    if (manifest->dependencies.empty()) {
        TML_LOG_INFO("pkg", "No dependencies to update.");
        return 0;
    }

    bool any_issues = false;
    int checked = 0;

    TML_LOG_INFO("pkg", "Checking dependencies...");
    for (const auto& [name, dep] : manifest->dependencies) {
        checked++;

        if (!dep.path.empty()) {
            // Check if path dependency exists
            fs::path dep_path = fs::current_path() / dep.path;
            if (!fs::exists(dep_path)) {
                TML_LOG_ERROR("pkg", name << " path not found: " << dep.path);
                any_issues = true;
            } else if (!fs::exists(dep_path / "tml.toml")) {
                TML_LOG_WARN("pkg", name << " has no tml.toml");
            } else {
                TML_LOG_INFO("pkg", "  ok: " << name << " (path: " << dep.path << ")");
            }
        } else if (!dep.git.empty()) {
            TML_LOG_INFO("pkg", "  git: " << name << " - " << dep.git);
            TML_LOG_INFO("pkg", "       (run 'git pull' in dependency directory to update)");
        } else if (!dep.version.empty()) {
            TML_LOG_WARN("pkg",
                         "  skip: " << name << " " << dep.version << " (registry not available)");
        }
    }

    TML_LOG_INFO("pkg", "Checked " << checked << " dependencies.");

    if (any_issues) {
        TML_LOG_ERROR("pkg", "Some dependencies have issues. See above for details.");
        return 1;
    }

    TML_LOG_INFO("pkg", "All path dependencies are valid.");
    TML_LOG_INFO("pkg",
                 "Note: For git dependencies, run 'git pull' in each dependency's directory.");
    return 0;
}

int run_publish(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TML_LOG_ERROR("pkg", "'tml publish' is not yet implemented");
    TML_LOG_INFO("pkg", "There is no TML package registry available yet.");
    TML_LOG_INFO("pkg", "To share your library, consider:");
    TML_LOG_INFO("pkg", "  - Publishing to GitHub/GitLab");
    TML_LOG_INFO("pkg", "  - Using git dependencies (coming soon)");

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
        TML_LOG_ERROR(
            "pkg",
            "No tml.toml found in current directory. Run 'tml init' to create a new project");
        return 1;
    }

    std::string package_name = argv[2];

    // Read current manifest
    std::string content = read_manifest_file(manifest_path);
    if (content.empty()) {
        TML_LOG_ERROR("pkg", "Could not read tml.toml");
        return 1;
    }

    // Check if dependency exists
    if (content.find(package_name + " =") == std::string::npos &&
        content.find(package_name + "=") == std::string::npos) {
        TML_LOG_ERROR("pkg", "Dependency '" << package_name << "' not found");
        return 1;
    }

    // Remove the dependency
    std::string new_content = remove_dependency_from_manifest(content, package_name);

    // Write updated manifest
    if (!write_manifest_file(manifest_path, new_content)) {
        TML_LOG_ERROR("pkg", "Could not write tml.toml");
        return 1;
    }

    TML_LOG_INFO("pkg", "- Removed " << package_name);
    return 0;
}

int run_deps(int argc, char* argv[]) {
    fs::path manifest_path = fs::current_path() / "tml.toml";
    if (!fs::exists(manifest_path)) {
        TML_LOG_ERROR(
            "pkg",
            "No tml.toml found in current directory. Run 'tml init' to create a new project");
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
        TML_LOG_ERROR("pkg", "Could not parse tml.toml");
        return 1;
    }

    if (manifest->dependencies.empty()) {
        TML_LOG_INFO("pkg", manifest->package.name << " v" << manifest->package.version);
        TML_LOG_INFO("pkg", "No dependencies.");
        return 0;
    }

    TML_LOG_INFO("pkg", manifest->package.name << " v" << manifest->package.version);

    if (show_tree) {
        // Resolve dependencies for tree view
        DependencyResolverOptions opts;
        DependencyResolver resolver(opts);
        auto result = resolver.resolve(*manifest, fs::current_path());

        if (!result.success) {
            TML_LOG_ERROR("pkg", result.error_message);
            return 1;
        }

        // Print dependency tree
        std::function<void(const std::string&, int, std::set<std::string>&)> print_tree;
        print_tree = [&](const std::string& name, int depth, std::set<std::string>& visited) {
            auto it = result.by_name.find(name);
            if (it == result.by_name.end())
                return;

            std::string indent(depth * 2, ' ');
            if (visited.count(name)) {
                TML_LOG_INFO("pkg", indent << "|-- " << name << " (*)");
            } else {
                TML_LOG_INFO("pkg", indent << "|-- " << name << " v" << it->second.version);
                visited.insert(name);
                for (const auto& child : it->second.dependencies) {
                    print_tree(child, depth + 1, visited);
                }
            }
        };

        std::set<std::string> visited;
        for (const auto& [name, dep] : manifest->dependencies) {
            print_tree(name, 0, visited);
        }
    } else {
        // Simple list view
        for (const auto& [name, dep] : manifest->dependencies) {
            if (!dep.version.empty()) {
                TML_LOG_INFO("pkg", "  " << name << " " << dep.version);
            } else if (!dep.path.empty()) {
                TML_LOG_INFO("pkg", "  " << name << " (path: " << dep.path << ")");
            } else if (!dep.git.empty()) {
                TML_LOG_INFO("pkg", "  " << name << " (git: " << dep.git << ")");
            } else {
                TML_LOG_INFO("pkg", "  " << name);
            }
        }
    }

    return 0;
}

} // namespace tml::cli
