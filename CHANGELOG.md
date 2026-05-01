# TML Changelog

All notable changes to the TML project are documented in the patch notes directory.

**Full patch notes:** [`docs/patches/`](docs/patches/)

---

## Releases

| Version | Date | Highlights |
|---------|------|------------|
| [0.3.40](docs/patches/v0.3.40.md) | 2026-05-01 | **C17 type-system value-pass crash fixed (phase24b)** — `compiler-tml/src/cc/types.tml` and `compiler-tml/src/cc/lower.tml`: every type-system entry point that took `env: CTypeEnv` (struct of four `HashMap` fields) by value now takes `env: ref CTypeEnv`. Pass-by-value of a struct whose fields hold heap-allocated bucket arrays caused drop glue on the callee's local copy to free the caller's buckets — the next call (`base_to_ctype` for the second function parameter, in the typedef-then-use case) crashed reading freed memory. Switched `base_to_ctype`, `apply_declarator`, `sizeof_type`/`sizeof_heap`, `alignof_type`/`alignof_heap`, `size_for_kind`, `layout_struct`, `layout_union`, `ptr_add_scale`, `resolve_type_name` to `ref` parameters; updated all call sites in `lower.tml` + the regression test. Regression: `compiler-tml/tests/native/c_frontend.test.tml::test_phase24b_base_to_ctype_typedef_repeat` (3 sequential `base_to_ctype` calls on the same env). |
| [0.3.39](docs/patches/v0.3.39.md) | 2026-05-01 | **C17 frontend parser unblocked** — fixed two compounding bugs that crashed `cp_parse_translation_unit` on `int x;`. (1) Dangling-Str leaks in `compiler-tml/src/cc/parser.tml`: five raw `parser.tokens.get(p)` reads inside loops aliased list-slot strings, so dropping the local `tok` freed payloads still owned by `parser.tokens`; routed every read through `cp_peek` and added `.duplicate()` at every escape site (~25 sites). (2) Codegen ABI mismatch in `compiler/src/codegen/llvm/expr/method_static_dispatch.cpp`: the struct→ptr fixup for static method calls relied on a FuncInfo lookup that missed for generic-impl methods like `Heap[T]::new` (instantiation queued, not yet registered at arg-processing time); added a fallback that mirrors the `impl.cpp:392-395` rule unconditionally for the first struct/enum arg. `tml cc t.c --emit=ast` now exits 0 on a real C source. Phase24 Phase 4 (essential.c / mem.c self-compile) unblocked. |
| [0.3.38](docs/patches/v0.3.38.md) | 2026-04-18 | **Self-contained LLVM bootstrap** — `scripts/build-llvm.bat` + `build-llvm-ar.bat` build the vendored LLVM submodule into `build/llvm/`; `find_package(LLVM CONFIG HINTS …)` auto-discovers it (no more `LLVM_DIR` env var, no external `F:/LLVM`). `TML_LLVM_HAS_AARCH64` gate so the compiler links cleanly against an X86-only LLVM. `LLDLinker` searches `build/llvm/bin/` for `lld-link.exe`. Fixed codegen K001 on `TemplateLiteralExpr.as_str()` (type inference was missing the template-literal case). Launcher now pulls the version from the generated `version_generated.hpp` — `tml --version` finally prints the real number. |
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
