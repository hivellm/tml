# 01 — Baseline Measurements

Measured 2026-07-17, warm OS cache.

| Metric | Value | Source |
|---|---|---|
| `tml --version` (process + DLL load, warm) | ~46–51 ms | measured |
| `tml check <trivial file>` (cold, no daemon) | ~461–466 ms | measured |
| `tml_compiler.dll` | **71.3 MB** (PDB 175 MB) | `build/debug/bin/plugins/` |
| `tml_codegen_x86.dll` | **51.9 MB** (PDB 47 MB) | same |
| `tml.exe` (thin launcher) | 1.34 MB | same |
| Cached test EXEs | **1339 files, 837 MB** | `build/debug/cache/tests/` |
| Per-test EXE size (each embeds stdlib) | ~345 KB | same |
| `obj_cache/` | 2755 objects, 273 MB | same |
| `tests.json` result cache | **618 bytes (≈empty)** | `build/debug/cache/tests.json` |
| Default build type | `debug` (`-O0`) | `scripts/build.bat:18` |

Prior-doc baselines (still directionally valid, `docs/analysis/benchmark/08-compilation.md`):

- Full test suite ~10 min (coverage).
- TML compile 27× Rust.
- Daemon cache-hit 22 ms vs cache-miss 7.7 s.
- Cold DLL load 2–3 s (the ~50 ms above is warm OS page cache — the cold penalty is real after reboot/rebuild).

**Key structural fact:** `tests.json` being ~empty means **the result cache is almost never populated**, so in practice most `tml test` invocations recompile from the obj-cache rather than skipping. Any compiler rebuild triggers `invalidate_all_exes()` (compiler hash = mtime:size of the 71 MB DLL), wiping the 837 MB EXE cache.

## phase40a baseline + warm-path results (2026-07-17)

Protocol: `subprocess` wall-clock timing from Python (`time.perf_counter` around
`tml.exe <cmd> .sandbox/phase40a_trivial.tml`, cwd = repo root), unchanged trivial
file, warm OS cache. Median over the listed n.

**Baseline (before fix, no daemon):**

| Command | n | Median | Range |
|---|---|---|---|
| `tml check` (cold subprocess) | 7 | **452.8 ms** | 422.7–580.4 ms |
| `tml build --emit-ir` (cold subprocess) | 5 | **842.9 ms** | 820.9–888.0 ms |

**Warm daemon path (after fix, `TML_DAEMON=1` subprocess — the exact path MCP now takes):**

| Command | n | Median | Range | vs baseline |
|---|---|---|---|---|
| `tml check` (daemon cache-hit) | 10 | **6.6 ms** | 6.2–25.3 ms | **69×** |
| `tml build --emit-ir` (daemon cache-hit) | 6 | **7.8 ms** | 6.7 ms–first-miss 585 ms | **108×** |

**Full MCP stack** (JSON-RPC request → `tml_mcp.exe` → subprocess → daemon → response,
measured by driving `tml_mcp.exe` over stdio): warm `check` = **26–31 ms** per call
(cold fallback ~405–465 ms only while the daemon (re)starts).

**GATE (task phase40a, item 1.6): `check` on unchanged file < 50 ms — PASS**
(6.6 ms CLI-level, ~28 ms through the whole MCP stack; baseline was ~453 ms).

First request after a daemon (re)start is a cache-miss and costs the normal
compile time (~405–620 ms observed) — it fills the daemon's mtime result cache;
every subsequent identical request on unchanged inputs is a cache-hit.

## phase40b debug vs release (2026-07-17)

Protocol: same as phase40a — Python `time.perf_counter` around a `subprocess`
invocation of the binary, cwd = repo root, **cold path** (no `TML_DAEMON` in the
child env, daemon not running), warm OS cache, 1 discarded warm-up run, then
n=7 interleaved runs per binary (D,R,D,R,…). Binaries built from the **same
working tree** (debug rebuilt 01:13, release 01:37, both after the last source
edit) — release is `scripts\build.bat release`, i.e. `-O3 -DNDEBUG` on all 40
CMake targets (verified in the generated `build.ninja`: 0 objects without `-O3`).

