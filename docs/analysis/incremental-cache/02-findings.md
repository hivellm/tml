# 02 — Findings (F-019 .. F-036)

Numbering continues `docs/analysis/tooling-performance/` (F-001..F-018). "41b/41c" = existing tasks
`.rulebook/tasks/phase41b_shared-stdlib-object/`, `phase41c_test-cache-parallelism/`.

---

**F-019 — Red-green persists ONLY CodegenUnit results; every earlier stage recomputes each invocation, and `check` uses no query cache at all.**
Evidence: green reuse is compile-time gated to `CodegenUnitResult` (`compiler/include/query/query_context.hpp:219-227`); the
disk cache stores IR/libs only (`save_ir`/`save_link_libs`, `query_incr.hpp:110-121`); intermediate stages get a
*key-derived constant* output fingerprint, not a result (`query_context.cpp:283-295`). `tml check` doesn't construct a
QueryContext at all — direct lex→parse→typecheck (`compiler/src/cli/commands/cmd_debug.cpp:244-308`). So parse/typecheck/
HIR/THIR/MIR run from scratch every invocation even when 100% GREEN; a GREEN build only skips IR *generation*.
Contrast: rustc/salsa persists every tracked query's result and replays diagnostics.
Impact: **High** (structural ceiling on all warm paths). Confidence: **High**. Related: F-016/F-017 (daemon masks this for unchanged files only).
**EVALUATED — REJECTED (phase42c, v0.3.75; ADR-010).** Routing `tml check` through `QueryContext` was investigated and
rejected as document-blocked: (1) no cross-process typecheck persistence — `force<>` only reuses `CodegenUnitResult`, so a
one-shot `check` recomputes parse+typecheck anyway (zero memo win); (2) no diagnostics replay + lossy result contract —
`TypecheckResult` flattens errors to strings, drops cascading errors, and has NO warnings field (warnings are a transient
`TML_LOG_WARN` side effect, `query_core.cpp:474-479`), so any cache-skip would silently drop diagnostics (unsafe for a
diagnostics-only command); (3) redundant — the phase40a/42b daemon result cache already memoizes `check` by argv-mtime +
`universe_epoch` and replays the LITERAL captured stdout/stderr/exit code (`cmd_daemon.cpp`), safely covering the realistic
warm workflow that in-process memo (one-shot) cannot. Unlocking prerequisite if revisited: a persisted, diagnostics-carrying
typecheck query — larger than a CLI re-route and still redundant with the daemon cache. See `docs/adr/ADR-010-check-query-routing.md`.

**F-020 — incr.bin is loaded, merged, and fully rewritten once PER TEST FILE (O(N²) I/O, ~27 GB churn per full run).**
Evidence: per-file worker loads the cache (`testing_compile.cpp:645-648`) and saves it (`:714-718` under `g_incr_cache_mutex`);
`save_incremental_cache` re-loads the whole existing file, merges, rewrites (`query_context.cpp:343-370`;
`IncrCacheWriter::merge_from` `query_incr.cpp:409-416`). Same pattern in unified mode (`testing_compile_parallel.cpp:247-249,273-277`).
At today's 10.25 MB × 1,339 files ≈ 13 GB read + 13 GB write per full suite run, serialized on one mutex.
41c's F-010 item covers the *mutex*; the deeper defect is whole-file read-merge-rewrite semantics per context.
Impact: **Very High**. Confidence: **High**. Related: extends F-010 (41c).

**F-021 — 10,000-entry cliff: at 9,282 entries the cache is one wave away from silent total invalidation.**
Evidence: `PrevSessionCache::load` rejects any file with `entry_count > 10000` (`query_incr.cpp:326-330`); merge-on-save
grows the entry set monotonically (`query_incr.cpp:409-416`) with no aging/pruning; measured header: 9,282 entries.
When crossed: load fails → treated as "no previous cache" → next save writes only the current context's entries →
cache effectively resets at random. Impact: **High**. Confidence: **High** (mechanism read; crossing not yet observed).

