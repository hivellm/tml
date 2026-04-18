# TML Changelog

All notable changes to the TML project are documented in the patch notes directory.

**Full patch notes:** [`docs/patches/`](docs/patches/)

---

## Releases

| Version | Date | Highlights |
|---------|------|------------|
| [0.3.37](docs/patches/v0.3.37.md) | 2026-04-17 | **Coverage library scaffold** — new `lib/coverage/` pure-TML reporter with LCOV / llvm-cov JSON / legacy-text ingest, LCOV / Cobertura / JSON / HTML SPA emit, static offline SPA with file tree + inline hit gutter + search. `tml coverage --input=... --format=...` routing shim in C++ dispatcher. Existing `tml cv` legacy mapping kept reachable. Tail: `docs/CODE_COVERAGE.md`, legacy + HTML schema docs. |
| [0.3.26–0.3.36](docs/patches/v0.3.26-0.3.36.md) | 2026-04-15–16 | **Rust parity for list iteration** (pointer-stepping, @inline, constant stride → AVX-512; 1.06× Rust), **std::protobuf** (11 modules, .proto parser + codegen), **4.4× faster cold check** (meta cache persistence), **SSO for Text** (≤23 chars inline, zero malloc), compiler-identifier interning, **full UzDB feedback response** (pipe-hang fix, MCP timeout, stderr routing, BTreeMap IntoIterator, int literal inference, `tml install` TML-dep resolver + `tml.lock`, async-file MVP + design doc, + 4 pre-existing bug fixes), **inline digit-extraction `I*::to_string`** + **HeapValidate-free `tml_str_free`** (41→31 ns/op, 24 %), **heap-mode fast path for `Text.print`/`println`** (no alloc+copy on log output). v0.3.34–v0.3.36 reserved. |
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
