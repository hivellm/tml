//! # Diagnostic System Interface
//!
//! This header defines the compiler error/warning formatting system.
//!
//! ## Error Code Categories
//!
//! | Prefix | Category    | Example                         |
//! |--------|-------------|----------------------------------|
//! | L      | Lexer       | L001 - Invalid character         |
//! | P      | Parser      | P001 - Unexpected token          |
//! | T      | Type check  | T001 - Type mismatch             |
//! | B      | Borrow      | B001 - Use after move            |
//! | C      | Codegen     | C001 - Codegen error             |
//!
//! ## Features
//!
//! - Multi-line source snippets with line numbers
//! - Primary and secondary labels (^^^ vs ---)
//! - Fix-it hints for automatic correction
//! - "Did you mean?" suggestions via Levenshtein distance
//! - JSON output for IDE integration

#pragma once

#include "common.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::cli {

// ============================================================================
// ANSI Color Codes
// ============================================================================

struct Colors {
    static constexpr const char* Reset = "\033[0m";
    static constexpr const char* Bold = "\033[1m";
    static constexpr const char* Dim = "\033[2m";
    static constexpr const char* Underline = "\033[4m";

    // Foreground colors
    static constexpr const char* Red = "\033[31m";
    static constexpr const char* Green = "\033[32m";
    static constexpr const char* Yellow = "\033[33m";
    static constexpr const char* Blue = "\033[34m";
    static constexpr const char* Magenta = "\033[35m";
    static constexpr const char* Cyan = "\033[36m";
    static constexpr const char* White = "\033[37m";

    // Bright colors
    static constexpr const char* BrightRed = "\033[91m";
    static constexpr const char* BrightGreen = "\033[92m";
    static constexpr const char* BrightYellow = "\033[93m";
    static constexpr const char* BrightBlue = "\033[94m";
    static constexpr const char* BrightCyan = "\033[96m";
};

// ============================================================================
// Error Code Categories
// ============================================================================
//
// Error codes follow the pattern: <category><number>
//
// Categories:
//   L - Lexer errors (tokenization)
//   P - Parser errors (syntax)
//   T - Type errors (type checking)
//   B - Borrow errors (ownership/lifetimes)
//   C - Codegen errors (LLVM IR generation)
//   E - General errors
//
// Examples:
//   L001 - Invalid character
//   L002 - Unterminated string
//   P001 - Unexpected token
//   P002 - Missing semicolon
//   T001 - Type mismatch
//   T002 - Unknown type
//   T003 - Unknown function
//   T004 - Argument count mismatch
//   B001 - Use after move
//   B002 - Cannot borrow as mutable
//   C001 - Codegen error
//
namespace ErrorCodes {
// Lexer errors (L000-L099)
constexpr const char* LEX_INVALID_CHAR = "L001";
constexpr const char* LEX_UNTERMINATED_STRING = "L002";
constexpr const char* LEX_INVALID_NUMBER = "L003";
constexpr const char* LEX_INVALID_ESCAPE = "L004";

// Parser errors (P000-P099)
constexpr const char* PARSE_UNEXPECTED_TOKEN = "P001";
constexpr const char* PARSE_MISSING_SEMICOLON = "P002";
constexpr const char* PARSE_MISSING_BRACE = "P003";
constexpr const char* PARSE_INVALID_EXPR = "P004";
constexpr const char* PARSE_EXPECTED_TYPE = "P005";

// Type errors (T000-T199)
constexpr const char* TYPE_MISMATCH = "T001";
constexpr const char* TYPE_UNKNOWN = "T002";
constexpr const char* FUNC_UNKNOWN = "T003";
constexpr const char* ARG_COUNT_MISMATCH = "T004";
constexpr const char* FIELD_UNKNOWN = "T005";
constexpr const char* METHOD_UNKNOWN = "T006";
constexpr const char* CANNOT_INFER = "T007";
constexpr const char* DUPLICATE_DEF = "T008";
constexpr const char* UNDECLARED_VAR = "T009";
constexpr const char* NOT_CALLABLE = "T010";

// Borrow errors (B000-B099)
constexpr const char* USE_AFTER_MOVE = "B001";
constexpr const char* CANNOT_BORROW_MUT = "B002";
constexpr const char* ALREADY_BORROWED = "B003";
constexpr const char* LIFETIME_MISMATCH = "B004";

// Codegen errors (C000-C099)
constexpr const char* CODEGEN_ERROR = "C001";
constexpr const char* CODEGEN_UNSUPPORTED = "C002";

// General errors (E000-E099)
constexpr const char* FILE_NOT_FOUND = "E001";
constexpr const char* IO_ERROR = "E002";
} // namespace ErrorCodes

// ============================================================================
// Diagnostic Severity
// ============================================================================

enum class DiagnosticSeverity {
    Error,
    Warning,
    Note,
    Help,
};

// Warning categories for filtering by level
enum class WarningCategory {
    Default, // Always shown unless -Wnone
    Extra,   // Shown with -Wextra or higher
    All,     // Shown with -Wall or higher
    Pedantic // Only shown with -Wpedantic
};

