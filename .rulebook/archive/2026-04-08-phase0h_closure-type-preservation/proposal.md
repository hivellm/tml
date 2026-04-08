# Proposal: Preserve ClosureType through Generic Struct Instantiation

**Task**: phase0h_closure-type-preservation
**Status**: Planned
**Priority**: P1 — blocks 18 tests in phase0g RC7
**Estimated effort**: 1–2 days (single specialist)
**Risk**: Medium — touches type checker instantiation + codegen where-clause resolver
**Depends on**: phase0g (RC7 enum part landed in commit `30026ce4`)
**Blocks**: Test coverage accuracy, iterator/Promise generic pipelines

## Why

18 tests in phase0g RC7 were blocked because generic structs receiving closures emitted unsubstituted type variables (`Maybe__T` instead of `Maybe__I32`) due to `ClosureType` being collapsed to `NamedType("Fn")` at struct instantiation.

## Problem

18 tests in `phase0g` RC7 remain blocked because generic functions/methods that
take a closure parameter emit LLVM IR with unsubstituted type variables in the
function **signature**, producing `Maybe__T` instead of `Maybe__I32`.

Representative failing test: `lib/core/tests/iter/iter_repeat_with.test.tml`

Generated IR (broken):

```llvm
define internal %struct.Maybe__T @"...RepeatWith__Fn4nextE"(ptr)
```

Expected:

```llvm
define internal %struct.Maybe__I32 @"...RepeatWith__Fn4nextE"(ptr)
```

## Root Cause

Verified via stderr instrumentation on RC7 closure work (see phase0g item 7.5):

1. When `RepeatWith[F]` is instantiated with a closure `|| 42`, the type checker
   records the struct's `type_args[0]` as **`NamedType("Fn")`** — a degenerate
   placeholder — instead of the rich `ClosureType { params: [], return: I32 }`.
2. Location: `compiler/src/codegen/llvm/expr/method_impl.cpp:556-557` reads
   `type_subs[F] = named.type_args[0]`, which returns the placeholder.
3. The where-clause resolver `resolve_where_clause_type_equalities` in
   `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:441-468` receives
   `param_name=F, concrete=NamedType("Fn")`. Its pattern matcher cannot extract
   `T` from pattern `func() -> T` because `concrete` is not a `FuncType`/
   `ClosureType`. `T` never enters `full_type_subs`.
4. The stub-emission guard in `compiler/src/codegen/llvm/decl/impl.cpp:980-1011`
   only fires when existing `full_type_subs` entries `contains_unresolved_generic`;
   it does NOT fire when impl-level generics are missing entirely. Body emission
   proceeds with `T` still free → mangled `Maybe__T`.

## Three Candidate Fix Approaches

**Approach A — Preserve ClosureType at struct instantiation (PREFERRED)**

Fix the upstream loss: when the type checker records `type_args` for a generic
struct that receives a closure, store the `ClosureType` directly (with its
params/return) instead of collapsing to `NamedType("Fn")`.

- Pros: Fixes the root cause. Benefits every downstream consumer (THIR, MIR,
  codegen). Other latent bugs may self-resolve.
- Cons: Touches type checker core and possibly cache serialization format.
- Files: `compiler/src/types/checker/expr_*.cpp` (closure arg handling),
  `compiler/src/types/type.hpp` (ensure `ClosureType` serializes/compares),
  possibly `compiler/src/types/env_serialize.cpp` for cache compat.

**Approach B — Pattern match through the degenerate `Fn` placeholder**

Extend `resolve_where_clause_type_equalities` to detect `NamedType("Fn")` and
look up the original `ClosureType` from a side-table populated during struct
instantiation.

- Pros: Localized change in codegen.
- Cons: Requires a new side-table plumbed from type checker to codegen. Leaves
  the upstream bug in place. Other consumers still see `Fn` placeholder.

**Approach C — Defer body emission until T is resolved at call site**

Delay generic method body emission until all type params are concrete at a
real call site, then substitute.

- Pros: No type checker changes.
- Cons: Architectural shift. Affects codegen ordering and stub guard. Higher
  risk of regressions.

**Recommendation**: Approach A. It is the root cause fix and aligns with how
Rust's type checker preserves `FnOnce/FnMut/Fn` trait bounds with concrete
argument/return types.

## Success Criteria

1. `iter_repeat_with.test.tml` emits `%struct.Maybe__I32` in the function
   signature, not `Maybe__T`.
2. All 18 RC7 deeper tests pass:
   - `compiler_iter_from_fn`
   - `iter_higher_order`
   - `core_iter_*` (several)
   - `core_option_*` (several)
   - `std_types_maybe_combinators`
3. No regressions in existing iterator/closure/Promise tests.
4. Defensive assertion in `mir_types.cpp::mangle_mir_type_arg` (already added
   in commit `30026ce4`) never fires in release builds on the test suite.
5. `phase0g` task file RC7 row drops from 18 → 0.

## Out of Scope

- FQN behavior collision (owned by `phase0i`)
- MODULE_NOT_FOUND test `use` fixes (trivial library-level work)
- NEON intrinsic tests (platform gating, not bugs)
- Test runner legacy-vs-MIR pipeline switch (separate task)
