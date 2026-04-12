# Tasks: Type Checker — Behavior Dispatch (Sub-phase 2d)

**Status**: In Progress (16/22)
**Depends on**: phase14c (inference engine), phase14a+14b (registration + modules)
**Blocks**: Phase 15 (HIR/THIR/MIR porting)
**Duration**: 8–10 weeks
**Risk**: High — integrates all previous sub-phases into full end-to-end type checking
**C++ reference**: ~4,951 LOC → ~3,200 TML

---

## Phase 1: Behavior (Trait) Registration (4 items)

- [x] 1.1 Create `compiler-tml/src/types/behaviors/common.tml` — behavior system module root (dir named `behaviors` because `behavior` is a keyword)
- [x] 1.2 Create `compiler-tml/src/types/behaviors/registry.tml` — `BehaviorRegistry`: ImplBlock storage, behavior/impl registration, method lookup
- [x] 1.3 Implement behavior registration: register_behavior with reserved-name check, method/assoc-type/super-behavior recording
- [x] 1.4 Implement impl block registration: register_impl_block with missing-method verification, qualified function registration

## Phase 2: Trait Solver (6 items)

- [x] 2.1 Create `compiler-tml/src/types/behaviors/solver.tml` — trait resolution engine
- [x] 2.2 Implement `resolve_behavior(ty, behavior_name) -> Maybe[ImplBlock]` — direct lookup + TypeEnv fallback
- [x] 2.3 Implement generic behavior bounds: `check_bounds(ty, bounds) -> List[Str]` with auto-impl for primitives
- [x] 2.4 Implement behavior inheritance: `check_super_behaviors` verifies all parent behaviors are satisfied
- [x] 2.5 Implement associated type resolution: `resolve_associated_type` checks impl bindings then behavior defaults
- [x] 2.6 Implement coherence checking: `check_coherence` detects overlapping impl blocks

## Phase 3: Method Dispatch (4 items)

- [x] 3.1 Create `compiler-tml/src/types/behaviors/dispatch.tml` — method lookup with MethodResolution result type
- [x] 3.2 Implement inherent method lookup: search inherent impl blocks + qualified TypeEnv functions
- [x] 3.3 Implement behavior method lookup: search all behavior impls via env_get_behavior_impls + registry
- [x] 3.4 Implement dispatch priority: inherent > behavior > auto-deref (through Ref and Heap)

## Phase 4: Coercion Insertion (4 items)

- [x] 4.1 Create `compiler-tml/src/types/coercion.tml` — coercion types and pass
- [x] 4.2 Implement implicit coercions: integer widening (I8→I64), float widening (F32→F64), ref coercion, deref through Heap
- [x] 4.3 Implement operator desugaring finalization: `desugar_op_final` resolves to concrete behavior methods with Self substitution
- [x] 4.4 Implement associated type normalization: `normalize_type` recursively resolves Named, Ref, Tuple, Func types

## Phase 5: Full Pipeline Integration (2 items)

- [ ] 5.1 Wire all 4 sub-phases together: registration (14a) → module resolution (14b) → inference (14c) → behavior dispatch (14d) → output TypeEnv + coercion-annotated AST
- [ ] 5.2 Test end-to-end: full type checking of 50 stdlib modules → identical output to C++

## Phase 6: Differential Testing — Full Suite (2 items)

- [ ] 6.1 Run TML type checker on ALL 1,700+ test files → compare TypeEnv with C++ output
- [ ] 6.2 IR-diff: compile test files with TML type checker feeding C++ downstream → identical IR

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
