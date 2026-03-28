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
//! | `borrow_errors.cpp`   | B001-B030    | Ownership/lifetime errors      |
//! | `codegen_errors.cpp`  | C001-C050    | Code generation errors         |
//! | `general_errors.cpp`  | E001-E006, W001-W015 | General + warning codes |
//! | `preproc_errors.cpp`  | PP001-PP010  | Preprocessor errors            |
//! | `mir_errors.cpp`      | M001-M020    | MIR building errors            |
//! | `backend_errors.cpp`  | K001-K011    | LLVM backend errors            |
//! | `backend_errors.cpp`  | N001-N008    | Linker (LLD) errors            |
//! | `testing_errors.cpp`  | X001-X010    | Test runner errors             |
//! | `reflection_errors.cpp` | R001-R005  | Reflection intrinsic errors    |

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

/// Get explanations for Borrow errors (B001-B030)
const std::unordered_map<std::string, std::string>& get_borrow_explanations();

/// Get explanations for Codegen errors (C001-C050)
const std::unordered_map<std::string, std::string>& get_codegen_explanations();

/// Get explanations for General errors (E001-E006) and warning codes (W001-W015)
const std::unordered_map<std::string, std::string>& get_general_explanations();

/// Get explanations for Preprocessor errors (PP001-PP010)
const std::unordered_map<std::string, std::string>& get_preprocessor_explanations();

/// Get explanations for MIR building errors (M001-M020)
const std::unordered_map<std::string, std::string>& get_mir_explanations();

/// Get explanations for LLVM backend errors (K001-K011) and linker errors (N001-N008)
const std::unordered_map<std::string, std::string>& get_backend_explanations();

/// Get explanations for test runner errors (X001-X010)
const std::unordered_map<std::string, std::string>& get_testing_explanations();

/// Get explanations for reflection intrinsic errors (R001-R005)
const std::unordered_map<std::string, std::string>& get_reflection_explanations();

} // namespace tml::cli::explain
