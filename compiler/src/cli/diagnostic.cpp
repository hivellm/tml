TML_MODULE("compiler")

//! # Diagnostic System
//!
//! This file implements the diagnostic emitter for compiler errors, warnings,
//! and notes. It produces rich, Rust-style error messages with source context.
//!
//! ## Output Format
//!
//! ```text
//! error[E0001]: cannot borrow `x` as mutable because it is already borrowed
//!   --> src/main.tml:5:10
//!     |
//!   4 | let r = ref x
//!     |         ----- immutable borrow occurs here
//!   5 | let m = mut ref x
//!     |         ^^^^^^^^^ mutable borrow occurs here
//!     |
//!   = note: first borrow must be released before mutably borrowing
//!   = help: consider using a block scope to limit the first borrow
//! ```
//!
//! ## Diagnostic Components
//!
//! | Component      | Description                              | Color         |
//! |----------------|------------------------------------------|---------------|
//! | Header         | Severity + code + message                | Red/Yellow    |
//! | Location       | File:line:column                         | Blue          |
//! | Primary Label  | Main error location (^^^)                | Red           |
//! | Secondary Label| Related locations (---)                  | Blue          |
//! | Notes          | Additional context                       | Cyan          |
//! | Help           | Suggestions for fixing                   | Green         |
//! | Fix-it         | Concrete code replacement                | Green         |
//!
//! ## Output Formats
//!
//! - **Text**: Human-readable terminal output with ANSI colors
//! - **JSON**: Machine-readable format for IDE integration

#include "diagnostic.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <set>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace tml::cli {

// ============================================================================
// Terminal Detection
// ============================================================================

/// Detects if the terminal supports ANSI color codes.
///
/// On Windows 10+, enables virtual terminal processing for ANSI support.
/// On Unix, checks if stderr is a TTY and TERM is not "dumb".
bool terminal_supports_colors() {
#ifdef _WIN32
    // Enable ANSI colors on Windows 10+
    HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
        return false;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
        return false;

    // Try to enable virtual terminal processing
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(hOut, dwMode))
        return true;

    // Fallback: check if output is a terminal
    return isatty(fileno(stderr)) != 0;
#else
    // Unix: check if stderr is a terminal and TERM is set
    if (!isatty(fileno(stderr)))
        return false;

    const char* term = getenv("TERM");
    if (!term)
        return false;

    // Most terminals support colors
    return std::string(term) != "dumb";
#endif
}

// ============================================================================
// Global Emitter
// ============================================================================

static DiagnosticEmitter* g_emitter = nullptr;

DiagnosticEmitter& get_diagnostic_emitter() {
    if (!g_emitter) {
        static DiagnosticEmitter emitter(std::cerr);
        emitter.set_color_enabled(terminal_supports_colors());
        g_emitter = &emitter;
    }
    return *g_emitter;
}

// ============================================================================
// DiagnosticEmitter Implementation
// ============================================================================

DiagnosticEmitter::DiagnosticEmitter(std::ostream& out) : out_(out) {
    use_colors_ = terminal_supports_colors();
}

void DiagnosticEmitter::set_source_content(const std::string& path, const std::string& content) {
    source_files_[path] = content;
}

std::string DiagnosticEmitter::get_source_line(const std::string& path, uint32_t line) const {
    auto it = source_files_.find(path);
    if (it == source_files_.end())
        return "";

    const std::string& content = it->second;
    uint32_t current_line = 1;
    size_t line_start = 0;

    for (size_t i = 0; i < content.size(); ++i) {
        if (current_line == line) {
            size_t line_end = content.find('\n', i);
            if (line_end == std::string::npos)
                line_end = content.size();
            return content.substr(line_start, line_end - line_start);
        }
        if (content[i] == '\n') {
            current_line++;
            line_start = i + 1;
        }
    }

    return "";
}

std::string DiagnosticEmitter::severity_string(DiagnosticSeverity sev) const {
    switch (sev) {
    case DiagnosticSeverity::Error:
        return "error";
    case DiagnosticSeverity::Warning:
        return "warning";
    case DiagnosticSeverity::Note:
        return "note";
    case DiagnosticSeverity::Help:
        return "help";
    }
    return "unknown";
}

