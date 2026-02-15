# Modules with 0% Coverage — Complete Analysis

**Date**: 2026-02-15
**Current Coverage**: 62.4% (2,596/4,161 functions)
**Tests**: 7,616 tests in 603 files — **0 failures**

## Executive Summary

Of **50 modules with 0% coverage** (439 functions):

- ✅ **9 modules** (24 functions) — Behavior definitions only, **don't need tests**
- ⏸️ **4 modules** (36 functions) — **Not implemented**, no code exists
- ❌ **37 modules** (379 functions) — **IMPLEMENTED but no tests** ← FOCUS HERE

### Changes since last update (2026-02-13)

- Coverage rose from 50.3% → 62.4% (+508 functions covered, +121 test files)
- `crypto/kdf` — was 0%, now **39.1%** (9/23). Removed from 0% list.
- `crypto/rsa` — was 0%, now **44.4%** (8/18). Removed from 0% list.
- `crypto/ecdh` — was 0%, now **69.2%** (18/26). Removed from 0% list.
- `crypto/dh` — was 0%, now **16.7%** (4/24). Removed from 0% list.
- `crypto/key` — was 0%, now **22.0%** (13/59). Removed from 0% list.
- `crypto/sign` — was 0%, now **20.0%** (9/45). Removed from 0% list.
- `iter/adapters/step_by` — was 0%, now **100%** (2/2). Removed from 0% list.
- `iter/sources/from_fn` — was 0%, now **100%** (2/2). Removed from 0% list.
- `iter/sources/once_with` — was 0%, now **66.7%** (2/3). Removed from 0% list.
- `slice/sort` — was 0%, now **33.3%** (4/12). Removed from 0% list.
- `slice/cmp` — was 0%, now partial. Removed from 0% list.
- All Phase 1 compiler bugs fixed (associated types, closures, drop glue, exception inheritance)
- Iterator adapters (Enumerate, Zip, Fuse, Chain) now compile and run correctly

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

**Note**: Some of these (e.g. `convert`) may gain testable implementations later.

**Action**: None. Behaviors are tested through their implementations.

---

## 2. Not Implemented Modules (4 modules, 36 functions)

**No code implemented**, so nothing to test:

1. `crypto/constants` — 14 functions
2. `coverage` — 12 functions
3. `future` — 12 functions (requires async runtime)
4. `precompiled_symbols` — 1 function

**Note**: `runner` (9 functions) is the test runner itself — not a testable module.

**Action**: Wait for implementation or remove from library.

---

## 3. IMPLEMENTED BUT NO TESTS (37 modules, 379 functions)

**HIGH PRIORITY** — These modules have implemented code but 0% coverage.

### 3.1 Network/UDP (5 modules, 128 functions) — HIGH IMPACT

1. **`net/pending/udp`** — 38 functions (async UDP socket)
2. **`net/udp`** — 32 functions (sync UDP socket)
3. **`net/pending/async_tcp`** — 25 functions (async TCP)
4. **`net/pending/parser`** — 19 functions (HTTP/protocol parsing)
5. **`net/pending/async_udp`** — 14 functions (UDP async variants)

**Impact**: 128 network functions without tests = PRODUCTION BUG RISK

---

### 3.2 Iterators (27 modules, 116 functions) — MEDIUM IMPACT

**NOTE**: All iterator adapter compiler bugs are now fixed (Phase 8/9/14 complete). These adapters are **ready for testing**.

**Adapters** (17 modules, 42 functions):
- `iter/adapters/peekable` — 7 functions
- `iter/adapters/rev` — 4 functions
- `iter/adapters/chain` — 3 functions
- `iter/adapters/cloned` — 3 functions
- `iter/adapters/copied` — 3 functions
- `iter/adapters/cycle` — 3 functions
- `iter/adapters/enumerate` — 3 functions
- `iter/adapters/fuse` — 3 functions
- `iter/adapters/inspect` — 3 functions
- `iter/adapters/zip` — 3 functions
- `iter/adapters/filter_map` — 2 functions
- `iter/adapters/flat_map` — 2 functions
- `iter/adapters/flatten` — 2 functions
- `iter/adapters/intersperse` — 2 functions
- `iter/adapters/map_while` — 2 functions
- `iter/adapters/scan` — 2 functions
- `iter/adapters/skip_while` — 2 functions
- `iter/adapters/take_while` — 2 functions

**Sources** (3 modules, 8 functions):
- `iter/sources/empty` — 3 functions
- `iter/sources/repeat_with` — 3 functions
- `iter/sources/successors` — 2 functions

**Traits** (2 modules, 41 functions):
- `iter/traits/accumulators` — 22 functions (sum, product, etc.)
- `iter/traits/iterator` — 19 functions (default trait methods)

