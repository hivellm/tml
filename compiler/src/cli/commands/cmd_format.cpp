//! # Code Formatting Command
//!
//! This file implements the `tml fmt` command for formatting source files.
//!
//! ## Usage
//!
//! ```bash
//! tml fmt                          # Format all files in current dir (recursive)
//! tml fmt src/                     # Format all files in src/ (recursive)
//! tml fmt file.tml                 # Format a single TML file
//! tml fmt file.cpp                 # Format a single C++ file via clang-format
//! tml fmt --check                  # Check formatting without changing files
//! ```
//!
//! ## Supported File Types
//!
//! - `.tml` — Formatted via the built-in AST-based formatter
//! - `.c`, `.cpp`, `.h`, `.hpp` — Formatted via `clang-format` (must be on PATH or LLVM install)
//!
//! ## Process
//!
//! 1. Detect file type by extension (or recurse if directory)
//! 2. TML files: lex → parse → format AST → write
//! 3. C/C++ files: delegate to clang-format

#include "cmd_format.hpp"

#include "cli/diagnostic.hpp"
#include "cli/utils.hpp"
#include "common.hpp"
#include "format/formatter.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "log/log.hpp"
#include "parser/parser.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace tml;

namespace tml::cli {

// ============================================================================
// clang-format Discovery
// ============================================================================

/// Finds the clang-format binary on the system.
/// Checks known LLVM installation paths on Windows, falls back to PATH.
static std::string find_clang_format() {
#ifdef _WIN32
    std::vector<std::string> paths = {
        "F:/LLVM/bin/clang-format.exe",
        "C:/Program Files/LLVM/bin/clang-format.exe",
        "C:/LLVM/bin/clang-format.exe",
    };
    for (const auto& p : paths) {
        if (fs::exists(p)) {
            return p;
        }
    }
#endif
    // Fall back to PATH
    return "clang-format";
}

/// Checks if clang-format is actually available (can be executed).
static bool clang_format_available() {
    static int cached = -1;
    if (cached >= 0) {
        return cached != 0;
    }
#ifdef _WIN32
    int rc = std::system("clang-format --version >nul 2>&1");
#else
    int rc = std::system("clang-format --version >/dev/null 2>&1");
#endif
    // Also check our discovered path if the above failed
    if (rc != 0) {
        auto cf = find_clang_format();
        if (cf != "clang-format") {
            std::string cmd = "\"" + cf + "\" --version";
#ifdef _WIN32
            cmd += " >nul 2>&1";
#else
            cmd += " >/dev/null 2>&1";
#endif
            rc = std::system(cmd.c_str());
        }
    }
    cached = (rc == 0) ? 1 : 0;
    return cached != 0;
}

// ============================================================================
// File Extension Helpers
// ============================================================================

static bool is_tml_file(const fs::path& p) {
    auto ext = p.extension().string();
    return ext == ".tml";
}

static bool is_cpp_file(const fs::path& p) {
    auto ext = p.extension().string();
    return ext == ".c" || ext == ".cpp" || ext == ".h" || ext == ".hpp";
}

// ============================================================================
// TML Formatting (AST-based)
// ============================================================================

/// Emits all lexer errors using the diagnostic emitter.
static void emit_all_lexer_errors(DiagnosticEmitter& emitter, const lexer::Lexer& lex) {
    for (const auto& error : lex.errors()) {
        emitter.error(error.code.empty() ? "L001" : error.code, error.message, error.span);
    }
}

// Emit a parser error using the diagnostic emitter (with fix-it hints)
static void emit_parser_error(DiagnosticEmitter& emitter, const parser::ParseError& error) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.code = error.code.empty() ? "P001" : error.code;
    diag.message = error.message;
    diag.primary_span = error.span;
    diag.notes = error.notes;

    // Convert parser FixItHints to DiagnosticFixIts
    for (const auto& fix : error.fixes) {
        diag.fixes.push_back(DiagnosticFixIt{
            .span = fix.span, .replacement = fix.replacement, .description = fix.description});
    }

    emitter.emit(diag);
}

// Emit all parser errors using the diagnostic emitter
static void emit_all_parser_errors(DiagnosticEmitter& emitter,
                                   const std::vector<parser::ParseError>& errors) {
    for (const auto& error : errors) {
        emit_parser_error(emitter, error);
    }
}

