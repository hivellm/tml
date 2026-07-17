# 02 — C++ Compiler Build Performance

**F-001 — The daily-driver compiler is an unoptimized `-O0` debug build. — RESOLVED (phase40b, 2026-07-17)**
Evidence (original): `scripts/build.bat:18` (`BUILD_TYPE=debug` default); `compiler/CMakeLists.txt` only sets explicit `/O2`/`-O3` for `tml_runtime`, `tml_json_runtime`, `tml_search_runtime` (lines 348–360, 988–998) — the entire frontend/codegen (`tml_types`, `tml_codegen`, `tml_mir`, …) inherits the Debug config's no-optimization flags. Every `check`/`build`/`test` runs the compiler's C++ at `-O0`.
Impact: **High** — prior benchmark attributes ~2–3× compile-speed headroom to this alone. Confidence: **High**.

**Resolution (phase40b):**
- Release-config audit: under the Zig CC toolchain (`cmake/toolchains/zig.cmake`, identified as Clang/GNU-frontend) CMake populates `CMAKE_C/CXX_FLAGS_RELEASE = -O3 -DNDEBUG`; nothing in `compiler/CMakeLists.txt` references `CMAKE_BUILD_TYPE` conditionally or clears per-config flags on the Clang path, and all `target_compile_options` are additive. Verified in the generated release `build.ninja`: **all 40 targets, every object, carry `-O3`; 0 unoptimized objects.** No CMakeLists fix was needed — F-001 was purely the debug *default build type*.
- `scripts\build.bat release` builds clean: 507/507 ninja steps, 0 release-only compile/link errors, ~1–2 min wall on this machine (zig cc cache + high parallelism; expect longer cold). Output: `build/release/bin/tml.exe` (1.28 MB) + `plugins/tml_compiler.dll` (77.2 MB) + `plugins/tml_codegen_x86.dll` (52.0 MB) + tools/test/mcp plugins. PDBs are still produced (release is debuggable at a coarse level; `-O3` reordering applies).
- Measured delta (full protocol + tables in `01-measurements.md`, "phase40b debug vs release"): `tml check` trivial **1.86×**, `tml check` real 714-line module **2.04×**, 49-file suite compile+run **2.11×** (66.6 s → 31.5 s). Identical results: 49/49 pass on both binaries, byte-identical `check` diagnostics, and the working tree's pre-existing K001 failures reproduce byte-identically under both (no optimization-induced miscompile).

## Daily-driver workflow (debug vs release, post-phase40b)

**Where the binaries live**
- Debug: `build/debug/bin/tml.exe` (+ `plugins/`) — built by `scripts\build.bat` (default).
- Release: `build/release/bin/tml.exe` (+ `plugins/`) — built by `scripts\build.bat release`. Fully separate output tree; the two coexist.

**When to use which**
- **Release** — direct CLI invocations that do heavy compilation and don't involve the MCP daemon: long suite runs, batch `check`/`build` over many files, benchmarks, C→TML migration sweeps. ~2× wall-clock across the board.
- **Debug** — (a) C++ compiler debugging (unoptimized code, accurate stepping, `-g`); (b) everything that flows through MCP/daemon (see below); (c) the binary the default rebuild loop (`/build-compiler`, `scripts\build.bat`) refreshes after every C++ change.

**How MCP/daemon pick a binary (and why we did NOT flip them to release)**
- `.mcp.json` starts `./build/debug/bin/tml_daemon.exe` (MCP stdio server). Its tools spawn `tml.exe` via `get_tml_executable()` (`compiler/src/mcp/mcp_tools.cpp:752`), whose hardcoded candidate order prefers `./build/debug/bin/tml.exe`; with `TML_DAEMON=1` (phase40a) that child forwards to the warm `tml.exe __daemon_server` — the same debug binary. Selecting release is therefore a **C++ code change** (candidate reorder), not a safe/reversible config flip.
- The test result/EXE cache is **shared and debug-anchored**: `build/debug/cache/tests.json` + `cache/tests/` are hardcoded (`testing_coordinator.cpp:1140`), and the cache key fingerprints the *running binary's own* `plugins/tml_compiler.dll` (`testing_test_cache.cpp:562`). Every debug↔release alternation logs `[cache] Compiler/runtime changed — invalidating all cached EXEs` and recompiles everything. Mixed-binary usage across concurrent sessions would thrash the ~837 MB EXE cache continuously.
- Staleness footgun: the daily C++ fix loop rebuilds **debug only**. An MCP wired to release would silently serve *stale* compiler code after every codegen fix until someone remembers `scripts\build.bat release`.
- Marginal benefit: phase40a's daemon already answers unchanged-input MCP `check`/`build` in ~7 ms (cache-hit path does not exercise the compiler); release only speeds up cache-miss compiles.

**Decision: documented, not flipped.** MCP/daemon stay on debug. Revisit if/when (a) the test cache becomes binary-aware (per-config cache dirs or config-qualified keys) and (b) the rebuild loop refreshes both configs.

**Exact switch procedure (if you accept the caveats above):** edit `.mcp.json` `command` → `./build/release/bin/tml_daemon.exe`, reorder the candidates in `get_tml_executable()` (`compiler/src/mcp/mcp_tools.cpp:755-761`) to prefer `./build/release/bin/tml.exe`, rebuild, and adopt the discipline of running `scripts\build.bat release` after every C++ change. Reverse by restoring both.

**Keeping release fresh:** after C++ changes, run `cmd //c "scripts\\build.bat release"` — with a warm zig cc cache this is minutes, not the cold ~full-rebuild cost. The release tree is additive; it never interferes with the debug workflow except through the shared test cache noted above.

**F-002 — 123 MB of monolithic plugin DLLs loaded per invocation, no lazy-load.**
Evidence: `tml_compiler.dll` 71 MB + `tml_codegen_x86.dll` 52 MB (`build/debug/bin/plugins/`); loaded via `plugin::Loader`. Warm ≈ 50 ms, cold 2–3 s (`08-compilation.md:29`). The codegen DLL is loaded even for `check` (type-only) work.
Impact: **Medium** (High when cold). Confidence: **High**.

**F-003 — PCH and unity builds don't apply to the default (Zig CC) toolchain.**
Evidence: `compiler/CMakeLists.txt:916` guards `target_precompile_headers(...)` with `if(MSVC)` — but ADR-007 makes **Zig CC the default** (`build.bat:33,226–258`), so PCH is skipped on the default build path. `tml_types` explicitly disables unity builds (comment line 592). ~366 source files compiled without PCH on the default toolchain.
Impact: **Medium** (slower incremental C++ rebuilds → slows every codegen fix cycle). Confidence: **High**.

**F-004 — version-header rebuild cascade already fixed (mitigated).**
Evidence: `build.bat:145–153` (build number no longer auto-increments), `CMakeLists.txt:270–276` (version header generated into build dir, not source). Noted as *resolved* so it isn't re-attempted.
Impact: N/A (historical). Confidence: **High**.
