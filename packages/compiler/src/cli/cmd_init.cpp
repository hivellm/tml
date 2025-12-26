#include "cmd_init.hpp"
#include "utils.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace tml::cli {

namespace {

/**
 * Get project name from current directory
 */
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
                std::cerr << "Error: --name requires a value\n";
                return 1;
            }
        } else if (arg == "--no-src") {
            create_src = false;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: tml init [options]\n";
            std::cout << "\n";
            std::cout << "Initialize a new TML project in the current directory.\n";
            std::cout << "\n";
            std::cout << "Options:\n";
            std::cout << "  --lib              Create a library project (default: binary)\n";
            std::cout << "  --bin [path]       Create a binary project with optional path\n";
            std::cout << "  --name <name>      Set project name (default: directory name)\n";
            std::cout << "  --no-src           Don't create src/ directory or source files\n";
            std::cout << "  --help, -h         Show this help message\n";
            std::cout << "\n";
            std::cout << "Examples:\n";
            std::cout << "  tml init                    # Create binary project\n";
            std::cout << "  tml init --lib              # Create library project\n";
            std::cout << "  tml init --name my_app      # Set custom name\n";
            std::cout << "  tml init --bin src/app.tml  # Custom binary path\n";
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            std::cerr << "Use 'tml init --help' for usage information\n";
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
        std::cerr << "Error: tml.toml already exists in current directory\n";
        std::cerr << "Remove it or run 'tml init' in a different directory\n";
        return 1;
    }

    // Generate and write tml.toml
    std::string manifest_content = generate_manifest(project_name, is_lib, bin_path);

    std::ofstream manifest_file(manifest_path);
    if (!manifest_file) {
        std::cerr << "Error: Cannot create tml.toml\n";
        return 1;
    }
    manifest_file << manifest_content;
    manifest_file.close();

    std::cout << "Created tml.toml\n";

    // Create source directory and files if requested
    if (create_src) {
        fs::path src_dir = fs::current_path() / "src";

        if (is_lib) {
            fs::path lib_file = src_dir / "lib.tml";
            if (create_source_file(lib_file, true)) {
                std::cout << "Created " << to_forward_slashes(lib_file.string()) << "\n";
            } else {
                std::cerr << "Warning: Could not create " << lib_file << "\n";
            }
        } else {
            fs::path main_file = bin_path.empty() ? src_dir / "main.tml" : fs::path(bin_path);
            if (create_source_file(main_file, false)) {
                std::cout << "Created " << to_forward_slashes(main_file.string()) << "\n";
            } else {
                std::cerr << "Warning: Could not create " << main_file << "\n";
            }
        }

        // Create build directory
        fs::path build_dir = fs::current_path() / "build";
        fs::create_directories(build_dir);
        std::cout << "Created build/\n";
    }

    std::cout << "\n";
    std::cout << "Initialized TML project: " << project_name << "\n";
    std::cout << "\n";

    if (is_lib) {
        std::cout << "Next steps:\n";
        std::cout << "  1. Edit src/lib.tml\n";
        std::cout << "  2. Build: tml build\n";
        std::cout << "  3. Run tests: tml test\n";
    } else {
        std::cout << "Next steps:\n";
        std::cout << "  1. Edit " << (bin_path.empty() ? "src/main.tml" : bin_path) << "\n";
        std::cout << "  2. Build and run: tml run\n";
        std::cout << "  3. Build only: tml build\n";
    }

    return 0;
}

} // namespace tml::cli
