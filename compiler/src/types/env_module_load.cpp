TML_MODULE("compiler")

//! # Type Environment - Module Support
//!
//! This file implements module loading and import resolution.
//!
//! ## Module Loading
//!
//! `load_module()` performs:
//! 1. Read source file from disk
//! 2. Lex and parse the module
//! 3. Register types and functions in module registry
//! 4. Process nested imports recursively
//!
//! ## Import Resolution
//!
//! | Import Syntax               | Resolution                    |
//! |-----------------------------|-------------------------------|
//! | `use std::io::print`        | Single symbol import          |
//! | `use std::io::{print, read}`| Multiple symbol import        |
//! | `use std::io::*`            | Glob import                   |
//! | `use std::io as io`         | Aliased import                |
//!
//! ## Path Resolution

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"
#include "types/env.hpp"
#include "types/module.hpp"
#include "types/module_binary.hpp"
#include "types/parsed_module_file.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <shared_mutex>

namespace tml::types {

static ParseResult parse_tml_file(const std::string& file_path) {
    ParseResult result;
    result.success = false;

    std::ifstream file(file_path);
    if (!file) {
        result.errors.push_back(parser::ParseError{
            "Failed to open file: " + file_path, SourceSpan{}, {}, {} // notes, fixes
        });
        return result;
    }

    result.source_code =
        std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Preprocess the source code (handles #if, #ifdef, etc.)
    auto pp_config = preprocessor::Preprocessor::host_config();
    preprocessor::Preprocessor pp(pp_config);
    auto pp_result = pp.process(result.source_code, file_path);

    // Check for preprocessor errors
    if (!pp_result.success()) {
        for (const auto& diag : pp_result.diagnostics) {
            if (diag.severity == preprocessor::DiagnosticSeverity::Error) {
                result.errors.push_back(parser::ParseError{
                    "Preprocessor error: " + diag.message, SourceSpan{}, {}, {}});
            }
        }
        return result;
    }

    // Use preprocessed source for lexing
    auto source = lexer::Source::from_string(pp_result.output, file_path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        result.lex_errors = lex.errors();
        return result;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = std::filesystem::path(file_path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        result.errors = std::get<std::vector<parser::ParseError>>(std::move(parse_result));
        return result;
    }

    // Store the preprocessed source (not raw) so codegen can re-lex it
    // without needing to run the preprocessor again.
    result.source_code = pp_result.output;

    auto parsed_module = std::get<parser::Module>(std::move(parse_result));
    result.decls = std::move(parsed_module.decls);
    result.success = true;
    return result;
}

bool TypeEnv::load_module_from_file(const std::string& module_path, const std::string& file_path) {
    if (!module_registry_) {
        return false;
    }

    // Check if module is already registered
    if (module_registry_->has_module(module_path)) {
        return true; // Already loaded
    }

    // Check for circular dependency - if we're already loading this module, skip
    if (loading_modules_.count(module_path) > 0) {
        TML_DEBUG_LN("[MODULE] Skipping circular dependency: " << module_path);
        return true; // Return true to allow compilation to proceed
    }

    // Mark module as being loaded
    loading_modules_.insert(module_path);

    // RAII guard to remove from loading set on any return path
    struct LoadingGuard {
        std::unordered_set<std::string>& set;
        const std::string& path;
        bool completed = false;
        LoadingGuard(std::unordered_set<std::string>& s, const std::string& p) : set(s), path(p) {}
        ~LoadingGuard() {
            if (!completed)
                set.erase(path);
        }
        void mark_completed() {
            completed = true;
        }
    } loading_guard(loading_modules_, module_path);

    // ParsedModuleFile defined at namespace scope (shared with env_module_load_decls.cpp)
    std::vector<ParsedModuleFile> all_parsed;
    bool had_errors = false;

    // Check if this is a mod.tml file - if so, load all sibling .tml files
    auto fs_path = std::filesystem::path(file_path);
    TML_DEBUG_LN("[MODULE] load_module_from_file: " << file_path << " (stem: " << fs_path.stem()
                                                    << ")");
    if (fs_path.stem() == "mod") {
        auto dir = fs_path.parent_path();
        TML_DEBUG_LN("[MODULE] Loading directory module from: " << dir);

        // Load all .tml files in the directory (including mod.tml itself)
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tml") {
                auto entry_path = entry.path().string();
                bool is_mod = (entry.path().stem() == "mod");
                TML_DEBUG_LN("[MODULE]   Parsing: " << entry.path().filename());
                auto parsed = parse_tml_file(entry_path);
                if (parsed.success) {
                    TML_DEBUG_LN("[MODULE]   OK: " << entry.path().filename());
                    all_parsed.push_back(ParsedModuleFile{std::move(parsed.decls),
                                                          std::move(parsed.source_code), is_mod});
                } else {
                    had_errors = true;
                    // Only log parse errors if in fatal mode (normal compilation).
                    // In non-fatal mode (meta preload), these are expected for some
                    // library files that use unsupported syntax and would spam the output.
                    if (abort_on_module_error_) {
                        TML_LOG_ERROR("types", "[D001] === MODULE PARSE ERROR ===");
                        TML_LOG_ERROR("types", "[D001] Failed to parse: " << entry_path);

                        for (const auto& err : parsed.lex_errors) {
                            TML_LOG_ERROR("types", entry_path << ":" << err.span.start.line << ":"
                                                              << err.span.start.column
                                                              << ": lexer error: " << err.message);
                        }

                        int error_count = 0;
                        for (const auto& err : parsed.errors) {
                            TML_LOG_ERROR("types", entry_path << ":" << err.span.start.line << ":"
                                                              << err.span.start.column
                                                              << ": error: " << err.message);
                            if (++error_count >= 5) {
                                if (parsed.errors.size() > 5) {
                                    TML_LOG_ERROR("types", "... and " << (parsed.errors.size() - 5)
                                                                      << " more errors");
                                }
                                break;
                            }
                        }
                        TML_LOG_ERROR("types", "=========================");
                    } else {
                        TML_DEBUG_LN("[MODULE] Parse error in " << entry_path << " ("
                                                                << parsed.errors.size()
                                                                << " errors, skipping)");
                    }
                }
            }
        }
    } else {
        // Single file module
        auto parsed = parse_tml_file(file_path);
        if (!parsed.success) {
            if (abort_on_module_error_) {
                TML_LOG_ERROR("types", "[D001] === MODULE PARSE ERROR ===");
                TML_LOG_ERROR("types", "[D001] Failed to parse: " << file_path);

                for (const auto& err : parsed.lex_errors) {
                    TML_LOG_ERROR("types", file_path << ":" << err.span.start.line << ":"
                                                     << err.span.start.column
                                                     << ": lexer error: " << err.message);
                }

                int error_count = 0;
                for (const auto& err : parsed.errors) {
                    TML_LOG_ERROR("types", file_path << ":" << err.span.start.line << ":"
                                                     << err.span.start.column
                                                     << ": error: " << err.message);
                    if (++error_count >= 5) {
                        if (parsed.errors.size() > 5) {
                            TML_LOG_ERROR("types", "... and " << (parsed.errors.size() - 5)
                                                              << " more errors");
                        }
                        break;
                    }
                }
                TML_LOG_ERROR("types", "=========================");

                // Log fatal but don't abort — let caller handle the error
                TML_LOG_ERROR("types", "[D001] Cannot continue - module '" << module_path
                                                                           << "' failed to parse");
            } else {
                TML_DEBUG_LN("[MODULE] Parse error in " << file_path << " (" << parsed.errors.size()
                                                        << " errors, skipping)");
            }
            return false;
        }
        all_parsed.push_back(ParsedModuleFile{std::move(parsed.decls),
                                              std::move(parsed.source_code),
                                              /*is_from_mod_file=*/true});
    }

    // If any file in a directory module failed to parse, abort (unless in non-fatal mode)
    if (had_errors) {
        if (abort_on_module_error_) {
            TML_LOG_ERROR("types", "[D001] Cannot continue - module '" << module_path
                                                                       << "' has parse errors");
            return false;
        }
        // In non-fatal mode, continue with successfully parsed files if any
        if (all_parsed.empty()) {
            return false;
        }
        TML_DEBUG_LN("[MODULE] Continuing with " << all_parsed.size()
                                                 << " successfully parsed files (despite errors)");
    }

    if (all_parsed.empty()) {
        if (abort_on_module_error_) {
            TML_LOG_ERROR("types", "[D001] Module '" << module_path
                                                     << "' is empty or all files failed to parse");
            return false;
        }
        return false;
    }
    TML_DEBUG_LN("[MODULE] Parsed " << all_parsed.size() << " files for module: " << module_path);

    // Create module structure and extract declarations
    Module mod;
    mod.name = module_path;

    extract_module_declarations(module_path, file_path, all_parsed, mod);

    // Capture re-export source paths and private imports before moving the module.
    // After std::move(mod), these vectors are no longer accessible.
    std::vector<std::string> re_export_sources;
    re_export_sources.reserve(mod.re_exports.size());
    for (const auto& re_export : mod.re_exports) {
        re_export_sources.push_back(re_export.source_path);
    }
    std::vector<std::string> private_import_sources = mod.private_imports;

    module_registry_->register_module(module_path, std::move(mod));

    // Mark as completed so guard doesn't remove it (it's already registered)
    loading_guard.mark_completed();
    loading_modules_.erase(module_path);

    // Load re-export source modules to ensure they're in the current registry
    for (const auto& source_path : re_export_sources) {
        load_native_module(source_path, /*silent=*/true);
    }

    // Load private import modules to ensure transitive dependencies are available.
    // Without this, functions from deep module paths (e.g., std::http::router::router)
    // that are imported by library modules (e.g., app.tml's "use std::http::router::router::{...}")
    // would not be registered in the module registry, causing the codegen to miss their
    // declare/define statements and produce incorrect LLVM IR (wrong return types).
    // The cache paths (GlobalModuleCache, binary meta) already do this — this brings
    // the file-loading path to parity.
    for (const auto& import_path : private_import_sources) {
        bool loaded = load_native_module(import_path, /*silent=*/true);
        if (!loaded) {
            // Strip last segment — it may be a symbol name, not a module
            // (e.g., "core::option::Maybe" where the module is "core::option")
            auto last_sep = import_path.rfind("::");
            if (last_sep != std::string::npos) {
                load_native_module(import_path.substr(0, last_sep), /*silent=*/true);
            }
        }
    }

    return true;
}

} // namespace tml::types
