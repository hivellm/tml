#pragma once

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <string>
#include <vector>

namespace tml::types {

/// Holds a parsed .tml source file (decls + raw source text).
struct ParsedModuleFile {
    std::vector<parser::DeclPtr> decls;
    std::string source_code;
    bool is_from_mod_file;
};

/// Accumulated result of lexing + parsing a module file.
struct ParseResult {
    bool success;
    std::vector<parser::DeclPtr> decls;
    std::string source_code;
    std::vector<parser::ParseError> errors;
    std::vector<lexer::LexerError> lex_errors;
};

// ---------------------------------------------------------------------------
// Helper utilities shared between env_module_load.cpp and
// env_module_load_decls.cpp (previously duplicated as static functions).
// ---------------------------------------------------------------------------

/// Return the TML type name string for a parser TypePtr.
std::string get_tml_type_name(const parser::TypePtr& type);

/// Format a double with full precision for LLVM IR inline constants.
std::string format_float_const(double val);

/// Try to extract a compile-time constant scalar value string from an expression.
/// Returns an empty string if the expression is not a recognisable constant.
std::string try_extract_scalar_const_value(const parser::Expr* expr);

/// Try to extract a compile-time constant value (scalar or tuple) from a ConstDecl.
/// Writes the resolved TML type name into @p tml_type.
std::string try_extract_module_const_value(const parser::ConstDecl& const_decl,
                                           std::string& tml_type);

} // namespace tml::types
