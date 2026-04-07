# Proposal: Fix 214 Test Compile Failures

**Task**: phase0g_fix-214-compile-failures
**Status**: Planned
**Priority**: P0 — 12% of test suite fails to compile
**Estimated effort**: 2–4 weeks
**Risk**: High — touches compiler core (type checker, codegen, meta preloader)

## Problem

214 out of 1753 test suites (12%) fail to compile. These are pre-existing compiler bugs
across 7 root causes. While 88% of tests pass, a 12% failure rate undermines confidence
in the compiler and blocks the self-hosting effort (Phase 12+).

## Root Cause Breakdown

| Root Cause | Tests | % of Total | Subsystem |
|-----------|-------|-----------|-----------|
| MODULE_NOT_FOUND (std::http) | 119 | 56% | Meta preloader / module resolution |
| UNKNOWN_METHOD (closures/iterators) | 24 | 11% | Type checker / method resolution |
| TYPE_RETURNS_UNIT (Maybe/Outcome) | 18 | 8% | Type checker / generic return types |
| LINK failures (missing symbols) | 13 | 6% | Test runtime linker |
| UNDEF_SYMBOL (generic instantiation) | 12 | 6% | Codegen / monomorphization |
| GEP_UNSIZED (unsized types) | 10 | 5% | Codegen / type layouts |
| IR_TYPE_MISMATCH (Maybe[T] layout) | 8 | 4% | Codegen / generic type naming |
| Other (parse, overflow, ref) | 10 | 5% | Various |

## Proposed Strategy

Fix in order of impact (highest test count first):

1. **MODULE_NOT_FOUND (119)** — Fix meta preloader to handle deeply nested module paths.
   Single fix in env_module_loading.cpp, unblocks 56% of failures.

2. **UNKNOWN_METHOD (24)** — Fix method resolution for closure parameters and iterator
   combinators. Likely a type checker bug with generic closure types.

3. **TYPE_RETURNS_UNIT (18)** — Fix generic method return type propagation for
   Maybe[T] and Outcome[T,E]. Type checker loses generic type parameter in return position.

4. **LINK (13)** — Add missing C runtime objects to test runtime archive.

5. **UNDEF_SYMBOL (12)** — Fix codegen to emit all generic function instantiations.

6. **GEP_UNSIZED (10)** — Ensure struct types are fully defined before GEP.

7. **IR_TYPE_MISMATCH (8)** — Fix Maybe[T] monomorphization to produce correct LLVM type names.

## Files to Modify

- `compiler/src/types/env_module_loading.cpp` — module search depth
- `compiler/src/types/checker/expr_call_method.cpp` — method resolution
- `compiler/src/types/checker/expr_call_method_types.cpp` — generic return types
- `compiler/src/testing/testing_compile.cpp` — test runtime archive
- `compiler/src/codegen/mir/instructions_call.cpp` — generic instantiation
- `compiler/src/codegen/mir/mir_types.cpp` — type layouts and naming

## Success Criteria

- 0 compile failures (1753/1753 tests compile)
- All previously passing tests still pass (no regressions)

## Dependencies

- None (can start immediately)
- Blocks: Phase 12 confidence, coverage tracking accuracy
