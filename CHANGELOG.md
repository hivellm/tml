# TML Project Changelog

All notable changes to the TML project will be documented in this file and in the component-specific changelogs listed below.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Component Changelogs

For detailed changes in each component, see:

| Component | Changelog | Description |
|-----------|-----------|-------------|
| **Compiler** | [compiler/CHANGELOG.md](compiler/CHANGELOG.md) | Codegen, parser, type checker, HIR/THIR/MIR, query system, LLVM backend, build system |
| **Core Library** | [lib/core/CHANGELOG.md](lib/core/CHANGELOG.md) | Fundamental behaviors, smart pointers, iterators, fmt, cell, mem, str, derive macros |
| **Standard Library** | [lib/std/CHANGELOG.md](lib/std/CHANGELOG.md) | Collections, HTTP, crypto, zlib, JSON, regex, search, SQLite, net, sync, stream |
| **Test Framework** | [lib/test/CHANGELOG.md](lib/test/CHANGELOG.md) | Test runner, assertions, benchmarking, coverage, subprocess architecture |
| **Backtrace** | [lib/backtrace/CHANGELOG.md](lib/backtrace/CHANGELOG.md) | Stack trace capture, symbol resolution, panic integration |

---

## [0.2.4] — 2026-03-25

### Added (Compiler)

- **`--debug-layers` flag** for `tml test` — on failure, automatically emits HIR, MIR, and LLVM IR for failing test functions with diagnosis hints identifying the likely bug layer
- **`--emit-hir` flag** for `tml build` — emits HIR (High-level IR) to `.hir` file for debugging
- **MIR `print_function_by_name`** convenience function — extract and print a single MIR function
- **MCP call logger** — NDJSON logging of all MCP tool invocations (tool, params, timestamp, duration) to `mcp-call-log.jsonl` using C FILE* for Windows compatibility
- **MCP `debug_layers` parameter** on the `test` tool — enables `--debug-layers` from MCP clients
- **`TML_DEBUG_LAYERS=1` env var** — toggles debug-layers as default in MCP test tool (for A/B experiment conditions)
- **`TML_MCP_LOG_DIR` env var** — override log file directory for MCP call logger
- **Diagnosis hints** — `generate_diagnosis_hints()` analyzes error + IR content to suggest likely bug layer (parser, type system, HIR, codegen, runtime)

### Added (Documentation)

- **`docs/user/ch13-04-debug-layers.md`** — user guide for the debug-layers feature
- **`docs/papers/llm-ir-debugging/`** — research paper structure for LLM-assisted debugging through multi-layer IR exposure
- **`docs/papers/llm-ir-debugging/scripts/analyze_logs.py`** — analysis script with session labeling, condition comparison, statistical tests (Welch's t-test, Cohen's d), tool transition heatmaps

## [0.2.3] — 2026-03-25

Major stdlib expansion: 8 tasks completed, panic recovery, compiler hints, FFI types, new collections and sync primitives.

### Added (Core Library)

- **Panic Recovery** (`core::panic`) — catch_unwind, hooks, PanicInfo
  - `PanicInfo` struct — message, file, line, column
  - `set_hook(fn_ptr)` / `clear_hook()` — install custom panic handler called before crash
  - `catch_unwind_fn(fn_ptr) -> CatchResult` — catch panics via setjmp/longjmp
  - `resume_unwind(msg)` — re-panic after catching
  - `CatchResult` enum: `Ok` | `Panicked(Str)`
  - C runtime: `tml_set_panic_hook`, `tml_catch_unwind_fn`, hook call in `panic()`

- **Compiler Hints** (`core::hint`) — optimization directives
  - `unreachable_unchecked()` — LLVM `unreachable` instruction
  - `black_box_i64/bool/f64()` — inline asm memory clobber (prevents constant folding)
  - `spin_loop_hint()` — x86 PAUSE instruction for busy-wait loops
  - `likely(Bool) / unlikely(Bool)` — branch prediction via `@llvm.expect.i1`
  - `assume(Bool)` — `@llvm.assume` for optimizer assertions

- **Core FFI types** (`core::ffi`) — Type-safe C interop wrappers
  - `c_void`, `c_int`, `c_uint`, `c_long`, `c_ulong`, `c_longlong`, `c_ulonglong`
  - `c_float`, `c_double`, `c_size_t`, `c_ssize_t`, `c_ptrdiff_t`, `c_intptr_t`, `c_uintptr_t`
  - `c_long_bits()`, `c_long_max()` — platform-aware (32 on Windows, 64 on Unix)
  - `CStr` — borrowed C string: `from_ptr`, `to_str`, `to_owned_str`, `len`, `is_empty`, `byte_at`
  - Migration guide in module doc comments

