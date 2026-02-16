# Modules with 0% Coverage — Complete Analysis

**Date**: 2026-02-15 (evening update)
**Current Coverage**: 71.2% (2,953/4,147 functions)
**Tests**: 7,883 tests in 616 files — **0 failures**

## Executive Summary

Of **37 modules with 0% coverage** (309 functions):

- ✅ **9 modules** (24 functions) — Behavior definitions only, **don't need tests**
- ⏸️ **3 modules** (12 functions) — **Not testable** (test runner internals, coverage internals, precompiled_symbols)
- 🐛 **10 modules** (47 functions) — **BLOCKED by compiler bugs** (generic monomorphization in test pipeline)
- ❌ **15 modules** (226 functions) — **IMPLEMENTED but no tests** ← FOCUS HERE

### Changes since last update (2026-02-15 morning → evening)

- Coverage rose from 67.8% → 71.2% (+143 functions covered, +173 tests, +14 files)
- **New test files added this session:**
  - `lib/core/tests/fmt/fmt_rt_coverage.test.tml` — 9 tests for Count, FormatSpec, Placeholder, Argument
  - `lib/core/tests/any/any_coverage.test.tml` — 6 tests for TypeId::of, debug_string
  - `lib/core/tests/error/error_coverage.test.tml` — 14 tests for Error, SimpleError, ErrorChain
  - `lib/core/tests/hash/hash_bytes.test.tml` — tests for hash bytes functions
  - `lib/core/tests/result/result_coverage.test.tml` — 8 tests for Outcome Debug/Display, unwrap_or_default, contains
  - `lib/std/tests/types/types_coverage.test.tml` — 10 tests for free functions (unwrap, expect, ok_or, etc.)
  - `lib/core/tests/alloc/shared_coverage.test.tml` — 2 tests for is_unique, shared() free fn
  - `lib/core/tests/alloc/sync_coverage.test.tml` — 4 tests for new, get, strong_count, is_unique, sync() free fn
  - `lib/core/tests/ops/range_coverage.test.tml` — 10 tests for Range, RangeInclusive, RangeFull, Bound free fns
  - `lib/core/tests/ops/ops_bit_assign.test.tml` — 23 tests for bitwise assign operations
  - `lib/core/tests/ops/ops_bit_trait_methods.test.tml` — 42 tests for bit trait methods
  - `lib/core/tests/slice/slice_rotate_copy.test.tml` — 4 tests for rotate_left/right, split_at, copy_from_slice
- **crash-debug-improvements task COMPLETED** — archived to `archive/2026-02-15-crash-debug-improvements`
  - Deliberate STACK_OVERFLOW and write AV tests verified VEH handler, `_resetstkoflw()`, abort-suite logic
  - Crash test files moved to `.sandbox/crash_verification_tests/` (not in coverage suite)
- **Modules that improved:**
  - `fmt/rt` — was ~33%, now **55.6%** (10/18)
  - `any` — was ~35%, now **47.8%** (11/23)
  - `error` — was ~31%, now **50.0%** (16/32)
  - `result` — was ~67%, now **75.8%** (25/33)
  - `types` — was ~65%, now **95.7%** (22/23)
  - `alloc/shared` — was ~56%, now **68.8%** (11/16)
  - `alloc/sync` — was ~56%, now **68.8%** (11/16)
  - `ops/range` — was ~67%, now **80.5%** (33/41)
  - `ops/bit` — was ~65%, now **76.1%** (70/92)
  - `hash` — was ~58%, now **65.4%** (34/52)

### Compiler bugs identified (blocking 10 modules)

**Generic monomorphization bug in test pipeline**: Adapters with `type Item = B` where `B` is an extra impl generic not in the struct generate `Maybe__B` instead of `Maybe__I32` in LLVM IR. Works in legacy `run` pipeline but fails in query-based `test` pipeline.

Blocked modules: `filter_map`, `scan`, `map_while`, `empty`, `repeat_with`, `successors`, `cloned`, `copied`, `peekable` (also has nested Maybe codegen bug), `intersperse` (Counter::Item not resolved).

### Additional codegen bugs discovered this session

- **`ref T` parameter codegen**: `Range::contains(this, item: ref T)` generates `ptr 3` instead of passing address — blocks testing all `contains()` methods on Range types
- **Generic method crashes**: `Shared::duplicate()` and `Shared::try_unwrap()` crash with ACCESS_VIOLATION (null pointer READ at 0x4) — generic codegen for reference-counted types
- **Class method dispatch**: Exception classes using `class...extends` can't be tested — class vtable dispatch codegen issues

---

## 1. Behavior Definitions Only (9 modules, 24 functions)

**Don't need tests** — these are just interface definitions:

