# Proposal: Type Checker — Type Inference (Sub-phase 2c)

## Why

Type inference is the core of the TML type checker. Without it, every expression in every source
file has an unknown type. The C++ implementation (~7,229 LOC across 13 files) implements
Hindley-Milner inference with constraint generation and unification. Porting this to TML is
the single hardest sub-phase of the compiler self-hosting project: it contains more edge cases,
more subtle invariants, and more cross-cutting concerns than any other sub-phase. Completing it
unblocks phase14d (behavior dispatch) and everything downstream through Phase 15 (HIR/THIR/MIR).

## What Changes

Port the following C++ files to TML:

- `checker/expr.cpp` (652 LOC) — expression type inference dispatcher
- `checker/expr_call.cpp` (802 LOC) — function call resolution and generic instantiation
- `checker/expr_call_method.cpp` (1,363 LOC) — method call dispatch (largest single file)
- `checker/expr_call_method_types.cpp` (668 LOC) — generic method instantiation, type param substitution
- `checker/expr_ops.cpp` (615 LOC) — operator type checking, operator desugaring to behavior calls
- `checker/expr_special.cpp` (368 LOC) — special expressions: sizeof, typeof, compile-time queries
- `checker/stmt.cpp` (446 LOC) — statement checking: let, assign, expr-stmt, return
- `checker/control.cpp` (302 LOC) — control flow: if/else, loop, when, break-with-value
- `checker/const_eval.cpp` (299 LOC) — compile-time constant evaluation for generic bounds
- `checker/helpers.cpp` (325 LOC) — shared utilities: scope push/pop, error accumulation
- `checker/resolve.cpp` (452 LOC) — name resolution: locals, imports, module paths
- `checker/types_checker.cpp` (678 LOC) — type compatibility: subtyping, coercion eligibility
- `include/types/checker.hpp` (259 LOC) — `TypeChecker` class definition and public API

New TML modules produced:

- `compiler-tml/src/types/infer/mod.tml` — inference engine module root
- `compiler-tml/src/types/infer/unify.tml` — union-find with path compression
- `compiler-tml/src/types/checker/check_expr.tml` — expression inference dispatcher
- `compiler-tml/src/types/checker/check_call.tml` — call and method resolution
- `compiler-tml/src/types/checker/check_stmt.tml` — statement and control flow checking
- `compiler-tml/src/types/checker/check_pattern.tml` — pattern matching and exhaustiveness

## Key Decisions

**Union-find with path compression for type variables.** Each fresh `TypeVar` gets a unique
integer ID. The union-find structure maps IDs to either another ID (union edge) or a concrete
`Type` (resolved leaf). Path compression during `resolve()` keeps average lookup O(α(n)).
This matches exactly what the C++ implementation does in its `InferCtx`.

**Constraint-based inference, not on-the-fly unification.** Generate all constraints from the
AST first (each expression produces `Constraint::Eq(TypeVar, Type)`), then solve the constraint
set by running unification. This separation makes error messages cleaner — all unification
failures are reported together rather than stopping at the first mismatch.

**Generic instantiation at call sites.** When a generic function `func foo[T](x: T) -> T` is
called with argument type `I32`, instantiate T=I32 immediately at the call site. Substitute T
through the return type and all parameter types before unifying with the actual arguments.
Bounds (`T: Display`) are checked lazily after instantiation, not during constraint generation.

**Associated type normalization is NOT done here.** Replacing `<T as Iterator>::Item` with its
concrete type is a coercion-pass concern (phase14d). During inference, associated type
projections remain as opaque `AssocType(T, "Iterator", "Item")` nodes. Unification treats them
as equal only if the projection is structurally identical. This matches the C++ invariant
documented in phase12c.

## Known Invariants (from Phase 0 invariant document — phase12c REQUIRED)

- Type variables use union-find; never nest TypeVar inside TypeVar after resolve()
- Generic bounds are checked lazily at instantiation, not during constraint generation
- Associated type normalization happens during coercion insertion (phase14d), not here
- The 4-phase type checker order is a hard constraint: registration → imports → impls → bodies
- Exhaustiveness checking runs after all `when` arm patterns are typed, not during arm typing
- Integer literals default to I32; float literals default to F64; can be overridden by annotation

## Architecture

```
compiler-tml/src/types/
  infer/
    mod.tml          -- InferCtx: union-find state, fresh var generation
    unify.tml        -- unify(a, b), resolve(ty), occurs_check(var, ty)
  checker/
    check_expr.tml   -- infer_expr(expr, ctx) -> Type
    check_call.tml   -- infer_call(callee, args, ctx), infer_method(recv, name, args, ctx)
    check_stmt.tml   -- check_stmt(stmt, ctx), check_let(binding, ctx)
    check_pattern.tml -- check_pattern(pat, scrutinee_ty, ctx), exhaustiveness(arms, ty)
```

The `InferCtx` struct holds: the union-find table, the current scope chain (List of HashMap),
accumulated errors (List of TypeError), and a reference to the TypeEnv from phase14a/14b.
It is threaded through all check functions as a mutable parameter.

## Success Criteria

Differential TypeEnv comparison: run TML type inference on 50+ stdlib modules and all 1,700+
test files, serialize the inferred TypeEnv (type of every expression, every binding, every
return site), and compare byte-for-byte with the C++ inference output. Zero diffs required
before this phase is marked complete.

## Risk Assessment

CRITICAL. This is the hardest sub-phase:
- Unification edge cases: recursive types, higher-ranked types, occurs-check failures
- Generic instantiation: type param substitution must handle nested generics correctly
- Method dispatch interacts with inference: receiver type may still be a TypeVar when method
  is looked up, requiring deferred resolution
- Error recovery: inference must continue after type errors to report all errors, not just first
- The occurs check (prevent TypeVar = List[TypeVar]) must be correct or the engine loops

Plan: implement unify.tml first and test thoroughly with unit tests before touching check_expr.

## Dependencies

- **Requires**: phase14b complete (module resolution, imports available in TypeEnv)
- **Requires**: phase12c invariant document (MANDATORY — do not start without it)
- **Blocks**: phase14d (behavior dispatch needs a working inference engine as its foundation)
