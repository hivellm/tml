TML_MODULE("compiler")

//! # HIR Lowering Error Explanations
//!
//! Error codes H001-H015 for HIR (High-level IR) lowering errors.
//! These are internal compiler diagnostics emitted when the HIR builder
//! encounters invalid or unsupported constructs during lowering from AST.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_hir_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"H001", R"EX(
Unsupported expression type in HIR lowering [H001]

The HIR builder encountered an expression node that it does not know how
to lower from the AST to High-level IR. This is an internal compiler error
— all expression types that the parser can produce should be handled by
the HIR lowering pass.

Common causes:
1. A new syntax was added to the parser but HIR lowering was not updated
2. A desugaring pass produced a node type that HIR doesn't handle

This is always a compiler bug. Please report it with the source file
and the specific expression that triggered this error.

Related: H002 (type resolution failure)
)EX"},

        {"H002", R"EX(
Type resolution failure in HIR lowering [H002]

The HIR builder could not resolve a type reference to a concrete type.
This can happen when:

1. A named type has an empty path (internal invariant violated)
2. A type node does not match any recognized type category

This is an internal compiler error — the type checker should have
resolved all types before HIR lowering begins. If you see this error,
it indicates a bug in the type checker or HIR builder coordination.

Please report this error with the source file that triggered it.

Related: H001 (unsupported expression), T001 (type mismatch)
)EX"},

        {"H003", R"EX(
Null type argument in monomorphization [H003]

The HIR builder attempted to monomorphize (specialize) a generic function
but encountered a null/missing type argument. This indicates that the
type inference pass did not fully resolve all type parameters before
HIR lowering began.

Common causes:
1. An inferred type was left as a type variable (not fully constrained)
2. A generic function was called with insufficient type information
3. A bug in the type inference constraint solver

This is typically a compiler bug. Check whether the type checker emitted
any type errors before this HIR error appeared.

Related: H004 (monomorphization depth), H005 (type param not resolved)
)EX"},

        {"H004", R"EX(
Monomorphization depth exceeded [H004]

The HIR builder's monomorphization (generic specialization) recursion depth
exceeded the limit of 128 levels. This usually indicates infinite recursive
generic instantiation.

Example of code that can trigger this:
```tml
func foo[T]() {
    foo[List[T]]()  // Each call wraps T in another List
}
```

To fix this:
1. Check for recursive generic type instantiations
2. Ensure base cases exist in recursive generic functions
3. Consider using trait objects (`dyn Behavior`) to avoid deep generics

Related: H003 (null type arg), H005 (type param not resolved)
)EX"},

        {"H005", R"EX(
Type parameter not resolved in HIR lowering [H005]

The HIR builder encountered a type parameter reference that could not be
resolved to a concrete type during monomorphization. This means the
generic type substitution map did not contain a mapping for this
type parameter.

Common causes:
1. A generic function was not fully instantiated before lowering
2. A type parameter from an outer scope was not properly captured
3. A bug in the monomorphization substitution logic

This is an internal compiler error. Please report it with the specific
generic function and call site that triggered this error.

Related: H003 (null type arg), H004 (depth exceeded)
)EX"},

        {"H006", R"EX(
Invalid HIR node structure [H006]

The HIR builder detected a structurally invalid HIR node — a node that
violates the invariants expected by the lowering pass. This is an
internal compiler error indicating a bug in an earlier compilation stage.

This is always a compiler bug. Please report it with the source file.
)EX"},

        {"H007", R"EX(
Closure capture analysis failure [H007]

The HIR builder could not determine the capture set for a closure
expression. This can happen when a variable referenced inside a closure
is not found in the surrounding scope at the time of HIR lowering.

Common causes:
1. A scope was exited before the closure was lowered
2. A variable was renamed or removed during desugaring
3. A macro or syntax expansion produced invalid variable references

Related: B001 (use after move), T040 (cannot capture by value)
)EX"},

        {"H008", R"EX(
Field index resolution failure [H008]

The HIR builder could not resolve a field access to a concrete field index.
This means the struct or enum type did not have the expected field at the
time of HIR lowering.

Common causes:
1. The type checker did not fully resolve the struct type
2. A field was renamed between type checking and HIR lowering
3. An anonymous struct field access could not be matched

Related: T025 (unknown field), H002 (type resolution failure)
)EX"},

        {"H009", R"EX(
Enum variant index resolution failure [H009]

The HIR builder could not resolve an enum variant reference to a concrete
variant index. This means the enum type did not have the expected variant
at the time of HIR lowering.

Common causes:
1. The type checker did not fully resolve the enum type
2. A variant was renamed between type checking and HIR lowering
3. Pattern matching desugaring produced invalid variant references

Related: T027 (unknown variant), H008 (field index failure)
)EX"},

        {"H010", R"EX(
Iterator desugaring failure [H010]

The HIR builder could not desugar a `loop (item in collection)` expression.
This means the collection type does not implement the `Iterator` behavior,
or the desugaring could not find the required iterator methods.

To fix this:
1. Ensure the collection type implements `Iterator`
2. Check that `into_iter()` or `iter()` is available for the type
3. Use explicit iterator methods if the auto-desugaring fails

Related: T030 (behavior not implemented), H001 (unsupported expression)
)EX"},

        {"H011", R"EX(
Pattern match desugaring failure [H011]

The HIR builder could not desugar a pattern match (`when`) expression.
This can happen when the pattern structure does not match the type of the
matched value, or when an exhaustiveness check fails during lowering.

Common causes:
1. A pattern uses fields or variants that don't exist on the type
2. A match arm guard could not be lowered
3. An or-pattern could not be expanded

Related: T035 (non-exhaustive match), H009 (variant resolution)
)EX"},

        {"H012", R"EX(
`if let` / `when let` desugaring failure [H012]

The HIR builder could not desugar a `when let` (if-let) expression into
a pattern match. This is an internal compiler error — the HIR builder
should always be able to convert `when let` into an equivalent `when`.

Related: H011 (pattern match desugaring), P040 (invalid pattern)
)EX"},

        {"H013", R"EX(
Method resolution failure in HIR [H013]

The HIR builder could not resolve a method call to a specific function.
This means the type checker did not fully resolve the method dispatch
before HIR lowering began.

Common causes:
1. A trait method was not resolved to a concrete implementation
2. An auto-deref chain exceeded the maximum deref depth
3. A method call on a generic type was not monomorphized

Related: T020 (method not found), H002 (type resolution failure)
)EX"},

        {"H014", R"EX(
Operator desugaring failure [H014]

The HIR builder could not desugar an operator expression (e.g., `a + b`)
into a method call (e.g., `a.add(b)`). This means the type checker did
not resolve which `Add` implementation to use for the operand types.

Common causes:
1. The operator behavior is not implemented for the operand types
2. Type inference left ambiguous types for the operands
3. A custom operator could not be resolved to a method

Related: T031 (operator not supported), H013 (method resolution)
)EX"},

        {"H015", R"EX(
Constant evaluation failure in HIR [H015]

The HIR builder could not evaluate a compile-time constant expression.
This means a value that should be known at compile time (array length,
generic const parameter, etc.) could not be computed during HIR lowering.

Common causes:
1. A const expression references a non-const variable
2. A const generic parameter was not provided at the call site
3. A const expression contains a runtime-only operation

Related: T050 (const eval error), H003 (null type arg in monomorphization)
)EX"},

    }; // db
    return db;
}

} // namespace tml::cli::explain
