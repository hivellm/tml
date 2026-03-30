# TML Changelog

All notable changes to the TML project are documented in the patch notes directory.

**Full patch notes:** [`docs/patches/`](docs/patches/)

---

## Releases

| Version | Date | Highlights |
|---------|------|------------|
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
