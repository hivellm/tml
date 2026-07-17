# Tasks: Workspace + Package Registry

## 1. Manifest files
- [x] 1.1 Add `[workspace] members = [...]` to root `tml.toml`
- [x] 1.2 Create `lib/core/tml.toml` with `[package] name = "core"`
- [x] 1.3 Create `lib/std/tml.toml` with `[package] name = "std"` and `[dependencies] core`
- [x] 1.4 Create `lib/test/tml.toml` with `[package] name = "test"` and `[dependencies] core, std`
- [x] 1.5 Create `compiler-tml/tml.toml` with `[package] name = "compiler"` and `[dependencies] core, std, test`

## 2. Directory layout
- [x] 2.1 Move `compiler-tml/serial/*.tml` → `compiler-tml/src/serial/*.tml`
- [x] 2.2 Verify `compiler-tml/src/serial/mod.tml` is the package entrypoint

## 3. PackageRegistry (C++)
- [x] 3.1 Create `compiler/include/package/package_registry.hpp` with `PackageInfo` and `PackageRegistry` singleton
- [x] 3.2 Create `compiler/src/package/package_registry.cpp` — parse workspace `tml.toml`, enumerate members, parse each member manifest, populate map
- [x] 3.3 Add dependency-graph validation (cycle detection, missing-dep error)
- [x] 3.4 Add public API: `lookup_for_module(module_path)`, `has_package_for(module_path)`, `all_packages()`
- [x] 3.5 Reuse existing TOML parser (do not add new dependency)
- [x] 3.6 Register new sources in `compiler/CMakeLists.txt`

## 4. Refactor hardcoded prefix sites
- [x] 4.1 Replace prefix checks in `compiler/src/types/env_module_loading.cpp` (centralized in `resolve_lib_module_path` — all branches now PackageRegistry-aware)
- [x] 4.2 Replace `should_cache` whitelist in `compiler/src/types/module.cpp` (uses `PackageRegistry::is_package_module`)
- [x] 4.2b Fix symbol-strip fallback in `env_module_load_decls.cpp` to apply same `shares_root` check as primary path (was producing duplicated paths like `compiler::serial::reader::compiler::serial`)
- [x] 4.3 Replace `resolve_module_source_path` hardcoded `lib/<name>/src/` in `compiler/src/types/module_binary_read.cpp` — now consults PackageRegistry first
- [x] 4.4 `compiler/src/codegen/llvm/core/generate.cpp` — verified clean (no hardcoded prefixes)
- [x] 4.5 Replace in `compiler/src/codegen/llvm/core/generate_cache.cpp` (uses `PackageRegistry::is_package_module`)
- [x] 4.6 `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp` — verified clean
- [x] 4.7 `compiler/src/codegen/llvm/core/optimization_passes.cpp` — verified clean
- [x] 4.8 `compiler/src/codegen/llvm/core/runtime_modules.cpp` / `runtime_modules_tml.cpp` — verified clean
- [x] 4.9 `compiler/src/codegen/llvm/expr/infer_methods.cpp` / `method_impl.cpp` / `method_impl_module.cpp` — verified clean
- [x] 4.10 Audited remaining sites — fixed `build.cpp`, `builder_run.cpp`, `run_profiled.cpp` (build.tml dispatch whitelist) and `module_metadata.cpp` (compiled-path resolver) to use PackageRegistry. Only remaining literal is `env_lookups.cpp:331` (`"core::"+lower_type` for primitive type lookup — legitimate, not a prefix check).

## 5. Update TML imports
- [x] 5.1 `compiler-tml/src/serial/mod.tml` — change `std::serial::*` self-refs to `compiler::serial::*`
- [x] 5.2 `compiler-tml/src/serial/reader.tml` — same
- [x] 5.3 `compiler-tml/src/serial/ast.tml` — same
- [x] 5.4 `compiler-tml/src/serial/typeenv.tml` — same
- [x] 5.5 `compiler/tests/serial/ast_roundtrip.test.tml` — `use compiler::serial::*`

## 6. Build and verify
- [x] 6.1 Rebuild compiler via `scripts\build.bat`
- [x] 6.2 Run `serial_reader_test.exe` — passes 26/26
- [x] 6.3 Run `compiler-tml/tests/serial/buffer_basic.test.tml` — passes 1/1
- [x] 6.4 Run smoke suite `lib/core/tests/alloc/alloc.test.tml` — passes 1/1 (regression check OK)
- [x] 6.5 Run smoke suite `lib/std/tests/aio/simple.test.tml` — passes 1/1

## 6b. Artifact root separation (TML vs C++)
- [x] 6b.1 Add `get_tml_artifact_root()` helper — routes TML output to `<workspace>/target/{debug,release}` when workspace active
- [x] 6b.2 `get_build_dir`, `get_deps_cache_dir`, `get_run_cache_dir` route through it
- [x] 6b.3 `find_build_root()` in `module_binary.cpp` prefers `<workspace>/target/debug` when workspace active
- [x] 6b.4 Verify `tml check` writes to `target/debug/` instead of polluting `build/debug/`

## 6c. Multi-bin / multi-lib manifest targets
- [x] 6c.1 Add `output_name` field to `BuildOptions` (overrides source stem)
- [x] 6c.2 Thread `effective_name` through `run_build_impl` (legacy) output paths
- [x] 6c.3 Thread `effective_name` through `run_build_with_queries` output paths
- [x] 6c.4 Add manifest-driven dispatch in `dispatcher.cpp`: `tml build` with no path iterates `[[bin]]` + `[lib]`
- [x] 6c.5 Support `--bin <name>` and `--lib` filter flags
- [x] 6c.6 Error when no targets defined or filter finds nothing

## 6d. Rename crate → lib (TML terminology, not Rust)
- [x] 6d.1 `LibConfig::crate_types` → `lib_types`
- [x] 6d.2 TOML key `crate-type = [...]` → `lib-type = [...]`
- [x] 6d.3 CLI flag `--crate-type=<t>` → `--lib-type=<t>`
- [x] 6d.4 MCP tool param `crate_type` → `lib_type`
- [x] 6d.5 Update `tml init` template, help text, doc comments
- [x] 6d.6 Update tests (`test_out_dir.sh`, `ffi_test.cpp`)

## 7. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 7.1 Update or create documentation covering the implementation (docs/specs/34-PACKAGE-MANIFEST.md created)
- [x] 7.2 Write tests covering the new behavior (compiler/tests/package/test_package_registry.sh — workspace member outside lib/, package-name import resolution, unknown-package negative case). Standalone gtest exec was attempted but blocked by pre-existing Zig CC + iostream linkage issue in libtml_package; integration script exercises the same code paths against real tml.exe.
- [x] 7.3 Run tests and confirm they pass — `bash compiler/tests/package/test_package_registry.sh` passes both PASS cases; section 6.2-6.5 smoke tests (serial_reader 26/26, buffer_basic, alloc, aio) also exercise PackageRegistry end-to-end.