const char* DiagnosticEmitter::severity_color(DiagnosticSeverity sev) const {
    switch (sev) {
    case DiagnosticSeverity::Error:
        return Colors::BrightRed;
    case DiagnosticSeverity::Warning:
        return Colors::BrightYellow;
    case DiagnosticSeverity::Note:
        return Colors::BrightCyan;
    case DiagnosticSeverity::Help:
        return Colors::BrightGreen;
    }
    return Colors::Reset;
}

void DiagnosticEmitter::emit_header(const Diagnostic& diag) {
    // Format: error[E0001]: message
    out_ << color(Colors::Bold) << color(severity_color(diag.severity))
         << severity_string(diag.severity);

    if (!diag.code.empty()) {
        out_ << "[" << diag.code << "]";
    }

    out_ << color(Colors::Reset) << color(Colors::Bold) << ": " << diag.message
         << color(Colors::Reset) << "\n";
}

void DiagnosticEmitter::emit_labeled_line(const std::string& file_path, uint32_t line,
                                          const std::vector<DiagnosticLabel>& labels,
                                          int line_width) {
    std::string source_line = get_source_line(file_path, line);
    if (source_line.empty()) {
        return;
    }

    // Collect all labels on this line
    std::vector<const DiagnosticLabel*> line_labels;
    for (const auto& label : labels) {
        if (label.span.start.line == line) {
            line_labels.push_back(&label);
        }
    }

    // Sort by column position
    std::sort(line_labels.begin(), line_labels.end(),
              [](const DiagnosticLabel* a, const DiagnosticLabel* b) {
                  return a->span.start.column < b->span.start.column;
              });

    // Source line with line number
    out_ << color(Colors::BrightBlue) << std::setw(line_width) << line << " | "
         << color(Colors::Reset) << source_line << "\n";

    // Build the underline with multiple labels
    out_ << color(Colors::BrightBlue) << std::setw(line_width) << "" << " | "
         << color(Colors::Reset);

    // Track positions for rendering
    size_t current_pos = 0;

    for (const auto* label : line_labels) {
        uint32_t start_col = label->span.start.column > 0 ? label->span.start.column - 1 : 0;
        uint32_t end_col =
            label->span.end.line == line
                ? (label->span.end.column > label->span.start.column ? label->span.end.column - 1
                                                                     : start_col + 1)
                : static_cast<uint32_t>(source_line.length());

        // Pad spaces to reach start position
        while (current_pos < start_col && current_pos < source_line.length()) {
            out_ << ' ';
            current_pos++;
        }

        // Emit underline characters
        const char* label_color = label->is_primary ? Colors::BrightRed : Colors::BrightBlue;
        char underline_char = label->is_primary ? '^' : '-';

        out_ << color(label_color);
        while (current_pos < end_col && current_pos < source_line.length() + 1) {
            out_ << underline_char;
            current_pos++;
        }
        out_ << color(Colors::Reset);
    }

    // Add message for the last (rightmost) primary label on this line
    for (auto it = line_labels.rbegin(); it != line_labels.rend(); ++it) {
        if ((*it)->is_primary && !(*it)->message.empty()) {
            out_ << " " << color(Colors::BrightRed) << (*it)->message << color(Colors::Reset);
            break;
        }
    }

    out_ << "\n";

    // Emit secondary label messages on separate lines if they have messages
    for (const auto* label : line_labels) {
        if (!label->is_primary && !label->message.empty()) {
            out_ << color(Colors::BrightBlue) << std::setw(line_width) << "" << " | "
                 << color(Colors::Reset);

            // Pad to label position
            uint32_t start_col = label->span.start.column > 0 ? label->span.start.column - 1 : 0;
            for (uint32_t i = 0; i < start_col; ++i) {
                out_ << ' ';
            }
            out_ << color(Colors::BrightBlue) << "|" << color(Colors::Reset) << "\n";

            out_ << color(Colors::BrightBlue) << std::setw(line_width) << "" << " | "
                 << color(Colors::Reset);
            for (uint32_t i = 0; i < start_col; ++i) {
                out_ << ' ';
            }
            out_ << color(Colors::BrightBlue) << label->message << color(Colors::Reset) << "\n";
        }
    }
}