// ============================================================================
// Diagnostic Message
// ============================================================================

struct DiagnosticLabel {
    SourceSpan span;
    std::string message;
    bool is_primary; // Primary label shown with ^^^, secondary with ---
};

// Fix-it hint for automatic code correction
struct DiagnosticFixIt {
    SourceSpan span;         // Where to apply the fix
    std::string replacement; // Text to insert/replace
    std::string description; // Human-readable description
};

struct Diagnostic {
    DiagnosticSeverity severity;
    std::string code;    // Error code (e.g., "E0001", "T001")
    std::string message; // Main error message
    SourceSpan primary_span;
    std::vector<DiagnosticLabel> labels;                         // Additional labeled spans
    std::vector<std::string> notes;                              // Additional notes
    std::vector<std::string> help;                               // Help messages with suggestions
    std::vector<DiagnosticFixIt> fixes;                          // Fix-it hints
    WarningCategory warning_category = WarningCategory::Default; // For filtering warnings
};

// ============================================================================
// Diagnostic Emitter
// ============================================================================

class DiagnosticEmitter {
public:
    explicit DiagnosticEmitter(std::ostream& out = std::cerr);

    // Configuration
    void set_color_enabled(bool enabled) {
        use_colors_ = enabled;
    }
    void set_source_content(const std::string& path, const std::string& content);

    // Emit diagnostics
    void emit(const Diagnostic& diag);

    // Convenience methods for quick diagnostics
    void error(const std::string& code, const std::string& message, const SourceSpan& span,
               const std::vector<std::string>& notes = {});

    void warning(const std::string& code, const std::string& message, const SourceSpan& span,
                 const std::vector<std::string>& notes = {});

    // Warning with category for filtering by warning level
    void warning(const std::string& code, const std::string& message, const SourceSpan& span,
                 WarningCategory category, const std::vector<std::string>& notes = {});

    void note(const std::string& message, const SourceSpan& span);

    // Check if a warning should be emitted based on current warning level
    static bool should_emit_warning(WarningCategory category);

    // Statistics
    size_t error_count() const {
        return error_count_;
    }
    size_t warning_count() const {
        return warning_count_;
    }
    void reset_counts() {
        error_count_ = 0;
        warning_count_ = 0;
    }

private:
    std::ostream& out_;
    bool use_colors_ = true;
    std::unordered_map<std::string, std::string> source_files_; // path -> content
    size_t error_count_ = 0;
    size_t warning_count_ = 0;

    // Color helpers
    const char* color(const char* code) const {
        return use_colors_ ? code : "";
    }

    // Formatting helpers
    void emit_header(const Diagnostic& diag);
    void emit_source_snippet(const SourceSpan& span, const std::vector<DiagnosticLabel>& labels);
    void emit_secondary_labels(const std::vector<DiagnosticLabel>& labels, int line_width);
    void emit_notes(const std::vector<std::string>& notes);
    void emit_help(const std::vector<std::string>& help);
    void emit_fixes(const std::vector<DiagnosticFixIt>& fixes);

    // JSON output
    void emit_json(const Diagnostic& diag);
    static std::string escape_json_string(const std::string& s);

    std::string get_source_line(const std::string& path, uint32_t line) const;
    std::string severity_string(DiagnosticSeverity sev) const;
    const char* severity_color(DiagnosticSeverity sev) const;

    // Multi-span helpers
    void emit_labeled_line(const std::string& file_path, uint32_t line,
                           const std::vector<DiagnosticLabel>& labels, int line_width);
};

// ============================================================================
// Global Diagnostic Emitter
// ============================================================================

// Get the global diagnostic emitter
DiagnosticEmitter& get_diagnostic_emitter();

// Check if terminal supports colors
bool terminal_supports_colors();

// ============================================================================
// "Did You Mean?" Suggestions
// ============================================================================

/**
 * Compute Levenshtein (edit) distance between two strings.
 * Used for fuzzy matching to suggest corrections.
 */
size_t levenshtein_distance(const std::string& s1, const std::string& s2);

/**
 * Find the best matching candidate from a list of options.
 * Returns the closest match if within threshold, or empty string if none found.
 *
 * @param input The misspelled input string
 * @param candidates List of valid candidates to match against
 * @param max_distance Maximum edit distance to consider (default: 3)
 * @return Best matching candidate, or empty string if none within threshold
 */
std::string find_similar(const std::string& input, const std::vector<std::string>& candidates,
                         size_t max_distance = 3);

/**
 * Find multiple similar candidates, sorted by distance.
 *
 * @param input The misspelled input string
 * @param candidates List of valid candidates to match against
 * @param max_results Maximum number of suggestions to return
 * @param max_distance Maximum edit distance to consider
 * @return Vector of matching candidates, sorted by similarity
 */
std::vector<std::string> find_similar_candidates(const std::string& input,
                                                 const std::vector<std::string>& candidates,
                                                 size_t max_results = 3, size_t max_distance = 3);

} // namespace tml::cli