**F-022 — incr IR store never garbage-collected: 9,720 files / 2.2 GB of mostly-orphaned full-stdlib IR dumps; GREEN hits pointlessly rewrite their own IR.**
Evidence: `save_ir` writes `ir/<kind>_<keyhash>.ll` (`query_incr.cpp:492-509`); nothing anywhere deletes from `ir/`
(`cmd_cache.cpp` touches only `cache/run`). Each `.ll` embeds the whole stdlib (~230 KB avg, F-006). Key churn (F-023)
orphans files. Bonus waste: the GREEN path re-saves the identical IR + libs it just loaded (`query_context.cpp:408-413`).
Impact: **High** (disk; write amplification). Confidence: **High**.

**F-023 — CodegenUnitKey embeds `test_entry_index` (position in suite): adding/removing/reordering ONE test file turns every subsequent file RED.**
Evidence: key fields incl. `test_entry_index`, equality is defaulted over all fields (`query_key.hpp:114-123`);
tests set `qopts.test_entry_index = i` = loop index within the suite (`testing_compile.cpp:625`; unified: `:232` global index).
Also `has_cached_library_state` toggling (41b work) flips every key. Serialization confirms both are part of identity
(`query_incr.cpp:164-176`). Result: green reuse and the IR store are invalidated by *suite membership*, not by content —
and phase41a aggregation makes membership more volatile. Note the entry index only affects the generated `tml_test_N`
wrapper symbol, not the module body. Impact: **High**. Confidence: **High**.

**F-024 — Compiler-build hash = DLL mtime: every rebuild/relink wipes incr.bin even when the DLL content is unchanged in behavior.**
Evidence: `compiler_build_hash()` = CRC32C of the module's `last_write_time` (`query_incr.cpp:40-79`); mismatch → whole
cache rejected (`query_incr.cpp:318-323`). Same policy family as F-014 (test cache, 41c owns that side) — but 41c's fix
does not touch this call site. Contrast: ccache/sccache hash content; rustc invalidates on crate metadata hash.
Impact: **High** during active compiler development (the dominant workflow here). Confidence: **High**.

**F-025 — One options_hash per incr.bin: alternating configs (coverage↔normal, O0↔O2, defines) mutually evict each other's entries.**
Evidence: load resets prev-session on hash mismatch (`query_context.cpp:325-329`); save merges only same-hash entries and
then overwrites the single file with the new hash (`query_context.cpp:356-362`, `query_incr.cpp:418-443`). Coverage runs
share the same `build/debug` cache dir as normal runs. Contrast: rustc keeps separate incremental session dirs per config.
Impact: **Medium**. Confidence: **High**.

**F-026 — Per-file QueryContexts re-fingerprint the world: every loaded stdlib source re-read+hashed and all 367 .meta files re-hashed, once per compiled file.**
Evidence: each context computes `lib_env_fp` by hashing every `.meta` file (`query_context.cpp:315` →
`compute_library_env_fingerprint`, `query_incr.cpp:639-662`); green verification re-reads and CRCs every ReadSource dep —
which since the W5 fix includes all ~300+ transitively loaded library sources (`query_core.cpp:436-452`;
`fingerprint_source` reads whole file, `query_fingerprint.cpp:59-76`). No session-level (mtime-gated) fingerprint memo is
shared across contexts, so a 1,339-file run re-hashes the same unchanged stdlib ~1,339×. Impact: **Medium-High**. Confidence: **High**.

**F-027 — `.ast.bin` sidecar is trusted UNCONDITIONALLY: stale cached AST silently overrides an edited source file.**
Evidence: `QueryContext::parse_module` uses `<source>.ast.bin` whenever it exists — no source-hash, no mtime check, and it
bypasses `force()` so no ReadSource dep/fingerprint is recorded either (`query_context.cpp:134-173`). It is also re-read and
re-deserialized on every call (bypasses the in-memory cache). Under-invalidation: compiles old code after an edit until the
sidecar is manually deleted. Candidate contributor to "flaky module-path corruption" (phase27c).
Impact: **High (correctness)**. Confidence: **High** (code path); Medium on how often `.ast.bin` files are actually present
(produced by the TML frontend flow).
**RESOLVED (phase42b, v0.3.73).** The sidecar fast-path moved into the `provide_parse_module` query provider
(`query_core.cpp`), so it now (a) records the `ReadSource` dependency + fingerprint (red-green invalidates the parse when
the source changes), (b) flows through the query cache instead of re-deserializing on every call, and (c) is used ONLY when
the sidecar is at least as new as the source (mtime gate) — an edited source is never overridden. A corrupt/incompatible
sidecar now falls through to a real parse instead of failing a valid compile. Content-hash validation is not possible
(the `.ast.bin` format is written by the frozen TML frontend, `lib/std/src/serial/ast.tml`, and embeds no source hash), so
mtime is the available freshness signal. Repro: `compiler/tests/cli/cache_staleness.sh` (F-027) — pre-fix a garbage sidecar
broke a valid `build` with "ast.bin deserialization failed"; post-fix the newer source is parsed and that error never appears.

