# Tasks: Document Type Checker Invariants

**Status**: Done (22/22)
**Depends on**: None (read-only task, can start immediately)
**Blocks**: phase12 Era 1 Phase 2 (type checker porting requires invariant knowledge)
**Duration**: 3–4 weeks
**Risk**: High — undocumented invariants are the #1 risk for self-hosting
**Output**: `docs/specs/typechecker-invariants.md` — COMPLETE (4117 lines, ~82 pages, 176 invariants, 41 Section 6 contract items)

---

## Phase 1: Type Registration

- [x] 1.1 Read `compiler/src/types/checker/core.cpp` (1,412 LOC) — document registration order and phase dependencies
- [x] 1.2 Read `compiler/src/types/type.cpp` (965 LOC) — document Type representation, variants, equality
- [x] 1.3 Read `compiler/include/types/type.hpp` (427 LOC) — document Type class hierarchy and fields
- [x] 1.4 Read `compiler/include/types/env.hpp` (807 LOC) — document TypeEnv fields, maps, lookup tables
- [x] 1.5 Write Section 1 of invariant doc: type registration order, dependencies, and invariants that hold after each registration phase
  - Output: `.rulebook/tasks/phase12c_typechecker-invariants/specs/section1_type_registration.md`
  - 25 invariants documented, ~15 pages, 50+ source citations
  - Also read: env_core.cpp, env_definitions.cpp, env_lookups.cpp (partial), builtins/*.cpp, builtins_cache.cpp

## Phase 2: Module Resolution

- [x] 2.1 Read `compiler/src/types/env_module_loading.cpp` (875 LOC) — document module load sequence
- [x] 2.2 Read `compiler/src/types/env_module_load_decls.cpp` (1,253 LOC) — document declaration loading
- [x] 2.3 Read `compiler/src/types/env_module_load.cpp` (508 LOC) — document import resolution algorithm
- [x] 2.4 Write Section 2: import resolution, visibility rules, re-export handling, `pub use` semantics — written to `.rulebook/tasks/phase12c_typechecker-invariants/specs/section2_module_resolution.md` (23 invariants, 7 surprising findings, ~18 pages)

## Phase 3: Impl Processing

- [x] 3.1 Read `compiler/src/types/checker/core_oop.cpp` (1,067 LOC) — document behavior impl processing
- [x] 3.2 Read `compiler/src/types/checker/decl_struct.cpp` (1,207 LOC) — document struct and enum checking
- [x] 3.3 Read `compiler/src/types/env_lookups.cpp` (1,265 LOC) — document all lookup algorithms
- [x] 3.4 Write Section 3: behavior registration, method resolution, coherence rules, `extend` semantics — written to `.rulebook/tasks/phase12c_typechecker-invariants/specs/section3_impl_processing.md` (33 invariants, ~18 pages)

## Phase 4: Body Checking and Inference

- [x] 4.1 Read `compiler/src/types/checker/expr.cpp` (652 LOC) — document expression type inference
- [x] 4.2 Read `compiler/src/types/checker/expr_call.cpp` (802 LOC) — document call resolution and overloading
- [x] 4.3 Read `compiler/src/types/checker/expr_call_method.cpp` (1,363 LOC) — document method dispatch algorithm
- [x] 4.4 Read `compiler/src/types/checker/expr_call_method_types.cpp` (668 LOC) — document generic method instantiation
- [x] 4.5 Write Section 4: inference rules, unification, generic instantiation, method resolution order — written to specs/section4_body_inference.md (34 invariants, 7 surprising findings, ~22 pages)

## Phase 5: Cross-Cutting Invariants

- [x] 5.1 Read remaining checker files — `expr_ops.cpp` (615), `expr_special.cpp` (368), `stmt.cpp` (446), `control.cpp` (302), `const_eval.cpp` (299), `helpers.cpp` (325), `resolve.cpp` (452), `types_checker.cpp` (678)
- [x] 5.2 Read `compiler/src/types/builtins/` (10 files, ~1,309 LOC total) — document builtin type registration order and assumptions
- [x] 5.3 Read environment files — `env_core.cpp` (128), `env_definitions.cpp` (139), `env_scope.cpp` (60), `env_module_support.cpp` (524)
- [x] 5.4 Write Section 5: ordering dependencies, global state invariants, error recovery behavior, scope chain rules — written to specs/section5_cross_cutting.md (61 invariants, 10 surprising findings, ~22 pages)
- [x] 5.5 Review complete document for consistency — verified all 5 section files internally consistent; cross-references added; contradictions noted in Appendix C (Known Gaps); no outright contradictions found between sections
- [x] 5.6 Write Section 6: invariants the self-hosted type checker MUST preserve to produce identical TypeEnv output as the C++ implementation
  - 41 contract items across 7 categories: Registration (R-01..07), Module (M-01..07), Impl (IM-01..07), Inference (IN-01..08), Error Recovery (ER-01..05), Global State (GS-01..07), plus Compatibility Test Plan (TP-01..22)
  - Output consolidated to: `docs/specs/typechecker-invariants.md` (4117 lines, ~82 pages, 257 KB)
  - Appendix A: unified invariant index (all 176 invariants with file:line)
  - Appendix B: 11 latent bugs and surprising findings
  - Appendix C: 6 known gaps
  - Appendix D: terminology glossary (30 terms)
