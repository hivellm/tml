# Tasks: Type Checker — Type Inference (Sub-phase 2c)

**Status**: Planned (0/25)
**Depends on**: phase14b (modules resolved, imports available), phase12c (invariant document REQUIRED)
**Blocks**: phase14d (behavior dispatch needs inference engine)
**Duration**: 10–14 weeks (LONGEST sub-phase)
**Risk**: CRITICAL — Hindley-Milner inference has subtle corner cases
**C++ reference**: ~7,229 LOC → ~4,700 TML

---

## Phase 1: Inference Engine Core (5 items)

- [ ] 1.1 Create `compiler-tml/src/types/infer/mod.tml` — inference engine module root
- [ ] 1.2 Create `compiler-tml/src/types/infer/unify.tml` — union-find with path compression for type variables
- [ ] 1.3 Implement `TypeVar` — fresh type variable generation with unique IDs
- [ ] 1.4 Implement `unify(a: Type, b: Type) -> Outcome[Unit, TypeError]` — recursive structural unification
- [ ] 1.5 Implement `resolve(ty: Type) -> Type` — follow union-find chain to concrete type
- [ ] 1.6 Test: unify(I32, I32) = Ok, unify(I32, Str) = Err, unify(TypeVar, I32) = Ok(I32)

## Phase 2: Expression Type Checking (6 items)

- [ ] 2.1 Create `compiler-tml/src/types/checker/check_expr.tml` — expression type inference dispatcher
- [ ] 2.2 Implement literal inference: integer literals → I32, float → F64, string → Str, bool → Bool
- [ ] 2.3 Implement variable lookup: resolve name in scope chain → return type
- [ ] 2.4 Implement binary ops: infer left/right, unify, lookup operator behavior impl → result type
- [ ] 2.5 Implement field access: infer receiver type, lookup field by name → field type
- [ ] 2.6 Implement index access: infer receiver, infer index, check Index behavior impl → element type

## Phase 3: Call & Method Resolution (5 items)

- [ ] 3.1 Create `compiler-tml/src/types/checker/check_call.tml` — function/method call type checking
- [ ] 3.2 Implement function call: resolve callee, infer args, unify param types, instantiate generics → return type
- [ ] 3.3 Implement method call: infer receiver, search impl blocks for method, resolve self type → return type
- [ ] 3.4 Implement generic instantiation: collect constraints from args, solve for type params, substitute into return type
- [ ] 3.5 Implement operator desugaring: `a + b` → lookup `Add` impl for type of `a`, call `add(a, b)`

## Phase 4: Statement & Control Flow Checking (4 items)

- [ ] 4.1 Create `compiler-tml/src/types/checker/check_stmt.tml` — statement type checking
- [ ] 4.2 Implement `let` binding: infer RHS type, unify with annotation if present, register in scope
- [ ] 4.3 Implement `if/else`: check condition is Bool, infer both branches, unify branch types
- [ ] 4.4 Implement `loop`: check condition is Bool, infer body, handle `break` with value type

## Phase 5: Pattern & When Checking (3 items)

- [ ] 5.1 Create `compiler-tml/src/types/checker/check_pattern.tml` — pattern type checking for `when`
- [ ] 5.2 Implement pattern matching: check each arm pattern against scrutinee type, bind variables
- [ ] 5.3 Implement exhaustiveness checking: verify all enum variants / value ranges covered

## Phase 6: Differential Testing (2 items)

- [ ] 6.1 Infer types for 50 stdlib modules → serialize inferred TypeEnv → compare with C++ output
- [ ] 6.2 Infer types for full test suite → verify zero diffs against C++ inference
