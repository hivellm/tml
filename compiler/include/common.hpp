//! # Common Definitions
//!
//! This module provides common types, utilities, and constants used throughout
//! the TML compiler. It establishes the foundational abstractions that all
//! other compiler components depend on.
//!
//! ## Overview
//!
//! The common module includes:
//!
//! - **Version Information**: Compiler version constants
//! - **Compiler Options**: Global configuration for compilation
//! - **Source Locations**: Types for tracking source code positions
//! - **Result Type**: Error handling without exceptions
//! - **Smart Pointers**: Aliases for unique and shared pointers
//!
//! ## Design Philosophy
//!
//! TML follows these principles in its internal API:
//!
//! - **No Exceptions**: All errors are returned via `Result<T, E>`
//! - **Explicit Ownership**: Use `Box<T>` for unique ownership, `Rc<T>` for shared
//! - **Compile-time Safety**: Prefer constexpr and type safety over runtime checks

#ifndef TML_COMMON_HPP
#define TML_COMMON_HPP

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace tml {

// ============================================================================
// Version Information
// ============================================================================

/// The compiler version string (e.g., "0.1.0").
constexpr const char* VERSION = "0.1.0";

/// Major version number.
constexpr int VERSION_MAJOR = 0;

/// Minor version number.
constexpr int VERSION_MINOR = 1;

/// Patch version number.
constexpr int VERSION_PATCH = 0;

// ============================================================================
// Compiler Configuration
// ============================================================================

/// Warning severity levels for compiler diagnostics.
///
/// These levels correspond to common compiler warning flags and control
/// which warnings are emitted during compilation.
enum class WarningLevel {
    None = 0, ///< Suppress all warnings
    Default,  ///< Default warnings only
    Extra,    ///< Extra warnings (like `-Wextra`)
    All,      ///< All warnings (like `-Wall`)
    Pedantic  ///< Pedantic warnings (like `-Wpedantic`)
};

/// Output format for compiler diagnostics.
enum class DiagnosticFormat {
    Text, ///< Human-readable text output (default)
    JSON  ///< Machine-readable JSON output for IDE integration
};

/// Global compiler configuration options.
///
/// These options affect all compilation operations and can be set via
/// command-line flags or programmatically.
///
/// # Example
///
/// ```cpp
/// CompilerOptions::verbose = true;
/// CompilerOptions::optimization_level = 2;
/// CompilerOptions::debug_info = true;
/// ```
struct CompilerOptions {
    /// Enable verbose/debug output to stderr.
    static inline bool verbose = false;

    /// Optimization level: 0-3 for O0-O3, 4 for Os, 5 for Oz.
    static inline int optimization_level = 0;

    /// Include DWARF debug information in output.
    static inline bool debug_info = false;

    /// Debug info detail level: 0=none, 1=minimal, 2=standard, 3=full.
    static inline int debug_level = 0;

    /// Target triple for cross-compilation (empty = host system).
    static inline std::string target_triple;

    /// Sysroot path for cross-compilation.
    static inline std::string sysroot;

    /// Warning level for diagnostics.
    static inline WarningLevel warning_level = WarningLevel::Default;

    /// Treat warnings as errors (`-Werror`).
    static inline bool warnings_as_errors = false;

    /// Output format for diagnostics.
    static inline DiagnosticFormat diagnostic_format = DiagnosticFormat::Text;

    /// Enable code coverage instrumentation (function-level tracking).
    static inline bool coverage = false;

    /// Output path for coverage HTML report (function-level).
    static inline std::string coverage_output;

    /// Enable LLVM source code coverage instrumentation.
    /// This enables -fprofile-instr-generate and -fcoverage-mapping for
    /// line-by-line coverage analysis using llvm-cov.
    static inline bool coverage_source = false;

    /// Output directory for LLVM source coverage reports.
    static inline std::string coverage_source_dir = "coverage";

    /// Enable memory leak checking at runtime.
    /// When enabled, compiled programs track all allocations and report
    /// unfreed memory at exit. Always enabled in debug builds by default.
    static inline bool check_leaks = true;

    /// Force use of external tools (clang, system linker) instead of
    /// built-in LLVM backend and LLD. Useful for debugging or when
    /// the self-contained tools are not available.
    static inline bool use_external_tools = false;

    /// Enable runtime profiling instrumentation.
    /// When enabled, the compiled program generates a .cpuprofile file
    /// that can be loaded in Chrome DevTools or VS Code.
    static inline bool profile = false;

    /// Output path for the .cpuprofile file.
    /// Default: "profile.cpuprofile"
    static inline std::string profile_output = "profile.cpuprofile";

    /// Enable backtrace printing on panic.
    /// When enabled, the runtime prints a stack trace when panic() or assert() fails.
    static inline bool backtrace = false;
};

// ============================================================================
// Debug Macros
// ============================================================================

/// Outputs a debug message to stderr if verbose mode is enabled.
///
/// This macro is a no-op when `CompilerOptions::verbose` is false.
#define TML_DEBUG(msg)                                                                             \
    do {                                                                                           \
        if (::tml::CompilerOptions::verbose) {                                                     \
            std::cerr << msg;                                                                      \
        }                                                                                          \
    } while (0)

/// Outputs a debug message with newline to stderr if verbose mode is enabled.
#define TML_DEBUG_LN(msg)                                                                          \
    do {                                                                                           \
        if (::tml::CompilerOptions::verbose) {                                                     \
            std::cerr << msg << "\n";                                                              \
        }                                                                                          \
    } while (0)

// ============================================================================
// Source Location Types
// ============================================================================

/// A precise location in source code.
///
/// `SourceLocation` identifies a specific position in a source file,
/// used for error reporting and source mapping.
///
/// # Fields
///
/// - `file`: Path to the source file
/// - `line`: 1-based line number
/// - `column`: 1-based column number
/// - `offset`: 0-based byte offset from file start
/// - `length`: Length of the source element in bytes
struct SourceLocation {
    /// Path to the source file.
    std::string_view file;

    /// Line number (1-based).
    uint32_t line;

    /// Column number (1-based).
    uint32_t column;

    /// Byte offset from start of file (0-based).
    uint32_t offset;

    /// Length of the source element in bytes.
    uint32_t length;

    [[nodiscard]] auto operator==(const SourceLocation& other) const -> bool = default;
};

/// A span of source code from start to end location.
///
/// `SourceSpan` represents a contiguous region of source code, typically
/// corresponding to a single AST node or token sequence.
struct SourceSpan {
    /// Start location of the span.
    SourceLocation start;

    /// End location of the span.
    SourceLocation end;

    /// Merges two spans into one that covers both.
    ///
    /// The result spans from the start of `a` to the end of `b`.
    [[nodiscard]] static auto merge(const SourceSpan& a, const SourceSpan& b) -> SourceSpan {
        return {a.start, b.end};
    }
};

// ============================================================================
// Result Type
// ============================================================================

/// A type that represents either a success value or an error.
///
/// `Result<T, E>` is used for operations that can fail, allowing error
/// handling without exceptions. This follows the Rust convention.
///
/// # Example
///
/// ```cpp
/// Result<int, std::string> parse_int(std::string_view s) {
///     // ... parsing logic ...
///     if (error) return "invalid integer";
///     return value;
/// }
///
/// auto result = parse_int("42");
/// if (is_ok(result)) {
///     int value = unwrap(result);
/// }
/// ```
template <typename T, typename E = std::string> using Result = std::variant<T, E>;

/// Checks if a Result contains a success value.
template <typename T, typename E>
[[nodiscard]] constexpr auto is_ok(const Result<T, E>& result) -> bool {
    return std::holds_alternative<T>(result);
}

/// Checks if a Result contains an error.
template <typename T, typename E>
[[nodiscard]] constexpr auto is_err(const Result<T, E>& result) -> bool {
    return std::holds_alternative<E>(result);
}

/// Extracts the success value from a Result.
///
/// # Panics
///
/// Throws `std::bad_variant_access` if the Result contains an error.
template <typename T, typename E> [[nodiscard]] constexpr auto unwrap(Result<T, E>& result) -> T& {
    return std::get<T>(result);
}

/// Extracts the success value from a const Result.
template <typename T, typename E>
[[nodiscard]] constexpr auto unwrap(const Result<T, E>& result) -> const T& {
    return std::get<T>(result);
}

/// Extracts the error value from a Result.
///
/// # Panics
///
/// Throws `std::bad_variant_access` if the Result contains a success value.
template <typename T, typename E>
[[nodiscard]] constexpr auto unwrap_err(Result<T, E>& result) -> E& {
    return std::get<E>(result);
}

/// Extracts the error value from a const Result.
template <typename T, typename E>
[[nodiscard]] constexpr auto unwrap_err(const Result<T, E>& result) -> const E& {
    return std::get<E>(result);
}

// ============================================================================
// Smart Pointer Aliases
// ============================================================================

/// Unique ownership pointer (like Rust's `Box<T>`).
///
/// `Box<T>` represents unique ownership of a heap-allocated value.
/// When the Box is dropped, the value is deallocated.
template <typename T> using Box = std::unique_ptr<T>;

/// Reference-counted shared pointer (like Rust's `Rc<T>`).
///
/// `Rc<T>` allows multiple owners of the same heap-allocated value.
/// The value is deallocated when the last Rc is dropped.
template <typename T> using Rc = std::shared_ptr<T>;

/// Creates a new Box containing the given value.
///
/// # Example
///
/// ```cpp
/// auto ptr = make_box<MyStruct>(arg1, arg2);
/// ```
template <typename T, typename... Args> [[nodiscard]] auto make_box(Args&&... args) -> Box<T> {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

/// Creates a new Rc containing the given value.
///
/// # Example
///
/// ```cpp
/// auto ptr = make_rc<MyStruct>(arg1, arg2);
/// ```
template <typename T, typename... Args> [[nodiscard]] auto make_rc(Args&&... args) -> Rc<T> {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

} // namespace tml

#endif // TML_COMMON_HPP
