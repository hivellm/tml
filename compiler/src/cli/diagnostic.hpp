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
#include "types/type.hpp"

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

// Parser errors (P000-P099)
constexpr const char* PARSE_UNEXPECTED_TOKEN = "P001";
constexpr const char* PARSE_MISSING_SEMICOLON = "P002";
constexpr const char* PARSE_MISSING_BRACE = "P003";
constexpr const char* PARSE_INVALID_EXPR = "P004";
constexpr const char* PARSE_EXPECTED_TYPE = "P005";
constexpr const char* PARSE_EXPECTED_IDENTIFIER = "P006";
constexpr const char* PARSE_EXPECTED_PATTERN = "P007";
constexpr const char* PARSE_EXPECTED_COLON = "P008";
constexpr const char* PARSE_EXPECTED_COMMA = "P009";
constexpr const char* PARSE_EXPECTED_PAREN = "P010";
constexpr const char* PARSE_EXPECTED_BRACKET = "P011";
constexpr const char* PARSE_EXPECTED_ARROW = "P012";
constexpr const char* PARSE_EXPECTED_EQUALS = "P013";
constexpr const char* PARSE_INVALID_LITERAL = "P014";
constexpr const char* PARSE_UNCLOSED_STRING = "P015";
constexpr const char* PARSE_UNCLOSED_BLOCK = "P016";
constexpr const char* PARSE_UNCLOSED_PAREN = "P017";
constexpr const char* PARSE_UNCLOSED_BRACKET = "P018";
constexpr const char* PARSE_INVALID_OPERATOR = "P019";
constexpr const char* PARSE_EXPECTED_BLOCK = "P020";
constexpr const char* PARSE_EXPECTED_FUNC_NAME = "P021";
constexpr const char* PARSE_EXPECTED_TYPE_NAME = "P022";
constexpr const char* PARSE_EXPECTED_FIELD_NAME = "P023";
constexpr const char* PARSE_EXPECTED_PARAM_NAME = "P024";
constexpr const char* PARSE_EXPECTED_VARIANT_NAME = "P025";
constexpr const char* PARSE_EXPECTED_MODULE_NAME = "P026";
constexpr const char* PARSE_EXPECTED_BEHAVIOR_NAME = "P027";
constexpr const char* PARSE_INVALID_VISIBILITY = "P028";
constexpr const char* PARSE_INVALID_DECORATOR = "P029";
constexpr const char* PARSE_DUPLICATE_MODIFIER = "P030";
constexpr const char* PARSE_INVALID_GENERIC_PARAM = "P031";
constexpr const char* PARSE_INVALID_WHERE_CLAUSE = "P032";
constexpr const char* PARSE_EXPECTED_IMPL_TARGET = "P033";
constexpr const char* PARSE_EXPECTED_METHOD_NAME = "P034";
constexpr const char* PARSE_INVALID_TERNARY = "P035";
constexpr const char* PARSE_INVALID_CLOSURE = "P036";
constexpr const char* PARSE_EXPECTED_LOOP_BODY = "P037";
constexpr const char* PARSE_EXPECTED_IF_CONDITION = "P038";
constexpr const char* PARSE_EXPECTED_WHEN_SCRUTINEE = "P039";
constexpr const char* PARSE_EXPECTED_MATCH_ARM = "P040";
constexpr const char* PARSE_INVALID_STRUCT_LITERAL = "P041";
constexpr const char* PARSE_INVALID_ARRAY_LITERAL = "P042";
constexpr const char* PARSE_INVALID_TUPLE = "P043";
constexpr const char* PARSE_EXPECTED_RETURN_TYPE = "P044";
constexpr const char* PARSE_EXPECTED_FIELD_TYPE = "P045";
constexpr const char* PARSE_EXPECTED_PARAM_TYPE = "P046";
constexpr const char* PARSE_INVALID_INTERPOLATION = "P047";
constexpr const char* PARSE_UNCLOSED_INTERPOLATION = "P048";
constexpr const char* PARSE_INVALID_PROPERTY = "P049";
constexpr const char* PARSE_EXPECTED_GET_OR_SET = "P050";
constexpr const char* PARSE_INVALID_CONSTRUCTOR = "P051";
constexpr const char* PARSE_EXPECTED_CLASS_MEMBER = "P052";
constexpr const char* PARSE_INVALID_USE_PATH = "P053";
constexpr const char* PARSE_EXPECTED_USE_ITEM = "P054";
constexpr const char* PARSE_INVALID_LOWLEVEL = "P055";
constexpr const char* PARSE_EXPECTED_BOUND = "P056";
constexpr const char* PARSE_INVALID_REF_TYPE = "P057";
constexpr const char* PARSE_EXPECTED_ENUM_BODY = "P058";
constexpr const char* PARSE_EXPECTED_STRUCT_BODY = "P059";
constexpr const char* PARSE_INVALID_BREAK = "P060";
constexpr const char* PARSE_INVALID_CONTINUE = "P061";
constexpr const char* PARSE_INVALID_RETURN = "P062";
constexpr const char* PARSE_EXPECTED_NAMESPACE = "P063";
constexpr const char* PARSE_INVALID_TEMPLATE = "P064";
constexpr const char* PARSE_UNCLOSED_TEMPLATE = "P065";

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
constexpr const char* MISSING_TYPE_ANNOTATION = "T011";
constexpr const char* INVALID_ASSIGNMENT = "T012";
constexpr const char* IMMUTABLE_ASSIGN = "T013";
constexpr const char* CONDITION_NOT_BOOL = "T014";
constexpr const char* BRANCH_TYPE_MISMATCH = "T015";
constexpr const char* RETURN_TYPE_MISMATCH = "T016";
constexpr const char* INVALID_DEREFERENCE = "T017";
constexpr const char* INVALID_REFERENCE = "T018";
constexpr const char* OPERATOR_TYPE_MISMATCH = "T019";
constexpr const char* DIVISION_BY_ZERO = "T020";
constexpr const char* INVALID_CAST = "T021";
constexpr const char* STRUCT_UNKNOWN = "T022";
constexpr const char* ENUM_UNKNOWN = "T023";
constexpr const char* VARIANT_UNKNOWN = "T024";
constexpr const char* BEHAVIOR_UNKNOWN = "T025";
constexpr const char* BEHAVIOR_NOT_IMPL = "T026";
constexpr const char* MODULE_NOT_FOUND = "T027";
constexpr const char* INVALID_EXTERN = "T028";
constexpr const char* MISSING_RETURN = "T029";
constexpr const char* BREAK_OUTSIDE_LOOP = "T030";
constexpr const char* CONTINUE_OUTSIDE_LOOP = "T031";
constexpr const char* AWAIT_OUTSIDE_ASYNC = "T032";
constexpr const char* INVALID_TRY_OPERATOR = "T033";
constexpr const char* WRONG_VARIANT_ARGS = "T034";
constexpr const char* PATTERN_TYPE_MISMATCH = "T035";
constexpr const char* TUPLE_ARITY_MISMATCH = "T036";
constexpr const char* CONST_EVAL_ERROR = "T037";
constexpr const char* REDEFINE_BUILTIN = "T038";
constexpr const char* CIRCULAR_DEPENDENCY = "T039";
constexpr const char* ABSTRACT_INSTANTIATION = "T040";
constexpr const char* SEALED_EXTENSION = "T041";
constexpr const char* VALUE_CLASS_VIRTUAL = "T042";
constexpr const char* VALUE_CLASS_ABSTRACT = "T043";
constexpr const char* POOL_VALUE_CONFLICT = "T044";
constexpr const char* MISSING_ABSTRACT_IMPL = "T045";
constexpr const char* BASE_CLASS_NOT_FOUND = "T046";
constexpr const char* INTERFACE_NOT_FOUND = "T047";
constexpr const char* INVALID_BASE_ACCESS = "T048";
constexpr const char* POINTER_METHOD_ERROR = "T049";
constexpr const char* ITERATOR_TYPE_ERROR = "T050";
constexpr const char* RANGE_TYPE_ERROR = "T051";
constexpr const char* OBJECT_SAFETY_ERROR = "T052";
constexpr const char* GENERIC_CONSTRAINT_ERROR = "T053";
constexpr const char* LIFETIME_ERROR = "T054";

