# Proposal: Workspace + Package Registry (Rust-style)

## Why

Module namespaces (`core::`, `std::`, `test::`) are hardcoded in ~49 sites across 15 C++ files. There is no way for a new package — specifically the self-hosted `compiler-tml` — to declare its own namespace without patching the compiler. This blocks phase12f, which needs `compiler-tml` to exist as a first-class TML package importable as `compiler::*`. The fix is a Cargo-style workspace: a workspace file lists members, each member declares its package name in its own `tml.toml`, and the compiler builds a `PackageRegistry` at boot that replaces all hardcoded prefix checks.

## What Changes

- Root `tml.toml` gains `[workspace] members = [...]`.
- Each member (`lib/core`, `lib/std`, `lib/test`, `compiler-tml`) gets its own `tml.toml` declaring `[package] name = "..."` and `[dependencies]`.
- Source root is fixed at `<member>/src/` (Rust convention, no override).
- New `compiler/src/package/package_registry.{hpp,cpp}` parses the workspace and exposes `lookup(prefix) → PackageInfo`.
- All 49 hardcoded prefix sites are replaced by calls to `PackageRegistry`.
- `GlobalModuleCache::should_cache` consults the registry instead of a static whitelist.
- `compiler-tml/serial/` moves to `compiler-tml/src/serial/` to match the convention.
- Internal `.tml` imports in the moved serial files change from `std::serial::*` to `compiler::serial::*`.
- Explicit `[dependencies]` per package — Rust-style, catches accidental coupling.

## Impact

- Affected specs: `docs/specs/33-SERIAL-FORMAT.md` (paths), new package manifest spec.
- Affected code: `compiler/src/types/env_module_loading.cpp`, `module.cpp`, `module_binary_read.cpp`, ~9 codegen files under `compiler/src/codegen/llvm/`, `compiler/CMakeLists.txt`, 5 files in `compiler-tml/src/serial/`, `compiler/tests/serial/ast_roundtrip.test.tml`.
- Breaking change: YES — `std::serial::*` no longer exists; any consumer must use `compiler::serial::*` and declare `compiler` as a dependency. Only internal use so far.
- User benefit: adding a new TML package requires zero C++ changes — only a new directory + `tml.toml` entry. First step toward a real package ecosystem and the self-hosting compiler living as a proper TML project.
