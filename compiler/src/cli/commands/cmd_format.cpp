//! # Code Formatting Command
//!
//! This file implements the `tml fmt` command for formatting TML source files.
//!
//! ## Usage
//!
//! ```bash
//! tml fmt                     # Format all .tml files in current dir
//! tml fmt src/*.tml           # Format specific files
//! tml fmt --check             # Check formatting without changing files
//! ```
//!
//! ## Formatting Rules
//!
//! The formatter ensures consistent code style:
//! - 4-space indentation
//! - Consistent spacing around operators
//! - Proper brace placement
//! - Sorted imports
//!
//! ## Process
//!
//! 1. Lex and parse the source file
//! 2. Run the AST through the formatter
//! 3. Write formatted output (or check for differences)

#include "cmd_format.hpp"

#include "common.hpp"
#include "cli/diagnostic.hpp"
#include "format/formatter.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "cli/utils.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace tml;

namespace tml::cli {

/// Emits all lexer errors using the diagnostic emitter.
static void emit_all_lexer_errors(DiagnosticEmitter& emitter, const lexer::Lexer& lex) {
    for (const auto& error : lex.errors()) {
        emitter.error("L001", error.message, error.span);
    }
}

// Emit a parser error using the diagnostic emitter (with fix-it hints)
static void emit_parser_error(DiagnosticEmitter& emitter, const parser::ParseError& error) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.code = "P001";
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

int run_fmt(const std::string& path, bool check_only, bool verbose) {
    auto& diag = get_diagnostic_emitter();

    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
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
            std::cerr << path << " would be reformatted\n";
            return 1;
        }
        if (verbose) {
            std::cout << path << " is correctly formatted\n";
        }
        return 0;
    }

    std::ofstream out(path);
    if (!out) {
        std::cerr << "error: Cannot write to " << path << "\n";
        return 1;
    }
    out << formatted;
    out.close();

    if (verbose) {
        std::cout << "Formatted " << path << "\n";
    } else {
        std::cout << "fmt: " << path << "\n";
    }

    return 0;
}

} // namespace tml::cli
