# Tasks: Rust-style build.tml + Native Lib Resolution

**Status**: Planning. 0/32 (0%).
**Depends on**: None (purely additive to existing compiler)
**Blocks**: phase8f_db-postgres (PostgreSQL driver needs this to link libpq)

## Phase 1: Build Directive Data Structures (C++ compiler)

- [ ] 1.1 `compiler/include/codegen/codegen_backend.hpp` — Add `link_search_paths: std::set<std::string>` alongside existing `link_libs`
- [ ] 1.2 `compiler/include/query/query_key.hpp` — Add `link_search_paths` to `CodegenResult`
- [ ] 1.3 `compiler/include/cli/build_script.hpp` — New header: `BuildScriptResult` struct with `link_libs`, `link_search_paths`, `copy_artifacts`, `warnings`, `cfg_symbols`, `rerun_paths`
- [ ] 1.4 `compiler/include/cli/build_script.hpp` — `parse_build_directives(stdout: string) -> BuildScriptResult` declaration

## Phase 2: Build Script Parser (C++ compiler)

- [ ] 2.1 `compiler/src/cli/build_script.cpp` — Implement `parse_build_directives`: parse `tml:link-lib=`, `tml:link-search=`, `tml:copy-artifact=`, `tml:warning=`, `tml:cfg=`, `tml:rerun-if-changed=` from stdout lines
- [ ] 2.2 Unit test: parse valid directives → correct BuildScriptResult fields
- [ ] 2.3 Unit test: ignore non-`tml:` lines (normal program output)
- [ ] 2.4 Unit test: handle empty stdout, missing `=`, unknown directives gracefully

## Phase 3: Build Script Detection & Execution (C++ compiler)

- [ ] 3.1 `compiler/src/cli/builder/build.cpp` — In `build_command`: after resolving package path, check if `build.tml` exists in package root
- [ ] 3.2 `compiler/src/cli/builder/build.cpp` — If `build.tml` exists: compile it to a temp executable using the standard pipeline (reuse existing compile-to-exe logic)
- [ ] 3.3 `compiler/src/cli/builder/build.cpp` — Execute the temp executable as subprocess, capture stdout
- [ ] 3.4 `compiler/src/cli/builder/build.cpp` — Parse stdout via `parse_build_directives`, store result
- [ ] 3.5 `compiler/src/cli/builder/build.cpp` — Resolve `tml:link-search` paths relative to the package directory (e.g., `native/win-x64` → `lib/postgresql/native/win-x64`)
- [ ] 3.6 `compiler/src/cli/builder/build.cpp` — Merge build script `link_libs` + `link_search_paths` into codegen result
- [ ] 3.7 `compiler/src/cli/builder/build.cpp` — Print any `tml:warning=` messages to stderr

## Phase 4: Linker Integration (C++ compiler)

- [ ] 4.1 `compiler/src/backend/lld_linker.cpp` — Accept `link_search_paths` parameter; emit `-L <path>` for each entry before `-l<lib>` flags
- [ ] 4.2 `compiler/src/cli/builder/build.cpp` lines ~891 — Pass `link_search_paths` to linker alongside existing `link_libs`
- [ ] 4.3 `compiler/src/cli/builder/builder_run.cpp` lines ~316 — Same for `tml run` path
- [ ] 4.4 `compiler/src/cli/builder/run_profiled.cpp` lines ~270 — Same for profiled runs
- [ ] 4.5 `compiler/src/testing/testing_compile_parallel.cpp` lines ~483 — Same for test suite linking

## Phase 5: Post-Link Artifact Copying

- [ ] 5.1 `compiler/src/cli/builder/build.cpp` — After successful link, iterate `copy_artifacts` from BuildScriptResult
- [ ] 5.2 For each artifact: resolve path relative to package dir, copy to output dir (where .exe was placed)
- [ ] 5.3 Log each copy: `[build] Copying libpq.dll → build/debug/bin/`
- [ ] 5.4 Handle missing artifacts gracefully (warn, don't fail)

## Phase 6: Incremental Cache Integration

- [ ] 6.1 `compiler/src/query/query_incr.cpp` — Save `link_search_paths` alongside `link_libs` in incremental cache
- [ ] 6.2 `compiler/src/query/query_incr.cpp` — Load `link_search_paths` from incremental cache
- [ ] 6.3 `compiler/src/cli/builder/build.cpp` — Cache build script output; re-run only if `tml:rerun-if-changed` paths have newer mtime

## Phase 7: End-to-End Validation

- [ ] 7.1 Create `.sandbox/test_build_script/build.tml` — minimal build script that emits `tml:link-search=native` and `tml:warning=hello`
- [ ] 7.2 Create `.sandbox/test_build_script/src/mod.tml` — minimal TML file
- [ ] 7.3 Create `.sandbox/test_build_script/native/` with a dummy `.lib` file
- [ ] 7.4 Verify `tml build .sandbox/test_build_script/src/mod.tml` detects build.tml, runs it, and passes search paths to linker
- [ ] 7.5 Verify `tml:warning=` messages appear in build output
- [ ] 7.6 Verify DLL copying works with `tml:copy-artifact=`