void DiagnosticEmitter::emit_source_snippet(const SourceSpan& span,
                                            const std::vector<DiagnosticLabel>& labels) {
    std::string file_path(span.start.file);

    // Location line: --> file:line:column
    out_ << color(Colors::BrightBlue) << "  --> " << color(Colors::Reset) << file_path << ":"
         << span.start.line << ":" << span.start.column << "\n";

    // Get the source line
    std::string source_line = get_source_line(file_path, span.start.line);
    if (source_line.empty()) {
        // Can't show source, just show location
        return;
    }

    // Calculate line number width for alignment - consider all label lines
    uint32_t max_line = span.start.line;
    for (const auto& label : labels) {
        max_line = std::max(max_line, label.span.start.line);
        max_line = std::max(max_line, label.span.end.line);
    }
    int line_width = static_cast<int>(std::to_string(max_line).length());
    line_width = std::max(line_width, 4);

    // Empty line with pipe
    out_ << color(Colors::BrightBlue) << std::setw(line_width) << "" << " |" << color(Colors::Reset)
         << "\n";

    // Collect all lines that need to be shown
    std::set<uint32_t> lines_to_show;
    lines_to_show.insert(span.start.line);
    for (const auto& label : labels) {
        lines_to_show.insert(label.span.start.line);
    }

    // Create a combined labels list including the primary span
    std::vector<DiagnosticLabel> all_labels;

    // Add primary span as a label if not already in labels
    bool has_primary = false;
    for (const auto& label : labels) {
        if (label.is_primary) {
            has_primary = true;
            break;
        }
    }
    if (!has_primary) {
        DiagnosticLabel primary_label;
        primary_label.span = span;
        primary_label.is_primary = true;
        all_labels.push_back(primary_label);
    }
    for (const auto& label : labels) {
        all_labels.push_back(label);
    }

    // Sort lines
    std::vector<uint32_t> sorted_lines(lines_to_show.begin(), lines_to_show.end());
    std::sort(sorted_lines.begin(), sorted_lines.end());

    // Emit each line with its labels
    uint32_t prev_line = 0;
    for (uint32_t line : sorted_lines) {
        // Show ellipsis if there's a gap
        if (prev_line > 0 && line > prev_line + 1) {
            out_ << color(Colors::BrightBlue) << std::setw(line_width - 1) << "" << "..."
                 << color(Colors::Reset) << "\n";
        }

        emit_labeled_line(file_path, line, all_labels, line_width);
        prev_line = line;
    }

    // Empty line after
    out_ << color(Colors::BrightBlue) << std::setw(line_width) << "" << " |" << color(Colors::Reset)
         << "\n";
}

void DiagnosticEmitter::emit_secondary_labels(const std::vector<DiagnosticLabel>& labels,
                                              int line_width) {
    // Filter secondary labels that are in different files or far from primary
    for (const auto& label : labels) {
        if (!label.is_primary && !label.message.empty()) {
            std::string file_path(label.span.start.file);

            out_ << color(Colors::BrightBlue) << "  --> " << color(Colors::Reset) << file_path
                 << ":" << label.span.start.line << ":" << label.span.start.column << "\n";

            std::string source_line = get_source_line(file_path, label.span.start.line);
            if (!source_line.empty()) {
                out_ << color(Colors::BrightBlue) << std::setw(line_width) << "" << " |"
                     << color(Colors::Reset) << "\n";

                out_ << color(Colors::BrightBlue) << std::setw(line_width) << label.span.start.line
                     << " | " << color(Colors::Reset) << source_line << "\n";

                out_ << color(Colors::BrightBlue) << std::setw(line_width) << "" << " | "
                     << color(Colors::Reset);

                uint32_t start_col = label.span.start.column > 0 ? label.span.start.column - 1 : 0;
                uint32_t end_col = label.span.end.column > label.span.start.column
                                       ? label.span.end.column - 1
                                       : start_col + 1;

                for (uint32_t i = 0; i < start_col; ++i) {
                    out_ << ' ';
                }
                out_ << color(Colors::BrightBlue);
                for (uint32_t i = start_col; i < end_col; ++i) {
                    out_ << '-';
                }
                out_ << " " << label.message << color(Colors::Reset) << "\n";

                out_ << color(Colors::BrightBlue) << std::setw(line_width) << "" << " |"
                     << color(Colors::Reset) << "\n";
            }
        }
    }
}

