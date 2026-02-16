# TML Library Coverage Report

**Date:** 2026-02-16
**Overall Coverage:** 3049/4147 functions (73.5%)
**Tests:** 8727 passed across 739 files
**Duration:** 14403ms

---

## Executive Summary

The TML standard library has **73.5% function coverage** across 203 modules. While 62 modules are at 100% and the core language primitives are well-tested, there are **66 modules below 50% coverage** representing ~816 uncovered functions. The most critical gaps are in the **iterator traits/adapters** subsystem, **async primitives**, and **networking** modules.

---

## Coverage Distribution

| Coverage Band | Modules | Functions Covered | Functions Total | Notes |
|:-------------:|:-------:|:-----------------:|:---------------:|-------|
| 100% | 62 | 1417 | 1417 | Fully tested |
| 50-99% | 75 | 1329 | 1714 | Partial coverage |
| 26-49% | 13 | 114 | 283 | Needs attention |
| 1-25% | 16 | 65 | 373 | Significant gaps |
| 0% | 37 | 0 | 360 | Completely untested |
| **Total** | **203** | **3049** | **4147** | **73.5%** |

---

## Priority 0 — Critical: 0% Coverage (37 modules, ~360 functions)

These modules have **zero test coverage** and represent the most urgent gaps.

### Iterator Traits (51 functions)

| Module | Functions | Description |
|--------|:---------:|-------------|
| `iter/traits/accumulators` | 22 | sum, product, fold, reduce — core aggregation ops |
| `iter/traits/iterator` | 19 | Base Iterator behavior — foundation for all iteration |
| `iter/traits/double_ended` | 5 | Bidirectional iteration (next_back, rfold, rfind) |
| `iter/traits/exact_size` | 2 | ExactSizeIterator (len, is_empty) |
| `iter/traits/into_iterator` | 1 | IntoIterator conversion |
| `iter/traits/extend` | 1 | Extend behavior for collections |
| `iter/traits/from_iterator` | 1 | FromIterator / collect behavior |

**Impact:** These are the **most critical** gaps. Every iterator adapter and every `loop ... in` depends on these traits. Without tests, regressions here cascade across the entire library.

### Iterator Adapters (30 functions)

| Module | Functions | Description |
|--------|:---------:|-------------|
| `iter/adapters/peekable` | 7 | peek, peek_mut, next_if — widely used |
| `iter/adapters/rev` | 4 | Reverse iteration |
| `iter/adapters/copied` | 3 | Copy elements from references |
| `iter/adapters/cloned` | 3 | Clone elements from references |
| `iter/sources/empty` | 3 | Empty iterator constructor |
| `iter/sources/repeat_with` | 3 | Lazy repeat with closure |
| `iter/adapters/flat_map` | 2 | Map + flatten |
| `iter/adapters/flatten` | 2 | Flatten nested iterators |
| `iter/adapters/filter_map` | 2 | Combined filter + map |
| `iter/adapters/scan` | 2 | Stateful transformation |
| `iter/adapters/map_while` | 2 | Map while predicate holds |
| `iter/adapters/intersperse` | 2 | Insert separator between elements |
| `iter/sources/successors` | 2 | Generate from successor function |

**Impact:** High. These adapters are fundamental to idiomatic TML code. `flat_map`, `filter_map`, and `peekable` are especially common in real-world usage.

### Array & Slice Iteration (28 functions)

| Module | Functions | Description |
|--------|:---------:|-------------|
| `array/iter` | 19 | Array iterator implementation |
| `array/ascii` | 9 | ASCII operations on arrays |

**Impact:** Array iteration is fundamental. Without tests, `loop item in array` could silently break.

### Slice Iteration (19 functions)

| Module | Functions | Description |
|--------|:---------:|-------------|
| `slice/iter` | 19 | Slice iterator (windows, chunks, iter, iter_mut) |

**Impact:** Slice iteration is used throughout collections and string processing.

