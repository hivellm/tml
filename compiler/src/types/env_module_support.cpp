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
        if (lit.token.kind == lexer::TokenKind::StringLiteral)
            return std::string(lit.token.string_value().value);
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

} // namespace tml::types