void DiagnosticEmitter::emit_notes(const std::vector<std::string>& notes) {
    for (const auto& note : notes) {
        out_ << color(Colors::BrightCyan) << "  = note" << color(Colors::Reset) << ": " << note
             << "\n";
    }
}

void DiagnosticEmitter::emit_help(const std::vector<std::string>& help) {
    for (const auto& h : help) {
        out_ << color(Colors::BrightGreen) << "  = help" << color(Colors::Reset) << ": " << h
             << "\n";
    }
}

void DiagnosticEmitter::emit_fixes(const std::vector<DiagnosticFixIt>& fixes) {
    for (const auto& fix : fixes) {
        // Show the fix-it hint header
        out_ << color(Colors::BrightGreen) << "  = fix" << color(Colors::Reset) << ": "
             << fix.description << "\n";

        // Show the location
        std::string file_path(fix.span.start.file);
        out_ << color(Colors::BrightBlue) << "  --> " << color(Colors::Reset) << file_path << ":"
             << fix.span.start.line << ":" << fix.span.start.column << "\n";

        // Get source line
        std::string source_line = get_source_line(file_path, fix.span.start.line);
        if (source_line.empty()) {
            continue;
        }

        int line_width = 4;

        // Show the original line
        out_ << color(Colors::BrightBlue) << std::setw(line_width) << "" << " |"
             << color(Colors::Reset) << "\n";

        out_ << color(Colors::BrightBlue) << std::setw(line_width) << fix.span.start.line << " | "
             << color(Colors::Reset) << source_line << "\n";

        // Show the suggested replacement
        uint32_t start_col = fix.span.start.column > 0 ? fix.span.start.column - 1 : 0;
        uint32_t end_col =
            fix.span.end.column > fix.span.start.column ? fix.span.end.column - 1 : start_col;

        // Build the replacement visualization
        out_ << color(Colors::BrightBlue) << std::setw(line_width) << "" << " | "
             << color(Colors::Reset);

        // Pad to start position
        for (uint32_t i = 0; i < start_col; ++i) {
            out_ << ' ';
        }

        // Show what will be replaced/inserted
        if (fix.replacement.empty()) {
            // Deletion - show dashes for removed characters
            out_ << color(Colors::BrightRed);
            for (uint32_t i = start_col; i < end_col; ++i) {
                out_ << '-';
            }
            out_ << color(Colors::Reset) << "\n";
        } else if (start_col == end_col) {
            // Insertion
            out_ << color(Colors::BrightGreen) << "+" << fix.replacement << color(Colors::Reset)
                 << "\n";
        } else {
            // Replacement - show what will be inserted
            out_ << color(Colors::BrightGreen) << fix.replacement << color(Colors::Reset) << "\n";
        }

        out_ << color(Colors::BrightBlue) << std::setw(line_width) << "" << " |"
             << color(Colors::Reset) << "\n";
    }
}

void DiagnosticEmitter::emit(const Diagnostic& diag) {
    // Update counts
    if (diag.severity == DiagnosticSeverity::Error) {
        error_count_++;
    } else if (diag.severity == DiagnosticSeverity::Warning) {
        warning_count_++;
    }

    // Check if we should emit as JSON
    if (tml::CompilerOptions::diagnostic_format == tml::DiagnosticFormat::JSON) {
        emit_json(diag);
        return;
    }

    // Emit the diagnostic (text format)
    emit_header(diag);
    emit_source_snippet(diag.primary_span, diag.labels);
    emit_notes(diag.notes);
    emit_help(diag.help);
    emit_fixes(diag.fixes);
}