### Added (Standard Library)

- **CString Drop** (`std::ffi::cstring`) — `impl Drop for CString` now frees heap memory
- **BinaryHeap advanced** (`std::collections::binary_heap`)
  - `from_items(ref List[T])` — build heap from list
  - `into_sorted() -> List[T]` — heap sort ascending
  - `contains(T) -> Bool` — O(n) search
  - `extend(ref List[T])` — bulk push
- **MinHeap[T]** — min-heap variant (smallest first): `new`, `push`, `pop`, `peek`, `contains`
- **SemaphoreGuard** (`std::sync::semaphore`) — RAII guard with Drop
  - `acquire_guard()` — blocking acquire + auto-release guard
  - `try_acquire_guard() -> Maybe[SemaphoreGuard]` — non-blocking

### Fixed (Compiler)

- **`@derive(Reflect)` size/align** — TypeInfo now reports correct size and alignment via LLVM constant expressions. Previously hardcoded to 0.
- **`black_box` intrinsic** — new C++ intrinsic using inline asm with memory clobber
- **`spin_loop_hint` intrinsic** — new C++ intrinsic emitting x86 PAUSE

### Changed (Project)

- **Task reorganization** — 30 tasks renamed to `phase<X>-<NN>-<label>` format across 6 phases
- **Language completeness roadmap** updated from 41% to 79%
- **8 tasks archived**: core FFI, std FFI, panic recovery, compiler hints, BinaryHeap, Semaphore, WaitGroup, compiler unit tests

### Stats
- **Tests**: 1650+ passing, 92%+ coverage
- **Tasks**: 22 active, 8 archived this session

---

## [0.2.2] — 2026-03-22

Performance, profiling, and code quality release: Tracy profiler integration, HTTP at 183K req/s, and 700+ lowlevel blocks migrated to typed accessors.

### Added (Tracy Profiler Integration)

- **Tracy profiler** — Real-time frame profiling with 70+ instrumented zones across compiler pipeline and standard library
- **Profiler intrinsics** — Zero-cost `profiler::begin`/`profiler::end` in both AST and MIR codegen paths. No runtime overhead when `--profile` is not enabled
- **Instrumented zones** — Lexer, parser, type checker, HIR/MIR lowering, LLVM backend, MIR pass manager, query cache hits, HTTP hot paths, collections, I/O, option, cell, os, glob, math
- **`--profile` build flag** — `scripts\build.bat --profile` builds with Tracy support

### Added (HTTP Performance)

- **183K req/s** — Beats Node.js cluster mode by 32%, syscall-bound at 2 syscalls/request
- **Accept-per-worker architecture** — Each worker thread accepts its own connections
- **HTTP pipelining groundwork** — Non-destructive method/path parsing for future pipelining support
- **Pipelining fix** — Save/restore byte at request boundary

### Changed (Code Quality — lowlevel Migration)

- **702 lowlevel blocks migrated** across 25 files to typed accessor functions
- **HTTP module** — `shared_get`/`shared_set`, `node_get`/`node_set`, `table_read`/`table_write`, `rd()`/`wr()` byte accessors, `buf_hdr_get`/`buf_hdr_set`
- **Stream module** — `buffered.tml`, `pipe.tml` migrated to Buffer accessors
- **Runtime** — `multi_executor.tml`, `timer_wheel.tml` migrated to typed accessors
- **HTTP dispatch/router/parse/conn_pool/rate_limit/agent** — All migrated to structured accessors

### Stats
- **Tests**: 11,000+ across 1,400+ files
- **Coverage**: 15,528/15,628 functions (99%)
- **HTTP performance**: 183K req/s (single machine, Windows)

## [0.2.1] — 2026-03-21

Codegen quality release: 12 compiler bugs fixed, HTTP server compliance, cross-module generic field resolution.

### Fixed (Compiler — Codegen)

