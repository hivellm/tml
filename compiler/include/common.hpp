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

#include "log/log.hpp"

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
    /// @deprecated Use the logging system (log/log.hpp) instead.
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

    /// Use Polonius borrow checker instead of NLL.
    /// Polonius is more permissive, accepting programs where borrows
    /// are conditionally taken across branches.
    static inline bool polonius = false;

    // THIR pipeline is now the only path (HIR→MIR path removed).
    // use_thir field removed — always true.

    /// Emit overflow-checking intrinsics for integer +, -, * operators.
    /// When enabled, arithmetic overflow panics instead of being UB.
    /// Default: enabled in debug mode (O0), disabled at O1+.
    /// Use --checked-math / --no-checked-math to override.
    static inline bool checked_math = false;

    /// Enable Chrome DevTools Protocol inspector.
    /// When enabled, the compiled program starts a WebSocket server that
    /// speaks CDP and prints a devtools:// URL to stderr.
    static inline bool inspect = false;

    /// Break before user code (for --inspect-brk).
    /// When enabled, the program pauses and waits for a debugger to connect.
    static inline bool inspect_brk = false;

    /// Inspector WebSocket server port (default: 9229).
    static inline int inspect_port = 9229;

    /// Print per-function MIR→LLVM lowering time to stderr.
    /// Enabled by `--debug-codegen-timing`. Used to diagnose codegen
    /// timeouts caused by pathological functions (e.g., large `when`
    /// chains, large match dispatch) without needing a profiler.
    static inline bool debug_codegen_timing = false;

    /// Print the set of functions kept and removed by the
    /// dead_function_elimination MIR pass. Enabled by
    /// `--dump-dead-functions`. Used to diagnose why a slow or unused
    /// function is still being lowered in a given binary.
    static inline bool dump_dead_functions = false;
};

// ============================================================================
// Debug Macros (backward compatibility — delegates to unified logger)
// ============================================================================

/// Outputs a debug message via the unified logging system.
/// @deprecated Use TML_LOG_DEBUG("module", msg) directly.
#define TML_DEBUG(msg) TML_LOG_DEBUG("compiler", msg)

/// Outputs a debug message with newline via the unified logging system.
/// @deprecated Use TML_LOG_DEBUG("module", msg) directly.
#define TML_DEBUG_LN(msg) TML_LOG_DEBUG("compiler", msg)

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

/// Ownership fact exported by the borrow checker for a single named binding.
///
/// The borrow checker computes precise move/init state for every place but
/// historically discarded it (`provide_borrowcheck_module` returned only
/// success+errors). phase26b Step 2 exports these facts so the AST codegen can
/// suppress drops of moved-out bindings instead of relying on the syntactic
/// `consumed_vars_` set.
///
/// **Join key:** the binding's definition `SourceSpan` (`def_span`). Both the
/// borrow checker (`PlaceState.definition.span`, set from `let.span`) and the
/// AST codegen (`let.span` at each `register_for_drop` site) observe the SAME
/// cached parsed module, so the spans are byte-identical and uniquely identify
/// a binding (distinguishing shadowed names that a bare-name key cannot).
///
/// Granularity (i) only: `moved_out` is the monotonic "ever fully moved in the
/// function" verdict (an end-of-function snapshot of `OwnershipState::Moved`).
/// `moved_projections` (partial moves) and `conditional` (branch-dependent
/// state) are deferred to phase26b Step 4.
struct PlaceOwnershipFact {
    SourceSpan def_span;     ///< Definition span of the binding — the join key.
    std::string name;        ///< Source variable name (secondary/debug key).
    bool moved_out = false;  ///< True if the place was moved out (OwnershipState::Moved).
    bool initialized = true; ///< Whether the place is initialized at end of function.
    /// True if the place is moved on SOME control-flow paths but not all
    /// (conditional / branch-dependent move). Computed at `if`/`when`/`loop`
    /// merge points from per-branch ownership snapshots (phase26f 1.5). When set,
    /// `moved_out` is also true (the monotonic "ever moved" verdict is a superset),
    /// but the drop must be guarded by a runtime drop-flag rather than statically
    /// suppressed — otherwise the path where the move did NOT happen leaks.
    bool conditionally_moved = false;
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
