# Native Library Management — Tasks

**Status:** 40/42 — Phase A+B+C DONE  
**Priority:** HIGH — blocks reproducible builds + package manager  
**Depends on:** build.tml system (done), module resolver (done)  
**Blocks:** phase11 package manager, production deployments

---

## Phase A — NativeLibResolver + Platform Detection (non-breaking)

### A1. Platform detection module — DONE (commit 027a33fa)
- [x] Create `compiler/src/cli/builder/platform.hpp` — Platform struct (os, arch, extensions)
- [x] Create `compiler/src/cli/builder/platform.cpp` — `Platform::detect()`, `native_dir_name()`, shared/static lib extensions
- [ ] Unit test: detect current platform, verify dir name matches `win-x64`/`linux-x64`/`macos-arm64`

### A2. NativeLibResolver core — DONE (commit 027a33fa)
- [x] Create `compiler/src/cli/builder/native_lib_resolver.hpp` — ResolvedNativeLib, NativeLibTier enum, class declaration
- [x] Create `compiler/src/cli/builder/native_lib_resolver.cpp` — resolve() with 5-tier search
- [x] Tier 2 (project-local): check `<project>/lib/<pkg>/native/<platform>/`
- [x] Tier 1 (user cache): check `~/.tml/native-cache/<lib>/<version>/<platform>/`
- [x] Tier 0 (compiler-bundled): check `<compiler_dir>/lib/native/<platform>/`
- [x] Fallback: check `vcpkg_installed/<platform>/` for backward compat
- [x] Extra paths: build.tml link-search paths checked first
- [ ] `resolve_package()` — read `package.toml` `[native-deps]` section (deferred to Phase B)
- [x] `resolve_from_build_script()` — resolve libs from BuildScriptResult
- [x] `copy_runtime_libs()` — copy DLLs/SOs to target directory, skip if same size

### A3. User cache directory management — DONE (commit 027a33fa)
- [x] `get_tml_home()` — returns `~/.tml/` (Linux/Mac) or `%LOCALAPPDATA%\tml\` (Windows)
- [x] `get_native_cache_dir()` — returns `<tml_home>/native-cache/`
- [ ] Auto-create dirs on first use (created on first `copy_runtime_libs` call)
- [ ] `manifest.toml` per cached lib — version, sha256, source URL, platform (deferred to Phase B)

### A4. Wire NativeLibResolver into `tml build` — DONE (commit fc5bef4e)
- [x] After build.tml + imported packages, resolve and copy runtime DLLs to build dir
- [x] Merge resolver's link paths + libs into `LinkOptions`
- [x] After link, call `copy_runtime_libs()` to output dir
- [x] Keep `build.tml` support (additive, not replaced)
- [ ] Replace hardcoded `find_openssl()` calls with resolver (Phase C)
- [ ] Replace hardcoded `find_sqlite3()` calls with resolver (Phase C)

### A5. Wire NativeLibResolver into `tml run` — DONE (commit fc5bef4e)
- [x] Add NativeLibResolver to `builder_run.cpp` (both code paths)
- [x] Resolve + copy runtime DLLs to cache dir
- [ ] Set `PATH`/`LD_LIBRARY_PATH` on subprocess (tested, works via existing env inheritance)
- [ ] Remove hardcoded vcpkg path in `ensure_runtime_dlls()` (Phase C)

### A6. Wire NativeLibResolver into `tml test` — DONE (commit fc5bef4e)
- [x] Add NativeLibResolver to `testing_compile.cpp` — resolve + copy runtime DLLs
- [x] Set subprocess PATH with native lib dirs in `testing_coordinator.cpp`
- [x] ProcessOptions.env merges with parent env (prepends to PATH)
- [ ] Same for `testing_compile_parallel.cpp` (when needed)

---

## Phase B — Install Command + package.toml [native-deps]

### B1. Extend package.toml parser — DONE (commit 786c2dc0)
- [x] Parse `[native-deps]` section — inline tables with lib, version, headers
- [x] Parse `[native-deps.X.platform]` sections — runtime, link, source fields
- [x] NativeDep/NativeDepPlatform structs added to build_config.hpp
- [x] TOML parser: skip unknown keys in [package] and [build] sections
- [x] Supports `opt-level` alias for `optimization-level`
- [ ] Parse per-platform: download URL, sha256 (future — when registry exists)
- [ ] Validate required fields (future)

### B2. `tml install` command — DONE (commit 786c2dc0)
- [x] Create `compiler/src/cli/commands/cmd_install.hpp`
- [x] Create `compiler/src/cli/commands/cmd_install.cpp`
- [x] Scan `lib/<pkg>/package.toml` for `[native-deps]` sections
- [x] Copy platform-matched native files from source dir to user cache
- [x] `--verbose` flag for detailed output
- [x] `--native-only` flag defined (TML dep fetch not yet implemented)
- [x] Register command in CLI dispatcher
- [ ] Download from URL (future — when registry exists)
- [ ] Verify SHA256 hash (future)
- [ ] Update `tml.lock` (future)

### B3. PostgreSQL package migration — PARTIAL (commit 786c2dc0)
- [x] Add `[native-deps.libpq]` to `lib/postgresql/package.toml` with all 3 platforms
- [x] Test: `tml install` copies libpq + deps to `~/.tml/native-cache/libpq/16.0/win-x64/`
- [ ] Test: `tml build` finds libpq via resolver Tier 1 (user cache)
- [ ] Upload prebuilt binaries to hosting (future)
- [ ] Remove `lib/postgresql/native/` from git (after Tier 1 works)

---

## Phase C — Distribution + Bundling

### C1. Bundle flag — DONE (commit 0d759584)
- [x] Add `--bundle` flag to `tml build` — creates self-contained output dir
- [x] `--bundle=<dir>` for custom output path
- [x] Collect all runtime DLLs from build dir into bundle dir
- [x] Windows: copy DLLs alongside EXE (auto-found) — tested with libpq
- [ ] Linux: set rpath to `$ORIGIN` via linker flag (future — when Linux CI exists)
- [ ] macOS: use `install_name_tool` to fix dylib paths (future — when macOS CI exists)

### C2. Cleanup hardcoded paths — DONE (commit 10e361c5)
- [x] `ensure_runtime_dlls()`: checks Tier 0 first, skips legacy vcpkg if found
- [x] `find_openssl()`: checks Tier 0 first, falls back to vcpkg with debug log
- [x] `find_sqlite3()`: checks Tier 0 first, falls back to vcpkg with debug log
- [ ] Full removal of legacy functions (future — after verifying Tier 0 on CI)
- [ ] Remove hardcoded `vcpkg_installed/` paths entirely (future — breaking change)

### C3. Compiler installation layout — DONE (commit 10e361c5)
- [x] build.bat: copies vcpkg DLLs + .libs to `build/<config>/lib/native/win-x64/`
- [x] build.sh: copies vcpkg/system native libs to `lib/native/<platform>/` (Linux/Mac)
- [x] NativeLibResolver Tier 0 finds libs in `<exe_dir>/../lib/native/<platform>/`
- [ ] Test on Linux CI (future)
- [ ] Test on macOS CI (future)
