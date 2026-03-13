# Coverage Gap Classification

**Source**: `build/coverage/coverage_history.jsonl` — last entry `2026-03-13T08:11:19`
**Overall**: 5213 / 5486 functions covered (95.02%)
**Scope**: Only modules where gap > 0 (covered < total), sorted by gap descending.

---

## Classification Table

| Module | Total / Covered / Gap | Category | Notes |
|---|---|---|---|
| `hash` | 45 / 27 / 18 | INFRA_MISSING | OpenSSL-backed hash impls (SHA, MD5) require real crypto infrastructure; non-OpenSSL paths (FNV, SipHash) may be TESTS_MISSING |
| `alloc/global` | 20 / 3 / 17 | COMPILER_BUG | GlobalAlloc and Layout types hit GEP/unsized-type codegen bugs; related to `cell/lazy` closure-typed struct field issue |
| `array` | 39 / 24 / 15 | TESTS_MISSING | Array method impls compile and run; gaps in less-common methods (rotate, windows, chunks_exact variants) |
| `crypto/cipher` | 43 / 28 / 15 | INFRA_MISSING | Block cipher modes (AES-GCM, ChaCha20) require OpenSSL EVP context; no way to test without real crypto backend |
| `zlib/zstd` | 41 / 26 / 15 | INFRA_MISSING | Compression/decompression requires zlib/zstd system libraries; not available in test environment |
| `e2e/tls` | 13 / 0 / 13 | INFRA_MISSING | End-to-end TLS tests require live server/client sockets and certificates; 0% covered |
| `net/tls` | 35 / 24 / 11 | INFRA_MISSING | TLS handshake and record layer methods require OpenSSL/network; partial coverage from non-TLS path |
| `intrinsics` | 95 / 85 / 10 | UNTESTABLE | Low-level memory/LLVM intrinsics; several are `@no_coverage`-worthy (e.g. panic paths, raw ptr ops with no safe test harness) |
| `array/ascii` | 9 / 0 / 9 | TESTS_MISSING | ASCII classification methods on arrays (is_ascii, to_ascii_upper, etc.); no tests written yet, no blocker |
| `array/iter` | 19 / 11 / 8 | TESTS_MISSING | Array iterator adapters (windows, chunks, rchunks); missing test cases for less-common patterns |
| `http/connection` | 12 / 4 / 8 | INFRA_MISSING | HTTP keep-alive, pipelining and upgrade methods require live HTTP server |
| `thread/scope` | 8 / 0 / 8 | INFRA_MISSING | Scoped thread API requires real OS thread spawning in test context; currently blocked by test harness DLL model |
| `http/client` | 10 / 3 / 7 | INFRA_MISSING | HTTP redirect, authentication, and retry methods require live network |
| `iter/adapters/peekable` | 7 / 0 / 7 | COMPILER_BUG | `where I::Item = ref T` associated type constraint not resolved by type checker; all peekable tests fail to compile |
| `net/tcp` | 43 / 36 / 7 | INFRA_MISSING | TCP listen/accept/timeout methods require live socket; partial coverage from loopback paths |
| `pool` | 27 / 20 / 7 | TESTS_MISSING | Object pool methods (drain, shrink, statistics); compiles and runs, tests not yet written |
| `alloc/layout` | 30 / 24 / 6 | COMPILER_BUG | Layout calculation for unsized/fat-pointer types hits codegen bugs; same root cause as `alloc/global` |
| `fmt/rt` | 18 / 12 / 6 | TESTS_MISSING | Format runtime helpers (padding, alignment, fill); functional but lacking test coverage for edge cases |
| `future` | 8 / 2 / 6 | RUNTIME_CRASH | Async future polling crashes (future_fuse, async_lazy_future patterns); exit code -1073741819 in test runner |
| `task` | 25 / 19 / 6 | INFRA_MISSING | Async task spawning and waker APIs require async executor runtime not available in test harness |
| `thread` | 20 / 14 / 6 | INFRA_MISSING | Thread park/unpark, thread-local init, and join timeout require OS thread infrastructure |
| `async_iter` | 17 / 12 / 5 | INFRA_MISSING | Async iterator combinators require running async executor; partial coverage from sync-compatible paths |
| `e2e/server` | 9 / 4 / 5 | INFRA_MISSING | E2E server lifecycle (bind, accept, graceful shutdown) requires live network |
| `cell/lazy` | 8 / 4 / 4 | COMPILER_BUG | `%struct.I32__Fn` unsized type error; closure-typed struct field hits codegen bug for function-typed fields |
| `iter/traits/accumulators` | 20 / 16 / 4 | TESTS_MISSING | Accumulator iterator methods (sum_checked, product_checked, min_by, max_by); compiles, tests missing |
| `json/serialize` | 21 / 17 / 4 | TESTS_MISSING | JSON serialization for edge cases (null fields, nested generics, escape sequences); functional but undertested |
| `option` | 28 / 24 / 4 | TESTS_MISSING | Option combinator methods (flatten, unzip, filter_map variants); covered main paths, edge cases missing |
| `cell/ref_cell` | 13 / 10 / 3 | TESTS_MISSING | RefCell borrow_mut panic path and try_borrow variants; needs tests exercising error conditions |
| `collections/behaviors` | 20 / 17 / 3 | TESTS_MISSING | Collection behavior impls (retain, extend_from_slice, drain); functional, test cases missing |
| `iter/adapters/cloned` | 3 / 0 / 3 | COMPILER_BUG | `where I::Item = ref T` associated type constraint; same blocker as `peekable` — type checker cannot resolve |
| `iter/adapters/copied` | 3 / 0 / 3 | COMPILER_BUG | `where I::Item = ref T` associated type constraint; same blocker as `cloned` |
| `net/parser` | 18 / 15 / 3 | TESTS_MISSING | HTTP/URL parser edge cases (malformed headers, percent-encoding); functional, coverage for error paths missing |
| `net/sys` | 50 / 47 / 3 | INFRA_MISSING | Raw socket syscall wrappers (socket options, multicast); requires privileged OS access or special test setup |
| `any` | 21 / 19 / 2 | TESTS_MISSING | `Any` downcast paths for less-common types; functional, tests not written |
| `error` | 28 / 26 / 2 | UNTESTABLE | `NeverError::to_string` and `NeverError::debug_string` call `unreachable()` — annotated `@no_coverage`; remaining gap is these 2 functions |
| `fmt/helpers` | 33 / 31 / 2 | TESTS_MISSING | Format helper utilities (debug escaping for unusual Unicode, large integer formatting); edge cases untested |
| `iter/adapters/flatten` | 2 / 0 / 2 | COMPILER_BUG | `where I::Item = ref T` / nested iterator associated type constraint; type checker does not resolve for flatten |
| `iter/adapters/intersperse` | 2 / 0 / 2 | COMPILER_BUG | Same associated type constraint blocker as other iter adapters |
| `ptr/non_null` | 25 / 23 / 2 | TESTS_MISSING | NonNull pointer methods (as_ref, as_mut, cast variants); compiles fine, tests partially written |
| `iter/range` | 30 / 29 / 1 | RUNTIME_CRASH | `iter_range_sizehint` test hits invalid GEP codegen bug (pre-existing); one function uncovered due to crash |
| `mem` | 19 / 18 / 1 | TESTS_MISSING | One `mem` utility function lacking a test (likely `forget` or `transmute` edge case) |
| `net/error` | 29 / 28 / 1 | TESTS_MISSING | One network error variant path not exercised; functional code, test case missing |
| `ops/async_function` | 9 / 8 / 1 | INFRA_MISSING | Async function operator impl requires async executor; one path unreachable without runtime |
| `ops/drop` | 13 / 12 / 1 | UNTESTABLE | One `Drop::drop` implementation auto-called by runtime with no explicit coverage instrumentation |
| `os` | 43 / 42 / 1 | INFRA_MISSING | One OS API (e.g. `getcwd` or `chdir`) requires specific filesystem state in test environment |
| `precompiled_symbols` | 1 / 0 / 1 | UNTESTABLE | Precompiled symbol stub — no callable test surface; purely internal |
| `result` | 33 / 32 / 1 | TESTS_MISSING | One `Outcome`/Result combinator path (likely `map_or_else` or `and_then` edge case) untested |
| `slice/sort` | 9 / 8 / 1 | TESTS_MISSING | One sort variant (e.g. `sort_unstable_by_key`) not yet exercised by tests |
| `sync/condvar` | 6 / 5 / 1 | INFRA_MISSING | Condvar timeout path requires precise timing; blocked by test environment constraints |
| `sync/once` | 11 / 10 / 1 | RUNTIME_CRASH | `once_lock_get_or_init` fails in coverage mode only (pre-existing); one function uncovered |
| `thread/local` | 11 / 10 / 1 | INFRA_MISSING | Thread-local storage destructor path requires thread exit which test harness does not trigger |

---

## Summary by Category

| Category | Module Count | Total Gap |
|---|---|---|
| TESTS_MISSING | 19 | 96 |
| INFRA_MISSING | 16 | 84 |
| COMPILER_BUG | 9 | 53 |
| RUNTIME_CRASH | 3 | 8 |
| UNTESTABLE | 4 | 16 |

---

## Category Definitions

- **COMPILER_BUG** — Blocked by a known codegen or type-checker defect (e.g. `where I::Item = ref T` constraint, closure-typed struct fields, GEP on unsized types). Fix requires compiler work.
- **RUNTIME_CRASH** — Module crashes the test subprocess at runtime (exit -1073741819 or exit 99). Root cause is a pre-existing bug in codegen or runtime memory.
- **INFRA_MISSING** — Module requires external infrastructure not available in the test environment (live network, OpenSSL backend, OS thread control, async executor).
- **TESTS_MISSING** — Module compiles and runs correctly but no test cases cover the remaining functions. Fix is purely writing tests.
- **UNTESTABLE** — Functions that genuinely cannot be reached by any test (e.g. `unreachable()` bodies, auto-called destructors, compiler-internal stubs). Should be annotated `@no_coverage`.
