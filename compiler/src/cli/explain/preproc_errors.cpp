TML_MODULE("compiler")

//! # Preprocessor Error Explanations
//!
//! Error codes PP001-PP010 for conditional compilation errors.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_preprocessor_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"PP001", R"EX(
Unknown preprocessor directive [PP001]

The preprocessor encountered a `#` directive it does not recognize.

Example of erroneous code:

    #include "file.tml"      // not supported
    #pragma once             // not supported

How to fix:

Use only supported directives: `#if`, `#ifdef`, `#ifndef`, `#elif`,
`#else`, `#endif`, `#define`, `#undef`, `#error`, `#warning`.
)EX"},

        {"PP002", R"EX(
Unterminated #if block [PP002]

A `#if`, `#ifdef`, or `#ifndef` directive was not closed with `#endif`
before the end of the file.

Example of erroneous code:

    #if WINDOWS
    func platform_code() { }
    // missing #endif

How to fix:

    #if WINDOWS
    func platform_code() { }
    #endif

Every `#if` / `#ifdef` / `#ifndef` must have a matching `#endif`.
)EX"},

        {"PP003", R"EX(
#elif without matching #if [PP003]

A `#elif` directive was encountered without a preceding `#if`, `#ifdef`,
or `#ifndef`.

Example of erroneous code:

    #elif LINUX               // no matching #if
    func linux_code() { }
    #endif

How to fix:

    #if WINDOWS
    func windows_code() { }
    #elif LINUX
    func linux_code() { }
    #endif
)EX"},

        {"PP004", R"EX(
#else without matching #if [PP004]

A `#else` directive was encountered without a preceding `#if`, `#ifdef`,
or `#ifndef`.

Example of erroneous code:

    #else                     // no matching #if
    func fallback() { }
    #endif

How to fix:

    #if WINDOWS
    func windows_code() { }
    #else
    func fallback() { }
    #endif
)EX"},

        {"PP005", R"EX(
#endif without matching #if [PP005]

A `#endif` directive was encountered without a preceding `#if`, `#ifdef`,
or `#ifndef`.

Example of erroneous code:

    func my_func() { }
    #endif                    // no matching #if

How to fix:

Remove the extra `#endif`, or add the corresponding `#if` block:

    #if DEBUG
    func debug_func() { }
    #endif
)EX"},

        {"PP006", R"EX(
#define missing symbol name [PP006]

A `#define` directive was used without specifying a symbol name.

Example of erroneous code:

    #define                   // missing symbol name

How to fix:

    #define MY_FLAG           // define a boolean symbol
    #define VERSION 2         // define a symbol with a value
)EX"},

        {"PP007", R"EX(
#undef missing symbol name [PP007]

A `#undef` directive was used without specifying a symbol name.

Example of erroneous code:

    #undef                    // missing symbol name

How to fix:

    #undef MY_FLAG            // remove a previously defined symbol
)EX"},

        {"PP008", R"EX(
#error directive [PP008]

The source code contains a `#error` directive that was reached during
preprocessing. This is intentional — the file uses `#error` to fail
compilation under specific conditions.

Example:

    #if !WINDOWS && !LINUX
    #error "This code only supports Windows and Linux"
    #endif

How to fix:

Ensure the required platform or configuration symbols are defined, or
modify the conditional compilation guards if the constraint is wrong.
)EX"},

        {"PP009", R"EX(
Invalid preprocessor expression [PP009]

The preprocessor encountered an invalid or incomplete expression in a
`#if` or `#elif` condition.

Example of erroneous code:

    #if                       // missing condition
    #if defined()             // missing symbol name in defined()
    #if &&WINDOWS             // unexpected operator

How to fix:

    #if WINDOWS               // simple symbol check
    #if defined(WINDOWS)      // explicit defined() check
    #if WINDOWS && X86_64     // combined conditions

Supported operators: `&&` (or `and`), `||` (or `or`), `!` (or `not`),
`defined(SYMBOL)`, parentheses for grouping.
)EX"},

        {"PP010", R"EX(
Missing closing parenthesis in preprocessor expression [PP010]

A `(` in a preprocessor expression was not closed with `)`.

Example of erroneous code:

    #if (WINDOWS && X86_64    // missing closing )
    #if defined(WINDOWS       // missing closing ) after symbol

How to fix:

    #if (WINDOWS && X86_64)   // close the parenthesized expression
    #if defined(WINDOWS)      // close the defined() call
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