**F-028 — Daemon result cache keys on argv `.tml` mtimes only: edits to imported modules are invisible → stale check/build/emit results.**
Evidence: snapshot = `(path, mtime)` for each argv arg ending in `.tml` (`cmd_daemon.cpp:329-347`); hit requires only that
snapshot to be unchanged (`cmd_daemon.cpp:475-489`). Transitive imports (project-local modules, lib sources) are not in
argv, so `edit lib/foo.tml; tml check main.tml` (daemon warm) returns the previous result. Adjacent to the documented
artifact-regeneration caveat (05-mcp-warm-state.md) but distinct: this one returns wrong *diagnostics*.
Fix direction: reuse the query layer's transitive source set (cf. `collect_transitive_source_files`, `query_context.cpp:507-558`)
or a lib-tree max-mtime probe. Impact: **Medium-High (correctness)**. Confidence: **High**.
**RESOLVED (phase42b, v0.3.73).** The daemon result cache now stores a `universe_epoch` per entry = max mtime over the
transitive source universe (the lib source tree + each argv file's sibling directory) and a warm hit is served only when
BOTH the argv mtimes AND the universe epoch are unchanged (`cmd_daemon.cpp`). Editing an imported sibling module now
invalidates the warm result. The lib-tree sweep (~2.3k files) is throttled to ≤1×/750ms to protect the phase40a warm-latency
target; the argv-sibling scan is un-throttled so project-local imports are detected immediately. Repro:
`compiler/tests/cli/cache_staleness.sh` (F-028) — pre-fix `edit sibling; TML_DAEMON=1 check app.tml` returned the previous
exit code; post-fix it re-type-checks and reports the new error.

**F-029 — Daemon never revalidates module metadata: `preload_all_meta_caches` is `call_once` per process, so a long-lived daemon serves types from before your lib edit.**
Evidence: `std::call_once` + static result (`module_binary_read.cpp:1376-1379,1426-1428`); daemon handles requests by calling
`tml_main` in-process (`cmd_daemon.cpp:504-513`) in the same process for up to 30 min (`cmd_daemon.cpp:53`); only a
*compiler DLL* mtime change restarts it (`cmd_daemon.cpp:639-651`) — lib source edits do not. Module resolution consults the
already-populated `GlobalModuleCache` first (`module_binary_read.cpp:1304`). A cache-miss request (main file touched) still
type-checks against the stale preloaded interfaces of the edited library module.
Impact: **Medium-High (correctness)** — exactly the class of staleness that makes users reach for manual invalidation.
Confidence: **Medium** (module-load precedence traced through `load_native_module`/GlobalModuleCache, not runtime-verified;
verification recipe: warm daemon → edit a lib signature → `TML_DAEMON=1 tml check` an importing file → expect stale pass/fail).
**RESOLVED defensively (phase42b, v0.3.73) — see runtime-confirmation note.** Runtime confirmation showed F-029 does NOT
affect PROJECT-LOCAL modules: those are re-loaded per compile with a source-hash check (`env_module_loading.cpp`), so a
warm-daemon recompile after a sibling edit correctly re-reads them (verified — `cache_staleness.sh` F-029: a result-cache
MISS re-type-checks against the edited sibling). The hole is therefore STDLIB-only: `preload_all_meta_caches` was
`std::call_once`/process and `load_existing_meta_files` short-circuits once `GlobalModuleCache` is populated
(`module_binary_read.cpp:1109`), so a lib edit under a live daemon would be missed until restart. A stale-RESULT divergence
requires editing a `lib/` SOURCE signature under a warm daemon — out of scope here (lib/ is a shared tree; editing it is
hazardous with concurrent sessions), so it was NOT independently runtime-reproduced. Fix (applied because the mechanism is
confirmed and the F-028 lib-tree probe now exposes it): `preload_all_meta_caches` converted from `call_once` to a mutex +
resettable guard, plus `types::reset_meta_caches()` (drops `GlobalModuleCache`, resets the guard); the daemon calls it on a
per-request lib-epoch change (shared with the F-028 probe), so the recompile re-preloads fresh interfaces.

**F-030 — `tml cache invalidate` (and `mcp__tml__cache_invalidate`) is a stub that neither invalidates the caches that matter nor frees space.**
Evidence: MCP tool shells out to `tml cache invalidate` (`mcp_tools_project.cpp:34-93`). The command: (a) substring-matches
file *stems* against cache filenames (`cmd_cache.cpp:412,462`) — but run/obj/incr artifacts are *hash-named*, so nothing
matches; (b) the "MIR cache" block iterates and sets `found_any` without deleting anything (`cmd_cache.cpp:424-451`);
(c) the tests.json block detects the entry and explicitly does not rewrite it (`cmd_cache.cpp:474-506`);
(d) it never touches `cache/incr/`, `cache/tests/obj_cache/`, or `.ast.bin` sidecars. `cache info`/`clean` only see
`cache/run` (`cmd_cache.cpp:50-53`). The tool's existence is a symptom of F-027/F-028/F-029; its implementation violates
the no-stubs rule. Impact: **Medium**. Confidence: **High**.
**RESOLVED (phase42b, v0.3.73).** `run_cache_invalidate` (`cmd_cache.cpp`) now maps a source file to its artifacts across
every deterministically-mappable layer and deletes them, reporting exactly what was removed (to stdout, so it is visible
regardless of log level): (a) the `<source>.tml.ast.bin` sidecar; (b) its tests.json suite entries + suite EXEs, via a new
`TestResultCache::invalidate_source()` that reverse-looks-up the phase-8.5 `source_paths` and drops matching suites WITHOUT
touching `compiler_hash` (phase41c's global lever) then re-saves; (c) its incr.bin entries + `ir/*.ll|.libs|.search_paths`
sidecars, by loading `PrevSessionCache`, filtering entries by `QueryKey.file_path`, deleting the IR files, and rewriting
`incr.bin` with the survivors. The content-addressed `run/` and `obj_cache/` layers have no source→artifact backlink and are
self-healing, so they are reported as reclaimable only via `cache clean --all` (documented, not silently skipped). `cache
info`/`cache clean` now cover the whole cache tree (run/incr/tests/meta + loose files), not just `cache/run`. Verified:
`cache invalidate <hash test>` removed 1 exe + 6 incr entries + 2 ir files; `cache_staleness.sh` (F-030) covers the sidecar
class end-to-end.

**F-031 — No eviction anywhere: the LRU evictor is dead code; ~4.3 GB and growing monotonically.**
Evidence: `enforce_cache_limit()` implemented (`cmd_cache.cpp:285-362`) with **zero call sites** (grep: only header/comment).
`invalidate_all_exes()` clears JSON fields but never deletes the 1,516 EXE files (`testing_test_cache.hpp:115-120`).
Measured: incr/ir 2.2 GB + obj_cache 609 MB + tests 1.4 GB + run 51 MB. Impact: **Medium** (disk, cache-scan latency).
Confidence: **High**.
**RESOLVED (phase42c, v0.3.75).** New `cli::enforce_cache_caps()` (`cmd_cache.cpp`) LRU-evicts three
content-addressed layers to configurable caps at every test/build/run teardown (`incr_test_run_end`,
`build.cpp`, `builder_run.cpp` — separate from phase42a's incr/ir GC): `tests/obj_cache` (256 MB),
suite `*.exe` under `tests/` (512 MB), `cache/run` (128 MB); caps overridable via
`TML_CACHE_{OBJ,TESTS,RUN}_CAP_MB`. The exe evictor evicts UNREFERENCED orphans first and files
referenced by `tests.json` LAST (reusable EXEs survive longest), removing `.lib/.pdb/.exp/.ilk`
siblings with each `.exe`. `TestResultCache::invalidate_all_exes()` now DELETES the stale EXE files
(and siblings) it clears, not just the JSON fields (verified: a post-rebuild run deleted 14 referenced
stale EXEs; a forced 100 MB cap evicted 890 orphan EXEs, 503→99.8 MB, referenced suite survived).
Measured steady state under default caps across consecutive clean runs: obj_cache 39 MB, run 52 MB,
suite EXEs ≤ 94 MB — all bounded. `cache clean --all` remains the manual full reclaim.

**F-032 — Weak / timestamp-tainted cache keys in run + obj caches.**
Evidence: (a) run exe hash mixes object-file **mtimes** (`builder_helpers.cpp:138-147`) — any runtime-obj relink misses even
with identical content; (b) `generate_content_hash` is `std::hash<std::string>` (`builder_helpers.cpp:124-131`) — 64-bit,
implementation-defined (stable per binary, not per toolchain); (c) obj_cache/incr fingerprints are 2×CRC32C halves
(`query_fingerprint.cpp:28-46`) — this family already produced real collisions at 16-hex truncation (comment
`testing_compile.cpp:737-739`). Contrast: ccache→BLAKE3, rustc→SipHash-128. Impact: **Low-Medium**. Confidence: **High**.
**RESOLVED (a)+(b) (phase42c, v0.3.75).** `generate_exe_hash` now folds each object's **content** hash (streamed
`crc32c_file`) instead of its mtime, so a byte-identical relink is a cache HIT while any real content change still
invalidates (`builder_helpers.cpp`). `generate_content_hash` was migrated off `std::hash<std::string>` to the shared 128-bit
CRC32C content fingerprint (`query::fingerprint_string(...).to_hex()`), making keys deterministic across toolchains/builds.
(c) The 2×CRC32C incr/obj fingerprint family is left as-is — it is the same `Fingerprint` helper now reused here, and
widening it is a separate, higher-blast-radius change not needed for the touched sites. `generate_cache_key` still mixes the
thread id (it is a temp-file disambiguator, not a content key — intentionally left).

**F-033 — obj_cache can only ever skip the backend: its key is a hash of the generated IR, so the expensive frontend+stdlib emission must run before the cache can even be consulted.**
Evidence: key computed from `codegen_result.llvm_ir` after `qctx.codegen_unit(...)` returns
(`testing_compile.cpp:733-740`, `testing_compile_parallel.cpp:287-293`). With F-006 (stdlib re-emitted per file, 41b) the
pre-key work dominates. This is by construction — the fix is not in this layer (it is 41b + F-023/F-020 green reuse), but the
layer should not be mistaken for a compile cache. Impact: **Medium** (framing/architecture). Confidence: **High**.

**F-034 — Build-path object caching is split-brain: CGU path is content-addressed with hits, monolithic path recompiles always.**
Evidence: CGU objects keyed by content fp12 with cache-hit accounting (`build.cpp:1385-1432`); monolithic fallback writes
`cache/<module>.obj` unconditionally via `compile_ir_string_to_object` (`build.cpp:1440-1449`); CGU requires MIR success +
≥2 functions + `!no_cache` (`build.cpp:1345-1348`). CGU objs also accumulate in `cache/` with no GC. Impact: **Low**.
Confidence: **High**.

**F-035 — C++ toolchain rebuild is timestamp-based with no compiler cache, and every build kills all warm state, multiplying the mtime cascades.**
Evidence: ninja + zig cc, no ccache/sccache wiring (`scripts/build.bat:240-334`); CMake reconfigure every run
(`build.bat:305-339`); `taskkill` of tml.exe/tml_daemon.exe/tml_mcp.exe every run (`build.bat:191-197`). Combined with
F-024 + F-014: one relink ⇒ incr.bin wiped + all test EXEs invalidated + daemon dead. PCH still MSVC-only (F-003, known).
Impact: **Medium**. Confidence: **High**.

**F-036 — Eager whole-library meta preload per process: 367 modules deserialized + 367 source files re-hashed regardless of imports.**
Evidence: `preload_all_meta_caches()` called from check (`cmd_debug.cpp:279`), build (`build.cpp:50,1191`), run
(`builder_run.cpp:138`), tests (`testing_compile.cpp:91`), coverage (`cmd_coverage.cpp:357`), debug (`cmd_debug.cpp:279`);
`load_existing_meta_files` walks every `.meta`, reads+hashes each module's *sources* for staleness, then deserializes all of
them (`module_binary_read.cpp:1073-1157`). A validated *lazy* per-import loader already exists
(`load_module_from_cache`, `module_binary_read.cpp:863-906`). Directory modules hash **every .tml in the directory**
(`module_binary.cpp:34-56`). This is the tooling F-015 (03-check-performance.md) root, quantified: ~734 file reads + 367
deserializations before the first user line is type-checked. Impact: **Medium-High** (constant tax on every cold command).
Confidence: **High**.
**PARTIALLY RESOLVED (phase42c, v0.3.75) — mtime fast path; full lazy REJECTED as non-parity.**
The "replace eager with lazy" plan was investigated in depth and **rejected**: it cannot preserve byte-identical
diagnostics. The type checker, borrow checker, and codegen resolve unqualified references (primitive impl methods with no
`use` like `s.len()`; library behaviors in generic bounds like `[T: Hash]`; behavior-impls; type aliases) via **last-resort
`GlobalModuleCache::instance().get_all()` scans over the WHOLE library** — `env_lookups.cpp:156-162` (`lookup_behavior`,
comment: "populated by meta preload and contains all library modules"), `:190-196`, `:314-321` (`lookup_func` primitive
impls), plus `expr_call_method.cpp:312/829/1235/1375/1397`, `checker_core.cpp:328`, and ~9 codegen sites. TML today lets a
file reference anything defined anywhere in the ~367-module library without importing it, resolved through these full-cache
scans. A lazy subset (only `use`d modules + transitive deps) would leave the cache partially warm → those scans miss modules
→ diagnostics diverge. The parity set is effectively the whole library, not a stable <30-module prelude, so the "<30 modules
for a prelude-only file" target is **unachievable without a large semantic change** (requiring explicit `use` for currently
-implicit resolution) — which by definition is not byte-identical. Per the task's own precedence rule (byte-identical is the
mandatory correctness gate), eager preload stays the default.
Instead, the redundant-work half of the finding is fixed: a **directory-level mtime "nothing changed" fast path**
(`compute_lib_source_signature` + a `.preload_stamp`, `module_binary_read.cpp`) stats (never reads) the lib source tree and,
when the newest-mtime+file-count signature matches the last successful preload, SKIPS the per-module source read+CRC
re-validation — eliding the ~734-file staleness tax while loading the identical meta set (byte-identical: same 366 modules end
up in `GlobalModuleCache`). Measured on a prelude-only `check`: full-validation preload ~419 ms vs fast-path ~285 ms (~134 ms
/ ~32% off the preload), 366 modules loaded both ways, 0 diagnostic divergences over a 25-file corpus (fast vs full).
Override with `TML_NO_META_FASTPATH=1`. The stamp is written only after a full validation with nothing stale (insurance
against a failed regen), and `load_native_module` + `reset_meta_caches` still re-validate on actual use / lib-epoch change.