// Borrow errors (B000-B099)
constexpr const char* USE_AFTER_MOVE = "B001";
constexpr const char* MOVE_WHILE_BORROWED = "B002";
constexpr const char* ASSIGN_NOT_MUTABLE = "B003";
constexpr const char* ASSIGN_WHILE_BORROWED = "B004";
constexpr const char* BORROW_AFTER_MOVE = "B005";
constexpr const char* MUT_BORROW_NOT_MUTABLE = "B006";
constexpr const char* MUT_BORROW_WHILE_IMMUT = "B007";
constexpr const char* DOUBLE_MUT_BORROW = "B008";
constexpr const char* IMMUT_BORROW_WHILE_MUT = "B009";
constexpr const char* RETURN_LOCAL_REF = "B010";
constexpr const char* PARTIAL_MOVE = "B011";
constexpr const char* OVERLAPPING_BORROW = "B012";
constexpr const char* USE_WHILE_BORROWED = "B013";
constexpr const char* CLOSURE_CAPTURES_MOVED = "B014";
constexpr const char* CLOSURE_CAPTURE_CONFLICT = "B015";
constexpr const char* PARTIALLY_MOVED_VALUE = "B016";
constexpr const char* REBORROW_OUTLIVES_ORIGIN = "B017";
constexpr const char* AMBIGUOUS_RETURN_LIFETIME = "B031"; // E031 in docs
constexpr const char* INTERIOR_MUT_WARNING = "W001";
constexpr const char* BORROW_OTHER = "B099";

