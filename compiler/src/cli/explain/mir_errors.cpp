TML_MODULE("compiler")

//! # MIR Building Error Explanations
//!
//! Error codes M001-M020 for MIR (Mid-level IR) building errors.
//! These are internal compiler diagnostics emitted when the MIR builder
//! encounters invalid or unsupported constructs.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_mir_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"M001", R"EX(
Break outside of loop [M001]

A `break` expression was encountered outside of a loop body during MIR
construction. This is an internal compiler error — the parser and type
checker should have caught this before MIR building.

If you see this error, it likely indicates a desugaring pass produced
invalid HIR. Please report it as a compiler bug.

Related: M002 (continue outside of loop)
)EX"},

        {"M002", R"EX(
Continue outside of loop [M002]

A `continue` expression was encountered outside of a loop body during
MIR construction. This is an internal compiler error — the parser and
type checker should have caught this before MIR building.

If you see this error, it likely indicates a desugaring pass produced
invalid HIR. Please report it as a compiler bug.

Related: M001 (break outside of loop)
)EX"},

        {"M003", R"EX(
No current function in MIR builder [M003]

The MIR builder attempted to emit an instruction but no function is
currently being built. This is an internal compiler invariant violation.

Common causes:
1. An expression was lowered outside a function body
2. The function context was dropped prematurely
3. A builder method was called after the function was finalized

This is always a compiler bug. Please report it with the source file
that triggered this error.
)EX"},

        {"M004", R"EX(
Invalid current block in MIR builder [M004]

The MIR builder attempted to emit an instruction but the current basic
block is invalid or does not exist. This is an internal compiler error.

Common causes:
1. A block ID was used after the block was removed or replaced
2. A branch was emitted to a non-existent block
3. The block was not created before switching to it

This is always a compiler bug. Please report it with the source file
that triggered this error.

Related: M003 (no current function)
)EX"},

        {"M005", R"EX(
MIR verification failed: undefined value [M005]

After a MIR optimization pass, a value was found to be used before it
was defined (SSA violation). This indicates that a MIR pass produced
incorrect output.

The error message includes:
- Which pass ran before the failure was detected
- Which undefined value ID was used
- Which function and block contain the bad use

This is always a compiler bug in the named MIR pass. Please report it.

Related: M006 (pass corrupted MIR)
)EX"},

        {"M006", R"EX(
MIR pass corrupted the module [M006]

A MIR optimization pass ran and MIR verification detected that the
resulting module is invalid. The pass introduced an inconsistency that
violates MIR invariants.

This error is only emitted in debug builds (when `debug_mir` is enabled).
The error message identifies which pass caused the corruption.

This is always a compiler bug in the named MIR pass. Please report it
along with the source file and the pass name.

Related: M005 (undefined value after pass)
)EX"},

        {"M007", R"EX(
Potential infinite loop detected [M007]

Static analysis detected a loop that may never terminate. This is a
warning, not an error — the program will still compile.

The warning is emitted when:
1. A loop has no exit path (no break, return, or branch out of the loop)
2. AND the loop condition is always true, OR the condition variables are
   never modified inside the loop

Example of suspicious code:

    let x = 0
    loop (x < 10) {
        // x is never modified — loop runs forever
        do_something()
    }

How to fix: ensure the loop condition can become false, or add a break:

    let mut x = 0
    loop (x < 10) {
        do_something()
        x = x + 1    // x is modified — loop will terminate
    }
)EX"},

        {"M008", R"EX(
Potential memory leak detected [M008]

Static analysis detected a heap allocation that may not be freed before
the function returns. This is a warning (or error in strict mode).

The analysis checks that every allocation via `mem_alloc` has a
corresponding `mem_free` on all paths through the function.

Example of suspicious code:

    func leaky() {
        let ptr = lowlevel { mem_alloc(64) }
        if condition {
            return   // ptr is never freed on this path
        }
        lowlevel { mem_free(ptr) }
    }

How to fix: ensure `mem_free` is called on all exit paths, or use
owned types (`Heap[T]`, `Buffer`) that free automatically on drop.

Note: this analysis is conservative and may produce false positives for
code that intentionally leaks or transfers ownership through opaque means.
)EX"},

        {"M009", R"EX(
Unsupported statement in MIR builder [M009]

The MIR builder encountered a statement kind that it does not know how
to lower to MIR instructions. This indicates either an unsupported
language feature or an internal compiler issue.

If you see this error on valid TML code, please report it as a bug.
)EX"},

        {"M010", R"EX(
Type mismatch in MIR building [M010]

During MIR construction, a type mismatch was detected between the
expected and actual types of a value or expression. This typically
indicates a bug in the type checker or HIR lowering that allowed an
incorrectly typed expression through.

If this appears on code that passes the type checker, please report it
as a compiler bug.

Related: M003 (no current function), M004 (invalid block)
)EX"},

        {"M011", R"EX(
Undefined variable in MIR builder [M011]

The MIR builder could not find a variable in the current scope during
lowering. This is an internal compiler error — the type checker should
have caught undefined variables before MIR building.

If you see this error, please report it as a compiler bug.

Related: T207 (undefined variable/function at type-check time)
)EX"},

        {"M012", R"EX(
Block not found in MIR function [M012]

The MIR builder referenced a basic block by ID, but that block does not
exist in the current function. This is an internal invariant violation.

This is always a compiler bug. Please report it.
)EX"},

        {"M013", R"EX(
Invalid terminator in MIR block [M013]

A MIR basic block has an invalid or missing terminator instruction.
Every block must end with exactly one terminator (return, branch,
conditional branch, or switch). This invariant was violated.

This is always a compiler bug. Please report it.
)EX"},

        {"M014", R"EX(
Undefined function call target in MIR [M014]

The MIR builder attempted to emit a call instruction but could not
resolve the callee to a known function or value. This indicates the
callee was not properly registered during monomorphization or HIR
lowering.

This is typically a compiler bug. Please report it.

Related: C006 (method not found in codegen)
)EX"},

        {"M015", R"EX(
Struct field access error in MIR [M015]

The MIR builder could not resolve a struct field access. Either the
field index is out of range or the type is not a struct. This indicates
an inconsistency between the type checker's field resolution and the
MIR builder.

This is always a compiler bug. Please report it.
)EX"},

        {"M016", R"EX(
Enum variant access error in MIR [M016]

The MIR builder could not resolve an enum variant access. Either the
variant index is out of range or the type is not an enum. This indicates
an inconsistency between the type checker's variant resolution and the
MIR builder.

This is always a compiler bug. Please report it.
)EX"},

        {"M017", R"EX(
Closure environment error in MIR [M017]

The MIR builder encountered an error while building or accessing a
closure's captured environment. This could indicate that the closure
capture analysis produced incorrect results.

This is always a compiler bug. Please report it.
)EX"},

        {"M018", R"EX(
Generic instantiation error in MIR [M018]

The MIR builder failed to instantiate a generic function or type with
the provided type arguments. This can happen when monomorphization
produces an invalid concrete instantiation.

This is always a compiler bug. Please report it.

Related: C025 (generic instantiation in codegen)
)EX"},

        {"M019", R"EX(
ABI mismatch in MIR building [M019]

The MIR builder detected an ABI (Application Binary Interface) mismatch
between a function call site and the function declaration. This can
cause crashes at runtime if not caught.

Common causes:
1. A TML function calling an `@extern("c")` function with wrong argument types
2. Sret (struct return) convention mismatch
3. Calling convention disagreement

This is always a compiler bug. Please report it.

Related: M010 (type mismatch), C028 (sret mismatch in codegen)
)EX"},

        {"M020", R"EX(
Function build failed in MIR builder [M020]

The MIR builder failed to produce a valid MIR function for the named
source-level function. This is a catch-all for MIR build failures that
don't have a more specific code.

The error message will include the function name. This is an internal
compiler error — please report it with the source code that triggered
the failure.

Related: M003-M019 for more specific MIR build errors
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
