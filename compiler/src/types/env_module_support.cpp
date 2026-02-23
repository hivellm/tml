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
//!
//! Module paths are resolved relative to:
//! - Current file directory
//! - Library search paths (lib/core, lib/std)

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"
#include "types/env.hpp"
#include "types/module.hpp"
#include "types/module_binary.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <shared_mutex>

namespace tml::types {

// Helper: Extract the TML type name from a parser::TypePtr (for constants)
static std::string get_tml_type_name(const parser::TypePtr& type) {
    if (!type)
        return "I64"; // Default fallback

    if (type->is<parser::NamedType>()) {
        const auto& named = type->as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            return named.path.segments.back();
        }
    } else if (type->is<parser::TupleType>()) {
        const auto& tuple = type->as<parser::TupleType>();
        if (tuple.elements.empty())
            return "()";
        std::string result = "(";
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += get_tml_type_name(tuple.elements[i]);
        }
        result += ")";
        return result;
    }
    return "I64"; // Default for unknown types
}

/// Try to extract a compile-time constant scalar value from an expression.
/// Format a double with full precision for LLVM IR inline constants.
static std::string format_float_const(double val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.20g", val);
    // Ensure the string contains a decimal point so LLVM parses it as float
    std::string s(buf);
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
        s.find('E') == std::string::npos && s.find("inf") == std::string::npos &&
        s.find("nan") == std::string::npos) {
        s += ".0";
    }
    return s;
}

static std::string try_extract_scalar_const_value(const parser::Expr* expr) {
    if (!expr)
        return "";
    if (expr->is<parser::CastExpr>()) {
        const auto& cast = expr->as<parser::CastExpr>();
        if (cast.expr && cast.expr->is<parser::LiteralExpr>()) {
            expr = cast.expr.get();
        } else if (cast.expr && cast.expr->is<parser::UnaryExpr>()) {
            const auto& unary = cast.expr->as<parser::UnaryExpr>();
            if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
                const auto& lit = unary.operand->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral)
                    return std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
                if (lit.token.kind == lexer::TokenKind::FloatLiteral)
                    return format_float_const(-lit.token.float_value().value);
            }
            return "";
        } else {
            return "";
        }
    }
    if (expr->is<parser::UnaryExpr>()) {
        const auto& unary = expr->as<parser::UnaryExpr>();
        if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
            const auto& lit = unary.operand->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::IntLiteral)
                return std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
            if (lit.token.kind == lexer::TokenKind::FloatLiteral)
                return format_float_const(-lit.token.float_value().value);
        }
        if (unary.operand && unary.operand->is<parser::CastExpr>()) {
            const auto& cast = unary.operand->as<parser::CastExpr>();
            if (cast.expr && cast.expr->is<parser::LiteralExpr>()) {
                const auto& lit = cast.expr->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral)
                    return std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
                if (lit.token.kind == lexer::TokenKind::FloatLiteral)
                    return format_float_const(-lit.token.float_value().value);
            }
        }
        return "";
    }
    if (expr->is<parser::LiteralExpr>()) {
        const auto& lit = expr->as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::IntLiteral)
            return std::to_string(lit.token.int_value().value);
        if (lit.token.kind == lexer::TokenKind::FloatLiteral)
            return format_float_const(lit.token.float_value().value);
        if (lit.token.kind == lexer::TokenKind::BoolLiteral)
            return lit.token.bool_value() ? "1" : "0";
        if (lit.token.kind == lexer::TokenKind::NullLiteral)
            return "null";
        if (lit.token.kind == lexer::TokenKind::CharLiteral)
            return std::to_string(static_cast<uint32_t>(lit.token.char_value().value));
    }
    return "";
}

/// Try to extract a compile-time constant value (scalar or tuple) from a ConstDecl.
/// Returns the value string and sets tml_type. For tuples, value is the LLVM aggregate
/// literal (e.g., "{ i8 15, i8 1, i8 0 }") which is what the codegen expects.
static std::string try_extract_module_const_value(const parser::ConstDecl& const_decl,
                                                  std::string& tml_type) {
    tml_type = get_tml_type_name(const_decl.type);

    if (!const_decl.value)
        return "";

    // Handle tuple expressions: (15, 1, 0)
    if (const_decl.value->is<parser::TupleExpr>()) {
        const auto& tuple = const_decl.value->as<parser::TupleExpr>();
        if (tuple.elements.empty())
            return "zeroinitializer";

        // Get element LLVM types from the declared type
        std::vector<std::string> elem_llvm_types;
        if (const_decl.type && const_decl.type->is<parser::TupleType>()) {
            const auto& tuple_type = const_decl.type->as<parser::TupleType>();
            for (const auto& et : tuple_type.elements) {
                std::string tml_elem = get_tml_type_name(et);
                // Map TML type name to LLVM type for the value representation
                if (tml_elem == "U8" || tml_elem == "I8")
                    elem_llvm_types.push_back("i8");
                else if (tml_elem == "U16" || tml_elem == "I16")
                    elem_llvm_types.push_back("i16");
                else if (tml_elem == "U32" || tml_elem == "I32")
                    elem_llvm_types.push_back("i32");
                else if (tml_elem == "U64" || tml_elem == "I64")
                    elem_llvm_types.push_back("i64");
                else if (tml_elem == "Bool")
                    elem_llvm_types.push_back("i1");
                else
                    elem_llvm_types.push_back("i64");
            }
        }

        std::vector<std::string> elem_values;
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            std::string val = try_extract_scalar_const_value(tuple.elements[i].get());
            if (val.empty())
                return "";
            elem_values.push_back(val);
        }

        if (elem_llvm_types.size() != elem_values.size()) {
            elem_llvm_types.clear();
            for (size_t i = 0; i < elem_values.size(); ++i)
                elem_llvm_types.push_back("i64");
        }

        // Build LLVM aggregate value: { i8 15, i8 1, i8 0 }
        std::string value = "{ ";
        for (size_t i = 0; i < elem_values.size(); ++i) {
            if (i > 0)
                value += ", ";
            value += elem_llvm_types[i] + " " + elem_values[i];
        }
        value += " }";
        return value;
    }

    // Handle scalar expressions
    return try_extract_scalar_const_value(const_decl.value.get());
}

void TypeEnv::set_module_registry(std::shared_ptr<ModuleRegistry> registry) {
    module_registry_ = std::move(registry);
    // Modules will be loaded lazily when imported via 'use'
    // No hardcoded initialization here
}

