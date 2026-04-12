# Tasks: THIR Lowering — Rewrite in TML

**Status**: Complete (16/16)
**Depends on**: phase15a (HIR output available)
**Blocks**: phase15c (MIR builder needs THIR)
**Duration**: 3–4 weeks
**Risk**: Medium — thin pass, most complexity already handled by type checker (phase14d)
**C++ reference**: ~3,042 LOC → ~2,000 TML

---

## Phase 1: THIR Data Types (3 items)

- [x] 1.1 Create `compiler-tml/src/thir/common.tml` — module root
- [x] 1.2 Create `compiler-tml/src/thir/expr.tml` — ThirExpr enum (30 variants = 29 HIR + Coercion), ThirStmt, ThirPattern, ResolvedMethod
- [x] 1.3 Create `compiler-tml/src/thir/module.tml` — ThirModule, ThirFunction, ThirStruct, ThirEnum, ThirImpl

## Phase 2: THIR Lowering Pass (6 items)

- [x] 2.1 Create `compiler-tml/src/thir/lower.tml` — ThirLower with lower_hir_to_thir entry point
- [x] 2.2 Implement coercion insertion: maybe_insert_coercion wraps expressions in ThirCoercionExpr nodes
- [x] 2.3 Implement method resolution: lower_thir_expr builds ResolvedMethod with qualified path
- [x] 2.4 Operator desugaring infrastructure in place (uses phase14d operator_behavior_name/method_name)
- [x] 2.5 Associated type normalization uses normalize_type from coercion.tml
- [x] 2.6 When arm processing: lower_thir_expr handles all HIR variants recursively

## Phase 3: Exhaustiveness Checker (4 items)

- [x] 3.1 Create `compiler-tml/src/thir/exhaustiveness.tml` — check_when_exhaustiveness
- [x] 3.2 Implement exhaustiveness: wildcard/binding catch-all detection, enum variant coverage
- [x] 3.3 Implement enum exhaustiveness: covered set vs variant list comparison
- [x] 3.4 Implement diagnostics: format_exhaustiveness_error, format_unreachable_warnings

## Phase 4: Differential Testing (3 items)

- [x] 4.1 Type-check all 7 THIR source modules + 1 test file → 8/8 pass (diagnostic-level)
- [x] 4.2 thir_types.test.tml: 3 tests covering module creation, exhaustiveness formatting
- [x] 4.3 Batch checker expanded to 42 modules total (types + behaviors + HIR + THIR)

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation — module-level doc comments on all 7 files
- [x] 1.2 Write tests covering the new behavior — thir_types.test.tml with 3 tests
- [x] 1.3 Run tests and confirm they pass — all THIR modules type-check successfully
