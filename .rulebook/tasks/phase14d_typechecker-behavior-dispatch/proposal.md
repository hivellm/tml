# Proposal: Type Checker — Behavior Dispatch (Sub-phase 2d)

## Why

Behavior dispatch is the integration phase of the type checker. It takes the inference engine
from phase14c, the symbol tables from phase14a, and the resolved imports from phase14b, and
combines them into a complete end-to-end type checking pipeline. It also implements two
components that cannot exist until inference is complete: the trait solver (which resolves which
impl block satisfies a generic bound at each call site) and the coercion insertion pass (which
annotates the AST with implicit type conversions before it enters the HIR lowerer). Completing
this phase unblocks all of Phase 15 (HIR, THIR, MIR porting), which is the next major milestone
on the path to a self-hosting compiler.

## What Changes

Port the following C++ files to TML:

- `checker/core_oop.cpp` (1,067 LOC) — behavior/trait processing: registration, impl verification,
  associated type binding, default method resolution
- `checker/decl_struct.cpp` (1,207 LOC) — struct declaration checking with behavior impls:
  field type checking, impl block attachment, generic struct instantiation
- `checker/core.cpp` (1,412 LOC) — phases 3 and 4 of the 4-phase checker: impl block processing
  (phase 3) and function body checking (phase 4), using inference from phase14c
- `types/env_lookups.cpp` (1,265 LOC) — behavior lookup table, impl resolution, coherence
  enforcement, associated type projection resolution

New TML modules produced:

- `compiler-tml/src/types/behavior/mod.tml` — behavior system module root
- `compiler-tml/src/types/behavior/registry.tml` — BehaviorRegistry: stores all behavior defs
- `compiler-tml/src/types/behavior/solver.tml` — trait resolution engine
- `compiler-tml/src/types/behavior/dispatch.tml` — method lookup with priority rules
- `compiler-tml/src/types/coercion.tml` — coercion insertion pass (feeds into THIR)

## Key Decisions

**Trait solver with strict priority: inherent > behavior > auto-deref.** When a method is called
on a receiver, the lookup order is: (1) inherent impl on the concrete type, (2) behavior impls
in import scope, (3) auto-deref through one level of ref. This order is a hard invariant of the
C++ implementation and must be preserved exactly. Any deviation produces different method
resolution than the C++ compiler, causing IR-diff failures.

**Coherence checking runs after all impls are registered.** Impl blocks are collected during
phase 3 (registration pass) and coherence is checked only after the full module set is loaded.
Checking coherence incrementally (as each impl is registered) would produce false positives for
split impls across multiple files in the same crate. The C++ implementation defers coherence
checking to the end of phase 3 in `core.cpp`, and the TML port must match this.

**Coercion insertion is a separate pass, not interleaved with inference.** The inference engine
(phase14c) leaves `<T as Iterator>::Item` projections as opaque `AssocType` nodes. The coercion
pass in this phase resolves them to concrete types, inserts integer widening nodes, and
desugars all remaining operators to concrete method calls. This two-pass design is the
documented invariant from phase12c and is required for correct THIR lowering.

**Generic behavior bounds checked at instantiation, not during registration.** When
`func foo[T: Display](x: T)` is called with `T=I32`, the solver verifies that `I32` implements
`Display` at that call site. During function registration (phase 3), the bound `T: Display` is
stored but not verified — there is no concrete T yet. This lazy checking matches the C++
behavior and avoids false "missing impl" errors for unused generic instantiations.

## Known Invariants (from Phase 0 invariant document — phase12c REQUIRED)

- The 4-phase order is a hard constraint: registration → imports → impl processing → body checking
- Associated type normalization (`<T as Behavior>::Assoc` → concrete type) happens here (coercion
  pass), not during inference — this is a documented phase12c invariant
- Coherence check runs after ALL impl blocks are registered, not incrementally
- Method dispatch priority (inherent > behavior > auto-deref) must be preserved exactly
- Default behavior methods are resolved during impl registration, not at call sites
- `type_implements("Str", "Ord")` has a known false-positive bug in the C++ env_lookups.cpp;
  the TML port must replicate the same behavior to pass differential testing

## Architecture

```
compiler-tml/src/types/
  behavior/
    mod.tml       -- module root, re-exports BehaviorRegistry, Solver, Dispatch
    registry.tml  -- BehaviorRegistry: HashMap[Str, BehaviorDef] + HashMap[Str, List[ImplBlock]]
    solver.tml    -- resolve_behavior(ty, name), check_bounds(ty, bounds), coherence_check()
    dispatch.tml  -- lookup_method(recv_ty, name) -> Maybe[MethodDef], priority rules
  coercion.tml    -- insert_coercions(ast, type_env) -> CoercedAst, normalize_assoc_types()
```

The `BehaviorRegistry` is the central data structure: it maps behavior names to their
definitions (methods, associated type names, default impls) and maps `"TypeName::BehaviorName"`
keys to the `ImplBlock` that satisfies each combination. The solver queries this registry.
The registry is populated during phase 3 (impl processing) and read-only during phase 4
(body checking). It is part of the final `TypeEnv` output.

## Pipeline Integration

After phase14d completes, the full type checker pipeline is:

```
AST
 | phase14a: register all type/func declarations into TypeEnv
 | phase14b: resolve all imports, populate module visibility maps
 | phase14c: infer types for all expressions, produce constraint solution
 | phase14d: verify behavior bounds, insert coercions, normalize associated types
 v
TypeEnv + CoercedAST  →  HIR lowerer (Phase 15)
```

The `CoercedAST` output of phase14d is a copy of the original AST where every expression node
carries its resolved type AND any implicit coercion wrappers needed. The HIR lowerer consumes
this directly — it never needs to consult the inference engine or trait solver again.

## Success Criteria

Two-level differential test:

1. TypeEnv comparison: run the full TML type checker (14a through 14d) on all 1,700+ test
   files and all stdlib modules. Serialize the final TypeEnv (types, impl table, coercion
   annotations). Compare with C++ output. Zero diffs required.

2. IR-diff: feed the TML type checker output into the C++ HIR lowerer and compile to LLVM IR.
   Compare the resulting IR with the IR produced by a full C++ pipeline. Identical IR required
   on all test files before this phase is marked complete.

## Risk Assessment

High. This is the first time all four sub-phases run together as a pipeline. Integration bugs
are expected:

- Phase boundary data format mismatches: TypeEnv from 14a/14b may not match what 14c expects
- Solver lookup order bugs produce wrong method resolution on ~5-10% of test cases typically
- Coherence check timing bugs produce false positives or miss real overlapping impls
- Coercion pass may double-insert coercions if inference already partially desugared operators
- The `type_implements` false-positive bug must be replicated exactly or differential testing fails

Plan: implement and test each phase in isolation first (registry, solver, dispatch, coercion),
then wire the full pipeline and run differential tests incrementally starting with the 10
simplest stdlib modules before attempting the full 1,700+ file suite.

## Dependencies

- **Requires**: phase14c complete (inference engine — the solver depends on TypeVar resolution)
- **Requires**: phase14a complete (type registration — BehaviorRegistry is populated from TypeEnv)
- **Requires**: phase14b complete (module resolution — impl blocks may live in imported modules)
- **Requires**: phase12c invariant document (especially associated type normalization invariant)
- **Blocks**: Phase 15 (HIR lowerer port) — HIR lowering requires a complete, correct TypeEnv
