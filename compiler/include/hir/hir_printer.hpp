//! # HIR Pretty Printer
//!
//! This module implements pretty printing for HIR structures. The printer
//! produces human-readable output useful for debugging, testing, and
//! understanding the lowered representation.
//!
//! ## Overview
//!
//! The `HirPrinter` class converts HIR structures into formatted text that
//! resembles TML source code, with additional type annotations and structural
//! information.
//!
//! ## Usage
//!
//! ```cpp
//! #include "hir/hir_printer.hpp"
//!
//! // Print with colors (for terminal output)
//! HirPrinter printer(true);
//! std::cout << printer.print_module(module);
//!
//! // Print without colors (for file output or testing)
//! HirPrinter plain_printer(false);
//! std::string output = plain_printer.print_module(module);
//! ```
//!
//! ## Output Format
//!
//! The printer produces output that looks like TML source, with some
//! differences:
//! - Types are always explicit (no inference)
//! - Desugared forms are shown (var → let mut)
//! - Mangled names may appear for monomorphized items
//!
//! ## Example Output
//!
//! For this TML code:
//! ```tml
//! type Point { x: I32, y: I32 }
//!
//! func add_points(a: Point, b: Point) -> Point {
//!     return Point { x: a.x + b.x, y: a.y + b.y }
//! }
//! ```
//!
//! The printer produces:
//! ```
//! module test
//!
//! struct Point {
//!     x: I32
//!     y: I32
//! }
//!
//! func add_points(a: Point, b: Point) -> Point {
//!     return Point {
//!         x: (a.x + b.x),
//!         y: (a.y + b.y)
//!     }
//! }
//! ```
//!
//! ## Compiler Integration
//!
//! Use the `--emit-hir` flag to dump HIR during compilation:
//! ```bash
//! tml build file.tml --emit-hir
//! ```
//!
//! ## See Also
//!
//! - `docs/specs/31-HIR.md` - Complete HIR documentation
//! - `hir_module.hpp` - Module structure being printed
//! - `hir_expr.hpp` - Expression types being formatted

#pragma once

#include "hir/hir_id.hpp"
#include "hir/hir_module.hpp"

#include <string>

namespace tml::hir {

// ============================================================================
// HIR Pretty Printer
// ============================================================================

/// Pretty prints HIR for debugging and inspection.
///
/// The printer walks HIR structures and produces formatted text output.
/// It supports optional ANSI color codes for terminal display.
///
/// ## Color Scheme
///
/// When colors are enabled, the printer uses:
/// - **Keywords** (blue): `func`, `let`, `type`, etc.
/// - **Type names** (green): `I32`, `Bool`, `Point`, etc.
/// - **Literals** (yellow): `42`, `"hello"`, `true`
/// - **Comments** (gray): Internal annotations
///
/// ## Indentation
///
/// The printer maintains proper indentation for nested structures:
/// - Function bodies
/// - Block expressions
/// - Struct/enum fields
/// - Match arms
///
/// ## Thread Safety
///
/// A single `HirPrinter` instance is not thread-safe. Create separate
/// instances for concurrent printing.
class HirPrinter {
public:
    /// Construct a printer.
    ///
    /// @param use_colors If true, include ANSI color codes in output.
    ///                   Use true for terminal, false for files/testing.
    explicit HirPrinter(bool use_colors = false);

    // ========================================================================
    // Printing Methods
    // ========================================================================

    /// Print a complete module.
    ///
    /// Outputs all declarations in the module in a readable format.
    ///
    /// @param module The module to print
    /// @return Formatted string representation
    auto print_module(const HirModule& module) -> std::string;

    /// Print a function declaration.
    ///
    /// @param func The function to print
    /// @return Formatted string representation
    auto print_function(const HirFunction& func) -> std::string;

    /// Print a struct declaration.
    ///
    /// @param s The struct to print
    /// @return Formatted string representation
    auto print_struct(const HirStruct& s) -> std::string;

    /// Print an enum declaration.
    ///
    /// @param e The enum to print
    /// @return Formatted string representation
    auto print_enum(const HirEnum& e) -> std::string;

    /// Print an expression.
    ///
    /// @param expr The expression to print
    /// @return Formatted string representation
    auto print_expr(const HirExpr& expr) -> std::string;

    /// Print a statement.
    ///
    /// @param stmt The statement to print
    /// @return Formatted string representation
    auto print_stmt(const HirStmt& stmt) -> std::string;

    /// Print a pattern.
    ///
    /// @param pattern The pattern to print
    /// @return Formatted string representation
    auto print_pattern(const HirPattern& pattern) -> std::string;

    /// Print a type.
    ///
    /// @param type The type to print
    /// @return Formatted string representation
    auto print_type(const HirType& type) -> std::string;

private:
    /// Whether to include ANSI color codes.
    bool use_colors_;

    /// Current indentation level (in spaces).
    int indent_ = 0;

    // ========================================================================
    // Formatting Helpers
    // ========================================================================

    /// Get the current indentation string.
    /// @return String of spaces for current indent level
    auto indent() -> std::string;

    /// Increase indentation level.
    void push_indent();

    /// Decrease indentation level.
    void pop_indent();

    // ========================================================================
    // Color Helpers
    // ========================================================================

    /// Format a keyword with optional color.
    /// @param s The keyword text
    /// @return Colored (or plain) keyword string
    auto keyword(const std::string& s) -> std::string;

    /// Format a type name with optional color.
    /// @param s The type name
    /// @return Colored (or plain) type name string
    auto type_name(const std::string& s) -> std::string;

    /// Format a literal with optional color.
    /// @param s The literal text
    /// @return Colored (or plain) literal string
    auto literal(const std::string& s) -> std::string;

    /// Format a comment with optional color.
    /// @param s The comment text
    /// @return Colored (or plain) comment string
    auto comment(const std::string& s) -> std::string;
};

/// Convenience function for printing a module.
///
/// Creates a temporary printer and prints the module. For repeated
/// printing, create a `HirPrinter` instance instead.
///
/// @param module The module to print
/// @param use_colors Whether to include ANSI color codes
/// @return Formatted string representation
///
/// ## Example
/// ```cpp
/// HirModule module = builder.lower_module(ast);
/// std::cout << print_hir_module(module, true);
/// ```
inline auto print_hir_module(const HirModule& module, bool use_colors = false) -> std::string {
    HirPrinter printer(use_colors);
    return printer.print_module(module);
}

} // namespace tml::hir
