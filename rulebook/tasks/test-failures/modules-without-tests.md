# Modules with 0% Coverage — Complete Analysis

**Date**: 2026-02-17
**Current Coverage**: 75.7% (3,112/4,113 functions)
**Tests**: 8,912 tests in 768 files — **0 failures**

## Executive Summary

Of **31 modules with 0% coverage** (228 functions):

- ✅ **7 modules** (15 functions) — Behavior definitions only, **don't need tests**
- ⏸️ **3 modules** (12 functions) — **Not testable** (test runner internals, coverage internals, precompiled_symbols)
- 🐛 **10 modules** (47 functions) — **BLOCKED by compiler bugs** (generic monomorphization in test pipeline)
- ❌ **11 modules** (154 functions) — **IMPLEMENTED but no tests** ← FOCUS HERE

### Changes since last update (2026-02-15 → 2026-02-17)

- Coverage rose from 71.2% → 75.7% (+159 functions covered, +1,029 tests, +152 files)
- **75% target ACHIEVED** — 74 modules at 100%, 96 partial, 31 at 0%
- **Phase 3 (stdlib essentials) effectively COMPLETE** — 47/48 items (98%), only Read/Write/Seek behaviors blocked
- **New modules/features implemented:**
  - `std::regex` — Thompson's NFA regex engine with captures (30 tests)
  - `std::datetime` — date/time parsing and formatting (100%)
  - `std::random::ThreadRng` — per-thread PRNG with high-entropy seeding (4 tests)
  - `std::math` — trig, exp, rounding, utility functions (100%)
  - `std::collections::btreemap` — ordered map (100%)
  - `std::collections::btreeset` — ordered set (100%)
  - `std::collections::deque` — double-ended queue (100%)
  - `std::os` — env, cwd, process exec (97.1%)
  - `std::time` — Instant, SystemTime
  - `std::file::bufio` — BufReader, BufWriter, LineWriter
  - `__FILE__`, `__DIRNAME__`, `__LINE__` compile-time constants
  - `Shared[T]` memory leak fix (decrement_count codegen)
- **Modules that reached 100% since 2026-02-15:**
  - `random` — 7/7 (100%)
  - `datetime` — 15/15 (100%)
  - `math` — 28/28 (100%)
  - `collections/btreemap` — 13/13 (100%)
  - `collections/btreeset` — 9/9 (100%)
  - `collections/deque` — 15/15 (100%)
  - `ops/range` — 41/41 (100%)
  - `unicode/unicode_data` — 12/12 (100%)
  - `ptr/alignment` — 8/8 (100%)
  - `num/traits` — 56/56 (100%)
  - `num/integer` — 51/51 (100%)
  - `alloc/global` — 20/20 (100%)
  - `alloc/layout` — 30/30 (100%)

### Compiler bugs identified (blocking 10 modules)

**Generic monomorphization bug in test pipeline**: Adapters with `type Item = B` where `B` is an extra impl generic not in the struct generate `Maybe__B` instead of `Maybe__I32` in LLVM IR. Works in legacy `run` pipeline but fails in query-based `test` pipeline.

Blocked modules: `filter_map`, `scan`, `map_while`, `empty`, `repeat_with`, `successors`, `cloned`, `copied`, `peekable` (also has nested Maybe codegen bug), `intersperse` (Counter::Item not resolved).

### Additional codegen bugs discovered

- **Default behavior method dispatch**: Returns `()` instead of expected type — blocks Read/Write/Seek behaviors (3.6.3), iterator accumulators (sum, product, fold, reduce)
- **`ref T` parameter codegen**: `Range::contains(this, item: ref T)` generates `ptr 3` instead of passing address — blocks testing all `contains()` methods on Range types
- **Generic method crashes**: `Shared::duplicate()` and `Shared::try_unwrap()` crash with ACCESS_VIOLATION — generic codegen for reference-counted types
- **Class method dispatch**: Exception classes using `class...extends` can't be tested — class vtable dispatch codegen issues

---

## 1. Behavior Definitions Only (7 modules, 15 functions)

**Don't need tests** — these are just interface definitions:

1. `convert` — 2 functions (remaining behavior-only signatures)
2. `iter/traits/double_ended` — 4 functions
3. `iter/traits/exact_size` — 1 function
4. `iter/traits/extend` — 1 function (Extend behavior)
5. `iter/traits/from_iterator` — 1 function (FromIterator behavior)
6. `iter/traits/into_iterator` — 1 function (IntoIterator behavior)
7. `ops/function` — not in 0% list (partial coverage through closures)

**Note**: `ops/deref` and `ops/index` now have partial coverage through implementations.

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

## 4. IMPLEMENTED BUT NO TESTS (11 modules, 154 functions)

**HIGH PRIORITY** — These modules have implemented code but 0% coverage.

### 4.1 Network/Pending (3 modules, 58 functions) — HIGH IMPACT

1. **`net/pending/parser`** — 19 functions (HTTP/protocol parsing)
2. **`net/pending/async_tcp`** — 25 functions (async TCP)
3. **`net/pending/async_udp`** — 14 functions (UDP async variants)

**Impact**: Network modules without tests = PRODUCTION BUG RISK

---

### 4.2 Iterators — Remaining (5 modules, 63 functions) — MEDIUM IMPACT

**Traits** (2 modules, 38 functions):
- `iter/traits/accumulators` — 20 functions (sum, product, fold, reduce — **BLOCKED** by default behavior dispatch bug)
- `iter/traits/iterator` — 18 functions (default trait methods: count, last, nth, etc. — **BLOCKED** by default behavior dispatch bug)

**Remaining adapters** (1 module, 4 functions):
- `iter/adapters/rev` — 4 functions (requires DoubleEndedIterator)

**Array/Slice iterators** (2 modules, 38 functions):
- `array/iter` — 19 functions
- `slice/iter` — 19 functions

**Note**: `rev` requires DoubleEndedIterator implementations; `array/iter` and `slice/iter` need array/slice to implement IntoIterator. `accumulators` and `iterator` traits blocked by same default behavior dispatch bug that blocks Read/Write/Seek (3.6.3).

---

### 4.3 JSON Serialization (1 module, 21 functions) — HIGH IMPACT

- **`json/serialize`** — 21 functions
  - Automatic struct/enum serialization to JSON
  - JSON deserialization to TML types

**Impact**: Serialization without tests = POSSIBLE DATA CORRUPTION

---

### 4.4 Other Modules (4 modules, 24 functions)