---

## phase42a resolution — F-019..F-026 (v0.3.74)

The incr.bin red-green cache was made a real cross-run cache. Measured on
`core/hash` (14 units) with the rebuilt compiler:
- **Before (per findings): GREEN ≈ 0** — F-023 index shifts + F-024 mtime made
  every unit RED after any rebuild or membership change.
- **After: GREEN=14 RED=0 (100%)** on an unchanged warm rerun; **GREEN=13 RED=1**
  after touching ONE file (only the touched unit RED, 13 siblings reused).
- Per-suite incr I/O: **loads=4, saves=1** (was a full incr.bin load+rewrite PER
  FILE) — O(N²) read churn collapsed to ~O(1) per run via a shared load.
- ir/ store bounded and GC'd to referenced keys at run teardown.
- Determinism gate 28/28 at floor; clean-suite parity zero divergence
  (core/hash 14/14, compiler/borrow 12/12, std/json 23/23).

**F-019 — RESOLVED.** `IncrTelemetry` (`query_incr.hpp/.cpp`) counts GREEN/RED
plus incr.bin load/save count+time and ir/ GC bytes; `incr_telemetry_report()`
logs a per-run summary (`[incr] incr: GREEN=.. RED=.. (..% green) loads=.. saves=.. ir_gc=..`).
Reset at run start / reported at run end (`incr_test_run_begin/end`,
`testing_coordinator.cpp` run guard). (F-019's broader claim — that only
CodegenUnit results persist and earlier stages recompute — is a structural item
left as-is; this task instrumented and revived the CodegenUnit layer.)