1. `convert` — 6 functions (From, Into, TryFrom, TryInto, AsRef, AsMut)
2. `iter/traits/double_ended` — 5 functions
3. `ops/function` — 3 functions (Fn, FnMut, FnOnce)
4. `ops/deref` — 2 functions (Deref, DerefMut)
5. `ops/index` — 2 functions (Index, IndexMut)
6. `iter/traits/exact_size` — 2 functions
7. `iter/traits/extend` — 1 function
8. `iter/traits/from_iterator` — 1 function
9. `iter/traits/into_iterator` — 1 function

**Action**: None. Behaviors are tested through their implementations.

---

## 2. Not Testable (3 modules, 12 functions)

1. **`runner`** — 9 functions. Test runner internals — not a user-facing module.
2. **`coverage`** — 2 functions. Coverage tracking internals used by the test harness itself.
3. **`precompiled_symbols`** — 1 function. Has tests but coverage tracking doesn't register it — not a real gap.

---

## 3. BLOCKED BY COMPILER BUGS (10 modules, 47 functions)

These adapters/sources have working implementations but fail in the test pipeline due to generic monomorphization bugs. They work correctly via `tml run`.

**Iterator Adapters** (7 modules, 21 functions):
- `iter/adapters/filter_map` — 2 functions
- `iter/adapters/scan` — 2 functions
- `iter/adapters/map_while` — 2 functions
- `iter/adapters/intersperse` — 2 functions
- `iter/adapters/cloned` — 3 functions
- `iter/adapters/copied` — 3 functions
- `iter/adapters/peekable` — 7 functions (also has nested Maybe[Maybe[T]] codegen bug)

**Iterator Sources** (3 modules, 8 functions):
- `iter/sources/empty` — 3 functions
- `iter/sources/repeat_with` — 3 functions
- `iter/sources/successors` — 2 functions

**Bug**: `type Item = B` where B is an extra generic on the impl block (not on the struct) causes the test pipeline to generate `Maybe__B` instead of `Maybe__I32`. The legacy `run` pipeline monomorphizes correctly.

**Action**: Fix compiler bug in query pipeline's generic monomorphization, then add tests.

---

## 4. IMPLEMENTED BUT NO TESTS (15 modules, 226 functions)

**HIGH PRIORITY** — These modules have implemented code but 0% coverage.

### 4.1 Network/Pending (3 modules, 58 functions) — HIGH IMPACT

1. **`net/pending/parser`** — 19 functions (HTTP/protocol parsing)
2. **`net/pending/async_tcp`** — 25 functions (async TCP)
3. **`net/pending/async_udp`** — 14 functions (UDP async variants)

**Impact**: Network modules without tests = PRODUCTION BUG RISK

---

### 4.2 Iterators — Remaining (5 modules, 63 functions) — MEDIUM IMPACT

**Traits** (2 modules, 41 functions):
- `iter/traits/accumulators` — 22 functions (sum, product, etc.)
- `iter/traits/iterator` — 19 functions (default trait methods)

**Remaining adapters** (1 module, 4 functions):
- `iter/adapters/rev` — 4 functions (requires DoubleEndedIterator)

**Array/Slice iterators** (2 modules, 38 functions):
- `array/iter` — 19 functions
- `slice/iter` — 19 functions

**Note**: `rev` requires DoubleEndedIterator implementations; `array/iter` and `slice/iter` need array/slice to implement IntoIterator.

---

### 4.3 Remaining adapters with partial coverage

These adapters have SOME coverage but the `size_hint` function remains untested:

- `iter/adapters/chain` — 2/3 (66.7%) — missing `size_hint`
- `iter/adapters/enumerate` — 2/3 (66.7%) — missing `size_hint`
- `iter/adapters/zip` — 2/3 (66.7%) — missing `size_hint`
- `iter/adapters/fuse` — 2/3 (66.7%) — missing `size_hint`
- `iter/adapters/cycle` — 2/3 (66.7%) — missing `size_hint`
- `iter/adapters/inspect` — 2/3 (66.7%) — missing `size_hint`

---

### 4.4 JSON Serialization (1 module, 23 functions) — HIGH IMPACT

- **`json/serialize`** — 23 functions
  - Automatic struct/enum serialization to JSON
  - JSON deserialization to TML types

**Impact**: Serialization without tests = POSSIBLE DATA CORRUPTION

---

### 4.5 Other Modules (6 modules, 44 functions)

