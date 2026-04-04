TML_MODULE("compiler")

//! # Build Script Support
//!
//! Implements detection, execution, and parsing of build.tml scripts.
//! These scripts run before package compilation and emit `tml:` directives
//! to control linking and artifact management.

#include "cli/builder/build_script.hpp"

#include "log/log.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace tml::cli {

auto parse_build_directives(const std::string& stdout_text) -> BuildScriptResult {
    BuildScriptResult result;
    result.success = true;

    std::istringstream stream(stdout_text);
    std::string line;

    while (std::getline(stream, line)) {
        // Strip trailing \r if present (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Only process lines starting with "tml:"
        if (line.size() < 4 || line.substr(0, 4) != "tml:") {
            continue;
        }

        // Remove "tml:" prefix
        std::string directive = line.substr(4);

        // Split on first '=' to get name and value
        auto eq_pos = directive.find('=');
        if (eq_pos == std::string::npos) {
            // Directive without value — silently ignore
            continue;
        }

        std::string name = directive.substr(0, eq_pos);
        std::string value = directive.substr(eq_pos + 1);

        // Trim whitespace from value
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.erase(value.begin());
        }
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
            value.pop_back();
        }

        if (value.empty()) {
            continue;
        }

        if (name == "link-lib") {
            result.link_libs.insert(value);
        } else if (name == "link-search") {
            result.link_search_paths.push_back(fs::path(value));
        } else if (name == "copy-artifact") {
            result.copy_artifacts.push_back(fs::path(value));
        } else if (name == "warning") {
            result.warnings.push_back(value);
        } else if (name == "cfg") {
            result.cfg_symbols.insert(value);
        } else if (name == "rerun-if-changed") {
            result.rerun_paths.push_back(fs::path(value));
        }
        // Unknown directives are silently ignored
    }

    return result;
}

auto detect_package_dir(const fs::path& source_file) -> fs::path {
    fs::path dir = source_file;
    if (fs::is_regular_file(dir)) {
        dir = dir.parent_path();
    }

    // Walk up until we find package.toml or run out of parents
    const int max_depth = 20; // Safety limit
    for (int i = 0; i < max_depth; ++i) {
        if (dir.empty()) {
            break;
        }

        if (fs::exists(dir / "package.toml")) {
            return dir;
        }

        auto parent = dir.parent_path();
        if (parent == dir) {
            break; // Reached filesystem root
        }
        dir = parent;
    }

    return {}; // No package.toml found
}

/// Run a command and capture its stdout.
static std::string run_capture(const std::string& cmd, int& exit_code) {
    std::string output;

#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif

    if (pipe == nullptr) {
        exit_code = -1;
        return "";
    }

    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        output += buf;
    }

#ifdef _WIN32
    exit_code = _pclose(pipe);
#else
    exit_code = pclose(pipe);
#endif

    return output;
}