**`tml check` (cold subprocess):**

| Workload | debug median (range) | release median (range) | speedup |
|---|---|---|---|
| trivial file (3 lines) | 396.3 ms (390.0–477.8) | **213.0 ms** (188.8–237.2) | **1.86×** |
| `lib/core/src/task.tml` (714 lines, real module) | 5502.1 ms (5376.7–5719.5) | **2702.6 ms** (2633.4–2893.8) | **2.04×** |

`check` diagnostics are **byte-identical** between the two binaries for both
workloads (`diff` exit 0), including the exit codes.

**Suite compile+run** (49 test files: `core/hash` 14 + `compiler/borrow` 12 +
`std/json` 23; every run is a full fresh compile of all 49 test EXEs because the
shared test cache fingerprints the running binary's `tml_compiler.dll` — each
debug↔release alternation invalidates all cached EXEs; both binaries link the
same cached `tml_test_runtime.lib`):

| Run | debug wall | release wall |
|---|---|---|
| 1 | 73.37 s | 32.10 s |
| 2 | 66.60 s | 31.43 s |
| 3 | 65.62 s | 31.52 s |
| **median** | **66.60 s** | **31.52 s** → **2.11×** |

All 6 runs: **49/49 passed**, identical results debug vs release.

Sanity divergence check: the pre-existing K001 failures in the working tree
(4 × `core/str` tests, `cc_int_main`/`cc_with_stdio`) reproduce **byte-identically**
under both binaries — same error, same generated-IR line:column (e.g.
`ir:662:19`, `ir:4957:21`) — i.e. deterministic codegen, no optimization-induced
miscompile observed.

Note: the trivial-check floor includes ~50 ms of process + DLL load that does not
speed up with `-O3`; the pure compile fraction improves more than the end-to-end
1.86× suggests (the real-module workload shows the honest 2× compiler-work delta).

**GATE (task phase40b, item 1.6): release-vs-debug delta recorded (1.86–2.11×),
targeted suites green on the release binary — PASS.**

## phase41a suite-mode aggregation default (2026-07-17)

Protocol: same Python `perf_counter` wrapper, debug binary, cwd = repo root,
`--no-cache --no-fail-fast`, JSON reporter snapshots compared mechanically
(multiset of `(group, test, status)`; a compile-failed FILE in per-file mode
equals a failed test record in aggregated mode). Full-run discovery =
**2066 test files / 131 groups** (the "1339" in earlier docs was a stale cache
count).

**Link-step counts (warmth-independent — the F-005 gate):**

| Scope | per-file EXEs/links | aggregated (25/EXE) | reduction |
|---|---|---|---|
| full corpus (2066 files) | 2066 | **176** initial suites (measured pre-fallback) | **11.7×** |
| lib/core segment (801 files, completed run) | 801 | **81** (incl. 1 link-fallback → 6 per-file splits) | **9.9×** |
| 49-file set (hash+borrow+json) | 49 | **3** | 16.3× |
| core/str (32 files) | 32 | **2** | 16× |
| core/hash (14 files) | 14 | **1** | 14× |
| core/types (13 files) | 13 | **1** | 13× |

**Wall-clock (same binary, before = `--no-suite`, after = default):**

| Workload | per-file | aggregated | note |
|---|---|---|---|
| 49-set, warm obj/incr cache (n=2–3, median) | 11.0 s | **8.6 s** | 1.28× faster |
| 49-set, cold (first run after rebuild) | 67.3 s | 92.0 s | aggregated SLOWER cold: per-file codegen inside one chunk is serial; the 23-file std/json chunk is the critical path |
| core/str (5 pre-existing K001s) | 35.0 s | 47.3 s cold / **7.6 s** warm | identical 27 pass / 5 fail both modes |
| full corpus, cold compile phase | (old-binary attempt crashed at 52%, 1383 s — pre-existing 0xC0000409 phase27c flake) | ~62 min at 99% (run externally killed at ~60 min) | cold wall is dominated by per-file stdlib IR re-emission (F-006), which aggregation does not change |

