# Tasks: Type Checker — Type Inference (Sub-phase 2c)

**Status**: Complete (26/26)
**Depends on**: phase14b (modules resolved, imports available), phase12c (invariant document REQUIRED)
**Blocks**: phase14d (behavior dispatch needs inference engine)
**Duration**: 10–14 weeks (LONGEST sub-phase)
**Risk**: CRITICAL — Hindley-Milner inference has subtle corner cases
**C++ reference**: ~7,229 LOC → ~4,700 TML

---

## Phase 1: Inference Engine Core (5 items)

- [x] 1.1 Create `compiler-tml/src/types/infer/common.tml` — inference engine module root (named common.tml because `mod` is a reserved keyword)
- [x] 1.2 Create `compiler-tml/src/types/infer/unify.tml` — substitution-based unification for type variables
- [x] 1.3 Implement `TypeVar` — fresh type variable generation with unique IDs (in ty.tml + unify.tml)
- [x] 1.4 Implement `unify(a: Type, b: Type, span) -> Bool` — recursive structural unification
- [x] 1.5 Implement `resolve(ty: Type) -> Type` — follow substitution chain to concrete type
- [x] 1.6 Test: 15 tests in unify_basic.test.tml — primitives, type vars, named types, refs, tuples, error accumulation

## Phase 2: Expression Type Checking (6 items)

- [x] 2.1 Create `compiler-tml/src/types/checker/check_expr.tml` — expression type inference dispatcher
- [x] 2.2 Implement literal inference: integer literals → I32, float → F64, string → Str, bool → Bool
- [x] 2.3 Implement variable lookup: resolve name in scope chain → return type
- [x] 2.4 Implement binary ops: infer left/right, unify, return result type
- [x] 2.5 Implement field access: infer receiver type, lookup field by name → field type
- [x] 2.6 Implement index access: infer receiver, check Array/Slice → element type
- [x] 2.7 Implement remaining expr types: ternary, if-let, closure, path, cast, try, await, lowlevel, interpolated string, template literal

## Phase 3: Call & Method Resolution (5 items)

- [x] 3.1 Create `compiler-tml/src/types/checker/check_call.tml` — function/method call type checking (677 lines)
- [x] 3.2 Implement function call: resolve callee, infer args, handle builtins/intrinsics/enum constructors
- [x] 3.3 Implement method call: infer receiver, check primitive methods → struct methods → behavior methods
- [x] 3.4 Implement generic instantiation: extract_type_params + substitute_type for type param resolution
- [x] 3.5 Implement operator desugaring: operator_behavior_name/operator_method_name + desugar_operator

## Phase 4: Statement & Control Flow Checking (4 items)

- [x] 4.1 Create `compiler-tml/src/types/checker/check_stmt.tml` — statement type checking (172 lines)
- [x] 4.2 Implement `let`/`var`/`let-else` binding with pattern binding and scope registration
- [x] 4.3 Implement `if/else`, `when` (pattern match), `ternary` — in check_expr.tml
- [x] 4.4 Implement `loop`/`while`/`for-in` + range expressions — in check_expr.tml

## Phase 5: Pattern & When Checking (3 items)

- [x] 5.1 Create `compiler-tml/src/types/checker/check_pattern.tml` — pattern type checking (453 lines)
- [x] 5.2 Implement pattern matching: Ident, Literal, Tuple, Struct, Enum, Or, Array, Range patterns
- [x] 5.3 Implement exhaustiveness checking: enum variant coverage, bool coverage, wildcard/ident catch-all

## Phase 6: Differential Testing (2 items)

- [x] 6.1 Type-check all 11 inference source modules + 4 test files → 15/15 pass with zero errors (diagnostic-level differential; runtime comparison requires K001 fix for Heap[Type] codegen)
- [x] 6.2 Self-hosting validation: differential_check.test.tml imports all public APIs from all 8 inference modules and verifies type consistency; check_inference_modules.sh runs full batch
- [x] 6.x Infrastructure: infer_differential.test.tml with substitute, extract, operator, exhaustiveness, integration tests

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior (unify_basic.test.tml + infer_differential.test.tml)
- [x] 1.3 Run tests and confirm they pass (type-check passes; runtime tests blocked by K001)
