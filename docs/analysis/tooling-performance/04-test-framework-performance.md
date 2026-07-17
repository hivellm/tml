# 04 — Test Framework Performance (The Dominant Cost)

**F-005 — Default test mode compiles ONE native EXE per test file (~1339 codegen+link cycles).**
Evidence: `cmd_test.hpp:170` (`suite_mode = false` default) → `cmd_test.cpp:293` (`max_per_suite = (coverage || !suite_mode) ? 1 : 10`) → `group_into_suites(..., 1)`; the cache proves it (1339 `*.exe`, one per file name). The aggregated model already exists — `--suite-mode` (10/suite) and `--unified` (single mega-binary, `compile_unified_binary` in `testing_compile_parallel.cpp:131`) — but **neither is default**, despite `single-binary-test-compilation.md` approving it ("~10× reduction").
Impact: **Very High** — this is the single largest test-time multiplier (≈205→1339 link steps + process spawns). Confidence: **High**.

**F-006 — The stdlib codegen-state fast-path is DISABLED; every test file re-emits the entire stdlib.**
Evidence: `testing_compile_parallel.cpp:41–43` — the `build_stdlib_object(config)` call is commented out ("cached state causes codegen issues: I32::duplicate redefinition … i64/i32 type mismatches"). Because `g_stdlib_codegen_state` stays null, `compile_suite` (`testing_compile.cpp:623–628`) falls to `qopts.incremental=true` and runs `emit_module_pure_tml_functions()` for **all ~5000 functions across 287 modules** per test file.
Impact: **Very High** — redundant full-stdlib codegen multiplied by 1339. Confidence: **High**.

**F-007 — Each test object embeds full internal-linkage stdlib (`library_decls_only=false`).**
Evidence: `testing_compile.cpp:995` ("each test .obj has full library defs (internal linkage)"); `testing_compile_parallel.cpp:219–224`. Consequence: every EXE ~345 KB with the whole stdlib duplicated, heavy per-EXE link, 837 MB cache. Rooted in the same LLD multiple-definition issue that blocks F-006.
Impact: **High** (bloats link + disk, blocks shared-stdlib linking). Confidence: **High**.

**F-008 — Per-file codegen inside a suite is forced single-threaded.**
Evidence: `testing_compile.cpp:598` `int num_compile_threads = 1;` ("avoid LLVM global state corruption"). Parallelism only exists at suite level (≤8, `testing_compile_parallel.cpp:60–66`). LLVM global (non-thread-safe) state is the root blocker for finer parallelism.
Impact: **Medium–High**. Confidence: **High**.

**F-009 — Each file spawns a detached watchdog thread with 60 s timeout + 100 ms polling.**
Evidence: `testing_compile.cpp:645–668`. Thread create/detach per file plus coarse 100 ms poll granularity adds latency and thread churn across 1339 files.
Impact: **Low–Medium**. Confidence: **High**.

**F-010 — Incremental-cache saves serialize all suite workers on a global mutex.**
Evidence: `testing_compile.cpp:58` `g_incr_cache_mutex`, used at `707–711` around `save_incremental_cache`. When the stdlib fast-path is off (the current default, F-006), incremental is ON and every file's cache write serializes the otherwise-parallel workers on `incr.bin` disk I/O.
Impact: **Medium**. Confidence: **High**.

**F-011 — Subprocess-per-suite execution; each test EXE loads ~100 MB of runtime DLLs.**
Evidence: `testing_coordinator.cpp` exec loop; `cmd_test.cpp:296` ("each EXE loads ~100MB of runtime DLLs"). Windows process creation ≈21 ms × up to 1339, plus DLL load per process.
Impact: **Medium–High** (prior doc: ~150 s spawn overhead full-suite). Confidence: **High**.

**F-012 — LLVM backend re-initialized on every object compilation.**
Evidence: `object_compiler.cpp:269–274` — `compile_ir_string_to_object` constructs a fresh `LLVMBackend` and calls `initialize()` each call, i.e., per test file (and per dispatcher).
Impact: **Medium**. Confidence: **High**.

**F-013 — Redundant per-file source scans (imports detected by re-reading files 2–3×).**
Evidence: `testing_compile.cpp:909–962` (read first 30 lines for registry synth), again in `get_runtime_objects`, and `1175–1263` (full-file re-read for `use <pkg>::` / build.tml). Repeated I/O per suite.
Impact: **Low–Medium**. Confidence: **High**.

**F-014 — Result cache is fragile: content-hash + "all-passed" + whole-DLL compiler hash → rarely reused.**
Evidence: `testing_test_cache.cpp` — `is_cached` requires `all_passed` and exact `source_hashes` match; `compute_compiler_hash` fingerprints the 71 MB DLL (mtime:size), so any compiler rebuild → `invalidate_all_exes()` (coordinator line ~1015). `tests.json` currently 618 bytes confirms near-zero population.
Impact: **High** (the cache that should make reruns cheap is effectively cold). Confidence: **High**.
