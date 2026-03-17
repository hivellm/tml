# Plan: Fix the 4 codegen bugs failing in tests

## Diagnosis

All 4 bugs are **PRE-EXISTING in codegen** — the new test system works correctly.
The last known good state was Feb 28: 1096 passed, 45 failed, 73.47% coverage.

The 45 failures break down into 4 categories with identified root causes:

---

## Bug 1: `toowned_assoc` (1 test) — EASY, ~10 lines

**Error**: `'%this' defined with type 'i32' but expected 'ptr'`

**Cause**: `compiler/src/codegen/llvm/decl/impl.cpp:234-246` — when `this: ref This` with `This = I32`, the codegen only checks `is_mut_this` to decide whether to use `ptr`. But `ref This` is a reference by TYPE, not by the mut flag. Result: defines `(i32 %this)` but the body generates `load i32, ptr %this`.

**Fix**: At line ~234 of `impl.cpp`, add a check if the first parameter type is `RefType`. If so, force `this_type = "ptr"`.

---

## Bug 2: `union_basic` (1 test) — EASY (workaround), ~15 lines

**Error**: `invalid type for undef constant` — `%struct.IntOrPtr undef`

**Cause**: HIR builder and THIR lowering DO NOT support `UnionDecl`. The type is never declared in LLVM IR. The MIR codegen tries to use `insertvalue` (wrong for unions) with a type that doesn't exist.

**Fix (quick workaround)**: In `query_core.cpp:551-603`, add detection of `UnionDecl` that forces fallback to AST codegen (which already supports unions via `gen_union_decl`).

---

## Bug 3: `core/mem/*` (9 tests) — MEDIUM, ~20 lines

**Error**: `assertion failed at :19: into_inner should return original value`

**Cause**: `compiler/src/codegen/llvm/expr/method_impl.cpp:585` — when calling `ManuallyDrop::into_inner(slot)`, the call site assumes the first arg is always `ptr` for non-primitive types. But `into_inner` has `slot: ManuallyDrop[T]` (by-value, not `this`). Defines `(%struct.ManuallyDrop__I32 %slot)` but calls with `(ptr %t1)`. ABI mismatch → garbage.

**Fix**: At the call site (~line 585), check the function signature. If the first parameter is NOT `this`/`self`, use the actual struct type instead of hardcoded `"ptr"`.

---

## Bug 4: `core/any/*` (4 tests) — HARD, ~50+ lines

**Error**: `UNRESOLVED reference: @tml_N4core3any2ofE`

**Cause**: `TypeId::of[T]()` is a generic static function. The lazy library system (`runtime_modules.cpp`) finds the reference in pending but can't instantiate it because it doesn't do monomorphization of generics.

**Fix**: In the lazy resolution pass of `runtime_modules.cpp`, when a reference is in pending but is generic, perform instantiation with the concrete types derived from the call site.

---

## Execution order (easiest to hardest)

1. **Bug 1 (toowned_assoc)** — Fix in `impl.cpp` — 5 min
2. **Bug 2 (union_basic)** — Workaround in `query_core.cpp` — 5 min
3. **Bug 3 (core/mem)** — Fix in `method_impl.cpp` — 15 min
4. **Bug 4 (core/any)** — Fix in `runtime_modules.cpp` — 30 min

**AFTER all 4 fixes**: rebuild compiler ONCE, run tests ONCE.

## Anti-slowness strategy

- **DO NOT rebuild between each fix** — make all 4 C++ fixes first
- **Rebuild the compiler 1 time** after all edits
- **Run tests 1 time** after rebuild to validate everything
- If any fix is wrong, correct and rebuild again
- Goal: maximum 2 build+test cycles