### Core Language Features (20 functions)

| Module | Functions | Description |
|--------|:---------:|-------------|
| `convert` | 6 | From/Into conversion traits |
| `cell/lazy` | 5 | Lazy initialization (LazyCell) |
| `ops/function` | 3 | Fn/FnMut/FnOnce traits |
| `ops/deref` | 2 | Deref/DerefMut traits |
| `ops/index` | 2 | Index/IndexMut traits |
| `object` | 1 | Dynamic dispatch |
| `precompiled_symbols` | 1 | Symbol interning |

**Impact:** `convert`, `ops/function`, `ops/deref`, and `ops/index` are foundational behaviors used implicitly by many language features.

### Standard Library (32 functions)

| Module | Functions | Description |
|--------|:---------:|-------------|
| `json/serialize` | 23 | JSON serialization (ToJson implementations) |
| `thread/scope` | 9 | Scoped thread spawning |

**Impact:** `json/serialize` is critical for any application doing JSON output. `thread/scope` enables safe concurrent patterns.

### Networking — Pending (58 functions)

| Module | Functions | Description |
|--------|:---------:|-------------|
| `net/pending/async_tcp` | 25 | Async TCP (event-loop based) |
| `net/pending/parser` | 19 | HTTP/protocol parser |
| `net/pending/async_udp` | 14 | Async UDP |

**Impact:** Lower priority — these are explicitly in `pending/` status and not yet stable.

### Test Framework (11 functions)

| Module | Functions | Description |
|--------|:---------:|-------------|
| `runner` | 9 | Test runner internals |
| `coverage` | 2 | Coverage tracking |

**Impact:** Low — these are infrastructure modules that are exercised implicitly by running tests.

---

## Priority 1 — High: 1-25% Coverage (16 modules, ~308 functions)

These modules have minimal coverage and need significant test development.

| Module | Coverage | Covered/Total | Description |
|--------|:--------:|:-------------:|-------------|
| `report` | 8.0% | 2/25 | Test reporting |
| `future` | 8.3% | 1/12 | Future/Promise async primitive |
| `collections` | 10.5% | 2/19 | Std collections (BTreeMap, etc.) |
| `pool` | 13.8% | 4/29 | Object pool / memory pool |
| `async_iter` | 14.3% | 3/21 | Async iterator (Stream) |
| `net/pending/buffer` | 15.0% | 9/60 | Network buffer management |
| `net/pending/tcp` | 17.6% | 9/51 | TCP networking (pending) |
| `interfaces` | 18.5% | 5/27 | Type interfaces / dynamic dispatch |
| `alloc` | 18.8% | 3/16 | Allocator traits |
| `task` | 19.2% | 5/26 | Async task (Waker, Context, Poll) |
| `exception` | 20.0% | 1/5 | Exception/panic handling |
| `cell/ref_cell` | 20.0% | 3/15 | RefCell (runtime borrow checking) |
| `bench` | 22.2% | 2/9 | Benchmarking framework |
| `net/tcp` | 22.5% | 9/40 | TCP networking |
| `thread/local` | 25.0% | 3/12 | Thread-local storage |
| `pin` | 25.0% | 2/8 | Pin (immovable types for async) |

### Key Concerns

- **`future` + `async_iter` + `task`** (59 functions total): The async subsystem has almost no coverage. Any async feature work is flying blind.
- **`cell/ref_cell`** (15 functions): Runtime borrow checking is critical for interior mutability patterns.
- **`alloc`** (16 functions): Allocator traits affect all heap-allocated types.
- **`pool`** (29 functions): Memory pools are performance-critical infrastructure.

---

## Priority 2 — Medium: 26-49% Coverage (13 modules, ~283 functions)

These modules have partial coverage but still have significant gaps.

