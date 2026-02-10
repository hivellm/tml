//! # Debug Commands
//!
//! This file implements the `tml lex`, `tml parse`, and `tml check` commands
//! for debugging compilation phases.
//!
//! ## Debug Subcommands
//!
//! | Command           | Output                              |
//! |-------------------|-------------------------------------|
//! | `tml lex <file>`  | Token stream from lexer             |
//! | `tml parse <file>`| AST from parser                     |
//! | `tml check <file>`| Type checking results               |
//!
//! ## Usage
//!
//! ```bash
//! tml lex main.tml          # Show tokens
//! tml parse main.tml        # Show AST structure
//! tml check main.tml        # Run type checker
//! ```
//!
//! These commands are useful for debugging parser issues, understanding
//! how code is tokenized, and verifying type inference results.

#include "cmd_debug.hpp"

#include "cli/diagnostic.hpp"
#include "cli/utils.hpp"
#include "common.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "log/log.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"
#include "types/module.hpp"

#include <filesystem>
#include <iostream>
#include <set>
#include <tuple>

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

// Emit all type errors using the diagnostic emitter (with deduplication)
static void emit_all_type_errors(DiagnosticEmitter& emitter,
                                 const std::vector<types::TypeError>& errors) {
    bool has_root_cause = false;
    for (const auto& error : errors) {
        if (!error.is_cascading) {
            has_root_cause = true;
            break;
        }
    }

    std::set<std::tuple<std::string, uint32_t, uint32_t>> seen;
    size_t suppressed = 0;

    for (const auto& error : errors) {
        if (has_root_cause && error.is_cascading) {
            ++suppressed;
            continue;
        }
        auto key = std::make_tuple(error.code.empty() ? std::string("T001") : error.code,
                                   error.span.start.line, error.span.start.column);
        if (!seen.insert(key).second) {
            ++suppressed;
            continue;
        }
        auto code = error.code.empty() ? std::string("T001") : error.code;
        emitter.error(code, error.message, error.span, error.notes);
    }

    if (suppressed > 0) {
        std::cerr << "note: " << suppressed
                  << " additional error(s) suppressed (likely caused by previous error)\n";
    }
}

int run_lex(const std::string& path, bool verbose) {
    auto& diag = get_diagnostic_emitter();

    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        TML_LOG_ERROR("debug", e.what());
        return 1;
    }

    diag.set_source_content(path, source_code);

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (verbose) {
        TML_LOG_INFO("lexer", "Tokens (" << tokens.size() << "):");
        for (const auto& token : tokens) {
            std::ostringstream tok;
            tok << "  " << token.span.start.line << ":" << token.span.start.column << " "
                << lexer::token_kind_to_string(token.kind);
            if (token.kind == lexer::TokenKind::Identifier ||
                token.kind == lexer::TokenKind::IntLiteral ||
                token.kind == lexer::TokenKind::FloatLiteral ||
                token.kind == lexer::TokenKind::StringLiteral) {
                tok << " `" << token.lexeme << "`";
            }
            TML_LOG_INFO("lexer", tok.str());
        }
    }

    if (lex.has_errors()) {
        emit_all_lexer_errors(diag, lex);
        return 1;
    }

    if (!verbose) {
        TML_LOG_INFO("lexer", "Lexed " << tokens.size() << " tokens from " << path);
    }
    return 0;
}

int run_parse(const std::string& path, bool verbose) {
    auto& diag = get_diagnostic_emitter();

    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        TML_LOG_ERROR("debug", e.what());
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
    auto result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(result);
        emit_all_parser_errors(diag, errors);
        return 1;
    }

    const auto& module = std::get<parser::Module>(result);

    if (verbose) {
        TML_LOG_INFO("parser", "Module: " << module.name);
        TML_LOG_INFO("parser", "Declarations: " << module.decls.size());
        for (const auto& decl : module.decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();
                std::ostringstream sig;
                sig << "  func " << func.name << "(";
                for (size_t i = 0; i < func.params.size(); ++i) {
                    if (i > 0)
                        sig << ", ";
                    const auto& param = func.params[i];
                    if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
                        sig << param.pattern->as<parser::IdentPattern>().name;
                    } else {
                        sig << "_";
                    }
                }
                sig << ")";
                TML_LOG_INFO("parser", sig.str());
            } else if (decl->is<parser::StructDecl>()) {
                const auto& s = decl->as<parser::StructDecl>();
                TML_LOG_INFO("parser", "  type " << s.name << " { ... }");
            } else if (decl->is<parser::EnumDecl>()) {
                const auto& e = decl->as<parser::EnumDecl>();
                TML_LOG_INFO("parser", "  type " << e.name << " = ...");
            } else if (decl->is<parser::TraitDecl>()) {
                const auto& t = decl->as<parser::TraitDecl>();
                TML_LOG_INFO("parser", "  behavior " << t.name << " { ... }");
            } else if (decl->is<parser::ImplDecl>()) {
                TML_LOG_INFO("parser", "  impl ...");
            }
        }
    } else {
        TML_LOG_INFO("parser", "Parsed " << module.decls.size() << " declarations from " << path);
    }

    return 0;
}

int run_check(const std::string& path, bool verbose) {
    auto& diag = get_diagnostic_emitter();

    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        TML_LOG_ERROR("debug", e.what());
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

    // Initialize module registry for test module
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        emit_all_type_errors(diag, errors);
        return 1;
    }

    if (verbose) {
        const auto& env = std::get<types::TypeEnv>(check_result);
        TML_LOG_INFO("types", "Type check passed for " << path);
        TML_LOG_INFO("types", "Module: " << module.name);
        TML_LOG_INFO("types", "Declarations: " << module.decls.size());
        (void)env;
    } else {
        TML_LOG_INFO("types", "check: " << path << " ok");
    }

    return 0;
}

} // namespace tml::cli