std::string DiagnosticEmitter::escape_json_string(const std::string& s) {
    std::ostringstream result;
    for (char c : s) {
        switch (c) {
        case '"':
            result << "\\\"";
            break;
        case '\\':
            result << "\\\\";
            break;
        case '\b':
            result << "\\b";
            break;
        case '\f':
            result << "\\f";
            break;
        case '\n':
            result << "\\n";
            break;
        case '\r':
            result << "\\r";
            break;
        case '\t':
            result << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                // Control character - emit as \uXXXX
                result << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(c);
            } else {
                result << c;
            }
            break;
        }
    }
    return result.str();
}

void DiagnosticEmitter::emit_json(const Diagnostic& diag) {
    out_ << "{";
    out_ << "\"severity\":\"" << severity_string(diag.severity) << "\",";
    out_ << "\"code\":\"" << escape_json_string(diag.code) << "\",";
    out_ << "\"message\":\"" << escape_json_string(diag.message) << "\",";

    // Primary span
    out_ << "\"span\":{";
    out_ << "\"file\":\"" << escape_json_string(std::string(diag.primary_span.start.file)) << "\",";
    out_ << "\"start\":{\"line\":" << diag.primary_span.start.line
         << ",\"column\":" << diag.primary_span.start.column << "},";
    out_ << "\"end\":{\"line\":" << diag.primary_span.end.line
         << ",\"column\":" << diag.primary_span.end.column << "}";
    out_ << "},";

    // Labels
    out_ << "\"labels\":[";
    bool first = true;
    for (const auto& label : diag.labels) {
        if (!first)
            out_ << ",";
        first = false;
        out_ << "{";
        out_ << "\"message\":\"" << escape_json_string(label.message) << "\",";
        out_ << "\"is_primary\":" << (label.is_primary ? "true" : "false") << ",";
        out_ << "\"span\":{";
        out_ << "\"file\":\"" << escape_json_string(std::string(label.span.start.file)) << "\",";
        out_ << "\"start\":{\"line\":" << label.span.start.line
             << ",\"column\":" << label.span.start.column << "},";
        out_ << "\"end\":{\"line\":" << label.span.end.line
             << ",\"column\":" << label.span.end.column << "}";
        out_ << "}";
        out_ << "}";
    }
    out_ << "],";

    // Notes
    out_ << "\"notes\":[";
    first = true;
    for (const auto& note : diag.notes) {
        if (!first)
            out_ << ",";
        first = false;
        out_ << "\"" << escape_json_string(note) << "\"";
    }
    out_ << "],";

    // Help
    out_ << "\"help\":[";
    first = true;
    for (const auto& h : diag.help) {
        if (!first)
            out_ << ",";
        first = false;
        out_ << "\"" << escape_json_string(h) << "\"";
    }
    out_ << "],";

    // Fixes
    out_ << "\"fixes\":[";
    first = true;
    for (const auto& fix : diag.fixes) {
        if (!first)
            out_ << ",";
        first = false;
        out_ << "{";
        out_ << "\"description\":\"" << escape_json_string(fix.description) << "\",";
        out_ << "\"replacement\":\"" << escape_json_string(fix.replacement) << "\",";
        out_ << "\"span\":{";
        out_ << "\"file\":\"" << escape_json_string(std::string(fix.span.start.file)) << "\",";
        out_ << "\"start\":{\"line\":" << fix.span.start.line
             << ",\"column\":" << fix.span.start.column << "},";
        out_ << "\"end\":{\"line\":" << fix.span.end.line << ",\"column\":" << fix.span.end.column
             << "}";
        out_ << "}";
        out_ << "}";
    }
    out_ << "]";

    out_ << "}\n";
}

void DiagnosticEmitter::error(const std::string& code, const std::string& message,
                              const SourceSpan& span, const std::vector<std::string>& notes) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.code = code;
    diag.message = message;
    diag.primary_span = span;
    diag.notes = notes;
    emit(diag);
}