| Module | Coverage | Covered/Total | Uncovered | Description |
|--------|:--------:|:-------------:|:---------:|-------------|
| `array` | 30.2% | 13/43 | 30 | Array methods (map, zip, flatten) |
| `ops/async_function` | 30.8% | 4/13 | 9 | AsyncFn traits |
| `ops/try_trait` | 33.3% | 4/12 | 8 | Try/? operator support |
| `slice/sort` | 33.3% | 4/12 | 8 | Sorting algorithms |
| `tuple` | 33.3% | 4/12 | 8 | Tuple operations |
| `iter/adapters/map` | 33.3% | 1/3 | 2 | Map adapter |
| `iter/sources/repeat` | 33.3% | 1/3 | 2 | Repeat source |
| `net/pending/udp` | 36.8% | 14/38 | 24 | UDP networking (pending) |
| `thread` | 40.0% | 8/20 | 12 | Thread spawning/joining |
| `net/udp` | 43.8% | 14/32 | 18 | UDP networking |
| `iter/range` | 45.5% | 15/33 | 18 | Range iteration |
| `ptr/mut_ptr` | 46.4% | 13/28 | 15 | Mutable pointer ops |
| `any` | 47.8% | 11/23 | 12 | Dynamic typing (type_id, downcast) |

### Key Concerns

- **`array`** (30 uncovered): Array is a fundamental type; methods like `map`, `zip`, `flatten` need tests.
- **`ops/try_trait`** (8 uncovered): The `?` operator desugars through this — regressions break error handling.
- **`slice/sort`** (8 uncovered): Sorting correctness is critical for any data processing.
- **`iter/range`** (18 uncovered): Range iteration (`0 to 10`) is extremely common.

---

## Modules at 50-99% Coverage (75 modules)

These modules have reasonable coverage but still have gaps worth noting.

| Module | Coverage | Covered/Total | Description |
|--------|:--------:|:-------------:|-------------|
| `num/nonzero` | 50.0% | 3/6 | NonZero integer types |
| `iter/adapters/filter` | 50.0% | 1/2 | Filter adapter |
| `ptr/ops` | 51.9% | 14/27 | Pointer arithmetic |
| `ptr/non_null` | 52.0% | 13/25 | NonNull pointer |
| `ops/drop` | 53.3% | 8/15 | Drop/destructor trait |
| `option` | 53.6% | 15/28 | Maybe[T] methods |
| `file/file` | 54.5% | 6/11 | File I/O |
| `cache` | 55.6% | 20/36 | Caching system |
| `ops/coroutine` | 58.3% | 7/12 | Coroutine/generator |
| `fmt/rt` | 61.1% | 11/18 | Format runtime |
| `num/saturating` | 63.6% | 7/11 | Saturating arithmetic |
| `traits` | 65.2% | 15/23 | Core traits |
| `error` | 65.6% | 21/32 | Error handling |
| `collections/hashmap` | 66.7% | 10/15 | HashMap |
| `hash` | 67.3% | 35/52 | Hashing |
| `intrinsics` | 67.4% | 58/86 | Compiler intrinsics |
| `net/tls` | 67.6% | 23/34 | TLS/SSL |
| `num/overflow` | 68.4% | 13/19 | Overflow checking |
| `alloc/shared` | 68.8% | 11/16 | Shared[T] (Rc) |
| `alloc/sync` | 68.8% | 11/16 | Sync[T] (Arc alloc) |
| `net/socket` | 71.4% | 15/21 | Socket API |
| `fmt/traits` | 71.4% | 10/14 | Display/Debug traits |
| `slice` | 72.0% | 18/25 | Slice operations |
| `net/ip` | 72.4% | 42/58 | IP address types |
| `ptr/const_ptr` | 73.7% | 14/19 | Const pointer ops |
| `result` | 75.8% | 25/33 | Outcome[T,E] methods |
| `ops/bit` | 76.1% | 70/92 | Bitwise operators |
| `sync/mpsc` | 77.3% | 17/22 | Message passing channels |
| `net/sys` | 78.7% | 37/47 | Network syscalls |
| `alloc/global` | 79.2% | 19/24 | Global allocator |

