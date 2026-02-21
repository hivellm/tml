//! # Documentation Command Implementation
//!
//! This file implements the `tml doc` command for generating documentation.

#include "cmd_doc.hpp"

#include "cli/diagnostic.hpp"
#include "cli/utils.hpp"
#include "doc/extractor.hpp"
#include "doc/generators.hpp"
#include "lexer/lexer.hpp"
#include "log/log.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace tml::cli {

/// Emits parser errors using the diagnostic emitter.
static void emit_parser_errors(DiagnosticEmitter& emitter,
                               const std::vector<parser::ParseError>& errors) {
    for (const auto& error : errors) {
        Diagnostic diag;
        diag.severity = DiagnosticSeverity::Error;
        diag.code = "P001";
        diag.message = error.message;
        diag.primary_span = error.span;
        diag.notes = error.notes;
        emitter.emit(diag);
    }
}

int run_doc(const DocOptions& options) {
    if (options.input_files.empty() && !options.all_modules) {
        TML_LOG_ERROR("doc", "No input files specified. Usage: tml doc <file.tml> [options] or tml "
                             "doc --all [options]");
        return 1;
    }

    auto& diag = get_diagnostic_emitter();

    // Create output directory
    fs::create_directories(options.output_dir);

    // Extractor configuration
    doc::ExtractorConfig extract_config;
    extract_config.include_private = options.include_private;
    extract_config.include_internals = options.include_internals;
    extract_config.extract_examples = true;
    extract_config.resolve_links = true;

    doc::Extractor extractor(extract_config);

    // Collect all modules to document
    std::vector<std::pair<parser::Module, std::string>> modules;

    // Helper to check if a file should be documented
    auto is_documentable = [](const fs::path& path) -> bool {
        std::string filename = path.filename().string();
        // Skip test files and error test files
        if (filename.ends_with(".test.tml") || filename.ends_with(".error.tml")) {
            return false;
        }
        // Skip files in tests/ or examples/ directories
        std::string path_str = path.string();
        for (auto& c : path_str) {
            if (c == '\\')
                c = '/';
        }
        if (path_str.find("/tests/") != std::string::npos ||
            path_str.find("/examples/") != std::string::npos) {
            return false;
        }
        return path.extension() == ".tml";
    };

    // If --all, find all .tml source files in current directory and lib/
    std::vector<std::string> files = options.input_files;
    if (options.all_modules) {
        // Look for project files
        if (fs::exists("src")) {
            for (const auto& entry : fs::recursive_directory_iterator("src")) {
                if (is_documentable(entry.path())) {
                    files.push_back(entry.path().string());
                }
            }
        }
        if (fs::exists("lib")) {
            for (const auto& entry : fs::recursive_directory_iterator("lib")) {
                if (is_documentable(entry.path())) {
                    files.push_back(entry.path().string());
                }
            }
        }
        // Also check current directory
        for (const auto& entry : fs::directory_iterator(".")) {
            if (is_documentable(entry.path())) {
                files.push_back(entry.path().string());
            }
        }
    }

    if (files.empty()) {
        TML_LOG_ERROR("doc", "No .tml files found");
        return 1;
    }

    // Parse each file
    for (const auto& file : files) {
        TML_LOG_INFO("doc", "Processing: " << file);

        // Read file
        std::string source_code;
        try {
            source_code = read_file(file);
        } catch (const std::exception& e) {
            TML_LOG_ERROR("doc", "Could not read file '" << file << "': " << e.what());
            continue;
        }

        // Run preprocessor to handle #if/#ifdef/#define etc.
        auto pp_config = preprocessor::Preprocessor::host_config();
        preprocessor::Preprocessor pp(pp_config);
        auto pp_result = pp.process(source_code, file);

        if (!pp_result.success()) {
            for (const auto& pp_diag : pp_result.errors()) {
                TML_LOG_ERROR("doc", file << ":" << pp_diag.line << ":" << pp_diag.column << ": "
                                          << pp_diag.message);
            }
            continue;
        }

        // Use preprocessed source for lexing
        std::string preprocessed = pp_result.output;
        diag.set_source_content(file, preprocessed);

        // Lex
        auto source = lexer::Source::from_string(preprocessed, file);
        lexer::Lexer lex(source);
        auto tokens = lex.tokenize();

        if (lex.has_errors()) {
            for (const auto& err : lex.errors()) {
                diag.error("L001", err.message, err.span);
            }
            continue;
        }

        // Parse
        parser::Parser parser(std::move(tokens));
        auto module_name = fs::path(file).stem().string();
        auto parse_result = parser.parse_module(module_name);

        if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
            const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
            emit_parser_errors(diag, errors);
            continue;
        }

        auto& module = std::get<parser::Module>(parse_result);

        // Derive module path from file path
        std::string module_path = module_name;
        auto rel_path = fs::path(file).parent_path();
        if (!rel_path.empty() && rel_path != ".") {
            // Convert path separators to ::
            std::string path_str = rel_path.string();
            for (auto& c : path_str) {
                if (c == '/' || c == '\\') {
                    c = ':';
                }
            }
            // Remove consecutive colons and make proper path
            std::string clean_path;
            bool last_was_colon = false;
            for (char c : path_str) {
                if (c == ':') {
                    if (!last_was_colon && !clean_path.empty()) {
                        clean_path += "::";
                    }
                    last_was_colon = true;
                } else {
                    clean_path += c;
                    last_was_colon = false;
                }
            }
            if (!clean_path.empty()) {
                module_path = clean_path + "::" + module_name;
            }
        }

        modules.emplace_back(std::move(module), module_path);
    }

    if (modules.empty()) {
        TML_LOG_ERROR("doc", "No modules parsed successfully");
        return 1;
    }

    // Build module pointers for extraction
    std::vector<std::pair<const parser::Module*, std::string>> module_ptrs;
    for (const auto& [module, path] : modules) {
        module_ptrs.emplace_back(&module, path);
    }

    // Extract documentation
    auto doc_index = extractor.extract_all(module_ptrs);
    doc_index.crate_name = "TML Project";
    doc_index.version = "0.1.0";

    // Generate output based on format
    doc::GeneratorConfig gen_config;
    gen_config.title = doc_index.crate_name;
    gen_config.version = doc_index.version;
    gen_config.include_private = options.include_private;

    switch (options.format) {
    case DocFormat::Json: {
        doc::JsonGenerator generator(gen_config);
        fs::path output_file = fs::path(options.output_dir) / "docs.json";
        generator.generate_file(doc_index, output_file);
        TML_LOG_INFO("doc", "Generated: " << output_file);
        TML_LOG_INFO("doc", "Documentation written to " << output_file);
        break;
    }

    case DocFormat::Html: {
        doc::HtmlGenerator generator(gen_config);
        generator.generate_site(doc_index, options.output_dir);
        TML_LOG_INFO("doc", "Generated HTML documentation in " << options.output_dir);
        fs::path index_file = fs::path(options.output_dir) / "index.html";
        TML_LOG_INFO("doc", "Documentation written to " << index_file);

        // Open in browser if requested
        if (options.open_browser) {
#ifdef _WIN32
            std::string cmd = "start \"\" \"" + index_file.string() + "\"";
#elif __APPLE__
            std::string cmd = "open \"" + index_file.string() + "\"";
#else
            std::string cmd = "xdg-open \"" + index_file.string() + "\"";
#endif
            (void)system(cmd.c_str());
        }
        break;
    }

    case DocFormat::Markdown: {
        doc::MarkdownGenerator generator(gen_config);
        generator.generate_directory(doc_index, options.output_dir);
        TML_LOG_INFO("doc", "Generated Markdown documentation in " << options.output_dir);
        fs::path index_file = fs::path(options.output_dir) / "README.md";
        TML_LOG_INFO("doc", "Documentation written to " << index_file);
        break;
    }
    }

    // Summary
    size_t total_items = 0;
    for (const auto& module : doc_index.modules) {
        total_items += module.items.size();
    }
    TML_LOG_INFO("doc", "Documented " << doc_index.modules.size() << " modules, " << total_items
                                      << " items");

    return 0;
}