void TypeEnv::set_current_module(const std::string& module_path) {
    current_module_path_ = module_path;
}

void TypeEnv::set_source_directory(const std::string& dir_path) {
    source_directory_ = dir_path;
}

auto TypeEnv::source_directory() const -> const std::string& {
    return source_directory_;
}

auto TypeEnv::module_registry() const -> std::shared_ptr<ModuleRegistry> {
    return module_registry_;
}

auto TypeEnv::current_module() const -> const std::string& {
    return current_module_path_;
}

void TypeEnv::import_symbol(const std::string& module_path, const std::string& symbol_name,
                            std::optional<std::string> alias) {
    // Determine the local name (use alias if provided, otherwise original name)
    std::string local_name = alias.value_or(symbol_name);

    // Check for name conflicts - if a symbol with this name is already imported from a
    // different module, track the conflict. The user can resolve by using an alias.
    auto existing = imported_symbols_.find(local_name);
    if (existing != imported_symbols_.end()) {
        // Same symbol from same module is fine (duplicate import)
        if (existing->second.module_path == module_path &&
            existing->second.original_name == symbol_name) {
            return; // Already imported - no-op
        }

        // Conflict: same local name from different source
        // Store in conflict set for later error reporting during resolution
        import_conflicts_[local_name].insert(existing->second.module_path +
                                             "::" + existing->second.original_name);
        import_conflicts_[local_name].insert(module_path + "::" + symbol_name);
        TML_DEBUG_LN("[MODULE] Import conflict detected for '"
                     << local_name << "': " << existing->second.module_path
                     << "::" << existing->second.original_name << " vs " << module_path
                     << "::" << symbol_name);
    }

    // Create the imported symbol entry
    ImportedSymbol import{
        .original_name = symbol_name,
        .local_name = local_name,
        .module_path = module_path,
        .visibility = parser::Visibility::Public // Imported symbols are accessible
    };

    // Store the import (last one wins for now - user should use alias to resolve)
    imported_symbols_[local_name] = import;
}

void TypeEnv::import_all_from(const std::string& module_path) {
    if (!module_registry_) {
        return; // No module registry available
    }

    auto module = module_registry_->get_module(module_path);
    if (!module) {
        return; // Module not found
    }

    // Import all functions (skip qualified method names like Type::method - those are
    // resolved through their type import)
    for (const auto& [name, func_sig] : module->functions) {
        // Only import free functions, not impl methods (which contain ::)
        if (name.find("::") == std::string::npos) {
            import_symbol(module_path, name, std::nullopt);
        }
    }

    // Import all structs
    for (const auto& [name, struct_def] : module->structs) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all enums
    for (const auto& [name, enum_def] : module->enums) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all behaviors
    for (const auto& [name, behavior_def] : module->behaviors) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all classes (OOP)
    for (const auto& [name, class_def] : module->classes) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all interfaces (OOP)
    for (const auto& [name, interface_def] : module->interfaces) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all type aliases
    for (const auto& [name, type_ptr] : module->type_aliases) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all constants
    for (const auto& [name, value] : module->constants) {
        // Only import non-qualified constants (not Type::CONST)
        if (name.find("::") == std::string::npos) {
            import_symbol(module_path, name, std::nullopt);
        }
    }

    // Process re-exports (pub use declarations)
    for (const auto& re_export : module->re_exports) {
        // First, load the source module if not already loaded
        load_native_module(re_export.source_path);

        auto source_module = module_registry_->get_module(re_export.source_path);
        if (!source_module) {
            TML_DEBUG_LN(
                "[MODULE] Warning: Re-export source module not found: " << re_export.source_path);
            continue;
        }

        if (re_export.is_glob) {
            // Glob re-export: import all symbols from source module
            for (const auto& [name, func_sig] : source_module->functions) {
                if (name.find("::") == std::string::npos) {
                    import_symbol(re_export.source_path, name, std::nullopt);
                }
            }
            for (const auto& [name, struct_def] : source_module->structs) {
                import_symbol(re_export.source_path, name, std::nullopt);
            }
            for (const auto& [name, enum_def] : source_module->enums) {
                import_symbol(re_export.source_path, name, std::nullopt);
            }
            for (const auto& [name, behavior_def] : source_module->behaviors) {
                import_symbol(re_export.source_path, name, std::nullopt);
            }
            for (const auto& [name, class_def] : source_module->classes) {
                import_symbol(re_export.source_path, name, std::nullopt);
            }
            for (const auto& [name, interface_def] : source_module->interfaces) {
                import_symbol(re_export.source_path, name, std::nullopt);
            }
            for (const auto& [name, type_ptr] : source_module->type_aliases) {
                import_symbol(re_export.source_path, name, std::nullopt);
            }
            // Import constants from glob re-exports
            for (const auto& [name, const_val] : source_module->constants) {
                if (name.find("::") == std::string::npos) {
                    import_symbol(re_export.source_path, name, std::nullopt);
                }
            }

            // Recursively process re-exports from the source module
            for (const auto& nested_re_export : source_module->re_exports) {
                load_native_module(nested_re_export.source_path);
                auto nested_module = module_registry_->get_module(nested_re_export.source_path);
                if (nested_module && nested_re_export.is_glob) {
                    for (const auto& [name, func_sig] : nested_module->functions) {
                        if (name.find("::") == std::string::npos) {
                            import_symbol(nested_re_export.source_path, name, std::nullopt);
                        }
                    }
                    for (const auto& [name, struct_def] : nested_module->structs) {
                        import_symbol(nested_re_export.source_path, name, std::nullopt);
                    }
                    for (const auto& [name, enum_def] : nested_module->enums) {
                        import_symbol(nested_re_export.source_path, name, std::nullopt);
                    }
                    for (const auto& [name, behavior_def] : nested_module->behaviors) {
                        import_symbol(nested_re_export.source_path, name, std::nullopt);
                    }
                    for (const auto& [name, class_def] : nested_module->classes) {
                        import_symbol(nested_re_export.source_path, name, std::nullopt);
                    }
                    for (const auto& [name, interface_def] : nested_module->interfaces) {
                        import_symbol(nested_re_export.source_path, name, std::nullopt);
                    }
                    for (const auto& [name, type_ptr] : nested_module->type_aliases) {
                        import_symbol(nested_re_export.source_path, name, std::nullopt);
                    }
                    // Import constants from nested glob re-exports
                    for (const auto& [name, const_val] : nested_module->constants) {
                        if (name.find("::") == std::string::npos) {
                            import_symbol(nested_re_export.source_path, name, std::nullopt);
                        }
                    }
                }
            }
        } else if (!re_export.symbols.empty()) {
            // Specific symbols re-export
            for (const auto& symbol : re_export.symbols) {
                import_symbol(re_export.source_path, symbol, std::nullopt);
            }
        }
    }
}

