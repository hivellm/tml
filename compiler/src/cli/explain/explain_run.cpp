TML_MODULE("compiler")

//! # Explain Command Entry Point
//!
//! This file implements the `tml explain` command, which shows detailed
//! explanations for compiler error codes.
//!
//! ## Usage
//!
//! ```bash
//! tml explain T001     # Explain a type mismatch error
//! tml explain B001     # Explain a use-after-move error
//! tml explain L003     # Explain an invalid number literal error
//! ```
//!
//! ## Architecture
//!
//! The explanation database is split across category files:
//! - `lexer_errors.cpp`    — L001-L015
//! - `parser_errors.cpp`   — P001-P065
//! - `type_errors.cpp`     — T001-T054
//! - `borrow_errors.cpp`   — B001-B017
//! - `codegen_errors.cpp`  — C001-C035
//! - `general_errors.cpp`  — E001-E006
//! - `preproc_errors.cpp`  — PP001-PP010
//! - `mir_errors.cpp`      — M001-M020
//! - `backend_errors.cpp`  — K001-K011, N001-N008
//! - `testing_errors.cpp`  — X001-X010
//! - `reflection_errors.cpp` — R001-R005
//! - `query_errors.cpp`      — Q001-Q010
//! - `format_errors.cpp`     — F001-F010

#include "cli/commands/cmd_explain.hpp"
#include "cli/diagnostic.hpp"
#include "cli/explain/explain_internal.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::cli {

// ============================================================================
// Merged explanation database
// ============================================================================

static const std::unordered_map<std::string, std::string>& get_all_explanations() {
    static std::unordered_map<std::string, std::string> merged;
    static bool initialized = false;

    if (!initialized) {
        const auto& lexer = explain::get_lexer_explanations();
        const auto& parser = explain::get_parser_explanations();
        const auto& type = explain::get_type_explanations();
        const auto& type2 = explain::get_type_explanations_part2();
        const auto& borrow = explain::get_borrow_explanations();
        const auto& codegen = explain::get_codegen_explanations();
        const auto& general = explain::get_general_explanations();
        const auto& preproc = explain::get_preprocessor_explanations();
        const auto& mir = explain::get_mir_explanations();
        const auto& backend = explain::get_backend_explanations();
        const auto& testing = explain::get_testing_explanations();
        const auto& reflection = explain::get_reflection_explanations();
        const auto& query = explain::get_query_explanations();
        const auto& format = explain::get_format_explanations();
        const auto& hir = explain::get_hir_explanations();

        merged.insert(lexer.begin(), lexer.end());
        merged.insert(parser.begin(), parser.end());
        merged.insert(type.begin(), type.end());
        merged.insert(type2.begin(), type2.end());
        merged.insert(borrow.begin(), borrow.end());
        merged.insert(codegen.begin(), codegen.end());
        merged.insert(general.begin(), general.end());
        merged.insert(preproc.begin(), preproc.end());
        merged.insert(mir.begin(), mir.end());
        merged.insert(backend.begin(), backend.end());
        merged.insert(testing.begin(), testing.end());
        merged.insert(reflection.begin(), reflection.end());
        merged.insert(query.begin(), query.end());
        merged.insert(format.begin(), format.end());
        merged.insert(hir.begin(), hir.end());

        initialized = true;
    }

    return merged;
}

// ============================================================================
// run_explain implementation
// ============================================================================

int run_explain(const std::string& code, bool /*verbose*/) {
    // Normalize: uppercase, strip whitespace
    std::string normalized;
    normalized.reserve(code.size());
    for (char c : code) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            normalized += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }

    if (normalized.empty()) {
        std::cerr << "Usage: tml explain <error-code>\n";
        std::cerr << "Example: tml explain T001\n";
        return 1;
    }

    const auto& explanations = get_all_explanations();
    auto it = explanations.find(normalized);

    if (it != explanations.end()) {
        // Print with colored header
        bool colors = terminal_supports_colors();
        if (colors) {
            std::cout << Colors::Bold << Colors::BrightCyan;
        }
        std::cout << "Explanation for " << normalized;
        if (colors) {
            std::cout << Colors::Reset;
        }
        std::cout << "\n";

        // Print the explanation text
        std::cout << it->second;

        // Ensure trailing newline
        if (!it->second.empty() && it->second.back() != '\n') {
            std::cout << "\n";
        }
        return 0;
    }

    // Not found — suggest similar codes
    std::cerr << "No explanation available for error code `" << normalized << "`.\n\n";

    // Collect all known codes for suggestion
    std::vector<std::string> known_codes;
    known_codes.reserve(explanations.size());
    for (const auto& [key, _] : explanations) {
        known_codes.push_back(key);
    }

    auto suggestions = find_similar_candidates(normalized, known_codes, 3, 2);
    if (!suggestions.empty()) {
        std::cerr << "Did you mean:\n";
        for (const auto& suggestion : suggestions) {
            std::cerr << "  tml explain " << suggestion << "\n";
        }
        std::cerr << "\n";
    }

    // Show available categories
    std::cerr << "Available error code categories:\n";
    std::cerr << "  L001-L020   Lexer errors (tokenization)\n";
    std::cerr << "  P001-P065   Parser errors (syntax)\n";
    std::cerr << "  PP001-PP010 Preprocessor errors (conditional compilation)\n";
    std::cerr << "  T001-T084   Type errors (type checking)\n";
    std::cerr << "  B001-B017   Borrow errors (ownership/lifetimes)\n";
    std::cerr << "  C001-C035   Codegen errors (code generation)\n";
    std::cerr << "  M001-M020   MIR building errors\n";
    std::cerr << "  K001-K011   LLVM backend errors\n";
    std::cerr << "  N001-N008   Linker (LLD) errors\n";
    std::cerr << "  X001-X010   Test runner errors\n";
    std::cerr << "  R001-R005   Reflection intrinsic errors\n";
    std::cerr << "  Q001-Q010   Query system errors\n";
    std::cerr << "  F001-F010   Formatter/linter rule errors\n";
    std::cerr << "  E001-E006   General errors\n";

    return 1;
}

} // namespace tml::cli
