# TML Changelog

All notable changes to the TML project are documented in the patch notes directory.

**Full patch notes:** [`docs/patches/`](docs/patches/)

---

## Releases

| Version | Date | Highlights |
|---------|------|------------|
| [0.3.20](docs/patches/v0.3.20.md) | 2026-04-15 | fix(codegen): resolve K001 struct forward-reference — `llvm_type_name` now emits struct defs on-demand from module registry; fixes `EventEmitter`/`ReadableStream` undefined type blocking all `std::file` tests |
| [0.3.19](docs/patches/v0.3.19.md) | 2026-04-15 | feat(std): `File::sync()` and `File::datasync()` — fsync/fdatasync for WAL durability; `_commit(fd)` on Windows, `fsync`/`fdatasync` on Unix |
| [0.3.18](docs/patches/v0.3.18.md) | 2026-04-15 | perf(compiler): remove embedded LLD — native OS linker via subprocess; `tml_codegen_x86.dll` 78→58 MB (−26%); link.exe (Windows) / ld (Unix) primary, lld fallback |
| [0.3.17](docs/patches/v0.3.17.md) | 2026-04-15 | fix(parser): `match` keyword emits `error[S001]: use 'when' instead` — no more cascading parse errors for Rust/Swift/C# users |
| [0.3.16](docs/patches/v0.3.16.md) | 2026-04-14 | feat(daemon): persistent compilation daemon — `tml daemon start/stop/status`, named-pipe IPC, in-process result cache (22ms vs 98ms `cargo check` on Windows), DLL staleness detection, TML_DAEMON env var |
| [0.3.15](docs/patches/v0.3.15.md) | 2026-04-14 | build: add `scripts/build.bat release` — release DLLs compile 2.93× faster (6.9s→2.4s per file); `TML_COMPILER_BUILD=release` env var to use release DLLs from debug tml.exe; fix Clang `-Wdate-time` in release build |
| [0.3.14](docs/patches/v0.3.14.md) | 2026-04-14 | perf(loader): process-level DLL handle cache with mtime invalidation + background preload thread — eliminates double-`LoadLibrary` on repeated invocations; fixes static-destructor ordering crash on exit |
| [0.3.13](docs/patches/v0.3.13.md) | 2026-04-14 | perf(codegen): NRVO in MIR→LLVM path — sret wrapper calls forward `%sret` directly to callee, eliminating intermediate alloca+load+store; 10 regression tests added |
| [0.3.12](docs/patches/v0.3.12.md) | 2026-04-14 | perf(codegen): verified `insertvalue` chains for value-type struct construction — 1-5 ns/op (vs 16-32 ns/op old alloca path); created `struct_bench.tml` benchmark and 8 regression tests |
| [0.3.11](docs/patches/v0.3.11.md) | 2026-04-14 | feat(compiler): `--release` flag now fully propagates to test runner — LLVM O3 pipeline active end-to-end for `build`, `run`, and `test` commands; `tml.toml` `[profile.release]` supported |
| [0.3.10](docs/patches/v0.3.10.md) | 2026-04-14 | perf(codegen): emit LLVM `select` for branchless scalar if-else — chained if-else-if produces nested selects, eliminating branch mispredictions (CMOV on x86-64); +20% at debug, verified correct IR |
| [0.3.9](docs/patches/v0.3.9.md) | 2026-04-14 | perf(codegen): emit `llvm.assume(i ult n)` at start of every `for i in 0 to n` loop body — enables LICM of loop-invariant header loads and LLVM auto-vectorization; List Iteration 0 ns/op |
| [0.3.8](docs/patches/v0.3.8.md) | 2026-04-14 | fix(codegen): `and`/`or` now emit correct 2-block phi short-circuit — RHS not evaluated when LHS determines result; fixes stale `current_block_` bug across function boundaries |
| [0.3.7](docs/patches/v0.3.7.md) | 2026-04-14 | perf: `@inline` / `@always_inline` decorators now emit LLVM `alwaysinline` — `List.push/pop/get/set/len` inlined, list ops match or beat Rust (push 914M ops/s vs 687M, access 2.9B vs 1.4B) |
| [0.3.6](docs/patches/v0.3.6.md) | 2026-04-14 | perf: `when` integer expressions now emit LLVM `switch` (≥4 literal arms) — ~9.5× speedup vs icmp chain, TML/Rust parity |
| [0.3.5](docs/patches/v0.3.5.md) | 2026-04-14 | N002 fix: crypto/TLS C runtime compilation — `find_openssl()` now sets include path, all 9 crypto `.obj` files compile on first run |
| [0.3.4](docs/patches/v0.3.4.md) | 2026-04-14 | K001 fix: bool/i32 mismatch in `icmp eq`/`ne` — `zext i1` widening for mixed-width boolean comparisons in AST codegen |
| [0.3.3](docs/patches/v0.3.3.md) | 2026-04-14 | K001 fix: `core::str` methods (`len`, `is_empty`, `starts_with`, `ends_with`, `contains`, `trim`) in AST path — string benchmarks unblocked |
| [0.3.2](docs/patches/v0.3.2.md) | 2026-04-12 | K001 codegen fixes, runtime crash fix (InferCtx shared counter), `tml cv` in-process — 27/28 tests pass |
| [0.3.1](docs/patches/v0.3.1.md) | 2026-04-12 | Self-hosting compiler port (phases 14c–15d), `tml cv` coverage command, 84 TML modules, 215 tests |
| [0.3.0](docs/patches/v0.3.0.md) | 2026-04-11 | Language ergonomics — `for i in 0 to N`, `Point { x: 5, ..p1 }`, pattern guards, `let Pair { a, b } = p`, Bool struct fix |
| [0.2.16](docs/patches/v0.2.16.md) | 2026-04-11 | Language documentation overhaul — grammar, lexical spec, RFCs, PEG, tree-sitter, VS Code extension synchronized with actual parser |
| [0.2.15](docs/patches/v0.2.15.md) | 2026-04-10 | TML self-hosting frontend now default parser, `--stage=parser:cpp` fallback, 7 codegen fixes, 0.9% overhead |
| [0.2.14](docs/patches/v0.2.14.md) | 2026-04-06 | MIR consolidation, string interning, 86% compile failure reduction (214→29), RC1-RC9 codegen fixes, 27 Maybe/Outcome methods |
| [0.2.13](docs/patches/v0.2.13.md) | 2026-04-05 | `?.` optional chaining, `let-else` guard, MCP daemon, native lib management, research infra |
| [0.2.12](docs/patches/v0.2.12.md) | 2026-04-04 | Test runner build.tml support, PostgreSQL test suite, RawPtr codegen fix, per-test timeout CLI |
| [0.2.11](docs/patches/v0.2.11.md) | 2026-04-04 | AI library (`std::ia`): tensor, autograd, nn layers, optimizers, training — 34 files, 140 tests |
| [0.2.10](docs/patches/v0.2.10.md) | 2026-04-03 | Codegen architecture: MIR validation (ICE), centralized ABI module, pub use re-exports, CGValue wrapper, response builder restored |
| [0.2.9](docs/patches/v0.2.9.md) | 2026-04-03 | `std::db` module (ORM, query builders, migrations, SQLite adapter), SIMD strings (10x), SQLite 3-4x faster than Rust, `@column` field decorators, 4 codegen fixes |
| [0.2.8](docs/patches/v0.2.8.md) | 2026-04-01 | Chrome DevTools inspector, `tml inspect` debugger, `std::console`, flame graphs, LSP references/rename, DWARF structs/pointers/scopes |
| [0.2.7](docs/patches/v0.2.7.md) | 2026-03-29 | SIMD (SSE2/SSE4.2/AVX2/FMA/NEON), 53+ intrinsics, 9 vector types; parallel test compilation, 1 EXE/file, fail-fast, timestamped logs |
| [0.2.6](docs/patches/v0.2.6.md) | 2026-03-28 | Rust Parity Sprint (16/16) — List/HashMap/Str/File/Thread/Channels/env/Maybe extras |
| [0.2.5](docs/patches/v0.2.5.md) | 2026-03-26 | Reflection, OOP intrinsics, 91% doc coverage, MCP emit-ir in-process |
| [0.2.4](docs/patches/v0.2.4.md) | 2026-03-25 | `--debug-layers` IR diagnostics, MCP call logger, diagnosis hints |
| [0.2.3](docs/patches/v0.2.3.md) | 2026-03-25 | Panic recovery, compiler hints, core FFI types, BinaryHeap/MinHeap/Semaphore |
| [0.2.2](docs/patches/v0.2.2.md) | 2026-03-22 | Tracy profiler, HTTP 183K req/s, 702 lowlevel blocks migrated |
| [0.2.1](docs/patches/v0.2.1.md) | 2026-03-21 | 12 codegen fixes, HTTP RFC compliance (chunked/100-continue/405/501) |
| [0.2.0](docs/patches/v0.2.0.md) | 2026-03-19 | Query compilation, embedded LLVM/LLD (50x speedup), 35+ stdlib modules, HTTP/2, WebSocket |
| [0.1.0](docs/patches/v0.1.0.md) | 2025-12-22 | Initial release |

## Component Changelogs

For changes scoped to a specific component:

| Component | Changelog |
|-----------|-----------|
| **Compiler** | [compiler/CHANGELOG.md](compiler/CHANGELOG.md) |
| **Core Library** | [lib/core/CHANGELOG.md](lib/core/CHANGELOG.md) |
| **Standard Library** | [lib/std/CHANGELOG.md](lib/std/CHANGELOG.md) |
| **Test Framework** | [lib/test/CHANGELOG.md](lib/test/CHANGELOG.md) |
| **Backtrace** | [lib/backtrace/CHANGELOG.md](lib/backtrace/CHANGELOG.md) |