**Array/Slice iterators** (2 modules, 38 functions):
- `array/iter` — 19 functions
- `slice/iter` — 19 functions

**Impact**: 116 iterator functions = CORE FUNCTIONALITY WITHOUT TESTS

---

### 3.3 JSON Serialization (1 module, 23 functions) — HIGH IMPACT

- **`json/serialize`** — 23 functions
  - Automatic struct/enum serialization to JSON
  - JSON deserialization to TML types

**Impact**: Serialization without tests = POSSIBLE DATA CORRUPTION

---

### 3.4 Crypto (2 modules, 16 functions) — MEDIUM IMPACT

1. **`crypto/x509_test_minimal`** — 8 functions (minimal X.509 certificate tests)
2. **`crypto/constants`** — 14 functions *(not implemented — listed in Section 2)*

**Note**: 6 crypto modules moved OUT of 0% since last update:
- `crypto/key` — now 22.0% (13/59)
- `crypto/ecdh` — now 69.2% (18/26)
- `crypto/sign` — now 20.0% (9/45)
- `crypto/rsa` — now 44.4% (8/18)
- `crypto/kdf` — now 39.1% (9/23)
- `crypto/dh` — now 16.7% (4/24)

---

### 3.5 Array Operations (1 module, 9 functions) — LOW IMPACT

- **`array/ascii`** — 9 functions (ASCII operations on arrays)

---

### 3.6 Other Modules (4 modules, 25 functions)

1. **`thread/scope`** — 9 functions (scoped threads)
2. **`runner`** — 9 functions (test runner internals — not a user module)
3. **`cell/lazy`** — 5 functions (lazy cell initialization)
4. **`object`** — 1 function (`reference_equals` — note: Object class methods work, see object.test.tml 7/7)

---

## Recommended Action Plan

### Phase 1: Iterator Adapters (~116 functions) — UNBLOCKED

All compiler bugs blocking iterator adapters have been fixed:
- Associated type substitution (8.1, 14.4, 14.5) ✅
- Closure capture/dispatch (9.x) ✅
- Generic nested adapters (14.2, 14.3) ✅
- Tuple-returning adapters (14.4) ✅
- Maybe[StructType] fields (14.5) ✅

1. `iter/adapters/*` — 42 functions (Enumerate, Zip, Fuse, Chain confirmed working)
2. `iter/sources/*` — 8 functions
3. `array/iter` + `slice/iter` — 38 functions
4. `iter/traits/accumulators` — 22 functions

### Phase 2: High Risk Modules (~31 functions)

1. `json/serialize` — 23 functions (CRITICAL)
2. `crypto/x509_test_minimal` — 8 functions

### Phase 3: Network (~128 functions)

1. `net/udp` + `net/pending/udp` — 70 functions
2. `net/pending/async_tcp` — 25 functions
3. `net/pending/parser` — 19 functions
4. `net/pending/async_udp` — 14 functions

### Phase 4: Remaining (~25 functions)

1. `array/ascii` — 9 functions
2. `thread/scope` — 9 functions
3. `cell/lazy` — 5 functions
4. `object` — 1 function

---

## Estimated Impact

**If all 37 implemented modules are tested:**
- Current coverage: 62.4% (2,596/4,161)
- Additional functions: +379
- **Projected coverage: 71.5% (2,975/4,161)**

**Gain**: +9.1% absolute coverage

---

## Important Notes

1. **Most compiler bugs are now FIXED** (see tasks.md — Phase 1 complete)
   - Iterator adapters fully unblocked ✅
   - Closure codegen fixed ✅
   - Associated type resolution fixed ✅
   - Exception inheritance fixed ✅
   - Only 8.4 (async iterators) deferred

2. **Modules that moved OUT of 0% since 2026-02-12:**
   - `num/overflow` — now 68.4%
   - `fmt/num` — now 85.7%
   - `crypto/kdf` — now 39.1%
   - `crypto/rsa` — now 44.4%
   - `crypto/ecdh` — now 69.2%
   - `crypto/dh` — now 16.7%
   - `crypto/key` — now 22.0%
   - `crypto/sign` — now 20.0%
   - `iter/adapters/step_by` — now 100%
   - `iter/sources/from_fn` — now 100%
   - `iter/sources/once_with` — now 66.7%
   - `slice/sort` — now 33.3%

3. **Prioritize by impact:**
   - CRITICAL: iterators (now unblocked), json/serialize, net
   - MEDIUM: crypto gaps, array/ascii
   - LOW: thread/scope, cell/lazy, object

4. **Test consolidation status:**
   - 603 test files, 7,616 tests — 0 failures
   - Coverage: 62.4% (up from 43.7% at start of test-failures work)
