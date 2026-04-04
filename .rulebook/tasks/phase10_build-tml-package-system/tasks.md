# Tasks: Rust-style build.tml + Native Lib Resolution

**Status**: In Progress. 24/32 (75%). Phases 1-5 implemented, Phase 6 deferred (not blocking), Phase 7 pending.
**Depends on**: None (purely additive to existing compiler)
**Blocks**: phase8f_db-postgres (PostgreSQL driver needs this to link libpq)

## Phase 1: Build Directive Data Structures (C++ compiler)

- [x] 1.1 `compiler/include/codegen/codegen_backend.hpp` — Added `link_search_paths` to `CodegenResult`
- [x] 1.2 `compiler/include/query/query_key.hpp` — Added `link_search_paths` to `CodegenUnitResult`
- [x] 1.3 `compiler/src/cli/builder/build_script.hpp` — `BuildScriptResult` struct with all fields
- [x] 1.4 `compiler/src/cli/builder/build_script.hpp` — `parse_build_directives`, `detect_package_dir`, `run_build_script`, `detect_and_run_build_script`

## Phase 2: Build Script Parser (C++ compiler)

- [x] 2.1 `compiler/src/cli/builder/build_script.cpp` — Full implementation of `parse_build_directives` with all 6 directive types
- [ ] 2.2 Unit test: parse valid directives → correct BuildScriptResult fields
- [ ] 2.3 Unit test: ignore non-`tml:` lines (normal program output)
- [ ] 2.4 Unit test: handle empty stdout, missing `=`, unknown directives gracefully

## Phase 3: Build Script Detection & Execution (C++ compiler)

- [x] 3.1 `build.cpp` — `detect_and_run_build_script` called before linking in both legacy and query paths
- [x] 3.2 `build_script.cpp` — Compiles build.tml via `_popen`/`popen` using self tml.exe
- [x] 3.3 `build_script.cpp` — Executes build script exe, captures stdout
- [x] 3.4 `build_script.cpp` — Parses stdout via `parse_build_directives`
- [x] 3.5 `build_script.cpp` — Resolves `link-search` paths relative to package directory
- [x] 3.6 `build.cpp` — Merges build script `link_libs` + search paths into link options
- [x] 3.7 `build_script.cpp` — Warnings printed via TML_LOG_WARN

## Phase 4: Linker Integration (C++ compiler)

- [x] 4.1 `object_compiler.cpp` — Passes `library_search_paths` to LLD (`library_paths`) and clang (`-L` flags)
- [x] 4.2 `build.cpp` — Passes search paths from build script to LinkOptions
- [x] 4.3 `builder_run.cpp` — Build script support for `tml run` (both link sections)
- [x] 4.4 `run_profiled.cpp` — Build script support for profiled runs
- [x] 4.5 `testing_compile_parallel.cpp` — Build script support for test suite linking

## Phase 5: Post-Link Artifact Copying

- [x] 5.1 `build.cpp` — Post-link loop iterates `copy_artifacts` from BuildScriptResult
- [x] 5.2 Resolves artifact path relative to package dir, copies to output dir
- [x] 5.3 Logs each copy via TML_LOG_INFO
- [x] 5.4 Missing artifacts emit TML_LOG_WARN, don't fail build

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