void DiagnosticEmitter::warning(const std::string& code, const std::string& message,
                                const SourceSpan& span, const std::vector<std::string>& notes) {
    warning(code, message, span, WarningCategory::Default, notes);
}

void DiagnosticEmitter::warning(const std::string& code, const std::string& message,
                                const SourceSpan& span, WarningCategory category,
                                const std::vector<std::string>& notes) {
    // Check if this warning should be emitted based on warning level
    if (!should_emit_warning(category)) {
        return;
    }

    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Warning;
    diag.code = code;
    diag.message = message;
    diag.primary_span = span;
    diag.notes = notes;
    diag.warning_category = category;

    // If -Werror is set, treat warnings as errors
    if (tml::CompilerOptions::warnings_as_errors) {
        diag.severity = DiagnosticSeverity::Error;
    }

    emit(diag);
}

bool DiagnosticEmitter::should_emit_warning(WarningCategory category) {
    tml::WarningLevel level = tml::CompilerOptions::warning_level;

    switch (level) {
    case tml::WarningLevel::None:
        return false;
    case tml::WarningLevel::Default:
        return category == WarningCategory::Default;
    case tml::WarningLevel::Extra:
        return category == WarningCategory::Default || category == WarningCategory::Extra;
    case tml::WarningLevel::All:
        return category != WarningCategory::Pedantic;
    case tml::WarningLevel::Pedantic:
        return true;
    }
    return true;
}

void DiagnosticEmitter::note(const std::string& message, const SourceSpan& span) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Note;
    diag.message = message;
    diag.primary_span = span;
    emit(diag);
}

// ============================================================================
// "Did You Mean?" Suggestions Implementation
// ============================================================================
//
// Uses Levenshtein distance (edit distance) to find similar identifiers
// when an unknown name is encountered. This helps suggest corrections like:
//
//   error: unknown function `pirntln`
//     = help: did you mean `println`?

/// Computes the Levenshtein distance between two strings.
///
/// The Levenshtein distance is the minimum number of single-character
/// edits (insertions, deletions, substitutions) required to transform
/// one string into another. Case-insensitive comparison is used.
size_t levenshtein_distance(const std::string& s1, const std::string& s2) {
    const size_t m = s1.length();
    const size_t n = s2.length();

    // Early exits for empty strings
    if (m == 0)
        return n;
    if (n == 0)
        return m;

    // Use two rows for space efficiency
    std::vector<size_t> prev_row(n + 1);
    std::vector<size_t> curr_row(n + 1);

    // Initialize first row
    for (size_t j = 0; j <= n; ++j) {
        prev_row[j] = j;
    }

    // Fill in the rest of the matrix
    for (size_t i = 1; i <= m; ++i) {
        curr_row[0] = i;

        for (size_t j = 1; j <= n; ++j) {
            // Case-insensitive comparison
            char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(s1[i - 1])));
            char c2 = static_cast<char>(std::tolower(static_cast<unsigned char>(s2[j - 1])));

            size_t cost = (c1 == c2) ? 0 : 1;

            curr_row[j] = std::min({prev_row[j] + 1,          // deletion
                                    curr_row[j - 1] + 1,      // insertion
                                    prev_row[j - 1] + cost}); // substitution
        }

        std::swap(prev_row, curr_row);
    }

    return prev_row[n];
}

/// Finds the most similar candidate to the input string.
///
/// Returns an empty string if no candidate is within max_distance.
std::string find_similar(const std::string& input, const std::vector<std::string>& candidates,
                         size_t max_distance) {
    if (input.empty() || candidates.empty()) {
        return "";
    }

    std::string best_match;
    size_t best_distance = max_distance + 1;

    for (const auto& candidate : candidates) {
        // Skip if length difference is too large
        size_t len_diff = input.length() > candidate.length() ? input.length() - candidate.length()
                                                              : candidate.length() - input.length();
        if (len_diff > max_distance) {
            continue;
        }

        size_t dist = levenshtein_distance(input, candidate);
        if (dist < best_distance) {
            best_distance = dist;
            best_match = candidate;
        }
    }

    return best_match;
}