---

## Modules at 100% Coverage (62 modules)

These modules are fully tested. Listed for completeness and to highlight areas of strength.

| Category | Modules |
|----------|---------|
| **Crypto** | `cipher`, `constants`, `dh`, `ecdh`, `error`, `hash`, `hmac`, `kdf`, `key`, `random`, `rsa`, `sign`, `x509`, `x509_test_minimal` (14 modules) |
| **Compression** | `brotli`, `crc32`, `deflate`, `error`, `gzip`, `options`, `stream` (7 modules) |
| **Formatting** | `builders`, `float`, `formatter`, `helpers` (4 modules) |
| **Collections** | `buffer`, `list`, `class_collections` (3 modules) |
| **Allocation** | `layout`, `heap` (2 modules + `arena`) |
| **File System** | `dir`, `path` (2 modules) |
| **Search** | `bm25`, `distance`, `hnsw` (3 modules) |
| **JSON** | `builder`, `types` (2 modules) |
| **Sync** | `atomic`, `barrier`, `queue`, `stack` (4 modules) |
| **Core** | `ascii/char`, `assertions`, `builtins`, `cell/cell`, `clone`, `marker`, `mem`, `range`, `soo`, `text`, `unicode/unicode_data` (11 modules) |
| **Ops** | `arith`, `range` (2 modules) |
| **Other** | `borrow`, `cmp`, `default`, `glob`, `log`, `net/error`, `num/integer`, `num/traits`, `os`, `profiler`, `ptr/alignment`, `reflect`, `time`, `types`, `unicode`, `char/convert`, `char/methods`, `str`, `fmt/impls`, `sync/mutex` (remaining) |

---

## Recommended Action Plan

### Phase 1: Iterator Foundation (Est. ~150 functions)

The iterator subsystem is the most impactful gap. Tests should be created in this order:

1. `iter/traits/iterator` — Base trait (next, size_hint, count, last, nth)
2. `iter/traits/double_ended` — next_back, rfold, rfind
3. `iter/traits/exact_size` — len, is_empty
4. `iter/traits/accumulators` — sum, product, fold, reduce, min, max
5. `iter/traits/into_iterator` — IntoIterator conversion
6. `iter/traits/extend` — Extend behavior
7. `iter/traits/from_iterator` — collect behavior
8. `iter/adapters/map` — Map (upgrade from 33%)
9. `iter/adapters/filter` — Filter (upgrade from 50%)
10. `iter/adapters/peekable` — Peek operations
11. `iter/adapters/rev` — Reverse iteration
12. `iter/adapters/flat_map` + `flatten` — Nested iteration
13. `iter/adapters/filter_map` — Combined filter+map
14. `iter/adapters/copied` + `cloned` — Value copying
15. Remaining adapters: `scan`, `map_while`, `intersperse`
16. `iter/sources/empty`, `repeat_with`, `successors`
17. `iter/range` — Range iteration (upgrade from 46%)
18. `array/iter` + `slice/iter` — Collection iterators

### Phase 2: Core Types (Est. ~80 functions)

1. `convert` — From/Into traits
2. `ops/function` — Fn traits
3. `ops/deref` + `ops/index` — Operator overloading
4. `ops/try_trait` — ? operator (upgrade from 33%)
5. `cell/lazy` + `cell/ref_cell` — Interior mutability
6. `pin` — Pin (upgrade from 25%)
7. `exception` — Panic handling (upgrade from 20%)
8. `array` — Array methods (upgrade from 30%)
9. `tuple` — Tuple ops (upgrade from 33%)
10. `any` — Dynamic typing (upgrade from 48%)
11. `option` — Maybe[T] (upgrade from 54%)

### Phase 3: Async Subsystem (Est. ~60 functions)

1. `future` — Future trait and combinators
2. `task` — Waker, Context, Poll
3. `async_iter` — Async iteration (Stream)
4. `ops/async_function` — AsyncFn traits

