# Tasks: Type Checker — Behavior Dispatch (Sub-phase 2d)

**Status**: Planned (0/22)
**Depends on**: phase14c (inference engine), phase14a+14b (registration + modules)
**Blocks**: Phase 15 (HIR/THIR/MIR porting)
**Duration**: 8–10 weeks
**Risk**: High — integrates all previous sub-phases into full end-to-end type checking
**C++ reference**: ~4,951 LOC → ~3,200 TML

---

## Phase 1: Behavior (Trait) Registration (4 items)

- [ ] 1.1 Create `compiler-tml/src/types/behavior/mod.tml` — behavior system module root
- [ ] 1.2 Create `compiler-tml/src/types/behavior/registry.tml` — `BehaviorRegistry`: stores all behavior definitions and impl blocks
- [ ] 1.3 Implement behavior registration: parse `behavior Foo { ... }` → register methods, associated types, default impls
- [ ] 1.4 Implement impl block registration: parse `impl Foo for Bar { ... }` → register method implementations, verify all required methods present

## Phase 2: Trait Solver (6 items)

- [ ] 2.1 Create `compiler-tml/src/types/behavior/solver.tml` — trait resolution engine
- [ ] 2.2 Implement `resolve_behavior(ty: Type, behavior: Str) -> Maybe[ImplBlock]` — find impl for type+behavior pair
- [ ] 2.3 Implement generic behavior bounds: `func foo[T: Display](x: T)` → verify T implements Display at call site
- [ ] 2.4 Implement behavior inheritance: `behavior Foo: Bar` → require Bar impl when implementing Foo
- [ ] 2.5 Implement associated type resolution: `<T as Iterator>::Item` → look up associated type in impl
- [ ] 2.6 Implement coherence checking: no overlapping impls for same type+behavior pair

## Phase 3: Method Dispatch (4 items)

- [ ] 3.1 Create `compiler-tml/src/types/behavior/dispatch.tml` — method lookup and dispatch
- [ ] 3.2 Implement inherent method lookup: search struct's own impl block for method
- [ ] 3.3 Implement behavior method lookup: search all behavior impls for type, resolve method
- [ ] 3.4 Implement dispatch priority: inherent methods > behavior methods > auto-deref methods

## Phase 4: Coercion Insertion (4 items)

- [ ] 4.1 Create `compiler-tml/src/types/coercion.tml` — coercion insertion pass (feeds THIR)
- [ ] 4.2 Implement implicit coercions: integer widening (I8→I32), ref coercion (ref T → ref T), deref coercion
- [ ] 4.3 Implement operator desugaring finalization: all operators resolved to concrete method calls
- [ ] 4.4 Implement associated type normalization: replace all `<T as Behavior>::Assoc` with concrete types

## Phase 5: Full Pipeline Integration (2 items)

- [ ] 5.1 Wire all 4 sub-phases together: registration (14a) → module resolution (14b) → inference (14c) → behavior dispatch (14d) → output TypeEnv + coercion-annotated AST
- [ ] 5.2 Test end-to-end: full type checking of 50 stdlib modules → identical output to C++

## Phase 6: Differential Testing — Full Suite (2 items)

- [ ] 6.1 Run TML type checker on ALL 1,700+ test files → compare TypeEnv with C++ output
- [ ] 6.2 IR-diff: compile test files with TML type checker feeding C++ downstream → identical IR
