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

### Fixed (Codegen Gaps — 2026-03-19)

- **async/await type mismatch** — Fixed state machine codegen producing i64 where i32 was expected. Added AwaitInst handler to MIR codegen path. Async functions with `.await` now work end-to-end for all return types.
- **Incremental cache staleness** — `compiler_build_hash()` used `__DATE__/__TIME__` of a single source file, causing stale cache hits after codegen changes. Now uses the compiler binary's last-write-time for reliable invalidation.
- **dyn Behavior dispatch** — Full vtable dispatch across 5 compiler layers (HIR, type checker, THIR, MIR, codegen). Fixed UB in DCE pass (inserting into unordered_set during iteration).
- **sret convention for indirect calls** — Function pointer calls returning structs were missing sret convention, causing SEGFAULT. Direct calls had sret but indirect calls skipped it.

### Stats
- **Tests**: ~1553 tests, 1180 passing, 0 failures, ~373 compile errors (pre-existing)
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