DocOptions parse_doc_args(int argc, char* argv[]) {
    DocOptions options;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_doc_help();
            exit(0);
        } else if (arg == "--verbose" || arg == "-v") {
            options.verbose = true;
        } else if (arg == "--all") {
            options.all_modules = true;
        } else if (arg == "--include-private") {
            options.include_private = true;
        } else if (arg == "--include-internals") {
            options.include_internals = true;
        } else if (arg == "--open") {
            options.open_browser = true;
        } else if (arg.starts_with("--format=")) {
            std::string format = arg.substr(9);
            if (format == "json") {
                options.format = DocFormat::Json;
            } else if (format == "html") {
                options.format = DocFormat::Html;
            } else if (format == "md" || format == "markdown") {
                options.format = DocFormat::Markdown;
            } else {
                TML_LOG_WARN("doc", "Unknown format '" << format << "', using html");
                options.format = DocFormat::Html;
            }
        } else if (arg.starts_with("--output=") || arg.starts_with("-o=")) {
            options.output_dir = arg.substr(arg.find('=') + 1);
        } else if (arg[0] != '-') {
            // Input file
            options.input_files.push_back(arg);
        } else {
            TML_LOG_WARN("doc", "Unknown option '" << arg << "'");
        }
    }

    return options;
}

void print_doc_help() {
    std::cerr << R"(
TML Documentation Generator

Usage: tml doc [file.tml...] [options]
       tml doc --all [options]

Options:
  --all               Document all .tml files in project
  --format=<fmt>      Output format: json, html, md (default: html)
  --output=<dir>      Output directory (default: docs)
  -o=<dir>            Alias for --output
  --include-private   Include private (non-pub) items
  --include-internals Include items marked @internal
  --open              Open documentation in browser after generation
  --verbose, -v       Show detailed output
  --help, -h          Show this help

Examples:
  tml doc main.tml                    # Document single file
  tml doc src/*.tml --format=json     # Output as JSON
  tml doc --all --open                # Document project and open in browser
  tml doc lib/core.tml -o=api-docs    # Custom output directory

Output Formats:
  html     - Interactive HTML website with search
  json     - Machine-readable JSON for tooling
  md       - Markdown files for wikis/READMEs

)";
}

} // namespace tml::cli