- **Pin-through trait method dispatch** — 4 interconnected bugs: type checker unwraps Pin[ref T] to find trait methods on T, generic inference handles ref-wrapped bare generics, method_impl unwraps Pin receivers, static dispatch infers bare generics in ref params
- **Cross-module generic struct field resolution** — `lookup_struct` now follows re-export chains (matching `lookup_enum`). Fixes `Ready[I32].value` resolving as `()` instead of `Maybe[I32]`
- **Range struct type declaration** — MIR codegen emits struct types for library structs used in StructInitInst (e.g., Range[T])
- **ptr_read/ptr_write multi-field structs** — 4 fixes across pipeline: type_params registration, HIR generic substitution, MIR type_args propagation, codegen type resolution
- **Struct field mutation** — mutable struct alloca was dead code in thir_mir_builder (array fast-path returned before struct check)
- **Integer literal coercion in fnptr calls** — `f(42)` where `f: func(I64)` now emits sext from i32 to i64
- **Iterator::fold[B] monomorphization** — method-level generic dispatch, type param inference, GenericType handling
- **memcpy/memmove/memset MIR handlers** — added codegen handlers + LLVM intrinsic declarations
- **copy_nonoverlapping/copy/write_bytes** — registered in type checker (had handlers but no type registration)

### Fixed (HTTP Server)

- **ServerResponse Bool fields → I64** — fixes i1 layout corruption when passed through fn ptrs, unblocking middleware hooks

### Added (HTTP Server — Phase 2+3 Complete)

- **Chunked transfer-encoding** (RFC 7230 §4.1) — decode_chunked, encode_chunk, recv_chunked_body, worker integration
- **Expect: 100-continue** — sends `HTTP/1.1 100 Continue` before body reading
- **405 Method Not Allowed + Allow header** — probes all 7 method radix trees
- **501 Not Implemented** — for unrecognized HTTP methods
- **URL percent-decoding** — verified already implemented, 14 tests added
- **Date header** — 404/501 responses now include RFC 7231 Date via app_build_response
- **Idle timeout enforcement** — SO_RCVTIMEO on keep-alive connections
- **Middleware hooks re-enabled** — onRequest, preHandler, onResponse wired into app_dispatch
- **Custom error handler** — onError hooks called on 404 before default response

### Added (Core Library)