/// Formats a single .tml file using the built-in AST formatter.
static int run_fmt_tml(const std::string& path, bool check_only, bool verbose) {
    auto& diag = get_diagnostic_emitter();

    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        TML_LOG_ERROR("fmt", "Failed to read file: " << e.what());
        return 1;
    }

    diag.set_source_content(path, source_code);

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        emit_all_lexer_errors(diag, lex);
        return 1;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        emit_all_parser_errors(diag, errors);
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    format::FormatOptions options;
    format::Formatter formatter(options);
    auto formatted = formatter.format(module);

    if (check_only) {
        if (formatted != source_code) {
            TML_LOG_WARN("fmt", path << " would be reformatted");
            return 1;
        }
        if (verbose) {
            TML_LOG_INFO("fmt", path << " is correctly formatted");
        }
        return 0;
    }

    std::ofstream out(path);
    if (!out) {
        TML_LOG_ERROR("fmt", "Cannot write to " << path);
        return 1;
    }
    out << formatted;
    out.close();

    if (verbose) {
        TML_LOG_INFO("fmt", "Formatted " << path);
    } else {
        TML_LOG_INFO("fmt", "fmt: " << path);
    }

    return 0;
}

// ============================================================================
// C/C++ Formatting (clang-format delegation)
// ============================================================================

/// Formats a single C/C++ file by delegating to clang-format.
static int run_fmt_cpp(const std::string& path, bool check_only, bool verbose) {
    auto clang_fmt = find_clang_format();

    // Quote the binary path if it contains spaces
    std::string quoted_bin = clang_fmt;
    if (clang_fmt.find(' ') != std::string::npos) {
        quoted_bin = "\"" + clang_fmt + "\"";
    }

    // Quote the file path
    std::string quoted_path = "\"" + path + "\"";

    std::string cmd;
    if (check_only) {
        cmd = quoted_bin + " --dry-run --Werror " + quoted_path;
#ifdef _WIN32
        cmd += " 2>nul";
#else
        cmd += " 2>/dev/null";
#endif
    } else {
        cmd = quoted_bin + " -i " + quoted_path;
    }

    int rc = std::system(cmd.c_str());

    if (check_only) {
        if (rc != 0) {
            TML_LOG_WARN("fmt", path << " would be reformatted (clang-format)");
            return 1;
        }
        if (verbose) {
            TML_LOG_INFO("fmt", path << " is correctly formatted");
        }
        return 0;
    }

    if (rc != 0) {
        TML_LOG_ERROR("fmt", "clang-format failed on " << path);
        return 1;
    }

    if (verbose) {
        TML_LOG_INFO("fmt", "Formatted " << path << " (clang-format)");
    } else {
        TML_LOG_INFO("fmt", "fmt: " << path);
    }

    return 0;
}

// ============================================================================
// Directory Formatting
// ============================================================================

/// Recursively formats all formattable files in a directory.
static int run_fmt_directory(const std::string& dir_path, bool check_only, bool verbose) {
    int total = 0;
    int errors = 0;
    bool need_clang_format = false;

    // First pass: check if we'll need clang-format
    for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
        if (entry.is_regular_file() && is_cpp_file(entry.path())) {
            need_clang_format = true;
            break;
        }
    }

    if (need_clang_format && !clang_format_available()) {
        TML_LOG_WARN("fmt", "clang-format not found — C/C++ files will be skipped. "
                            "Install LLVM or add clang-format to PATH.");
    }

    bool has_clang_fmt = need_clang_format && clang_format_available();

    for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        auto path = entry.path();
        if (is_tml_file(path)) {
            total++;
            if (run_fmt_tml(path.string(), check_only, verbose) != 0) {
                errors++;
            }
        } else if (is_cpp_file(path) && has_clang_fmt) {
            total++;
            if (run_fmt_cpp(path.string(), check_only, verbose) != 0) {
                errors++;
            }
        }
    }

    if (total == 0) {
        TML_LOG_INFO("fmt", "No formattable files found in " << dir_path);
    } else if (check_only) {
        if (errors > 0) {
            TML_LOG_WARN("fmt", errors << " of " << total << " files need formatting");
        } else {
            TML_LOG_INFO("fmt", "All " << total << " files are correctly formatted");
        }
    } else {
        TML_LOG_INFO("fmt", "Formatted " << total << " files (" << errors << " errors)");
    }

    return errors > 0 ? 1 : 0;
}

// ============================================================================
// Public Entry Point
// ============================================================================

int run_fmt(const std::string& path, bool check_only, bool verbose) {
    fs::path p(path);

    // Directory: recurse
    if (fs::is_directory(p)) {
        return run_fmt_directory(path, check_only, verbose);
    }

    // Single file: dispatch by extension
    if (is_tml_file(p)) {
        return run_fmt_tml(path, check_only, verbose);
    }

    if (is_cpp_file(p)) {
        if (!clang_format_available()) {
            TML_LOG_ERROR("fmt",
                          "clang-format not found. Install LLVM or add clang-format to PATH.");
            return 1;
        }
        return run_fmt_cpp(path, check_only, verbose);
    }

    TML_LOG_ERROR("fmt", "Unsupported file type: " << p.extension().string()
                                                   << " (supported: .tml, .c, .cpp, .h, .hpp)");
    return 1;
}

} // namespace tml::cli