**F-020 — RESOLVED.** `get_shared_prev_session()` returns one shared, read-only
`PrevSessionCache` per run, memoized by (path, mtime); every per-file
QueryContext points at the one parsed copy instead of re-reading incr.bin. The
per-file/per-suite whole-file load-merge-rewrite is gone (write side already
batched per-suite in 41c; read side is now once-per-run). ~13 GB read + 13 GB
write per full suite run → ~O(total).

**F-021 — RESOLVED.** The hard `entry_count > 10000` load rejection (a silent
total-reset cliff) is replaced by graceful session-recency aging at SAVE:
`IncrCacheWriter::write(..., max_entries)` caps to `MAX_INCR_ENTRIES` (100k,
current-session entries first). Load only guards against corruption (5M limit).

**F-022 — RESOLVED.** `gc_ir_store()` deletes `ir/<stem>.ll|.libs|.search_paths`
not referenced by any surviving key; run at teardown (`incr_run_teardown`) from
the final on-disk partitions (safe after parallel per-suite writes). The GREEN
path no longer re-saves identical IR/libs it just loaded
(`try_mark_green_codegen`). The format bump (v2→v3) orphans all old v2 IR, which
the first v3 teardown reclaims. (The finding's 2.2 GB was a point-in-time
measurement; the store is now bounded to the run's working set + GC'd.)

