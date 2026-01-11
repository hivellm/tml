//! # TML Preprocessor
//!
//! This module implements C-style preprocessor directives for TML.
//!
//! ## Supported Directives
//!
//! | Directive           | Description                           |
//! |---------------------|---------------------------------------|
//! | `#if EXPR`          | Conditional compilation               |
//! | `#ifdef SYMBOL`     | If symbol is defined                  |
//! | `#ifndef SYMBOL`    | If symbol is not defined              |
//! | `#elif EXPR`        | Else-if branch                        |
//! | `#else`             | Else branch                           |
//! | `#endif`            | End conditional block                 |
//! | `#define SYMBOL`    | Define a symbol                       |
//! | `#undef SYMBOL`     | Undefine a symbol                     |
//! | `#error "msg"`      | Emit compilation error                |
//! | `#warning "msg"`    | Emit compilation warning              |
//!
//! ## Predefined Symbols
//!
//! ### Operating Systems
//! - `WINDOWS`, `LINUX`, `MACOS`, `ANDROID`, `IOS`, `FREEBSD`
//! - `UNIX` (Linux, macOS, BSD, etc.)
//! - `POSIX` (POSIX-compliant systems)
//!
//! ### Architectures
//! - `X86_64`, `X86`, `ARM64`, `ARM`, `WASM32`, `RISCV64`
//!
//! ### Other
//! - `PTR_32`, `PTR_64` (pointer width)
//! - `LITTLE_ENDIAN`, `BIG_ENDIAN`
//! - `DEBUG`, `RELEASE`, `TEST` (build mode)
//!
//! ## Expression Syntax
//!
//! ```
//! #if WINDOWS && X86_64
//! #if defined(WINDOWS) || defined(LINUX)
//! #if !DEBUG && (WINDOWS || LINUX)
//! ```
//!
//! ## Example
//!
//! ```cpp
//! Preprocessor pp;
//! pp.define("DEBUG");
//! pp.set_target_os(TargetOS::Windows);
//! pp.set_target_arch(TargetArch::X86_64);
//!
//! auto result = pp.process(source);
//! if (!result.errors.empty()) {
//!     // Handle errors
//! }
//! // Use result.output with lexer
//! ```

#ifndef TML_PREPROCESSOR_HPP
#define TML_PREPROCESSOR_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::preprocessor {

// ============================================================================
// Target Platform Types
// ============================================================================

/// Target operating system for conditional compilation.
enum class TargetOS {
    Unknown,
    Windows,
    Linux,
    MacOS,
    Android,
    IOS,
    FreeBSD,
};

/// Target CPU architecture for conditional compilation.
enum class TargetArch {
    Unknown,
    X86_64,
    X86,
    ARM64,
    ARM,
    WASM32,
    RISCV64,
};

/// Target environment/ABI.
enum class TargetEnv {
    Unknown,
    MSVC,
    GNU,
    Musl,
};

/// Build mode for DEBUG/RELEASE/TEST symbols.
enum class BuildMode {
    Debug,
    Release,
    Test,
};

// ============================================================================
// Line Mapping
// ============================================================================

/// Maps output line numbers to original source line numbers.
///
/// After preprocessing removes conditionally excluded code, the line numbers
/// in the output no longer match the original source. This mapping allows
/// error messages to report correct source locations.
struct LineMapping {
    size_t output_line;   ///< Line number in preprocessed output (1-based)
    size_t source_line;   ///< Line number in original source (1-based)
    std::string filename; ///< Source filename (for #include support)
};

// ============================================================================
// Preprocessor Errors and Warnings
// ============================================================================

/// Severity level for preprocessor diagnostics.
enum class DiagnosticSeverity {
    Warning,
    Error,
};

/// A diagnostic message from the preprocessor.
struct PreprocessorDiagnostic {
    DiagnosticSeverity severity;
    std::string message;
    size_t line;   ///< Line number in original source
    size_t column; ///< Column number (1-based)
};

// ============================================================================
// Preprocessor Result
// ============================================================================

/// Result of preprocessing a source file.
struct PreprocessorResult {
    std::string output;                              ///< Preprocessed source code
    std::vector<LineMapping> line_map;               ///< Output-to-source line mapping
    std::vector<PreprocessorDiagnostic> diagnostics; ///< Errors and warnings

    /// Returns true if preprocessing succeeded (no errors).
    [[nodiscard]] bool success() const {
        for (const auto& d : diagnostics) {
            if (d.severity == DiagnosticSeverity::Error) {
                return false;
            }
        }
        return true;
    }

    /// Returns only the error diagnostics.
    [[nodiscard]] std::vector<PreprocessorDiagnostic> errors() const {
        std::vector<PreprocessorDiagnostic> result;
        for (const auto& d : diagnostics) {
            if (d.severity == DiagnosticSeverity::Error) {
                result.push_back(d);
            }
        }
        return result;
    }

    /// Returns only the warning diagnostics.
    [[nodiscard]] std::vector<PreprocessorDiagnostic> warnings() const {
        std::vector<PreprocessorDiagnostic> result;
        for (const auto& d : diagnostics) {
            if (d.severity == DiagnosticSeverity::Warning) {
                result.push_back(d);
            }
        }
        return result;
    }
};

// ============================================================================
// Preprocessor Configuration
// ============================================================================

