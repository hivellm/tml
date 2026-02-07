# Proposal: Increase Library Test Coverage from 35.9% to 70%+

## Status: PROPOSED

## Why

The TML standard library (lib/core, lib/std) currently has only **35.9% function coverage** (1359/3790 functions), with **77 modules at 0% coverage**. This means the majority of library code is untested, leaving critical bugs invisible and making refactoring dangerous. Core language primitives like type conversions, arithmetic operators, string operations, and comparisons have critically low coverage, which undermines confidence in the entire language ecosystem.

Key issues:
- **Core type operations untested**: `convert` (0%), `ops/arith` (9.2%), `fmt/impls` (2.8%), `cmp` (17%), `str` (13%)
- **Iteration infrastructure gaps**: `iter/range` (3%), `array/iter` (0%), `iter/traits/accumulators` (0%)
- **Collections completely untested**: `collections/class_collections` (0/60 functions)
- **Hash/Bit operations low**: `hash` (25%), `ops/bit` (21.2%)
- **Smart pointer gaps**: `alloc/heap` (38.5%), `alloc/shared` (56.2%), `alloc/sync` (56.2%)
- **Standard library modules at zero**: `json/serialize`, `fmt/float`, `cache`, `arena`

## What Changes

### New Test Files

Tests will be created incrementally (1-3 tests at a time, run immediately) across 10 phases covering the priority modules. All tests go into the existing test directories under `tests/`.

### Target Coverage

| Tier | Current | Target | Modules |
|------|---------|--------|---------|
| Tier 1: Core Language | ~15% avg | 80%+ | convert, ops/arith, fmt/impls, cmp, str, ops/bit, hash, intrinsics |
| Tier 2: Iteration | ~5% avg | 70%+ | iter/range, array/iter, iter/traits/accumulators, array |
| Tier 3: Collections & Data | ~5% avg | 60%+ | collections/class_collections, json/serialize, fmt/float, pool, cache |
| Tier 4: Smart Pointers | ~40% avg | 80%+ | alloc/heap, alloc/shared, alloc/sync, alloc, alloc/global |
| Tier 5: Concurrency | ~30% avg | 60%+ | sync/atomic, task |

### Overall Goal

Raise library coverage from **35.9% to 70%+** (~2660/3790 functions covered), adding approximately **1300 new function coverage entries**.

## Impact

- Affected specs: lib/core, lib/std modules
- Affected code: Test files only (no library code changes unless bugs found)
- Breaking change: NO
- User benefit: More reliable standard library, safer refactoring, better documentation through tests

## Risks

- Some modules at 0% may have codegen/runtime bugs that need fixing before tests can pass
- `net/pending/*` and `crypto/*` modules may require external dependencies or system resources
- Arena, pool, and cache modules may need lowlevel/FFI features that are still incomplete

## Approach

1. Follow the MANDATORY incremental test development workflow (1-3 tests at a time)
2. Run individual test files immediately after writing
3. Fix compiler/codegen bugs as they surface (these are bonus fixes)
4. Run `tml test --coverage` after completing each phase to track progress