- **Maybe::take()** — consumes self, returns the option (like Rust's Option::take)
- **pub use in core::future** — Context, Poll, Ready, Pending now publicly re-exported

### Tests

- 13 new Buffer tests (core ops, slice/str, endian roundtrips) — collections suite 73/73
- 14 URL percent-decoding tests
- 10 chunked transfer-encoding tests (8 decoder + 3 header detection)
- future_ready_value test now passes

## [0.2.0] — 2026-03-19

Major release with query-based incremental compilation, embedded LLVM/LLD, 35+ new standard library modules, and comprehensive test coverage reaching 99%.

### Highlights

- **Query-Based Compilation** — Demand-driven pipeline (like rustc) with cross-session incremental caching via Red-Green coloring. Near-instant rebuilds when source unchanged.

- **Embedded LLVM + LLD** — In-process IR-to-object compilation and linking. Zero subprocess spawning. Full test suite from ~15 min to ~17 seconds (50x improvement).

- **Zig CC as Default Compiler** — Zig CC (Clang 20.1.2 + bundled LLD) replaces MSVC for building the TML compiler itself.

- **THIR Layer** — Typed HIR between type checking and MIR, with advanced trait solver, numeric coercion insertion, and pattern exhaustiveness checking.

- **Polonius Borrow Checker** — Alternative Datalog-style solver (`--polonius`), strictly more permissive than NLL.

- **Standard Library Expansion** — 35+ new modules including:
  - `std::http` — Full HTTP server and client with router, TLS, cookies, multipart
  - `std::crypto` — SHA, AES-GCM, ChaCha20, RSA, ECDSA, Ed25519, X.509, DH/ECDH
  - `std::zlib` — Deflate, Gzip, Brotli, Zstd compression
  - `std::sqlite` — SQLite3 FFI bindings
  - `std::regex` — Thompson's NFA regex engine
  - `std::search` — BM25 text search + HNSW vector search
  - `std::aio` — Async I/O event loop (epoll/WSAPoll, timer wheel)
  - `std::stream` — Composable streams with backpressure
  - `std::math`, `std::datetime`, `std::os`, `std::glob`, `std::random`

- **Core Library Additions** — Smart pointers (`Heap[T]`, `Shared[T]`, `Sync[T]`), atomic operations, `@derive` macros (7 traits), reflection, arena/pool allocators, SIMD intrinsics.

- **Test Infrastructure** — Subprocess-based architecture (Go model), NDJSON protocol, suite-level filtering, coverage via `TML_COVERAGE_FILE`. No more hangs.

- **Runtime Migration** — Buffer, List, HashMap all pure TML. 6 dead C files deleted (2,661 lines). string.c reduced 59%. 23 hardcoded sync declares removed. Float math → LLVM intrinsics.

- **Performance** — O0 pipeline overhaul (SROA, Mem2Reg, EarlyCSE, inlining), SSA struct construction (`insertvalue`/`extractvalue`), entry-block alloca hoisting, nullable Maybe optimization (8 bytes instead of 16).

### Added (Async Network Stack — 2026-03-19)

- **Multi-threaded Executor** — Work-stealing executor with N workers, global task queue, graceful shutdown (`lib/std/src/runtime/multi_executor.tml`)
- **Thread Spawn** — `thread::spawn_fn`, `thread::spawn_i64`, `spawn_blocking` with trampoline pattern
- **AsyncRead/AsyncWrite** behaviors + `AsyncBufReader`/`AsyncBufWriter` (`lib/std/src/stream/async_io.tml`, `async_buffered.tml`)
- **BufferView** — Zero-copy buffer view for network protocols (`lib/std/src/net/buffer_view.tml`)
- **ALPN** — TLS protocol negotiation via `TlsContext::set_alpn_protocols()`
- **select2** — Future combinator for racing two futures (`lib/core/src/future/select.tml`)
- **Promise[T]** — JavaScript-style promises with resolve/reject/then/catch/all/race/any (`lib/std/src/promise/mod.tml`)
- **Observable[T]** — Reactive streams with 8 operators + Subject/BehaviorSubject/ReplaySubject (`lib/std/src/observable/mod.tml`)
- **WebSocket RFC 6455** — Frame codec, masking, handshake using `std::crypto::sha1` (`lib/std/src/http/websocket.tml`)
- **HTTP/2 RFC 7540** — Binary frame codec, 10 frame types, stream state machine, connection management (`lib/std/src/http/h2/`)
- **HPACK RFC 7541** — Header compression with static table (61 entries), dynamic table, integer/string codec
- **Controller pattern** — `Controller` behavior for route registration (`lib/std/src/http/controller.tml`)
- **@Get/@Post/@Put/@Delete/@Patch decorators** — Full compiler pipeline: parser → type checker → HIR → THIR → MIR → codegen. Generates `__tml_register_routes()` auto-registration
- **Pipe operator `|>`** — Left-associative syntactic sugar: `x |> f` → `f(x)`, `x |> f(a)` → `f(x, a)`, `x |> .method()` → `x.method()`
- **Waker::wake()** — Fixed vtable function pointer dispatch (was panicking)

### Fixed (Codegen — 2026-03-19)

- **Nested generic monomorphization** — `Poll[Outcome[I64, IoError]]` was generating `%struct.Outcome__I32__I32` instead of correct type. Fixed `expected_enum_type_` propagation in call.cpp.
- **Generic static → container push** — `list.push(GenericType::static_method(42))` emitted unresolved `GenericType__T` instead of `GenericType__I32`. Fixed type substitution in `infer_expr_type`.

### Fixed (Codegen Gaps — 2026-03-19)

- **async/await type mismatch** — Fixed state machine codegen producing i64 where i32 was expected. Added AwaitInst handler to MIR codegen path. Async functions with `.await` now work end-to-end for all return types.
- **Incremental cache staleness** — `compiler_build_hash()` used `__DATE__/__TIME__` of a single source file, causing stale cache hits after codegen changes. Now uses the compiler binary's last-write-time for reliable invalidation.
- **dyn Behavior dispatch** — Full vtable dispatch across 5 compiler layers (HIR, type checker, THIR, MIR, codegen). Fixed UB in DCE pass (inserting into unordered_set during iteration).
- **sret convention for indirect calls** — Function pointer calls returning structs were missing sret convention, causing SEGFAULT. Direct calls had sret but indirect calls skipped it.

### Stats
- **Tests**: 1599 tests, 1130 passing, 0 runtime failures, ~469 compile errors (pre-existing)
- **Coverage**: 15528/15628 functions (99%)
- **Compiler size**: ~100MB monolithic, or thin launcher + plugin DLLs (modular)

## [0.1.0] — 2025-12-22

Initial release of the TML language.

### Added
- TML compiler with lexer, parser, type checker, borrow checker, LLVM codegen
- Core library with fundamental behaviors (Clone, Eq, Ord, Hash, Display, Debug)
- Standard library with collections (List, HashMap, Buffer), file I/O, networking, threading, JSON
- Test framework with `@test` decorator and polymorphic assertions
- Backtrace library for stack trace capture and symbol resolution
- CLI with `build`, `run`, `test` commands
- VSCode extension with syntax highlighting