/// Finds multiple similar candidates, sorted by distance.
///
/// Returns up to max_results candidates within max_distance.
std::vector<std::string> find_similar_candidates(const std::string& input,
                                                 const std::vector<std::string>& candidates,
                                                 size_t max_results, size_t max_distance) {
    if (input.empty() || candidates.empty()) {
        return {};
    }

    // Collect all candidates with their distances
    std::vector<std::pair<std::string, size_t>> scored;
    scored.reserve(candidates.size());

    for (const auto& candidate : candidates) {
        // Skip if length difference is too large
        size_t len_diff = input.length() > candidate.length() ? input.length() - candidate.length()
                                                              : candidate.length() - input.length();
        if (len_diff > max_distance) {
            continue;
        }

        size_t dist = levenshtein_distance(input, candidate);
        if (dist <= max_distance) {
            scored.emplace_back(candidate, dist);
        }
    }

    // Sort by distance
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    // Extract top results
    std::vector<std::string> result;
    result.reserve(std::min(max_results, scored.size()));
    for (size_t i = 0; i < max_results && i < scored.size(); ++i) {
        result.push_back(scored[i].first);
    }

    return result;
}

// ============================================================================
// HIR Type Formatting for Error Messages
// ============================================================================

std::string format_hir_type(const types::TypePtr& type) {
    if (!type) {
        return "<unknown>";
    }

    return std::visit(
        [](const auto& t) -> std::string {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, types::PrimitiveType>) {
                switch (t.kind) {
                case types::PrimitiveKind::I8:
                    return "I8";
                case types::PrimitiveKind::I16:
                    return "I16";
                case types::PrimitiveKind::I32:
                    return "I32";
                case types::PrimitiveKind::I64:
                    return "I64";
                case types::PrimitiveKind::I128:
                    return "I128";
                case types::PrimitiveKind::U8:
                    return "U8";
                case types::PrimitiveKind::U16:
                    return "U16";
                case types::PrimitiveKind::U32:
                    return "U32";
                case types::PrimitiveKind::U64:
                    return "U64";
                case types::PrimitiveKind::U128:
                    return "U128";
                case types::PrimitiveKind::F32:
                    return "F32";
                case types::PrimitiveKind::F64:
                    return "F64";
                case types::PrimitiveKind::Bool:
                    return "Bool";
                case types::PrimitiveKind::Char:
                    return "Char";
                case types::PrimitiveKind::Str:
                    return "Str";
                case types::PrimitiveKind::Unit:
                    return "()";
                case types::PrimitiveKind::Never:
                    return "!";
                default:
                    return "<primitive>";
                }
            } else if constexpr (std::is_same_v<T, types::NamedType>) {
                std::string result = t.name;
                if (!t.type_args.empty()) {
                    result += "[";
                    for (size_t i = 0; i < t.type_args.size(); ++i) {
                        if (i > 0)
                            result += ", ";
                        result += format_hir_type(t.type_args[i]);
                    }
                    result += "]";
                }
                return result;
            } else if constexpr (std::is_same_v<T, types::RefType>) {
                std::string result = t.is_mut ? "mut ref " : "ref ";
                result += format_hir_type(t.inner);
                return result;
            } else if constexpr (std::is_same_v<T, types::PtrType>) {
                std::string result = t.is_mut ? "*mut " : "*";
                result += format_hir_type(t.inner);
                return result;
            } else if constexpr (std::is_same_v<T, types::ArrayType>) {
                return "[" + format_hir_type(t.element) + "; " + std::to_string(t.size) + "]";
            } else if constexpr (std::is_same_v<T, types::SliceType>) {
                return "[" + format_hir_type(t.element) + "]";
            } else if constexpr (std::is_same_v<T, types::TupleType>) {
                std::string result = "(";
                for (size_t i = 0; i < t.elements.size(); ++i) {
                    if (i > 0)
                        result += ", ";
                    result += format_hir_type(t.elements[i]);
                }
                result += ")";
                return result;
            } else if constexpr (std::is_same_v<T, types::FuncType>) {
                std::string result = "func(";
                for (size_t i = 0; i < t.params.size(); ++i) {
                    if (i > 0)
                        result += ", ";
                    result += format_hir_type(t.params[i]);
                }
                result += ") -> ";
                result += format_hir_type(t.return_type);
                return result;
            } else if constexpr (std::is_same_v<T, types::ClosureType>) {
                std::string result = "closure(";
                for (size_t i = 0; i < t.params.size(); ++i) {
                    if (i > 0)
                        result += ", ";
                    result += format_hir_type(t.params[i]);
                }
                result += ") -> ";
                result += format_hir_type(t.return_type);
                return result;
            } else if constexpr (std::is_same_v<T, types::TypeVar>) {
                return "?" + std::to_string(t.id);
            } else if constexpr (std::is_same_v<T, types::GenericType>) {
                return t.name;
            } else if constexpr (std::is_same_v<T, types::ConstGenericType>) {
                return "const " + t.name;
            } else if constexpr (std::is_same_v<T, types::DynBehaviorType>) {
                return "dyn " + t.behavior_name;
            } else if constexpr (std::is_same_v<T, types::ImplBehaviorType>) {
                return "impl " + t.behavior_name;
            } else {
                return "<type>";
            }
        },
        type->kind);
}

