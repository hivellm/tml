# 06 — Phased Remediation Plan

## Phase 0 — Zero-risk wins (hours)

- **P0.1 (F-017/F-016):** Make MCP `check`/`build`/`emit-ir` route through the daemon: have `execute_command` set `TML_DAEMON=1` and auto-`daemon start`, OR run `tml_main` in-process in `tml_mcp.exe` with `Loader::set_daemon_mode(true)`. Expected: repeated `check` ~460 ms → ~22 ms.
- **P0.2 (F-001):** Provide/document a `release` compiler build for daily use; measure the 2–3× claim. Low risk, big constant-factor win.
- **Gate:** `mcp__tml__check` on unchanged file < 50 ms; `tml check` release vs debug wall-clock delta recorded.

## Phase 1 — Test aggregation (days)

- **P1.1 (F-005):** Flip default to `--suite-mode` (or `--unified`) for full runs; keep `max_per_suite=1` only for coverage. Reduces link steps 1339 → ~30 (or 1). The code paths already exist and are tested behind flags.
- **P1.2 (F-011):** Confirm `run_all_mode=true` default (it is, `testing_coordinator.hpp:60`) is actually exercised — subprocess count should already be per-suite not per-test.
- **Gate:** full non-coverage `tml test` wall-clock before/after; link-step count logged.

## Phase 2 — Kill redundant stdlib codegen (days–weeks; root-cause work)

- **P2.1 (F-006/F-007):** Fix the LLD multiple-definition / `I32::duplicate` / i64-i32 issues that forced `library_decls_only=false` and disabled the stdlib codegen-state cache. Re-enable `build_stdlib_object` so test objs link a **shared** stdlib instead of each embedding it. This is the highest-value structural fix; needs `codegen-debugger`/`mir-expert`.
- **P2.2 (F-012):** Reuse one initialized `LLVMBackend`/TargetMachine per worker thread instead of per object.
- **Gate:** per-file compile time drop; per-EXE size drop from ~345 KB; cache size ≪ 837 MB.

## Phase 3 — Caching & parallelism polish (days)

- **P3.1 (F-014):** Loosen result-cache invalidation — hash only the compiler's *interface/version*, not the 71 MB DLL mtime, so ordinary rebuilds don't nuke 837 MB of EXEs; investigate why `tests.json` stays empty (persist reliably).
- **P3.2 (F-010):** Batch/append incremental-cache writes to avoid per-file global-mutex serialization.
- **P3.3 (F-013):** Scan each test file's imports once; thread the result through.
- **P3.4 (F-008/F-009):** Once LLVM global-state safety is addressed (P2), raise per-suite file parallelism > 1 and replace the per-file watchdog thread with a shared timer.

## Phase 4 — Build ergonomics (days)

- **P4.1 (F-003):** Apply PCH to the Zig/Clang toolchains (not just MSVC); revisit unity-build blockers for `tml_types`.
- **P4.2 (F-002):** Lazy/mmap plugin DLL loading; keep codegen DLL unloaded for `check`-only work.

## Key files for the implementer/tester

- Test pipeline: `compiler/src/testing/testing_compile.cpp`, `testing_compile_parallel.cpp`, `testing_coordinator.cpp`, `testing_test_cache.cpp`
- Config defaults: `compiler/include/testing/testing_coordinator.hpp`, `compiler/src/cli/commands/cmd_test.hpp`, `cmd_test.cpp`
- Object/link backend: `compiler/src/cli/builder/object_compiler.cpp`
- Daemon (warm state): `compiler/src/cli/commands/cmd_daemon.cpp`; routing `compiler/src/cli/dispatcher.cpp`
- MCP subprocess handlers: `compiler/src/mcp/mcp_tools.cpp`, `mcp_tools_codegen.cpp`, `mcp_tools_project.cpp`
- Build config: `scripts/build.bat`, `compiler/CMakeLists.txt`
- Prior art: `docs/analysis/compiler-internals/single-binary-test-compilation.md`, `docs/analysis/benchmark/08-compilation.md`
