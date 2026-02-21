//! # Explain Internal Interface
//!
//! Shared declarations for the explain command subsystem.
//! Each category module returns explanations for its error codes.
//!
//! ## Categories
//!
//! | Module                | Code Range   | Description                    |
//! |-----------------------|--------------|--------------------------------|
//! | `lexer_errors.cpp`    | L001-L015    | Tokenization errors            |
//! | `parser_errors.cpp`   | P001-P065    | Syntax errors                  |
//! | `type_errors.cpp`     | T001-T054    | Type checking errors           |
//! | `borrow_errors.cpp`   | B001-B017    | Ownership/lifetime errors      |
//! | `codegen_errors.cpp`  | C001-C014    | Code generation errors         |
//! | `general_errors.cpp`  | E001-E006    | General compiler errors        |
//! | `preproc_errors.cpp`  | PP001-PP002  | Preprocessor errors            |

#pragma once

#include <string>
#include <unordered_map>

namespace tml::cli::explain {

/// Get explanations for Lexer errors (L001-L015)
const std::unordered_map<std::string, std::string>& get_lexer_explanations();

/// Get explanations for Parser errors (P001-P065)
const std::unordered_map<std::string, std::string>& get_parser_explanations();

/// Get explanations for Type errors (T001-T054)
const std::unordered_map<std::string, std::string>& get_type_explanations();

/// Get explanations for Borrow errors (B001-B017)
const std::unordered_map<std::string, std::string>& get_borrow_explanations();

/// Get explanations for Codegen errors (C001-C014)
const std::unordered_map<std::string, std::string>& get_codegen_explanations();

/// Get explanations for General errors (E001-E006)
const std::unordered_map<std::string, std::string>& get_general_explanations();

/// Get explanations for Preprocessor errors (PP001-PP002)
const std::unordered_map<std::string, std::string>& get_preprocessor_explanations();

} // namespace tml::cli::explain