/// Configuration for the preprocessor.
struct PreprocessorConfig {
    TargetOS target_os = TargetOS::Unknown;
    TargetArch target_arch = TargetArch::Unknown;
    TargetEnv target_env = TargetEnv::Unknown;
    BuildMode build_mode = BuildMode::Debug;
    bool is_64bit = true;
    bool is_little_endian = true;

    /// User-defined symbols from -D flags.
    std::unordered_map<std::string, std::string> defines;
};

// ============================================================================
// Preprocessor
// ============================================================================

/// C-style preprocessor for TML source files.
///
/// The preprocessor handles conditional compilation directives like `#if`,
/// `#ifdef`, `#define`, etc. It runs before the lexer and produces filtered
/// source code.
class Preprocessor {
public:
    /// Constructs a preprocessor with default configuration.
    Preprocessor();

    /// Constructs a preprocessor with the given configuration.
    explicit Preprocessor(const PreprocessorConfig& config);

    // ========================================================================
    // Configuration
    // ========================================================================

    /// Sets the target operating system.
    void set_target_os(TargetOS os);

    /// Sets the target architecture.
    void set_target_arch(TargetArch arch);

    /// Sets the target environment.
    void set_target_env(TargetEnv env);

    /// Sets the build mode (Debug/Release/Test).
    void set_build_mode(BuildMode mode);

    /// Defines a symbol with no value.
    void define(const std::string& symbol);

    /// Defines a symbol with a value.
    void define(const std::string& symbol, const std::string& value);

    /// Undefines a symbol.
    void undefine(const std::string& symbol);

    /// Returns true if a symbol is defined.
    [[nodiscard]] bool is_defined(const std::string& symbol) const;

    /// Gets the value of a defined symbol (empty string if no value).
    [[nodiscard]] std::optional<std::string> get_value(const std::string& symbol) const;

    // ========================================================================
    // Processing
    // ========================================================================

    /// Preprocesses the given source code.
    ///
    /// @param source The source code to preprocess.
    /// @param filename The filename for error messages.
    /// @return The preprocessed result.
    [[nodiscard]] PreprocessorResult process(std::string_view source,
                                             const std::string& filename = "<input>");

    // ========================================================================
    // Target Detection (Static Helpers)
    // ========================================================================

    /// Detects the host operating system.
    [[nodiscard]] static TargetOS detect_host_os();

    /// Detects the host architecture.
    [[nodiscard]] static TargetArch detect_host_arch();

    /// Detects the host environment.
    [[nodiscard]] static TargetEnv detect_host_env();

    /// Parses a target triple string (e.g., "x86_64-unknown-linux-gnu").
    [[nodiscard]] static PreprocessorConfig parse_target_triple(const std::string& triple);

    /// Creates a config for the current host platform.
    [[nodiscard]] static PreprocessorConfig host_config();

private:
    PreprocessorConfig config_;
    std::unordered_set<std::string> defined_symbols_;
    std::unordered_map<std::string, std::string> symbol_values_;

    // Processing state
    struct ProcessingState {
        std::string_view source;
        std::string filename;
        size_t pos = 0;
        size_t line = 1;
        size_t column = 1;
        size_t output_line = 1;

        // Condition stack: true = currently outputting, false = skipping
        std::vector<bool> condition_stack;
        // Track if any branch in current #if chain was taken
        std::vector<bool> branch_taken_stack;

        std::string output;
        std::vector<LineMapping> line_map;
        std::vector<PreprocessorDiagnostic> diagnostics;
    };

    void setup_predefined_symbols();
    void process_impl(ProcessingState& state);

    // Line processing
    void process_line(ProcessingState& state, std::string_view line);
    bool is_directive_line(std::string_view line);
    void process_directive(ProcessingState& state, std::string_view line);
    void output_line(ProcessingState& state, std::string_view line);

    // Directive handlers
    void handle_if(ProcessingState& state, std::string_view expr);
    void handle_ifdef(ProcessingState& state, std::string_view symbol);
    void handle_ifndef(ProcessingState& state, std::string_view symbol);
    void handle_elif(ProcessingState& state, std::string_view expr);
    void handle_else(ProcessingState& state);
    void handle_endif(ProcessingState& state);
    void handle_define(ProcessingState& state, std::string_view rest);
    void handle_undef(ProcessingState& state, std::string_view symbol);
    void handle_error(ProcessingState& state, std::string_view message);
    void handle_warning(ProcessingState& state, std::string_view message);

    // Expression evaluation
    [[nodiscard]] bool evaluate_expression(ProcessingState& state, std::string_view expr);
    [[nodiscard]] bool parse_or_expr(ProcessingState& state, std::string_view& expr);
    [[nodiscard]] bool parse_and_expr(ProcessingState& state, std::string_view& expr);
    [[nodiscard]] bool parse_unary_expr(ProcessingState& state, std::string_view& expr);
    [[nodiscard]] bool parse_primary_expr(ProcessingState& state, std::string_view& expr);

    // Helpers
    [[nodiscard]] bool is_outputting(const ProcessingState& state) const;
    void report_error(ProcessingState& state, const std::string& message);
    void report_warning(ProcessingState& state, const std::string& message);
    static std::string_view trim(std::string_view sv);
    static std::string_view trim_start(std::string_view sv);
    static bool starts_with(std::string_view sv, std::string_view prefix);
    static std::string_view skip_whitespace(std::string_view& sv);
    static std::string_view read_identifier(std::string_view& sv);
};

} // namespace tml::preprocessor

#endif // TML_PREPROCESSOR_HPP