### Phase 4: Standard Library (Est. ~70 functions)

1. `json/serialize` — JSON output
2. `collections` — BTreeMap, etc.
3. `interfaces` — Dynamic dispatch
4. `alloc` — Allocator traits
5. `pool` — Memory pools
6. `slice/sort` — Sorting algorithms
7. `thread` + `thread/local` + `thread/scope` — Threading

### Phase 5: Networking (Est. ~150 functions)

1. `net/tcp` — TCP (upgrade from 23%)
2. `net/udp` — UDP (upgrade from 44%)
3. `net/pending/buffer` — Network buffers
4. `net/pending/tcp` — Pending TCP
5. `net/pending/udp` — Pending UDP
6. `net/pending/async_tcp` + `async_udp` — Async networking
7. `net/pending/parser` — Protocol parser

---

## Coverage Targets

| Milestone | Target | Functions to Cover | Priority |
|-----------|:------:|:------------------:|----------|
| **v1.1** | 80% | +270 functions | Phase 1 + Phase 2 |
| **v1.2** | 85% | +207 functions | Phase 3 + Phase 4 |
| **v1.3** | 90% | +373 functions | Phase 5 + remaining gaps |
| **v2.0** | 95%+ | All remaining | Full library coverage |

---

## Appendix: Full Module Listing (Sorted by Coverage)

