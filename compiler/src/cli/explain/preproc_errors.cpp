TML_MODULE("compiler")

//! # Preprocessor Error Explanations
//!
//! Error codes PP001-PP002 for conditional compilation errors.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_preprocessor_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"PP001", R"EX(
Preprocessor error [PP001]

An error occurred during preprocessing of conditional compilation directives
(`#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`, `#define`).

This usually means a directive is malformed or has a syntax error.

Example of erroneous code:

    #if                          // missing condition
    func platform_code() { }
    #endif

    #ifdef                       // missing symbol name
    func debug_code() { }
    #endif

How to fix:

    #if WINDOWS
    func platform_code() { }
    #endif

    #ifdef DEBUG
    func debug_code() { }
    #endif

Common causes:
- Missing condition after `#if`
- Missing symbol name after `#ifdef` or `#ifndef`
- Mismatched `#if`/`#endif` pairs
- Unknown preprocessor directive
)EX"},

        {"PP002", R"EX(
Preprocessor warning [PP002]

A non-fatal issue was detected during preprocessing. The code will still
compile, but the warning indicates a potential problem.

Common causes:
- Redefining an already-defined symbol with `#define`
- Using `#elif` or `#else` after an `#else` block
- Empty `#if` blocks that could be simplified

These warnings can usually be resolved by cleaning up the conditional
compilation structure.
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