auto run_build_script(const fs::path& build_script_path, const fs::path& package_dir,
                      const fs::path& tml_exe_path, bool verbose) -> BuildScriptResult {
    BuildScriptResult result;
    result.success = false;

    if (!fs::exists(build_script_path)) {
        result.error_message = "Build script not found: " + build_script_path.string();
        return result;
    }

    // Determine temp directory for build script compilation
    fs::path temp_dir = package_dir / ".build-script-cache";
    std::error_code ec;
    fs::create_directories(temp_dir, ec);
    if (ec) {
        result.error_message = "Failed to create build script cache dir: " + ec.message();
        return result;
    }

    fs::path temp_exe = temp_dir / "build_script.exe";

    // Step 1: Compile build.tml to executable
    // Use the tml compiler itself to build the script
    std::string compile_cmd;
#ifdef _WIN32
    // On Windows, cmd.exe requires the entire command wrapped in outer quotes
    // when inner arguments also contain quotes: cmd /c "..."
    compile_cmd = "\"\"" + tml_exe_path.string() + "\" build \"" + build_script_path.string() +
                  "\" --out-dir=\"" + temp_dir.string() + "\"\" 2>&1";
#else
    compile_cmd = "\"" + tml_exe_path.string() + "\" build \"" + build_script_path.string() +
                  "\" --out-dir=\"" + temp_dir.string() + "\" 2>&1";
#endif

    if (verbose) {
        TML_LOG_INFO("build", "[build.tml] Compiling: " << build_script_path.string());
    }

    int compile_exit = 0;
    std::string compile_output = run_capture(compile_cmd, compile_exit);

    if (compile_exit != 0) {
        result.error_message = "Build script compilation failed:\n" + compile_output;
        return result;
    }

    // Find the compiled executable
    // The compiler outputs to temp_dir with the module name
    fs::path actual_exe;
    if (fs::exists(temp_exe)) {
        actual_exe = temp_exe;
    } else {
        // Try to find any .exe in the temp dir (module name might differ)
        for (const auto& entry : fs::directory_iterator(temp_dir)) {
            if (entry.path().extension() == ".exe") {
                actual_exe = entry.path();
                break;
            }
        }
    }

    if (actual_exe.empty() || !fs::exists(actual_exe)) {
        result.error_message =
            "Build script compiled but no executable found in: " + temp_dir.string() +
            "\nCompiler output: " + compile_output;
        return result;
    }

    // Step 2: Execute the build script
    // Set working directory to package_dir via cd
    std::string run_cmd;
#ifdef _WIN32
    run_cmd = "\"cd /d \"" + package_dir.string() + "\" && \"" + actual_exe.string() + "\"\" 2>&1";
#else
    run_cmd = "cd \"" + package_dir.string() + "\" && \"" + actual_exe.string() + "\" 2>&1";
#endif

    if (verbose) {
        TML_LOG_INFO("build", "[build.tml] Running: " << actual_exe.string());
    }

    int run_exit = 0;
    std::string run_output = run_capture(run_cmd, run_exit);

    if (run_exit != 0) {
        result.error_message = "Build script execution failed (exit code " +
                               std::to_string(run_exit) + "):\n" + run_output;
        return result;
    }

    // Step 3: Parse the output for tml: directives
    result = parse_build_directives(run_output);

    // Print warnings immediately
    for (const auto& warn : result.warnings) {
        TML_LOG_WARN("build", "[build.tml] " << warn);
    }

    if (verbose && !result.link_libs.empty()) {
        std::string libs_str;
        for (const auto& lib : result.link_libs) {
            if (!libs_str.empty())
                libs_str += ", ";
            libs_str += lib;
        }
        TML_LOG_INFO("build", "[build.tml] Link libraries: " << libs_str);
    }

    if (verbose && !result.link_search_paths.empty()) {
        for (const auto& sp : result.link_search_paths) {
            TML_LOG_INFO("build", "[build.tml] Search path: " << sp.string());
        }
    }

    return result;
}

/// Get the path to the currently running tml executable.
static fs::path get_self_exe_path() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return fs::path(std::string(buf, len));
    }
#else
    std::error_code ec;
    auto exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        return exe;
    }
#endif
    return {};
}

auto detect_and_run_build_script(const std::string& source_path, bool verbose)
    -> BuildScriptResult {
    BuildScriptResult result;
    result.success = true; // No build.tml is a successful no-op

    fs::path source = fs::absolute(fs::path(source_path));
    fs::path package_dir = detect_package_dir(source);

    if (package_dir.empty()) {
        return result; // No package — no build script
    }

    fs::path build_script = package_dir / "build.tml";
    if (!fs::exists(build_script)) {
        return result; // No build script — no-op
    }

    // Guard against recursion: if we're compiling the build script itself, skip
    std::error_code ec_cmp;
    if (fs::equivalent(source, build_script, ec_cmp)) {
        return result; // Don't recurse into ourselves
    }

    TML_LOG_INFO("build", "[build.tml] Found build script: " << build_script.string());

    fs::path tml_exe = get_self_exe_path();
    if (tml_exe.empty()) {
        TML_LOG_WARN("build", "[build.tml] Cannot determine compiler path, skipping build script");
        return result;
    }

    result = run_build_script(build_script, package_dir, tml_exe, verbose);

    if (!result.success) {
        TML_LOG_WARN("build", "[build.tml] Build script failed: " << result.error_message);
        // Reset to no-op so the build can continue
        result = BuildScriptResult{};
        result.success = true;
    }

    return result;
}

} // namespace tml::cli