// Codegen errors (C000-C099)
constexpr const char* CODEGEN_ERROR = "C001";
constexpr const char* CODEGEN_UNSUPPORTED = "C002";
constexpr const char* CODEGEN_TYPE_ERROR = "C003";
constexpr const char* CODEGEN_FUNC_NOT_FOUND = "C004";
constexpr const char* CODEGEN_STRUCT_NOT_FOUND = "C005";
constexpr const char* CODEGEN_METHOD_NOT_FOUND = "C006";
constexpr const char* CODEGEN_INVALID_GENERIC = "C007";
constexpr const char* CODEGEN_MISSING_IMPL = "C008";
constexpr const char* CODEGEN_LLVM_ERROR = "C009";
constexpr const char* CODEGEN_LINK_ERROR = "C010";
constexpr const char* CODEGEN_ABI_ERROR = "C011";
constexpr const char* CODEGEN_RUNTIME_ERROR = "C012";
constexpr const char* CODEGEN_FFI_ERROR = "C013";
constexpr const char* CODEGEN_INTRINSIC_ERROR = "C014";

// Lexer errors (L000-L099)
constexpr const char* LEX_INVALID_CHAR = "L001";
constexpr const char* LEX_UNTERMINATED_STRING = "L002";
constexpr const char* LEX_INVALID_NUMBER = "L003";
constexpr const char* LEX_INVALID_ESCAPE = "L004";
constexpr const char* LEX_UNTERMINATED_CHAR = "L005";
constexpr const char* LEX_EMPTY_CHAR = "L006";
constexpr const char* LEX_MULTI_CHAR = "L007";
constexpr const char* LEX_INVALID_HEX = "L008";
constexpr const char* LEX_INVALID_BINARY = "L009";
constexpr const char* LEX_INVALID_OCTAL = "L010";
constexpr const char* LEX_NUMBER_OVERFLOW = "L011";
constexpr const char* LEX_UNTERMINATED_COMMENT = "L012";
constexpr const char* LEX_UNTERMINATED_RAW_STRING = "L013";
constexpr const char* LEX_INVALID_UNICODE = "L014";
constexpr const char* LEX_INVALID_INTERPOLATION = "L015";

// General errors (E000-E099)
constexpr const char* FILE_NOT_FOUND = "E001";
constexpr const char* IO_ERROR = "E002";
constexpr const char* INTERNAL_ERROR = "E003";
constexpr const char* COMMAND_ERROR = "E004";
constexpr const char* CONFIG_ERROR = "E005";
constexpr const char* DEPENDENCY_ERROR = "E006";
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

// ============================================================================
// HIR Type Formatting for Error Messages
// ============================================================================

/**
 * Format a HIR type for display in error messages.
 * Returns a human-readable string representation of the type.
 */
std::string format_hir_type(const types::TypePtr& type);

/**
 * Create a type mismatch diagnostic with expected/found types.
 * Provides rich context about the type mismatch.
 */
Diagnostic make_type_mismatch_diagnostic(const SourceSpan& span, const std::string& expected_type,
                                         const std::string& found_type,
                                         const std::string& context = "");

/**
 * Create a "cannot call non-function" diagnostic.
 */
Diagnostic make_not_callable_diagnostic(const SourceSpan& span, const std::string& type_name);

/**
 * Create an "unknown field" diagnostic with suggestions.
 */
Diagnostic make_unknown_field_diagnostic(const SourceSpan& span, const std::string& field_name,
                                         const std::string& type_name,
                                         const std::vector<std::string>& available_fields);

/**
 * Create an "unknown method" diagnostic with suggestions.
 */
Diagnostic make_unknown_method_diagnostic(const SourceSpan& span, const std::string& method_name,
                                          const std::string& type_name,
                                          const std::vector<std::string>& available_methods);

} // namespace tml::cli
