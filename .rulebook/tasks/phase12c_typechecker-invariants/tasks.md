# Tasks: Document Type Checker Invariants

**Status**: Planned (0/22)
**Depends on**: None (read-only task, can start immediately)
**Blocks**: phase12 Era 1 Phase 2 (type checker porting requires invariant knowledge)
**Duration**: 3–4 weeks
**Risk**: High — undocumented invariants are the #1 risk for self-hosting
**Output**: `docs/specs/typechecker-invariants.md`

---

## Phase 1: Type Registration

- [ ] 1.1 Read `compiler/src/types/checker/core.cpp` (1,412 LOC) — document registration order and phase dependencies
- [ ] 1.2 Read `compiler/src/types/type.cpp` (965 LOC) — document Type representation, variants, equality
- [ ] 1.3 Read `compiler/include/types/type.hpp` (427 LOC) — document Type class hierarchy and fields
- [ ] 1.4 Read `compiler/include/types/env.hpp` (807 LOC) — document TypeEnv fields, maps, lookup tables
- [ ] 1.5 Write Section 1 of invariant doc: type registration order, dependencies, and invariants that hold after each registration phase

## Phase 2: Module Resolution

- [ ] 2.1 Read `compiler/src/types/env_module_loading.cpp` (875 LOC) — document module load sequence
- [ ] 2.2 Read `compiler/src/types/env_module_load_decls.cpp` (1,253 LOC) — document declaration loading
- [ ] 2.3 Read `compiler/src/types/env_module_load.cpp` (508 LOC) — document import resolution algorithm
- [ ] 2.4 Write Section 2: import resolution, visibility rules, re-export handling, `pub use` semantics

## Phase 3: Impl Processing

- [ ] 3.1 Read `compiler/src/types/checker/core_oop.cpp` (1,067 LOC) — document behavior impl processing
- [ ] 3.2 Read `compiler/src/types/checker/decl_struct.cpp` (1,207 LOC) — document struct and enum checking
- [ ] 3.3 Read `compiler/src/types/env_lookups.cpp` (1,265 LOC) — document all lookup algorithms
- [ ] 3.4 Write Section 3: behavior registration, method resolution, coherence rules, `extend` semantics

## Phase 4: Body Checking and Inference

- [ ] 4.1 Read `compiler/src/types/checker/expr.cpp` (652 LOC) — document expression type inference
- [ ] 4.2 Read `compiler/src/types/checker/expr_call.cpp` (802 LOC) — document call resolution and overloading
- [ ] 4.3 Read `compiler/src/types/checker/expr_call_method.cpp` (1,363 LOC) — document method dispatch algorithm
- [ ] 4.4 Read `compiler/src/types/checker/expr_call_method_types.cpp` (668 LOC) — document generic method instantiation
- [ ] 4.5 Write Section 4: inference rules, unification, generic instantiation, method resolution order

## Phase 5: Cross-Cutting Invariants

- [ ] 5.1 Read remaining checker files — `expr_ops.cpp` (615), `expr_special.cpp` (368), `stmt.cpp` (446), `control.cpp` (302), `const_eval.cpp` (299), `helpers.cpp` (325), `resolve.cpp` (452), `types_checker.cpp` (678)
- [ ] 5.2 Read `compiler/src/types/builtins/` (10 files, ~1,309 LOC total) — document builtin type registration order and assumptions
- [ ] 5.3 Read environment files — `env_core.cpp` (128), `env_definitions.cpp` (139), `env_scope.cpp` (60), `env_module_support.cpp` (524)
- [ ] 5.4 Write Section 5: ordering dependencies, global state invariants, error recovery behavior, scope chain rules
- [ ] 5.5 Review complete document for consistency — verify all 38 type checker source files are covered and no invariants contradict each other
- [ ] 5.6 Write Section 6: invariants the self-hosted type checker MUST preserve to produce identical TypeEnv output as the C++ implementation