**F-023 — RESOLVED.** `test_entry_index` + `has_cached_library_state` removed
from `CodegenUnitKey` identity. The index-dependent IR bits (the `s{id}_`
internal-symbol prefix on the AST path; the `tml_test_N` entry wrapper on both
paths) now derive from a FILE-STABLE id (`stable_test_symbol_id(file_path)`) in
`query_core.cpp`; the NDJSON dispatcher declares/calls that same stable symbol
(new `DispatcherTestInfo::symbol_id`, keeping `index` for --list/--test-index/
event mapping), and the pre-link entry check uses it. `has_cached_library_state`
moved into the session `options_hash` partition (it selects different IR, so it
partitions rather than keys). Result: a suite membership change no longer REDs
sibling files (GREEN=13/14 on a one-file touch, verified).

**F-024 — RESOLVED.** `compiler_build_hash()` is the CRC32C of the compiler
DLL/EXE *content*, streamed once and memoized in a `<binary>.bhash` sidecar
keyed by (mtime,size). A no-op relink producing byte-identical output keeps the
cache GREEN; behavior-changing relinks still invalidate. (Analogous to phase41c's
content-aware fix for the tests.json result cache.)

**F-025 — RESOLVED.** Cache files are config-partitioned: `incr.<options_hash>.bin`
(`incr_cache_file_for`), with `options_hash` now including the cached-library-state
bit. `prune_incr_partitions` keeps the newest `MAX_INCR_PARTITIONS` (4) and drops
the legacy unpartitioned `incr.bin`. Alternating configs (coverage↔normal, O0↔O2,
defines) no longer mutually evict.

**F-026 — RESOLVED.** `fingerprint_source()` results are memoized process-globally,
gated on (mtime,size) (`query_fingerprint.cpp`), so unchanged stdlib/.meta sources
are read+hashed once per run instead of once per QueryContext (~1,339× → 1×). Both
`compute_library_env_fingerprint` and GREEN ReadSource re-verification benefit.

---

## Practical-effectiveness note (why GREEN rarely pays today)

For test runs, the layers stack as: suite result cache (skip everything) → reusable EXE (skip compile) → incr GREEN
(skip IR gen) → obj_cache (skip backend). The first two already cover "nothing changed" and "source unchanged but failed".
GREEN would matter exactly when a *different* file in the suite changed — but F-023 makes those keys RED via index shifts,
F-024 makes them RED after every compiler rebuild, and F-020 charges every file the full cache-rewrite tax regardless.
Net today: the incr layer mostly adds I/O. Fixing F-020/F-023/F-024 (or, failing that, disabling incr for tests once 41b's
cached-library-state path lands — it already sets `incremental=false`, `testing_compile.cpp:630-635`) are both defensible;
add GREEN-hit telemetry before choosing.
