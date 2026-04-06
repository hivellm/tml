# Tasks: Fix 214 Test Compile Failures — Root Cause Analysis

**Status**: Planned (0/25)
**Depends on**: None (can start immediately)
**Blocks**: Test coverage accuracy, Phase 12 confidence
**Duration**: 2–4 weeks
**Risk**: High — multiple compiler bugs across different subsystems
**Measured**: 1539/1753 tests pass (88%), 214 compile failures across 7 root causes

---

## Root Cause 1: MODULE_NOT_FOUND — std::http submodules (119 tests)

The meta preloader fails to index deeply nested HTTP submodules (app/, client/, framework/, middleware/, protocol/, request/, router/, server/, websocket/). Tests that import std::http fail with `[T027] Module 'std::http::*' not found`.

- [ ] 1.1 `compiler/src/types/env_module_loading.cpp` — investigate why nested subdirectories under std::http are not indexed by the meta preloader
- [ ] 1.2 `compiler/src/types/env_module_load.cpp` — fix module search to recurse into arbitrarily deep subdirectories
- [ ] 1.3 Verify: all std::http submodules discoverable after fix
- [ ] 1.4 Re-run affected tests — target 119 → 0 MODULE_NOT_FOUND failures

## Root Cause 2: UNKNOWN_METHOD — closure/iterator methods (24 tests)

Methods like `f`, `pred`, `gen`, `flatten`, `also`, `zip` not found. These are higher-order function parameters or iterator combinators.

- [ ] 2.1 Investigate: are these methods missing from behaviors, or is the type checker not resolving them?
- [ ] 2.2 `compiler/src/types/checker/expr_call_method.cpp` — check method resolution for closure/generic types
- [ ] 2.3 Fix method resolution for each missing method category
- [ ] 2.4 Re-run affected tests — target 24 → 0

## Root Cause 3: TYPE_RETURNS_UNIT — Maybe/Outcome methods return () (18 tests)

Methods on Maybe[T] and Outcome[T,E] (as_ref, as_mut, inspect, take, transpose, zip_with, ok_or_else) return `()` instead of the correct generic type. Type checker loses the return type.

- [ ] 3.1 `compiler/src/types/checker/expr_call_method.cpp` — trace return type inference for Maybe/Outcome methods
- [ ] 3.2 Check if the issue is in generic method instantiation or in behavior dispatch
- [ ] 3.3 Fix return type propagation for generic methods on Maybe[T] and Outcome[T,E]
- [ ] 3.4 Re-run affected tests — target 18 → 0

## Root Cause 4: LINK failures — missing runtime symbols (13 tests)

Link errors for `tml_process_*` (process spawning), socket symbols, and other runtime functions. The test runtime archive doesn't include all needed object files.

- [ ] 4.1 `compiler/src/testing/testing_compile.cpp` — check which C runtime objects are included in test runtime archive
- [ ] 4.2 Add missing runtime objects (process, net) to test runtime archive
- [ ] 4.3 Re-run affected tests — target 13 → 0

## Root Cause 5: UNDEF_SYMBOL — undefined IR symbols (12 tests)

Functions referenced in IR but never generated: `Arc__I32::new`, `from_json`, `default`, `runtime_type_info`. Generic instantiation or derive codegen not emitting required functions.

- [ ] 5.1 Categorize undefined symbols: generic instantiation vs derive vs manual
- [ ] 5.2 `compiler/src/codegen/mir/instructions_call.cpp` — check generic function instantiation
- [ ] 5.3 Fix codegen to emit all referenced generic instantiations
- [ ] 5.4 Re-run affected tests — target 12 → 0

## Root Cause 6: GEP_UNSIZED — getelementptr on unsized types (10 tests)

LLVM IR error: `base element of getelementptr must be sized`. Codegen produces GEP on an opaque/forward-declared struct type.

- [ ] 6.1 `compiler/src/codegen/mir/mir_types.cpp` — trace which types produce unsized GEP
- [ ] 6.2 Ensure all struct types are fully defined before GEP emission
- [ ] 6.3 Re-run affected tests — target 10 → 0

## Root Cause 7: IR_TYPE_MISMATCH — LLVM type conflicts (8 tests)

`Maybe__T` vs `Maybe__I32` type mismatch, `i32` vs struct type in insertvalue. Generic type monomorphization produces wrong LLVM types.

- [ ] 7.1 `compiler/src/codegen/mir/mir_types.cpp` — investigate Maybe[T] generic instantiation producing wrong struct name
- [ ] 7.2 Fix type naming/layout for monomorphized generics
- [ ] 7.3 Re-run affected tests — target 8 → 0

## Validation

- [ ] V.1 Run full test suite — target: 0 compile failures (1753/1753 pass)
