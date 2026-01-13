//! # Documentation Command
//!
//! This module implements the `tml doc` command for generating documentation
//! from TML source files.
//!
//! ## Usage
//!
//! ```bash
//! tml doc [file.tml] [options]
//! tml doc --all [options]
//! ```
//!
//! ## Options
//!
//! - `--format=<fmt>`: Output format (json, html, md). Default: html
//! - `--output=<dir>`: Output directory. Default: ./docs
//! - `--include-private`: Include private items
//! - `--open`: Open documentation in browser after generation

#ifndef TML_CLI_CMD_DOC_HPP
#define TML_CLI_CMD_DOC_HPP

#include <string>
#include <vector>

namespace tml::cli {

/// Documentation output format.
enum class DocFormat {
    Json,     ///< JSON format for tooling integration.
    Html,     ///< HTML format for web viewing.
    Markdown, ///< Markdown format for wikis/READMEs.
};

/// Options for the doc command.
struct DocOptions {
    std::vector<std::string> input_files; ///< Input files to document.
    std::string output_dir = "docs";      ///< Output directory.
    DocFormat format = DocFormat::Html;   ///< Output format.
    bool include_private = false;         ///< Include private items.
    bool include_internals = false;       ///< Include @internal items.
    bool all_modules = false;             ///< Document all modules in project.
    bool open_browser = false;            ///< Open in browser after generation.
    bool verbose = false;                 ///< Verbose output.
};

/// Runs the doc command with the given options.
///
/// @param options Documentation generation options.
/// @returns 0 on success, 1 on failure.
int run_doc(const DocOptions& options);

/// Parses command-line arguments for the doc command.
///
/// @param argc Argument count.
/// @param argv Argument values.
/// @returns Parsed options.
DocOptions parse_doc_args(int argc, char* argv[]);

/// Prints help for the doc command.
void print_doc_help();

} // namespace tml::cli

#endif // TML_CLI_CMD_DOC_HPP