| # | Module | Covered | Total | Coverage |
|:-:|--------|:-------:|:-----:|:--------:|
| 1 | `iter/adapters/peekable` | 0 | 7 | 0.0% |
| 2 | `iter/traits/into_iterator` | 0 | 1 | 0.0% |
| 3 | `array/ascii` | 0 | 9 | 0.0% |
| 4 | `iter/adapters/copied` | 0 | 3 | 0.0% |
| 5 | `iter/sources/empty` | 0 | 3 | 0.0% |
| 6 | `net/pending/async_tcp` | 0 | 25 | 0.0% |
| 7 | `iter/adapters/cloned` | 0 | 3 | 0.0% |
| 8 | `iter/adapters/map_while` | 0 | 2 | 0.0% |
| 9 | `json/serialize` | 0 | 23 | 0.0% |
| 10 | `iter/adapters/flat_map` | 0 | 2 | 0.0% |
| 11 | `iter/adapters/rev` | 0 | 4 | 0.0% |
| 12 | `iter/traits/accumulators` | 0 | 22 | 0.0% |
| 13 | `iter/sources/repeat_with` | 0 | 3 | 0.0% |
| 14 | `object` | 0 | 1 | 0.0% |
| 15 | `array/iter` | 0 | 19 | 0.0% |
| 16 | `iter/traits/iterator` | 0 | 19 | 0.0% |
| 17 | `net/pending/parser` | 0 | 19 | 0.0% |
| 18 | `slice/iter` | 0 | 19 | 0.0% |
| 19 | `precompiled_symbols` | 0 | 1 | 0.0% |
| 20 | `iter/adapters/flatten` | 0 | 2 | 0.0% |
| 21 | `iter/traits/double_ended` | 0 | 5 | 0.0% |
| 22 | `iter/adapters/filter_map` | 0 | 2 | 0.0% |
| 23 | `cell/lazy` | 0 | 5 | 0.0% |
| 24 | `iter/adapters/scan` | 0 | 2 | 0.0% |
| 25 | `ops/function` | 0 | 3 | 0.0% |
| 26 | `iter/sources/successors` | 0 | 2 | 0.0% |
| 27 | `net/pending/async_udp` | 0 | 14 | 0.0% |
| 28 | `coverage` | 0 | 2 | 0.0% |
| 29 | `iter/traits/exact_size` | 0 | 2 | 0.0% |
| 30 | `ops/deref` | 0 | 2 | 0.0% |
| 31 | `ops/index` | 0 | 2 | 0.0% |
| 32 | `convert` | 0 | 6 | 0.0% |
| 33 | `iter/traits/extend` | 0 | 1 | 0.0% |
| 34 | `iter/traits/from_iterator` | 0 | 1 | 0.0% |
| 35 | `thread/scope` | 0 | 9 | 0.0% |
| 36 | `runner` | 0 | 9 | 0.0% |
| 37 | `iter/adapters/intersperse` | 0 | 2 | 0.0% |
| 38 | `report` | 2 | 25 | 8.0% |
| 39 | `future` | 1 | 12 | 8.3% |
| 40 | `collections` | 2 | 19 | 10.5% |
| 41 | `pool` | 4 | 29 | 13.8% |
| 42 | `async_iter` | 3 | 21 | 14.3% |
| 43 | `net/pending/buffer` | 9 | 60 | 15.0% |
| 44 | `net/pending/tcp` | 9 | 51 | 17.6% |
| 45 | `interfaces` | 5 | 27 | 18.5% |
| 46 | `alloc` | 3 | 16 | 18.8% |
| 47 | `task` | 5 | 26 | 19.2% |
| 48 | `exception` | 1 | 5 | 20.0% |
| 49 | `cell/ref_cell` | 3 | 15 | 20.0% |
| 50 | `bench` | 2 | 9 | 22.2% |
| 51 | `net/tcp` | 9 | 40 | 22.5% |
| 52 | `thread/local` | 3 | 12 | 25.0% |
| 53 | `pin` | 2 | 8 | 25.0% |
| 54 | `array` | 13 | 43 | 30.2% |
| 55 | `ops/async_function` | 4 | 13 | 30.8% |
| 56 | `ops/try_trait` | 4 | 12 | 33.3% |
| 57 | `slice/sort` | 4 | 12 | 33.3% |
| 58 | `tuple` | 4 | 12 | 33.3% |
| 59 | `iter/adapters/map` | 1 | 3 | 33.3% |
| 60 | `iter/sources/repeat` | 1 | 3 | 33.3% |
| 61 | `net/pending/udp` | 14 | 38 | 36.8% |
| 62 | `thread` | 8 | 20 | 40.0% |
| 63 | `net/udp` | 14 | 32 | 43.8% |
| 64 | `iter/range` | 15 | 33 | 45.5% |
| 65 | `ptr/mut_ptr` | 13 | 28 | 46.4% |
| 66 | `any` | 11 | 23 | 47.8% |
| 67 | `num/nonzero` | 3 | 6 | 50.0% |
| 68 | `iter/adapters/filter` | 1 | 2 | 50.0% |
| 69 | `ptr/ops` | 14 | 27 | 51.9% |
| 70 | `ptr/non_null` | 13 | 25 | 52.0% |
| 71 | `ops/drop` | 8 | 15 | 53.3% |
| 72 | `option` | 15 | 28 | 53.6% |
| 73 | `file/file` | 6 | 11 | 54.5% |
| 74 | `cache` | 20 | 36 | 55.6% |
| 75 | `ops/coroutine` | 7 | 12 | 58.3% |
| 76 | `fmt/rt` | 11 | 18 | 61.1% |
| 77 | `num/saturating` | 7 | 11 | 63.6% |
| 78 | `traits` | 15 | 23 | 65.2% |
| 79 | `error` | 21 | 32 | 65.6% |
| 80 | `collections/hashmap` | 10 | 15 | 66.7% |
| 81 | `iter/sources/once_with` | 2 | 3 | 66.7% |
| 82 | `iter/sources/repeat_n` | 2 | 3 | 66.7% |
| 83 | `slice/cmp` | 6 | 9 | 66.7% |
| 84 | `cell/unsafe_cell` | 4 | 6 | 66.7% |
| 85 | `num/wrapping` | 8 | 12 | 66.7% |
| 86 | `iter/adapters/chain` | 2 | 3 | 66.7% |
| 87 | `iter/adapters/cycle` | 2 | 3 | 66.7% |
| 88 | `iter/sources/legacy` | 12 | 18 | 66.7% |
| 89 | `iter/adapters/enumerate` | 2 | 3 | 66.7% |
| 90 | `iter/adapters/fuse` | 2 | 3 | 66.7% |
| 91 | `iter/adapters/inspect` | 2 | 3 | 66.7% |
| 92 | `sync/Arc` | 12 | 18 | 66.7% |
| 93 | `iter/adapters/skip` | 2 | 3 | 66.7% |
| 94 | `iter/adapters/take` | 2 | 3 | 66.7% |
| 95 | `iter/adapters/zip` | 2 | 3 | 66.7% |
| 96 | `iter/sources/once` | 2 | 3 | 66.7% |
| 97 | `hash` | 35 | 52 | 67.3% |
| 98 | `intrinsics` | 58 | 86 | 67.4% |
| 99 | `net/tls` | 23 | 34 | 67.6% |
| 100 | `num/overflow` | 13 | 19 | 68.4% |
| 101 | `alloc/shared` | 11 | 16 | 68.8% |
| 102 | `alloc/sync` | 11 | 16 | 68.8% |
| 103 | `net/socket` | 15 | 21 | 71.4% |
| 104 | `fmt/traits` | 10 | 14 | 71.4% |
| 105 | `sync/condvar` | 5 | 7 | 71.4% |
| 106 | `slice` | 18 | 25 | 72.0% |
| 107 | `net/ip` | 42 | 58 | 72.4% |
| 108 | `ptr/const_ptr` | 14 | 19 | 73.7% |
| 109 | `sync/ordering` | 3 | 4 | 75.0% |
| 110 | `result` | 25 | 33 | 75.8% |
| 111 | `ops/bit` | 70 | 92 | 76.1% |
| 112 | `sync/mpsc` | 17 | 22 | 77.3% |
| 113 | `net/sys` | 37 | 47 | 78.7% |
| 114 | `alloc/global` | 19 | 24 | 79.2% |
| 115 | `bstr` | 4 | 5 | 80.0% |
| 116 | `sync/rwlock` | 13 | 16 | 81.2% |
| 117 | `char/decode` | 9 | 11 | 81.8% |
| 118 | `sync/once` | 9 | 11 | 81.8% |
| 119 | `iter` | 11 | 13 | 84.6% |
| 120 | `cell/once` | 11 | 13 | 84.6% |
| 121 | `alloc/heap` | 11 | 13 | 84.6% |
| 122 | `zlib/zstd` | 35 | 41 | 85.4% |
| 123 | `default` | 12 | 14 | 85.7% |
| 124 | `unicode` | 12 | 14 | 85.7% |
| 125 | `fmt/num` | 6 | 7 | 85.7% |
| 126 | `reflect` | 16 | 18 | 88.9% |
| 127 | `mem` | 18 | 20 | 90.0% |
| 128 | `borrow` | 18 | 20 | 90.0% |
| 129 | `num/traits` | 56 | 62 | 90.3% |
| 130 | `str` | 49 | 54 | 90.7% |
| 131 | `sync/mutex` | 12 | 13 | 92.3% |
| 132 | `unicode/char` | 16 | 17 | 94.1% |
| 133 | `net/dns` | 33 | 35 | 94.3% |
| 134 | `char/convert` | 22 | 23 | 95.7% |
| 135 | `types` | 22 | 23 | 95.7% |
| 136 | `cmp` | 51 | 53 | 96.2% |
| 137 | `fmt/impls` | 70 | 72 | 97.2% |
| 138 | `json/types` | 74 | 76 | 97.4% |
| 139 | `sync/atomic` | 118 | 121 | 97.5% |
| 140 | `char/methods` | 42 | 43 | 97.7% |
| 141 | `collections/class_collections` | 59 | 60 | 98.3% |
| 142-203 | *(62 modules at 100%)* | 1417 | 1417 | 100.0% |