**Result parity (the correctness gate):** IDENTICAL per-test multisets in every
completed comparison — 49-set (49 pass), core/str (27/5, the 5 pre-existing
K001 `mem::replace` compile errors appear identically), core/types (8/5,
including the pre-existing `maybe_unzip` link failure), plus
`compiler/tests/cli/test_aggregation.sh` 16/16 (crash/timeout offender
attribution + never-started-sibling re-run + compile-failure isolation +
coverage-stays-per-file).

Notes:
- Aggregation attacks LINK count and process count (F-005/F-011), not per-file
  IR emission (F-006) — cold-compile wall-clock is roughly unchanged; warm
  runs and full-suite runs win from ~1900 fewer LLD links + ~1900 fewer
  process spawns (each test EXE loads ~100 MB of runtime DLLs).
- Suites with heavy-generics files (std/stream, std/zlib) make 25-file chunks
  a long serial critical path when suite-count ≥ worker-count (intra-suite
  threads only activate when suites < workers).
- The obj-cache doubles in effective size: per-file entries (test index 0) and
  aggregated entries (positional index) have different IR fingerprints.

## phase41b shared-stdlib root-cause + F-012 backend reuse (2026-07-17)

Protocol: debug binary, cwd = repo root, `--no-suite` per-file for the parity
table below is NOT used — these are the default aggregated suite runs,
`--no-cache` (cold, cache invalidated by the compiler rebuild), wall = shell
`date` seconds around the process.

**F-006/F-007 shared-stdlib fast-path — reproduced blocker (NOT enabled).**
Re-enabling `build_stdlib_object` on a scratch build engaged the fast path on a
minimal 2-module bootstrap (state captured in **20.2 s**, 1235 KB IR); every test
object *and the shared object itself* then failed with
`use of undefined value '@tml_N4core7runtime3mem7replaceE_R1T1T'`
(un-monomorphized `core::runtime::mem::replace[T]`). Same error as the pre-existing
5× `core/str` K001 failures. An all-loadable-modules bootstrap **timed out >500 s**
just building the shared object. Full enablement deferred (phase27a K001 +
per-suite-scoped bootstrap redesign) — see `04-test-framework-performance.md` F-006.

**F-012 LLVM backend reuse — landed. Per-EXE size + cache baseline (unchanged, since
F-007 stays off):**

| Metric | Value |
|---|---|
| Per single-test EXE (embeds internal-linkage stdlib) | ~345 KB (`compiler_borrow_closure_capture.exe` = 345 600 B) |
| Cached test EXEs | 1518 files / 756 MB |
| Aggregated `core_hash.exe` (14 tests) | 912 KB |

**Clean-suite parity (F-012 build, fast-path OFF, `--no-cache` cold), zero divergence:**

| Suite | Result | Wall (cold) | Note |
|---|---|---|---|
| `core/hash` | **14/14** | 22.4 s | parity |
| `compiler/borrow` | **12/12** | 19 s | parity |
| `core/str` | 27/32 (5 fail) | 54 s | the 5 fails are the pre-existing `mem::replace` K001s, byte-identical |
| `core/alloc` | 43/44 | 63 s | the 1 miss `shared_sync_edge` is a pre-existing flaky concurrency test — passes 1/1 in isolation (2/2) |

F-012 is behaviour-transparent (identical object bytes); it reuses one initialized
`LLVMBackend`/`LLVMContext` per worker thread instead of constructing+`initialize()`
per object compilation (was `object_compiler.cpp:269–274`). The win is the removed
per-object `LLVMContextCreate`/`Dispose` churn across every test file and dispatcher
in a run; it does not change codegen output, so suite results and the determinism
sentinels are unaffected.

**GATE (task phase41b, item 1.7):** shared-stdlib "emit once" NOT achieved (blocked,
proven above); F-012 landed with **zero result divergence** on the representative
clean suites (pre-existing K001/flaky reproduce identically). Per-EXE size unchanged
(~345 KB) because F-007 (decls-only) is gated behind the same phase27a work.
