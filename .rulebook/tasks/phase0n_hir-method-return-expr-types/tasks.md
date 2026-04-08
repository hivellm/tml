# Tasks: HIR Builder — Use Type Checker Expression Map for Method Return Types

**Status**: Complete (10/10 done)
**Depends on**: phase0j (archived — fixes B-03/B-04 in type checker)
**Blocks**: Removal of `extract_type_params_from_args` from HIR builder
**Duration**: 1 day
**Risk**: Low — additive; fallback to existing substitution when expr_types_ has no entry

---

## 1. Investigation

- [x] 1.1 Read `compiler/src/types/env.hpp` — find `expr_types_` map and accessor API.
- [x] 1.2 Read `compiler/src/hir/hir_builder.hpp` — confirm `HirBuilder` has `TypeEnv` access and `get_expr_type()` can be extended to consult `expr_types_`.

## 2. Implementation

- [x] 2.1 In `hir_builder.cpp`/`get_expr_type()`: for `MethodCallExpr`, check `type_env_.get_expr_type(expr)` first; if concrete, return it directly.
- [x] 2.2 In `hir_builder_expr.cpp`/`lower_method_call`: before `substitute_method_generics`, try `get_expr_type(call_as_expr)`; if concrete, use as `return_type` and bypass substitution.
- [x] 2.3 Remove step 3 from `substitute_method_generics` (the `extract_type_params_from_args` loop, ~lines 643-649).
- [x] 2.4 Remove `extract_type_params_from_args` static function (~lines 49-156).

## 3. Verification

- [x] 3.1 Build: `scripts\build.bat` — clean build. ✓
- [x] 3.2 `suite="core/option"` — 24/34 passed (same pre-existing 10 failures). ✓
- [x] 3.3 `suite="core/iter"` 95/101 (identical to baseline), `suite="core/fmt"` 53/53. ✓

## 4. Tail (mandatory — enforced by rulebook v5.3.0)

- [x] 4.1 Remove `NOTE(phase0j-hir-cleanup)` comment from `hir_builder_expr.cpp`.
- [x] 4.2 Tests pass per 3.1–3.3.
- [x] 4.3 Commit: `refactor(hir): read method return types from type checker expr map, remove extract_type_params_from_args (phase0n)`.