auto TypeEnv::resolve_imported_symbol(const std::string& name) const -> std::optional<std::string> {
    auto it = imported_symbols_.find(name);
    if (it != imported_symbols_.end()) {
        // Return the full qualified name: module_path::original_name
        return it->second.module_path + "::" + it->second.original_name;
    }
    return std::nullopt;
}

auto TypeEnv::all_imports() const -> const std::unordered_map<std::string, ImportedSymbol>& {
    return imported_symbols_;
}

auto TypeEnv::has_import_conflict(const std::string& name) const -> bool {
    return import_conflicts_.find(name) != import_conflicts_.end();
}

auto TypeEnv::get_import_conflict_sources(const std::string& name) const
    -> std::optional<std::set<std::string>> {
    auto it = import_conflicts_.find(name);
    if (it != import_conflicts_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// Result type for parse_tml_file that includes error information
struct ParseResult {
    bool success;
    std::vector<parser::DeclPtr> decls;
    std::string source_code;
    std::vector<parser::ParseError> errors;
    std::vector<lexer::LexerError> lex_errors;
};

// Helper to parse a single TML file and extract public functions
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

    // Collect all declarations to process
    std::vector<std::pair<std::vector<parser::DeclPtr>, std::string>> all_parsed;
    bool had_errors = false;

    // Check if this is a mod.tml file - if so, load all sibling .tml files
    auto fs_path = std::filesystem::path(file_path);
    TML_DEBUG_LN("[MODULE] load_module_from_file: " << file_path << " (stem: " << fs_path.stem()
                                                    << ")");
    if (fs_path.stem() == "mod") {
        auto dir = fs_path.parent_path();
        TML_DEBUG_LN("[MODULE] Loading directory module from: " << dir);

        // Load all .tml files in the directory (except mod.tml itself)
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tml") {
                auto entry_path = entry.path().string();
                TML_DEBUG_LN("[MODULE]   Parsing: " << entry.path().filename());
                auto parsed = parse_tml_file(entry_path);
                if (parsed.success) {
                    TML_DEBUG_LN("[MODULE]   OK: " << entry.path().filename());
                    all_parsed.push_back(
                        std::make_pair(std::move(parsed.decls), std::move(parsed.source_code)));
                } else {
                    had_errors = true;
                    // Only log parse errors if in fatal mode (normal compilation).
                    // In non-fatal mode (meta preload), these are expected for some
                    // library files that use unsupported syntax and would spam the output.
                    if (abort_on_module_error_) {
                        TML_LOG_ERROR("types", "=== MODULE PARSE ERROR ===");
                        TML_LOG_ERROR("types", "Failed to parse: " << entry_path);

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
                TML_LOG_ERROR("types", "=== MODULE PARSE ERROR ===");
                TML_LOG_ERROR("types", "Failed to parse: " << file_path);

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

                // Panic - abort compilation
                TML_LOG_FATAL("types",
                              "Cannot continue - module '" << module_path << "' failed to parse");
                std::exit(1);
            } else {
                TML_DEBUG_LN("[MODULE] Parse error in " << file_path << " (" << parsed.errors.size()
                                                        << " errors, skipping)");
            }
            return false;
        }
        all_parsed.push_back(
            std::make_pair(std::move(parsed.decls), std::move(parsed.source_code)));
    }

    // If any file in a directory module failed to parse, abort (unless in non-fatal mode)
    if (had_errors) {
        if (abort_on_module_error_) {
            TML_LOG_FATAL("types",
                          "Cannot continue - module '" << module_path << "' has parse errors");
            std::exit(1);
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
            TML_LOG_FATAL("types",
                          "Module '" << module_path << "' is empty or all files failed to parse");
            std::exit(1);
        }
        return false;
    }
    TML_DEBUG_LN("[MODULE] Parsed " << all_parsed.size() << " files for module: " << module_path);

    // Create module structure and extract declarations
    Module mod;
    mod.name = module_path;

    SourceSpan builtin_span{};

    // Helper function to resolve simple types (primitive types for now)
    std::function<types::TypePtr(const parser::Type&)> resolve_simple_type;
    resolve_simple_type = [&resolve_simple_type](const parser::Type& type) -> types::TypePtr {
        if (type.is<parser::NamedType>()) {
            const auto& named = type.as<parser::NamedType>();
            // Build full path name (e.g., "I::Item" for associated types)
            std::string name;
            for (size_t i = 0; i < named.path.segments.size(); ++i) {
                if (i > 0)
                    name += "::";
                name += named.path.segments[i];
            }

            // Primitive types
            if (name == "I8")
                return make_primitive(PrimitiveKind::I8);
            if (name == "I16")
                return make_primitive(PrimitiveKind::I16);
            if (name == "I32")
                return make_primitive(PrimitiveKind::I32);
            if (name == "I64")
                return make_primitive(PrimitiveKind::I64);
            if (name == "I128")
                return make_primitive(PrimitiveKind::I128);
            if (name == "U8")
                return make_primitive(PrimitiveKind::U8);
            if (name == "U16")
                return make_primitive(PrimitiveKind::U16);
            if (name == "U32")
                return make_primitive(PrimitiveKind::U32);
            if (name == "U64")
                return make_primitive(PrimitiveKind::U64);
            if (name == "U128")
                return make_primitive(PrimitiveKind::U128);
            if (name == "F32")
                return make_primitive(PrimitiveKind::F32);
            if (name == "F64")
                return make_primitive(PrimitiveKind::F64);
            if (name == "Bool")
                return make_primitive(PrimitiveKind::Bool);
            if (name == "Char")
                return make_primitive(PrimitiveKind::Char);
            if (name == "Str")
                return make_primitive(PrimitiveKind::Str);
            if (name == "Unit")
                return make_unit();
            // Platform-sized types (map to 64-bit on 64-bit platforms)
            if (name == "Usize")
                return make_primitive(PrimitiveKind::U64); // Platform-sized unsigned
            if (name == "Isize")
                return make_primitive(PrimitiveKind::I64); // Platform-sized signed

            // std::file types
            if (name == "File")
                return std::make_shared<Type>(Type{NamedType{"File", "std::file", {}}});
            if (name == "Path")
                return std::make_shared<Type>(Type{NamedType{"Path", "std::file", {}}});

            // Other non-primitive types - resolve any generic type arguments
            std::vector<TypePtr> type_args;
            if (named.generics.has_value()) {
                for (const auto& arg : named.generics->args) {
                    // Only handle type arguments for now (not const generics)
                    if (arg.is_type()) {
                        type_args.push_back(resolve_simple_type(*arg.as_type()));
                    }
                }
            }
            return std::make_shared<Type>(Type{NamedType{name, "", std::move(type_args)}});
        } else if (type.is<parser::RefType>()) {
            const auto& ref = type.as<parser::RefType>();
            auto inner = resolve_simple_type(*ref.inner);
            return std::make_shared<Type>(
                Type{RefType{.is_mut = ref.is_mut, .inner = inner, .lifetime = ref.lifetime}});
        } else if (type.is<parser::FuncType>()) {
            const auto& func_type = type.as<parser::FuncType>();
            std::vector<TypePtr> param_types;
            for (const auto& param : func_type.params) {
                param_types.push_back(resolve_simple_type(*param));
            }
            TypePtr return_type = make_unit();
            if (func_type.return_type) {
                return_type = resolve_simple_type(*func_type.return_type);
            }
            return std::make_shared<Type>(
                Type{FuncType{std::move(param_types), return_type, false}});
        } else if (type.is<parser::TupleType>()) {
            const auto& tuple_type = type.as<parser::TupleType>();
            std::vector<TypePtr> element_types;
            for (const auto& elem : tuple_type.elements) {
                element_types.push_back(resolve_simple_type(*elem));
            }
            return make_tuple(std::move(element_types));
        } else if (type.is<parser::PtrType>()) {
            const auto& ptr = type.as<parser::PtrType>();
            auto inner = resolve_simple_type(*ptr.inner);
            return std::make_shared<Type>(Type{PtrType{ptr.is_mut, inner}});
        } else if (type.is<parser::ArrayType>()) {
            const auto& arr = type.as<parser::ArrayType>();
            auto element = resolve_simple_type(*arr.element);
            // Extract array size from literal expression
            size_t arr_size = 0;
            if (arr.size && arr.size->is<parser::LiteralExpr>()) {
                const auto& lit = arr.size->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    arr_size = static_cast<size_t>(lit.token.int_value().value);
                }
            }
            return make_array(element, arr_size);
        } else if (type.is<parser::SliceType>()) {
            const auto& slice = type.as<parser::SliceType>();
            auto element = resolve_simple_type(*slice.element);
            return make_slice(element);
        } else if (type.is<parser::DynType>()) {
            // Handle dyn behavior types (e.g., dyn Error)
            const auto& dyn = type.as<parser::DynType>();
            std::string behavior_name;
            if (!dyn.behavior.segments.empty()) {
                behavior_name = dyn.behavior.segments.back();
            }
            // Resolve type args if any
            std::vector<TypePtr> type_args;
            if (dyn.generics.has_value()) {
                for (const auto& arg : dyn.generics->args) {
                    if (arg.is_type()) {
                        type_args.push_back(resolve_simple_type(*arg.as_type()));
                    }
                }
            }
            return std::make_shared<Type>(
                Type{DynBehaviorType{behavior_name, std::move(type_args), dyn.is_mut}});
        }

        // Fallback: return I32 for unknown types
        TML_DEBUG_LN("[MODULE] Warning: Could not resolve type, using I32 as fallback");
        return make_primitive(PrimitiveKind::I32);
    };

    // Extract public declarations from all parsed files
    for (const auto& [decls, source_code] : all_parsed) {
        for (const auto& decl : decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();

                // Include public functions and @extern functions (even non-public).
                // @extern functions need to be registered so codegen can emit
                // proper 'declare' statements and use correct return types.
                if (func.vis != parser::Visibility::Public && !func.extern_abi.has_value()) {
                    continue;
                }

                // Convert parameter types
                std::vector<types::TypePtr> param_types;
                for (const auto& param : func.params) {
                    if (param.type) {
                        param_types.push_back(resolve_simple_type(*param.type));
                    }
                }

                // Convert return type
                types::TypePtr return_type;
                if (func.return_type.has_value()) {
                    const auto& type_ptr = func.return_type.value();
                    return_type = resolve_simple_type(*type_ptr);
                } else {
                    return_type = make_unit();
                }

                // Extract type parameters from generic declaration
                std::vector<std::string> type_params;
                for (const auto& gp : func.generics) {
                    type_params.push_back(gp.name);
                }

                // Create function signature
                FuncSig sig{.name = func.name,
                            .params = param_types,
                            .return_type = return_type,
                            .type_params = std::move(type_params),
                            .is_async = false,
                            .span = builtin_span,
                            .stability = StabilityLevel::Stable,
                            .deprecated_message = "",
                            .since_version = "1.0",
                            .where_constraints = {},
                            .is_lowlevel = func.is_unsafe};

                // Preserve @extern information for codegen
                if (func.extern_abi.has_value()) {
                    sig.extern_abi = func.extern_abi;
                    sig.extern_name = func.extern_name;
                }

                mod.functions[func.name] = sig;
            } else if (decl->is<parser::StructDecl>()) {
                const auto& struct_decl = decl->as<parser::StructDecl>();

                // Convert fields
                std::vector<StructFieldDef> fields;
                for (const auto& field : struct_decl.fields) {
                    if (field.type) {
                        StructFieldDef fdef;
                        fdef.name = field.name;
                        fdef.type = resolve_simple_type(*field.type);
                        fields.push_back(std::move(fdef));
                    }
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : struct_decl.generics) {
                    type_params.push_back(param.name);
                }

                // Create struct definition
                StructDef struct_def{.name = struct_decl.name,
                                     .type_params = std::move(type_params),
                                     .const_params = {},
                                     .fields = std::move(fields),
                                     .span = struct_decl.span};

                // Store in appropriate map based on visibility
                if (struct_decl.vis == parser::Visibility::Public) {
                    mod.structs[struct_decl.name] = std::move(struct_def);
                    TML_DEBUG_LN("[MODULE] Registered struct: " << struct_decl.name << " in module "
                                                                << module_path);
                } else {
                    // Store internal structs for use by the module's own impl methods
                    mod.internal_structs[struct_decl.name] = std::move(struct_def);
                    TML_DEBUG_LN("[MODULE] Registered internal struct: "
                                 << struct_decl.name << " in module " << module_path);
                }
            } else if (decl->is<parser::EnumDecl>()) {
                const auto& enum_decl = decl->as<parser::EnumDecl>();

                // Only include public enums
                if (enum_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Convert variants
                std::vector<std::pair<std::string, std::vector<TypePtr>>> variants;
                for (const auto& variant : enum_decl.variants) {
                    std::vector<TypePtr> payload_types;
                    // Handle tuple fields (e.g., Some(T))
                    if (variant.tuple_fields.has_value()) {
                        for (const auto& type_ptr : variant.tuple_fields.value()) {
                            payload_types.push_back(resolve_simple_type(*type_ptr));
                        }
                    }
                    // Handle struct fields (e.g., Point { x: I32, y: I32 })
                    if (variant.struct_fields.has_value()) {
                        for (const auto& field : variant.struct_fields.value()) {
                            if (field.type) {
                                payload_types.push_back(resolve_simple_type(*field.type));
                            }
                        }
                    }
                    variants.emplace_back(variant.name, std::move(payload_types));
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : enum_decl.generics) {
                    type_params.push_back(param.name);
                }

                // Create enum definition
                EnumDef enum_def{.name = enum_decl.name,
                                 .type_params = std::move(type_params),
                                 .const_params = {},
                                 .variants = std::move(variants),
                                 .span = enum_decl.span};

                mod.enums[enum_decl.name] = std::move(enum_def);
                TML_DEBUG_LN("[MODULE] Registered enum: " << enum_decl.name << " in module "
                                                          << module_path);
            } else if (decl->is<parser::ImplDecl>()) {
                const auto& impl_decl = decl->as<parser::ImplDecl>();

                // Get the type name being implemented from self_type
                std::string type_name;
                if (impl_decl.self_type && impl_decl.self_type->is<parser::NamedType>()) {
                    const auto& named = impl_decl.self_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        type_name = named.path.segments.back();
                    }
                }

                if (type_name.empty()) {
                    continue; // Skip if we couldn't determine the type name
                }

                // Register behavior implementation (e.g., impl[T] Drop for MutexGuard[T])
                // This is critical for is_trivially_destructible() to detect Drop impls
                // on imported library types, so that drop calls are emitted at scope exit.
                if (impl_decl.trait_type && impl_decl.trait_type->is<parser::NamedType>()) {
                    const auto& trait_named = impl_decl.trait_type->as<parser::NamedType>();
                    if (!trait_named.path.segments.empty()) {
                        std::string behavior_name = trait_named.path.segments.back();
                        register_impl(type_name, behavior_name);
                        mod.behavior_impls[type_name].push_back(behavior_name);
                        TML_DEBUG_LN("[MODULE] Registered behavior impl: "
                                     << type_name << " implements " << behavior_name
                                     << " in module " << module_path);
                    }
                }

                // Check if this is a behavior impl (has trait_type)
                bool is_behavior_impl = impl_decl.trait_type != nullptr;

                // Extract methods from impl block (methods is std::vector<FuncDecl>)
                for (const auto& func : impl_decl.methods) {
                    // Include ALL methods (public and private) from generic impl blocks.
                    // Private methods must be registered so they can be found during
                    // generic method instantiation (e.g., sort() calling quicksort()).
                    // For non-generic impls, still skip non-public methods.
                    bool is_generic_impl = !impl_decl.generics.empty();
                    if (func.vis != parser::Visibility::Public && !is_behavior_impl &&
                        !is_generic_impl) {
                        continue;
                    }

                    // Convert parameter types
                    std::vector<types::TypePtr> param_types;
                    for (const auto& param : func.params) {
                        if (param.type) {
                            param_types.push_back(resolve_simple_type(*param.type));
                        }
                    }

                    // Convert return type
                    types::TypePtr return_type;
                    if (func.return_type.has_value()) {
                        return_type = resolve_simple_type(*func.return_type.value());
                    } else {
                        return_type = make_unit();
                    }

                    // Create qualified function name: Type::method
                    std::string qualified_name = type_name + "::" + func.name;

                    // Extract type parameters from impl block and method's generic declaration
                    // Impl block generics (e.g., T in impl[T] Cell[T]) are needed for methods
                    // that use the type parameter without declaring their own generics
                    std::vector<std::string> method_type_params;
                    for (const auto& gp : impl_decl.generics) {
                        method_type_params.push_back(gp.name);
                    }
                    // Then add method's own type params (if any)
                    for (const auto& gp : func.generics) {
                        method_type_params.push_back(gp.name);
                    }

                    FuncSig sig{.name = qualified_name,
                                .params = param_types,
                                .return_type = return_type,
                                .type_params = std::move(method_type_params),
                                .is_async = false,
                                .span = builtin_span,
                                .stability = StabilityLevel::Stable,
                                .deprecated_message = "",
                                .since_version = "1.0",
                                .where_constraints = {},
                                .is_lowlevel = func.is_unsafe};

                    mod.functions[qualified_name] = sig;
                    TML_DEBUG_LN("[MODULE] Registered impl method: "
                                 << qualified_name << " in module " << module_path);
                }

                // Extract constants from impl block (e.g., const MIN: I32 = ...)
                for (const auto& const_decl : impl_decl.constants) {
                    // Only include public constants
                    if (const_decl.vis != parser::Visibility::Public) {
                        continue;
                    }

                    // Create qualified constant name: Type::CONST
                    std::string qualified_name = type_name + "::" + const_decl.name;
                    std::string tml_type;
                    std::string value = try_extract_module_const_value(const_decl, tml_type);
                    if (!value.empty()) {
                        mod.constants[qualified_name] = {value, tml_type};
                        TML_DEBUG_LN("[MODULE] Registered impl constant: "
                                     << qualified_name << " = " << value << " in module "
                                     << module_path);
                    }
                }
            } else if (decl->is<parser::ConstDecl>()) {
                // Handle module-level constants (not inside impl blocks)
                const auto& const_decl = decl->as<parser::ConstDecl>();
                std::string tml_type;
                std::string value = try_extract_module_const_value(const_decl, tml_type);
                if (!value.empty()) {
                    mod.constants[const_decl.name] = {value, tml_type};
                    TML_DEBUG_LN("[MODULE] Registered module constant: " << const_decl.name << " = "
                                                                         << value << " in module "
                                                                         << module_path);
                }
            } else if (decl->is<parser::InterfaceDecl>()) {
                // Handle interface declarations (OOP interfaces)
                const auto& iface_decl = decl->as<parser::InterfaceDecl>();

                // Only include public interfaces
                if (iface_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : iface_decl.generics) {
                    if (!param.is_const) {
                        type_params.push_back(param.name);
                    }
                }

                // Extract extended interfaces
                std::vector<std::string> extends;
                for (const auto& ext : iface_decl.extends) {
                    if (!ext.segments.empty()) {
                        extends.push_back(ext.segments.back());
                    }
                }

                // Build method signatures
                std::vector<InterfaceMethodDef> methods;
                for (const auto& method : iface_decl.methods) {
                    InterfaceMethodDef method_def;
                    method_def.is_static = method.is_static;
                    method_def.has_default = method.default_body.has_value();

                    // Build signature
                    FuncSig sig;
                    sig.name = method.name;
                    sig.is_async = false;
                    sig.span = method.span;

                    // Convert param types (simplified - skip 'this')
                    for (const auto& param : method.params) {
                        if (param.pattern && param.pattern->is<parser::IdentPattern>() &&
                            param.pattern->as<parser::IdentPattern>().name == "this") {
                            continue;
                        }
                        if (param.type) {
                            sig.params.push_back(resolve_simple_type(*param.type));
                        }
                    }

                    // Return type
                    if (method.return_type.has_value()) {
                        sig.return_type = resolve_simple_type(*method.return_type.value());
                    } else {
                        sig.return_type = make_unit();
                    }

                    method_def.sig = sig;
                    methods.push_back(method_def);
                }

                // Create interface definition
                InterfaceDef iface_def;
                iface_def.name = iface_decl.name;
                iface_def.type_params = std::move(type_params);
                iface_def.extends = std::move(extends);
                iface_def.methods = std::move(methods);
                iface_def.span = iface_decl.span;

                mod.interfaces[iface_decl.name] = std::move(iface_def);
                TML_DEBUG_LN("[MODULE] Registered interface: " << iface_decl.name << " in module "
                                                               << module_path);
            } else if (decl->is<parser::ClassDecl>()) {
                // Handle class declarations (OOP classes)
                const auto& class_decl = decl->as<parser::ClassDecl>();

                // Only include public classes
                if (class_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : class_decl.generics) {
                    if (!param.is_const) {
                        type_params.push_back(param.name);
                    }
                }

                // Extract base class
                std::optional<std::string> base_class;
                if (class_decl.extends.has_value()) {
                    const auto& ext = class_decl.extends.value();
                    if (!ext.segments.empty()) {
                        base_class = ext.segments.back();
                    }
                }

                // Extract implemented interfaces
                std::vector<std::string> interfaces;
                for (const auto& iface_type : class_decl.implements) {
                    if (auto* named = std::get_if<parser::NamedType>(&iface_type->kind)) {
                        if (!named->path.segments.empty()) {
                            interfaces.push_back(named->path.segments.back());
                        }
                    }
                }

                // Extract fields
                std::vector<ClassFieldDef> fields;
                for (const auto& field : class_decl.fields) {
                    ClassFieldDef field_def;
                    field_def.name = field.name;
                    field_def.is_static = field.is_static;
                    if (field.type) {
                        field_def.type = resolve_simple_type(*field.type);
                    }
                    fields.push_back(field_def);
                }

                // Extract methods
                std::vector<ClassMethodDef> methods;
                for (const auto& method : class_decl.methods) {
                    ClassMethodDef method_def;
                    method_def.is_static = method.is_static;
                    method_def.is_virtual = method.is_virtual;
                    method_def.is_override = method.is_override;
                    method_def.is_abstract = method.is_abstract;
                    method_def.is_final = method.is_final;
                    method_def.vis = MemberVisibility::Public; // Default to public

                    // Build signature
                    FuncSig sig;
                    sig.name = method.name;
                    sig.is_async = false;
                    sig.span = method.span;

                    // Convert param types (skip 'this')
                    for (const auto& param : method.params) {
                        if (param.pattern && param.pattern->is<parser::IdentPattern>() &&
                            param.pattern->as<parser::IdentPattern>().name == "this") {
                            continue;
                        }
                        if (param.type) {
                            sig.params.push_back(resolve_simple_type(*param.type));
                        }
                    }

                    // Return type
                    if (method.return_type.has_value()) {
                        sig.return_type = resolve_simple_type(*method.return_type.value());
                    } else {
                        sig.return_type = make_unit();
                    }

                    method_def.sig = sig;
                    methods.push_back(method_def);
                }

                // Create class definition
                ClassDef class_def;
                class_def.name = class_decl.name;
                class_def.type_params = std::move(type_params);
                class_def.base_class = base_class;
                class_def.interfaces = std::move(interfaces);
                class_def.fields = std::move(fields);
                class_def.methods = std::move(methods);
                class_def.is_abstract = class_decl.is_abstract;
                class_def.is_sealed = class_decl.is_sealed;
                class_def.span = class_decl.span;

                mod.classes[class_decl.name] = std::move(class_def);
                TML_DEBUG_LN("[MODULE] Registered class: " << class_decl.name << " in module "
                                                           << module_path);
            } else if (decl->is<parser::TypeAliasDecl>()) {
                const auto& alias_decl = decl->as<parser::TypeAliasDecl>();

                // Only include public type aliases
                if (alias_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Resolve the alias target type
                if (alias_decl.type) {
                    auto resolved = resolve_simple_type(*alias_decl.type);
                    mod.type_aliases[alias_decl.name] = resolved;

                    // Store generic parameter names
                    std::vector<std::string> generic_params;
                    for (const auto& gp : alias_decl.generics) {
                        generic_params.push_back(gp.name);
                    }
                    if (!generic_params.empty()) {
                        mod.type_alias_generics[alias_decl.name] = std::move(generic_params);
                    }
                    TML_DEBUG_LN("[MODULE] Registered type alias: "
                                 << alias_decl.name << " in module " << module_path);
                }
            } else if (decl->is<parser::ModDecl>()) {
                const auto& mod_decl = decl->as<parser::ModDecl>();

                // Only process public mod declarations
                if (mod_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Register submodule path: e.g., for "pub mod traits" in core::iter,
                // the submodule path is "core::iter::traits"
                std::string submod_path = module_path + "::" + mod_decl.name;
                mod.submodules[mod_decl.name] = submod_path;
                TML_DEBUG_LN("[MODULE] Registered submodule: " << mod_decl.name << " -> "
                                                               << submod_path);
            } else if (decl->is<parser::UseDecl>()) {
                const auto& use_decl = decl->as<parser::UseDecl>();

                // Build the source module path from use declaration
                std::string use_path;
                for (size_t i = 0; i < use_decl.path.segments.size(); ++i) {
                    if (i > 0)
                        use_path += "::";
                    use_path += use_decl.path.segments[i];
                }

                // Handle relative paths: if path doesn't start with known prefix,
                // treat it as relative to current module
                if (!use_path.empty() && use_path.find("core::") != 0 &&
                    use_path.find("std::") != 0 && use_path.find("test") != 0) {
                    // Relative path - prepend current module path
                    use_path = module_path + "::" + use_path;
                }

                // Load the dependency module (for all use declarations, not just public)
                // This ensures that methods from imported modules are available
                // Note: We use silent=true because the path might be an item import
                // (e.g., "use core::default::Default" where Default is a symbol, not a module)
                bool prev_abort_on_error = abort_on_module_error_;
                abort_on_module_error_ = false;

                // Try to load as module first
                bool loaded = load_native_module(use_path, /*silent=*/true);

                // If failed and path has multiple segments, last segment might be a symbol
                if (!loaded && use_decl.path.segments.size() > 1) {
                    std::string base_path;
                    for (size_t j = 0; j < use_decl.path.segments.size() - 1; ++j) {
                        if (j > 0)
                            base_path += "::";
                        base_path += use_decl.path.segments[j];
                    }
                    // Handle relative paths
                    if (!base_path.empty() && base_path.find("core::") != 0 &&
                        base_path.find("std::") != 0 && base_path.find("test") != 0) {
                        base_path = module_path + "::" + base_path;
                    }
                    load_native_module(base_path, /*silent=*/true);
                }

                abort_on_module_error_ = prev_abort_on_error;

                // Only create re-export entry for public use declarations
                if (use_decl.vis == parser::Visibility::Public) {
                    // Create re-export entry
                    std::string re_source_path = use_path;
                    std::vector<std::string> re_symbols =
                        use_decl.symbols.value_or(std::vector<std::string>{});

                    // Handle single symbol re-export: `pub use foo::bar::SymbolName`
                    // In this case, extract the symbol name and use the module path as source
                    if (!use_decl.is_glob && re_symbols.empty()) {
                        size_t last_sep = use_path.rfind("::");
                        if (last_sep != std::string::npos) {
                            std::string symbol_name = use_path.substr(last_sep + 2);
                            re_source_path = use_path.substr(0, last_sep);
                            re_symbols.push_back(symbol_name);
                        }
                    }

                    ReExport re_export{.source_path = re_source_path,
                                       .is_glob = use_decl.is_glob,
                                       .symbols = re_symbols,
                                       .alias = use_decl.alias};

                    mod.re_exports.push_back(std::move(re_export));
                    TML_DEBUG_LN("[MODULE] Registered re-export: "
                                 << use_path << (use_decl.is_glob ? "::*" : ""));
                } else {
                    // Track private use declarations for cache loading
                    // This ensures transitive dependencies are loaded when the module is
                    // retrieved from cache
                    mod.private_imports.push_back(use_path);
                    TML_DEBUG_LN("[MODULE] Registered private import: " << use_path);
                }
            } else if (decl->is<parser::TraitDecl>()) {
                const auto& trait_decl = decl->as<parser::TraitDecl>();

                // Only include public behaviors
                if (trait_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : trait_decl.generics) {
                    type_params.push_back(param.name);
                }

                // Extract method signatures and track default implementations
                std::vector<FuncSig> methods;
                std::set<std::string> methods_with_defaults;
                for (const auto& method : trait_decl.methods) {
                    FuncSig sig;
                    sig.name = method.name;
                    sig.is_async = false;
                    sig.span = method.span;

                    // Extract method's own type parameters
                    for (const auto& gp : method.generics) {
                        if (!gp.is_const) {
                            sig.type_params.push_back(gp.name);
                        }
                    }

                    // Convert param types (skip 'this')
                    for (const auto& param : method.params) {
                        if (param.pattern && param.pattern->is<parser::IdentPattern>() &&
                            param.pattern->as<parser::IdentPattern>().name == "this") {
                            continue;
                        }
                        if (param.type) {
                            sig.params.push_back(resolve_simple_type(*param.type));
                        }
                    }

                    // Return type
                    if (method.return_type.has_value()) {
                        sig.return_type = resolve_simple_type(*method.return_type.value());
                    } else {
                        sig.return_type = make_unit();
                    }

                    methods.push_back(sig);

                    // Track methods with default implementations
                    if (method.body.has_value()) {
                        methods_with_defaults.insert(method.name);
                    }
                }

                // Extract super behaviors (called super_traits in parser)
                std::vector<std::string> super_behaviors;
                for (const auto& super : trait_decl.super_traits) {
                    if (super && super->is<parser::NamedType>()) {
                        const auto& named = super->as<parser::NamedType>();
                        if (!named.path.segments.empty()) {
                            super_behaviors.push_back(named.path.segments.back());
                        }
                    }
                }

                // Create behavior definition
                BehaviorDef behavior_def{.name = trait_decl.name,
                                         .type_params = std::move(type_params),
                                         .const_params = {},
                                         .associated_types = {},
                                         .methods = std::move(methods),
                                         .super_behaviors = std::move(super_behaviors),
                                         .methods_with_defaults = std::move(methods_with_defaults),
                                         .span = trait_decl.span};

                mod.behaviors[trait_decl.name] = std::move(behavior_def);
                TML_DEBUG_LN("[MODULE] Registered behavior: " << trait_decl.name << " in module "
                                                              << module_path);
            }
        }
    }

    // Second pass: Register default behavior methods for impl blocks
    // This is done after behaviors are registered so we can look them up
    for (const auto& [decls, _src] : all_parsed) {
        for (const auto& decl : decls) {
            if (decl->is<parser::ImplDecl>()) {
                const auto& impl_decl = decl->as<parser::ImplDecl>();

                // Get the type name being implemented from self_type
                std::string type_name;
                if (impl_decl.self_type && impl_decl.self_type->is<parser::NamedType>()) {
                    const auto& named = impl_decl.self_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        type_name = named.path.segments.back();
                    }
                }

                if (type_name.empty() || !impl_decl.trait_type)
                    continue;

                // Get the behavior name
                if (!impl_decl.trait_type->is<parser::NamedType>())
                    continue;
                const auto& trait_named = impl_decl.trait_type->as<parser::NamedType>();
                std::string behavior_name;
                if (!trait_named.path.segments.empty()) {
                    behavior_name = trait_named.path.segments.back();
                }
                if (behavior_name.empty())
                    continue;

                // Look up the behavior definition - search current module first, then all modules
                std::optional<BehaviorDef> behavior_def_opt;
                auto behavior_it = mod.behaviors.find(behavior_name);
                if (behavior_it != mod.behaviors.end()) {
                    behavior_def_opt = behavior_it->second;
                } else {
                    // Search all registered modules for the behavior
                    for (const auto& [mod_name, mod_def] : module_registry_->get_all_modules()) {
                        auto it = mod_def.behaviors.find(behavior_name);
                        if (it != mod_def.behaviors.end()) {
                            behavior_def_opt = it->second;
                            TML_DEBUG_LN("[MODULE] Found behavior " << behavior_name
                                                                    << " in module " << mod_name);
                            break;
                        }
                    }
                }

                if (!behavior_def_opt)
                    continue;

                const auto& behavior_def = *behavior_def_opt;

                // Collect method names already provided by impl block
                std::set<std::string> impl_method_names;
                for (const auto& func : impl_decl.methods) {
                    impl_method_names.insert(func.name);
                }

                // Register default methods not overridden by impl
                for (const auto& bmethod : behavior_def.methods) {
                    if (impl_method_names.count(bmethod.name) == 0) {
                        // Register Type::method with behavior's method signature
                        std::string qualified_name = type_name + "::" + bmethod.name;
                        // Only register if not already registered
                        if (mod.functions.find(qualified_name) == mod.functions.end()) {
                            FuncSig sig{.name = qualified_name,
                                        .params = bmethod.params,
                                        .return_type = bmethod.return_type,
                                        .type_params = bmethod.type_params,
                                        .is_async = bmethod.is_async,
                                        .span = builtin_span,
                                        .stability = StabilityLevel::Stable,
                                        .deprecated_message = "",
                                        .since_version = "1.0",
                                        .where_constraints = bmethod.where_constraints,
                                        .is_lowlevel = bmethod.is_lowlevel};
                            mod.functions[qualified_name] = sig;
                            TML_DEBUG_LN("[MODULE] Registered default behavior method: "
                                         << qualified_name << " from " << behavior_name);
                        }
                    }
                }
            }
        }
    }

    // Check if module has any pure TML functions and collect source code
    std::string combined_source;
    for (const auto& [decls, src] : all_parsed) {
        for (const auto& decl : decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();
                // Check for functions with bodies
                if (func.vis == parser::Visibility::Public && !func.is_unsafe &&
                    func.body.has_value()) {
                    mod.has_pure_tml_functions = true;
                }
                // Also check for extern functions - they need declarations emitted
                if (func.vis == parser::Visibility::Public && func.extern_abi.has_value()) {
                    mod.has_pure_tml_functions = true;
                }
            }
            // Also check impl blocks for public methods with bodies
            else if (decl->is<parser::ImplDecl>()) {
                const auto& impl = decl->as<parser::ImplDecl>();
                for (const auto& method : impl.methods) {
                    if (method.vis == parser::Visibility::Public && !method.is_unsafe &&
                        method.body.has_value()) {
                        mod.has_pure_tml_functions = true;
                        break;
                    }
                }
            }
            // Also check class declarations for public methods with bodies
            else if (decl->is<parser::ClassDecl>()) {
                const auto& cls = decl->as<parser::ClassDecl>();
                for (const auto& method : cls.methods) {
                    if (method.vis == parser::MemberVisibility::Public && !method.is_abstract &&
                        method.body.has_value()) {
                        mod.has_pure_tml_functions = true;
                        break;
                    }
                }
            }
            // Modules with pub const declarations need codegen for constant registration
            else if (decl->is<parser::ConstDecl>()) {
                const auto& const_decl = decl->as<parser::ConstDecl>();
                if (const_decl.vis == parser::Visibility::Public) {
                    mod.has_pure_tml_functions = true;
                }
            }
        }
        if (!src.empty()) {
            combined_source += src + "\n";
        }
    }

    // Store source code if module has pure TML functions (for re-parsing in codegen)
    if (mod.has_pure_tml_functions) {
        mod.source_code = combined_source;
        mod.file_path = file_path;
    }

    // Register the module
    TML_DEBUG_LN("[MODULE] Loaded " << module_path << " from " << file_path << " ("
                                    << mod.functions.size() << " functions)");

    // Store in global cache for library modules before moving
    // This allows other compilation units to reuse the parsed module
    if (GlobalModuleCache::should_cache(module_path)) {
        GlobalModuleCache::instance().put(module_path, mod);
        TML_DEBUG_LN("[MODULE] Cached: " << module_path);

        // Write binary metadata cache (.tml.meta) for faster loading next run
        save_module_to_cache(module_path, mod, file_path);
    }

    // Capture re-export source paths before moving the module
    std::vector<std::string> re_export_sources;
    re_export_sources.reserve(mod.re_exports.size());
    for (const auto& re_export : mod.re_exports) {
        re_export_sources.push_back(re_export.source_path);
    }

    module_registry_->register_module(module_path, std::move(mod));

    // Mark as completed so guard doesn't remove it (it's already registered)
    loading_guard.mark_completed();
    loading_modules_.erase(module_path);

    // Load re-export source modules to ensure they're in the current registry
    for (const auto& source_path : re_export_sources) {
        load_native_module(source_path, /*silent=*/true);
    }

    return true;
}

} // namespace tml::types