1. **`array/ascii`** — 9 functions (ASCII operations on arrays — **BLOCKED**: method resolution doesn't discover impl blocks from submodules)
2. **`iter/adapters/flatten`** — 2 functions
3. **`iter/adapters/flat_map`** — 2 functions
4. **`thread/scope`** — 9 functions (scoped threads)
5. **`cell/lazy`** — 5 functions (lazy cell initialization)
6. **`object`** — 1 function (`reference_equals` — note: Object class methods work, see object.test.tml 7/7)

---

## Modules with Partial Coverage — Easy Wins

These modules are partially covered and could reach 100% with targeted tests:

| Module | Covered | Total | Pct | Missing |
|--------|---------|-------|-----|---------|
| `char/methods` | 42 | 43 | 97.7% | 1 function |
| `sync/atomic` | 118 | 121 | 97.5% | 3 functions |
| `json/types` | 74 | 76 | 97.4% | 2 functions |
| `fmt/impls` | 70 | 72 | 97.2% | 2 functions |
| `collections/class_collections` | 68 | 70 | 97.1% | 2 functions |
| `os` | 33 | 34 | 97.1% | 1 function |
| `time` | 29 | 31 | 93.5% | 2 functions |
| `regex` | 20 | 22 | 90.9% | 2 functions |

---

## Recommended Action Plan

### Phase 1: Fix Compiler Bug — Default Behavior Method Dispatch

Fixes `()` return type when calling default behavior methods on concrete types. Unblocks:
- `iter/traits/accumulators` — 20 functions (sum, product, fold, reduce)
- `iter/traits/iterator` — 18 functions (count, last, nth, etc.)
- Read/Write/Seek behaviors (3.6.3)

### Phase 2: Fix Compiler Bug — Generic Monomorphization (unblocks 10 modules, 47 functions)

Fix the test pipeline's generic monomorphization so `type Item = B` properly resolves to concrete types. Unblocks filter_map, scan, map_while, intersperse, cloned, copied, peekable, empty, repeat_with, successors.

### Phase 3: High Risk Modules (~21 functions)

1. `json/serialize` — 21 functions (CRITICAL)

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

**If all 11 testable modules get coverage:**
- Current coverage: 75.7% (3,112/4,113)
- Additional functions: +154
- **Projected coverage: 79.4% (3,266/4,113)**

**If compiler bugs are also fixed (+10 modules + 38 trait functions):**
- Additional functions: +85
- **Projected coverage: 81.5% (3,351/4,113)**

---

## Progress Tracker

| Date | Coverage | Functions | Tests | Files | Key Changes |
|------|----------|-----------|-------|-------|-------------|
| 2026-02-12 | 43.7% | 1,814/4,150 | ~4,000 | ~350 | Baseline |
| 2026-02-13 | 50.3% | 2,088/4,150 | ~5,500 | ~480 | Phase 1 compiler fixes |
| 2026-02-14 | 62.4% | 2,596/4,161 | 7,616 | 603 | Crypto 100%, coverage FFI rewrite |
| 2026-02-15 AM | 67.8% | 2,810/4,147 | 7,710 | 602 | Iterator adapters, Chain/Fuse fix |
| 2026-02-15 PM | 71.2% | 2,953/4,147 | 7,883 | 616 | fmt/rt, any, error, result, types, alloc, ops/range, ops/bit |
| 2026-02-16 | 75.1% | 3,005/4,000 | 8,500+ | 700+ | Math, time, os, collections, crypto tests |
| **2026-02-17** | **75.7%** | **3,112/4,113** | **8,912** | **768** | **Phase 3 complete: regex captures, ThreadRng, datetime, BTreeMap/Set, Deque, BufIO, Process** |

---

## Important Notes

1. **75% coverage target ACHIEVED** — 74 modules at 100% coverage. Phase 2 gate met.

2. **Phase 3 (stdlib essentials) effectively COMPLETE** — 47/48 items done. Only Read/Write/Seek behaviors remain, blocked by default behavior method dispatch compiler bug.

3. **Compiler bugs blocking 10 iterator modules** — Generic monomorphization in the test (query) pipeline generates `Maybe__B` instead of `Maybe__I32`. Works in legacy `run` pipeline.

4. **Default behavior method dispatch bug** — Returns `()` instead of expected type. Blocks iterator accumulators, Read/Write/Seek behaviors, and many default trait methods. This is the single most impactful compiler bug remaining.

5. **All crypto modules at 100% coverage** — 14 modules, 449 tests, complete.

6. **New 100% modules since 2026-02-15** — random, datetime, math, btreemap, btreeset, deque, ops/range, unicode/unicode_data, ptr/alignment, num/traits, num/integer, alloc/global, alloc/layout (13 new modules at 100%).

7. **Coverage requires `--no-cache`** — Cached test DLLs lack coverage instrumentation. Always use `--no-cache --coverage` for accurate results.

8. **`ref T` parameter codegen bug** — Affects all `Range::contains()` family of methods. The codegen passes an integer literal where a pointer is expected.

9. **Modules that moved OUT of 0% since 2026-02-12:**
   - `num/overflow` — now 68.4%
   - `fmt/num` — now 85.7%
   - `crypto/*` — all 14 modules now 100%
   - `random` — now 100% (was 0%)
   - `datetime` — now 100% (new)
   - `math` — now 100% (new)
   - `collections/btreemap` — now 100% (new)
   - `collections/btreeset` — now 100% (new)
   - `collections/deque` — now 100% (new)
   - `os` — now 97.1%
   - `time` — now 93.5%
   - `regex` — now 90.9% (new)
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
   - `slice/cmp` — now 100% (was 66.7%)

10. **74 modules at 100% coverage** — up from 59 on 2026-02-15.
