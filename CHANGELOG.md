# TML Changelog

All notable changes to the TML project are documented in the patch notes directory.

**Full patch notes:** [`docs/patches/`](docs/patches/)

---

## Releases

| Version | Date | Highlights |
|---------|------|------------|
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
