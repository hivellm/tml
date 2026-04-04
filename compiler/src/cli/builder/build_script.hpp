#pragma once

/// Build script support — detect and execute build.tml scripts.
///
/// A build.tml file in a package directory is compiled and executed before
/// the package itself. Its stdout is parsed for `tml:` directives that
/// control linking, artifact copying, and conditional compilation.
///
/// Supported directives:
/// - `tml:link-lib=<name>`          → link against library
/// - `tml:link-search=<path>`       → add library search path (-L)
/// - `tml:copy-artifact=<path>`     → copy file to output directory post-link
/// - `tml:warning=<msg>`            → emit a build warning
/// - `tml:cfg=<key>`                → define a conditional compilation symbol
/// - `tml:rerun-if-changed=<path>`  → rebuild when this file changes

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

/// Parsed result of a build.tml execution.
struct BuildScriptResult {
    /// Libraries to link against (from `tml:link-lib=`).
    std::set<std::string> link_libs;

    /// Library search paths (from `tml:link-search=`).
    std::vector<fs::path> link_search_paths;

    /// Artifacts to copy to output dir post-link (from `tml:copy-artifact=`).
    std::vector<fs::path> copy_artifacts;

    /// Warnings to emit (from `tml:warning=`).
    std::vector<std::string> warnings;

    /// Conditional compilation symbols (from `tml:cfg=`).
    std::set<std::string> cfg_symbols;

    /// Paths that trigger rebuild (from `tml:rerun-if-changed=`).
    std::vector<fs::path> rerun_paths;

    /// Whether the build script executed successfully.
    bool success = false;

    /// Error message if execution failed.
    std::string error_message;
};

/// Parse build script stdout into structured directives.
///
/// Lines prefixed with "tml:" are parsed as directives.
/// All other lines are passed through as normal output.
/// Unknown directive names are silently ignored.
auto parse_build_directives(const std::string& stdout_text) -> BuildScriptResult;

/// Detect the package directory for a source file.
///
/// Walks up from the source file's directory until finding a directory
/// containing `package.toml`, or returns empty path if not found.
auto detect_package_dir(const fs::path& source_file) -> fs::path;

/// Execute a build.tml script and return parsed results.
///
/// Compiles build.tml to a temporary executable, runs it, captures stdout,
/// and parses the `tml:` directives. If compilation or execution fails,
/// returns a result with success=false and an error message.
///
/// @param build_script_path  Path to the build.tml file
/// @param package_dir        The package root directory (build.tml's parent)
/// @param tml_exe_path       Path to the tml compiler executable
/// @param verbose            Enable verbose logging
auto run_build_script(const fs::path& build_script_path, const fs::path& package_dir,
                      const fs::path& tml_exe_path, bool verbose) -> BuildScriptResult;

/// Convenience function: detect package dir, check for build.tml, run it.
///
/// Returns a successful no-op result if no build.tml exists.
/// If the build script fails, logs a warning and returns a successful no-op.
auto detect_and_run_build_script(const std::string& source_path, bool verbose) -> BuildScriptResult;

} // namespace tml::cli
