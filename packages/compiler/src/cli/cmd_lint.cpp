#include "cmd_lint.hpp"

#include "tml/common.hpp"
#include "utils.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

namespace {

// ANSI color codes
const char* RED = "\033[31m";
const char* YELLOW = "\033[33m";
const char* GREEN = "\033[32m";
const char* CYAN = "\033[36m";
const char* DIM = "\033[2m";
const char* RESET = "\033[0m";

struct LintIssue {
    std::string file;
    int line;
    std::string code; // TAB, TRAIL, LENGTH, etc.
    std::string message;
    bool is_error; // true = error, false = warning
};

struct LintResult {
    std::vector<LintIssue> issues;
    int files_checked = 0;
    int errors = 0;
    int warnings = 0;
};

// Check a single file for lint issues
void lint_file(const fs::path& filepath, LintResult& result, bool fix_mode) {
    std::ifstream file(filepath);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << filepath << "\n";
        return;
    }

    result.files_checked++;
    std::string line;
    int line_number = 0;
    bool file_modified = false;
    std::vector<std::string> lines;

    while (std::getline(file, line)) {
        line_number++;
        std::string original_line = line;

        // Check for tabs (TML uses spaces)
        if (line.find('\t') != std::string::npos) {
            if (fix_mode) {
                // Replace tabs with 4 spaces
                std::string fixed;
                for (char c : line) {
                    if (c == '\t') {
                        fixed += "    ";
                    } else {
                        fixed += c;
                    }
                }
                line = fixed;
                file_modified = true;
            } else {
                result.issues.push_back(
                    {filepath.string(), line_number, "TAB", "contains tabs (use spaces)", true});
                result.errors++;
            }
        }

        // Check for trailing whitespace
        if (!line.empty()) {
            size_t last_non_space = line.find_last_not_of(" \t\r");
            if (last_non_space != std::string::npos && last_non_space < line.length() - 1) {
                if (fix_mode) {
                    line = line.substr(0, last_non_space + 1);
                    file_modified = true;
                } else {
                    result.issues.push_back(
                        {filepath.string(), line_number, "TRAIL", "trailing whitespace", true});
                    result.errors++;
                }
            }
        }

        // Check for very long lines (>120 chars)
        if (line.length() > 120) {
            result.issues.push_back({
                filepath.string(), line_number, "LENGTH",
                "line exceeds 120 characters (" + std::to_string(line.length()) + " chars)",
                false // Warning, not error
            });
            result.warnings++;
        }

        lines.push_back(line);
    }
    file.close();

    // Write fixed file if modifications were made
    if (fix_mode && file_modified) {
        std::ofstream out(filepath, std::ios::trunc);
        if (out) {
            for (size_t i = 0; i < lines.size(); ++i) {
                out << lines[i];
                if (i < lines.size() - 1) {
                    out << "\n";
                }
            }
            // Ensure file ends with newline
            out << "\n";
            std::cout << "  " << GREEN << "[FIXED]" << RESET << " " << filepath.filename().string()
                      << "\n";
        }
    }
}

// Recursively find all .tml files in a directory
void find_tml_files(const fs::path& dir, std::vector<fs::path>& files) {
    try {
        if (!fs::exists(dir))
            return;

        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tml") {
                files.push_back(entry.path());
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Warning: Cannot access " << dir << ": " << e.what() << "\n";
    }
}

void print_lint_help() {
    std::cout << "Usage: tml lint [options] [paths...]\n\n";
    std::cout << "Lint TML source files for style and common issues.\n\n";
    std::cout << "Options:\n";
    std::cout << "  --fix           Automatically fix issues where possible\n";
    std::cout << "  --quiet, -q     Only show errors (no warnings)\n";
    std::cout << "  --verbose, -v   Show all files being checked\n";
    std::cout << "  --help, -h      Show this help\n\n";
    std::cout << "If no paths are specified, lints the current directory.\n\n";
    std::cout << "Checks:\n";
    std::cout << "  TAB       Tabs (TML uses spaces for indentation)\n";
    std::cout << "  TRAIL     Trailing whitespace\n";
    std::cout << "  LENGTH    Lines exceeding 120 characters (warning)\n";
}

} // anonymous namespace

int run_lint(int argc, char* argv[]) {
    bool fix_mode = false;
    bool quiet = false;
    bool verbose = false;
    std::vector<std::string> paths;

    // Parse arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--fix") {
            fix_mode = true;
        } else if (arg == "--quiet" || arg == "-q") {
            quiet = true;
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            print_lint_help();
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            paths.push_back(arg);
        }
    }

    // Default to current directory if no paths specified
    if (paths.empty()) {
        paths.push_back(".");
    }

    std::cout << CYAN << "TML" << RESET << " " << DIM << "v" << VERSION << RESET << "\n\n";

    if (fix_mode) {
        std::cout << YELLOW << "Linting and fixing TML files..." << RESET << "\n";
    } else {
        std::cout << "Linting TML files...\n";
    }

    // Collect all .tml files
    std::vector<fs::path> files;
    for (const auto& path : paths) {
        fs::path p(path);
        if (fs::is_directory(p)) {
            find_tml_files(p, files);
        } else if (fs::is_regular_file(p) && p.extension() == ".tml") {
            files.push_back(p);
        } else if (fs::is_regular_file(p)) {
            std::cerr << "Warning: " << path << " is not a .tml file, skipping\n";
        } else {
            std::cerr << "Warning: " << path << " does not exist\n";
        }
    }

    if (files.empty()) {
        std::cout << "No .tml files found\n";
        return 0;
    }

    // Lint all files
    LintResult result;
    for (const auto& file : files) {
        if (verbose) {
            std::cout << "  Checking: " << file.string() << "\n";
        }
        lint_file(file, result, fix_mode);
    }

    // Print issues (if not in fix mode)
    if (!fix_mode) {
        for (const auto& issue : result.issues) {
            if (quiet && !issue.is_error)
                continue; // Skip warnings in quiet mode

            const char* color = issue.is_error ? RED : YELLOW;
            std::cout << "  " << color << "[" << issue.code << "]" << RESET << " " << issue.file
                      << ":" << issue.line << " - " << issue.message << "\n";
        }
    }

    // Print summary
    std::cout << "\n";
    std::cout << "Checked " << result.files_checked << " files\n";

    if (fix_mode) {
        std::cout << GREEN << "Lint fix complete" << RESET << "\n";
        return 0;
    }

    if (result.errors > 0 || result.warnings > 0) {
        if (result.errors > 0) {
            std::cout << RED << "Found " << result.errors << " error(s)" << RESET;
        }
        if (result.warnings > 0) {
            if (result.errors > 0)
                std::cout << ", ";
            std::cout << YELLOW << result.warnings << " warning(s)" << RESET;
        }
        std::cout << "\n";

        if (result.errors > 0) {
            std::cout << "Run " << CYAN << "tml lint --fix" << RESET << " to auto-fix errors\n";
            return 1;
        }
    } else {
        std::cout << GREEN << "All files passed lint checks" << RESET << "\n";
    }

    return 0;
}

} // namespace tml::cli