1. **`array/ascii`** — 9 functions (ASCII operations on arrays — **BLOCKED**: method resolution doesn't discover impl blocks from submodules)
2. **`iter/adapters/flatten`** — 2 functions
3. **`iter/adapters/flat_map`** — 2 functions
4. **`thread/scope`** — 9 functions (scoped threads)
5. **`cell/lazy`** — 5 functions (lazy cell initialization)
6. **`object`** — 1 function (`reference_equals` — note: Object class methods work, see object.test.tml 7/7)

---

## Recommended Action Plan

### Phase 1: Fix Compiler Bug — Generic Monomorphization (unblocks 10 modules, 47 functions)

Fix the test pipeline's generic monomorphization so `type Item = B` properly resolves to concrete types. This unblocks filter_map, scan, map_while, intersperse, cloned, copied, peekable, empty, repeat_with, successors.

### Phase 2: Iterator Traits (~63 functions)

1. `iter/traits/accumulators` — 22 functions (sum, product, etc.)
2. `iter/traits/iterator` — 19 functions (default trait methods: count, last, nth, etc.)
3. `array/iter` + `slice/iter` — 38 functions (depends on IntoIterator impl)

### Phase 3: High Risk Modules (~23 functions)

1. `json/serialize` — 23 functions (CRITICAL)

### Phase 4: Network Pending (~58 functions)

1. `net/pending/parser` — 19 functions
2. `net/pending/async_tcp` — 25 functions
3. `net/pending/async_udp` — 14 functions

### Phase 5: Remaining (~15 functions)

1. `thread/scope` — 9 functions
2. `cell/lazy` — 5 functions
3. `object` — 1 function

---

## Estimated Impact

**If all 15 testable modules get coverage:**
- Current coverage: 71.2% (2,953/4,147)
- Additional functions: +226
- **Projected coverage: 76.6% (3,179/4,147)**

**If compiler bug is also fixed (+10 modules):**
- Additional functions: +47
- **Projected coverage: 77.8% (3,226/4,147)** ← EXCEEDS 75% TARGET

**To reach 75% target (3,110 functions needed):**
- Need to cover +157 more functions
- Best candidates (partially covered, easy wins):
  - `intrinsics` — 32/86 (37.2%), +54 functions available
  - `net/ip` — 42/58 (72.4%), +16 functions available
  - `hash` — 34/52 (65.4%), +18 functions available
  - `option` — 15/28 (53.6%), +13 functions available
  - `slice` — 18/25 (72.0%), +7 functions available
  - `ops/bit` — 70/92 (76.1%), +22 functions available

---

## Progress Tracker

| Date | Coverage | Functions | Tests | Files | Key Changes |
|------|----------|-----------|-------|-------|-------------|
| 2026-02-12 | 43.7% | 1,814/4,150 | ~4,000 | ~350 | Baseline |
| 2026-02-13 | 50.3% | 2,088/4,150 | ~5,500 | ~480 | Phase 1 compiler fixes |
| 2026-02-14 | 62.4% | 2,596/4,161 | 7,616 | 603 | Crypto 100%, coverage FFI rewrite |
| 2026-02-15 AM | 67.8% | 2,810/4,147 | 7,710 | 602 | Iterator adapters, Chain/Fuse fix |
| 2026-02-15 PM | 71.2% | 2,953/4,147 | 7,883 | 616 | fmt/rt, any, error, result, types, alloc, ops/range, ops/bit |

---

## Important Notes

1. **Compiler bugs blocking 10 iterator modules** — Generic monomorphization in the test (query) pipeline generates `Maybe__B` instead of `Maybe__I32`. Works in legacy `run` pipeline. Fixing this unblocks 47 functions.

2. **All crypto modules at 100% coverage** — 14 modules, 449 tests, complete.

3. **Iterator Chain/Fuse mutation bug FIXED** — `Maybe[I]` wrapping caused pattern match to extract copies; mutations to inner iterator were lost. Fixed with boolean flag approach.

4. **Coverage requires `--no-cache`** — Cached test DLLs lack coverage instrumentation. Always use `--no-cache --coverage` for accurate results.

5. **Deliberate crash tests moved to .sandbox/** — `crash_stackoverflow.test.tml` and `crash_write_av.test.tml` verify VEH crash handling infrastructure but would block coverage runs (coverage aborts on any failure). These tests are kept in `.sandbox/crash_verification_tests/` and are not part of the regular test suite.

6. **`ref T` parameter codegen bug** — Affects all `Range::contains()` family of methods. The codegen passes an integer literal where a pointer is expected. This blocks testing ~10 range `contains()` functions.

7. **Modules that moved OUT of 0% since 2026-02-12:**
   - `num/overflow` — now 68.4%
   - `fmt/num` — now 85.7%
   - `crypto/*` — all 14 modules now 100%
   - `iter/adapters/step_by` — now 100%
   - `iter/adapters/skip_while` — now 100%
   - `iter/adapters/take_while` — now 100%
   - `iter/adapters/chain` — now 66.7%
   - `iter/adapters/enumerate` — now 66.7%
   - `iter/adapters/zip` — now 66.7%
   - `iter/adapters/fuse` — now 66.7%
   - `iter/adapters/cycle` — now 66.7%
   - `iter/adapters/inspect` — now 66.7%
   - `iter/sources/from_fn` — now 100%
   - `iter/sources/once_with` — now 66.7%
   - `slice/sort` — now 33.3%
   - `slice/cmp` — now 66.7%

8. **59 modules at 100% coverage** — up from ~45 at start of day.