Diagnostic make_type_mismatch_diagnostic(const SourceSpan& span, const std::string& expected_type,
                                         const std::string& found_type,
                                         const std::string& context) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.code = ErrorCodes::TYPE_MISMATCH;
    diag.message = "type mismatch";
    diag.primary_span = span;

    DiagnosticLabel label;
    label.span = span;
    label.message = "expected `" + expected_type + "`, found `" + found_type + "`";
    label.is_primary = true;
    diag.labels.push_back(label);

    if (!context.empty()) {
        diag.notes.push_back(context);
    }

    return diag;
}

Diagnostic make_not_callable_diagnostic(const SourceSpan& span, const std::string& type_name) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.code = ErrorCodes::NOT_CALLABLE;
    diag.message = "cannot call value of type `" + type_name + "`";
    diag.primary_span = span;

    DiagnosticLabel label;
    label.span = span;
    label.message = "not a function";
    label.is_primary = true;
    diag.labels.push_back(label);

    diag.notes.push_back("only function types can be called");

    return diag;
}

Diagnostic make_unknown_field_diagnostic(const SourceSpan& span, const std::string& field_name,
                                         const std::string& type_name,
                                         const std::vector<std::string>& available_fields) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.code = ErrorCodes::FIELD_UNKNOWN;
    diag.message = "no field `" + field_name + "` on type `" + type_name + "`";
    diag.primary_span = span;

    DiagnosticLabel label;
    label.span = span;
    label.message = "unknown field";
    label.is_primary = true;
    diag.labels.push_back(label);

    // Find similar field names
    std::string suggestion = find_similar(field_name, available_fields);
    if (!suggestion.empty()) {
        diag.help.push_back("did you mean `" + suggestion + "`?");
    }

    // List available fields if few enough
    if (available_fields.size() <= 5 && !available_fields.empty()) {
        std::string fields_str;
        for (size_t i = 0; i < available_fields.size(); ++i) {
            if (i > 0)
                fields_str += ", ";
            fields_str += "`" + available_fields[i] + "`";
        }
        diag.notes.push_back("available fields: " + fields_str);
    }

    return diag;
}

Diagnostic make_unknown_method_diagnostic(const SourceSpan& span, const std::string& method_name,
                                          const std::string& type_name,
                                          const std::vector<std::string>& available_methods) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.code = ErrorCodes::METHOD_UNKNOWN;
    diag.message = "no method `" + method_name + "` found for type `" + type_name + "`";
    diag.primary_span = span;

    DiagnosticLabel label;
    label.span = span;
    label.message = "unknown method";
    label.is_primary = true;
    diag.labels.push_back(label);

    // Find similar method names
    std::string suggestion = find_similar(method_name, available_methods);
    if (!suggestion.empty()) {
        diag.help.push_back("did you mean `" + suggestion + "`?");
    }

    return diag;
}

} // namespace tml::cli
