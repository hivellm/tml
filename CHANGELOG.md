# TML Changelog

All notable changes to the TML project are documented in the patch notes directory.

**Full patch notes:** [`docs/patches/`](docs/patches/)

---

## Releases

| Version | Date | Highlights |
|---------|------|------------|
| [0.3.30](docs/patches/v0.3.30.md) | 2026-04-16 | **Pipe-hang fix (phase0r)** — `tml check/run/build` no longer deadlock when stdout/stderr redirected to a pipe. Root cause: Windows CRT full-buffering + 64 KB pipe buffer. Fix: line-buffer stdout when non-TTY + `atexit` flush + `--no-color` / `--non-interactive` flags + `TML_NO_COLOR` env var. Unblocks MCP, CI, AI agents |
| [0.3.29](docs/patches/v0.3.29.md) | 2026-04-16 | **SSO for Text** (≤23 chars inline, zero malloc — 48→0 ns/op), string interning for compiler identifiers, io.cpp SSO-aware printing |
| [0.3.28](docs/patches/v0.3.28.md) | 2026-04-16 | **std::protobuf** complete (11 modules, .proto parser + TML codegen), **4.4× faster cold check** (meta cache persistence), module discovery fix, allocator attributes, encoding leak fix |
| [0.3.26–0.3.27](docs/patches/v0.3.26-0.3.27.md) | 2026-04-15 | **Rust parity for list iteration** — pointer-stepping for-in, @inline iterators, constant stride, AVX-512 `<8 x i64>` vectorization. TML 4.32B vs Rust 4.57B ops/s (1.06×, within noise) |
| [0.3.20–0.3.25](docs/patches/v0.3.20-0.3.25.md) | 2026-04-15 | BTreeMap for-in, 13 i32 fallbacks removed, K001 struct forward-ref fix, U128 display, std::msgpack, i128 arithmetic, parallel CGU codegen |
| [0.3.10–0.3.19](docs/patches/v0.3.10-0.3.19.md) | 2026-04-14–15 | LLVM select (CMOV), --release end-to-end, insertvalue structs, NRVO sret, DLL cache+preload, release DLLs (2.93×), daemon (22ms), match diagnostic, remove LLD (−26%), File::sync |
| [0.3.0–0.3.9](docs/patches/v0.3.0-0.3.9.md) | 2026-04-11–14 | Language ergonomics sprint, self-hosting port, K001 fixes (str/bool/enum), crypto linking, switch codegen, @inline, short-circuit, bounds-check elim |
| [0.2.10–0.2.16](docs/patches/v0.2.10-0.2.16.md) | 2026-04-03–11 | MIR validation, std::ia (AI), std::db (ORM/SQLite), optional chaining, let-else, self-hosting frontend, language docs overhaul |
| [0.2.0–0.2.9](docs/patches/v0.2.0-0.2.9.md) | 2026-03-19–04-03 | Query compilation, embedded LLVM/LLD, 35+ stdlib modules, HTTP/2, WebSocket, SIMD, Tracy profiler, DevTools inspector, Rust parity sprint |
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
