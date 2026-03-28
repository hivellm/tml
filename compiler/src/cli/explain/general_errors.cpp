TML_MODULE("compiler")

//! # General Error Explanations
//!
//! Error codes E001-E006 for general compiler errors.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_general_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"E001", R"EX(
File not found [E001]

The specified source file does not exist or cannot be read.

Example:

    tml build nonexistent.tml   // file does not exist

How to fix:

1. Check the file path for typos
2. Verify the file exists: the path is relative to the current directory
3. Check file permissions
)EX"},

        {"E002", R"EX(
I/O error [E002]

An error occurred while reading or writing a file. This can happen due
to permission issues, disk full, or corrupted files.

How to fix:

1. Check file permissions
2. Verify disk space is available
3. Check that the output directory exists
)EX"},

        {"E003", R"EX(
Internal compiler error [E003]

An unexpected internal error occurred in the compiler. This is a bug
in the TML compiler.

Please report this issue with:
1. The TML source file that triggered the error
2. The compiler version (`tml --version`)
3. The full error output
)EX"},

        {"E004", R"EX(
Command error [E004]

An invalid CLI command or arguments were provided. The command does not
exist or was invoked with incorrect options.

Example:

    tml bild main.tml          // 'bild' is not a valid command

How to fix:

    tml build main.tml         // correct command name

Available commands:

    tml build    Build a TML source file
    tml run      Build and run a TML source file
    tml test     Run tests
    tml check    Type check without compiling
    tml fmt      Format source files
    tml lint     Lint source files
    tml doc      Generate documentation
    tml explain  Explain an error code
    tml init     Initialize a new project

Use `tml --help` to see all available commands and options.
)EX"},

        {"E005", R"EX(
Configuration error [E005]

An error was found in the project configuration file (tml.toml) or
build configuration.

Common causes:

1. Malformed TOML syntax in tml.toml
2. Invalid configuration key or value
3. Incompatible configuration options

How to fix:

1. Check tml.toml for syntax errors
2. Verify configuration keys are spelled correctly
3. Refer to the documentation for valid configuration options

Example tml.toml:

    [project]
    name = "my_project"
    version = "1.0.0"

    [build]
    optimize = "O2"
)EX"},

        {"E006", R"EX(
Dependency error [E006]

An error occurred during dependency resolution. A required module,
library, or package could not be found or has conflicting versions.

Common causes:

1. A `use` statement references a module that is not installed
2. Circular dependencies between modules
3. Version conflict between dependencies
4. Missing standard library modules

How to fix:

1. Verify the module path is correct in `use` statements
2. Check that required packages are installed
3. Resolve version conflicts by updating dependencies
4. Ensure the standard library is properly installed

    use std::collections::HashMap    // requires std library
    use my_lib::utils                // requires my_lib package
)EX"},

        {"W001", R"EX(
Unused variable / Interior mutability warning [W001]

This code is W001, used for two distinct warnings:

1. **Unused variable** (from linter): A variable was declared but never read.
   Prefix the variable name with `_` to suppress: `let _unused = ...`

2. **Interior mutability** (from borrow checker): A type uses interior
   mutability (e.g., `Sync[T]`, `Cell[T]`) which bypasses borrow checking.
   This is safe when used correctly but requires care.

Related: W002 (unused import), W003 (unused function)
)EX"},

        {"W002", R"EX(
Unused import [W002]

A `use` declaration imports a module or item that is never referenced
in the file.

Example:

    use std::collections::HashMap   // imported but never used

How to fix: remove the unused import, or use it.

Related: W001 (unused variable), W003 (unused function)
)EX"},

        {"W003", R"EX(
Unused function [W003]

A function is declared but never called anywhere in the module.

Example:

    func helper() -> I32 { 42 }  // defined but never called

How to fix: remove the function if it is no longer needed, or call it.
For public API functions that are intentionally unused locally, this
warning may be suppressed.

Related: W001 (unused variable), W004 (unused parameter)
)EX"},

        {"W004", R"EX(
Unused parameter [W004]

A function parameter is declared but never used in the function body.

Example:

    func foo(x: I32, unused: I32) -> I32 { x }  // 'unused' never read

How to fix: prefix the parameter name with `_` to suppress the warning:

    func foo(x: I32, _unused: I32) -> I32 { x }

Or remove the parameter if the function signature allows it.

Related: W001 (unused variable)
)EX"},

        {"W005", R"EX(
Unused variable (compiler warning) [W005]

A variable was declared but its value is never read. Unlike W001 (linter),
this warning is emitted directly by the compiler during semantic analysis.

Example:

    let x = compute_value();  // x is never used after this

How to fix:
- Remove the variable if the value is not needed
- Prefix with `_` to signal intentional non-use: `let _x = ...`
- Use the value somewhere in the code

Related: W001 (linter unused variable), W006 (unused import)
)EX"},

        {"W006", R"EX(
Unused import (compiler warning) [W006]

A `use` declaration was found but the imported item is never referenced.
This is the compiler-level equivalent of the W002 linter warning.

Example:

    use std::io::File   // never used in this module

How to fix: remove unused imports.

Related: W002 (linter unused import)
)EX"},

        {"W007", R"EX(
Unreachable code [W007]

Code appears after a `return`, `break`, `continue`, or other statement
that always transfers control away. The code after it can never execute.

Example:

    func foo() -> I32 {
        return 42;
        let x = 10;  // unreachable
    }

How to fix: remove the unreachable code, or restructure the logic so
the code can be reached.

Related: W008 (shadowed variable)
)EX"},

        {"W008", R"EX(
Shadowed variable [W008]

A new variable declaration uses the same name as a variable already
in scope. The outer variable is hidden (shadowed) by the inner one.

Example:

    let x = 10;
    let x = 20;  // shadows the first x

While shadowing is sometimes intentional (type narrowing, rebinding),
it can be a source of confusion. Consider using distinct names.

Related: W007 (unreachable code)
)EX"},

        {"W009", R"EX(
Deprecated item used [W009]

You are using a function, type, or module marked as `@deprecated`.
It may be removed in a future version.

Check the deprecation message for the recommended replacement.

Related: W010 (implicit narrowing)
)EX"},

        {"W010", R"EX(
Implicit numeric narrowing [W010]

A numeric value is implicitly converted to a smaller type, which may
lose information if the value exceeds the target type's range.

Example:

    let x: I64 = large_value();
    let y: I32 = x;  // implicit narrowing: may lose bits

How to fix: use an explicit cast to acknowledge the narrowing:

    let y: I32 = x as I32;

Or use a checked conversion that returns Maybe[I32].

Related: T005 (type mismatch), W011 (possible division by zero)
)EX"},

        {"W011", R"EX(
Possible division by zero [W011]

A division or modulo operation may divide by zero at runtime. The
divisor could be zero based on static analysis.

Example:

    func divide(a: I32, b: I32) -> I32 { a / b }  // b could be 0

How to fix: add a runtime check:

    if b == 0 { return 0; }
    a / b

Related: W010 (implicit narrowing)
)EX"},

        {"W012", R"EX(
Unused function (compiler warning) [W012]

A function is declared but never called. This is the compiler-level
equivalent of the W003 linter warning.

Related: W003 (linter unused function), W001 (unused variable)
)EX"},

        {"W013", R"EX(
Empty match arm [W013]

A `when` (match) arm has an empty body `{}`. This may be intentional
(silently ignoring a case) but is often a mistake.

Example:

    when value {
        Some(x) => { process(x) },
        Nothing => {},  // intentionally empty, but suspicious
    }

If intentional, consider adding a comment explaining why the arm
is empty.

Related: W007 (unreachable code)
)EX"},

        {"W014", R"EX(
Contract always true/false [W014]

A contract condition (assert, require, ensure) is a compile-time
constant that always evaluates to true or false.

Example:

    assert(true);   // W014: always true, no-op
    assert(false);  // W014: always false, always panics

How to fix: remove trivial contracts, or replace with meaningful conditions.

Related: W011 (division by zero)
)EX"},

        {"W015", R"EX(
Large stack allocation [W015]

A local variable or array allocation is very large (typically >1MB).
Placing large values on the stack risks a stack overflow.

Example:

    let buffer: [U8; 2_000_000] = [0; 2_000_000];  // 2MB on stack

How to fix: allocate large values on the heap instead:

    let buffer = List::with_capacity(2_000_000);

Related: W010 (implicit narrowing)
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
