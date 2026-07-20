# 07 — Query System, Incremental Compilation Reality & C++ Codebase Health

**Findings:** L-120..L-128 · **Method:** code audit + measured numbers (`.ninja_log`, timed CLI runs, LOC counts) · **Builds on:** F-012..F-014 (`../architecture-performance-review/03-compiler-startup-cost.md`), ADR-010, `docs/analysis/incremental-cache/`.

## Summary

The "query-based incremental compilation" promised by ADR-002 exists as a well-built but shallow skeleton: 9 query kinds with in-memory memoization and dependency recording, but only 1 of 9 (CodegenUnit) has any cross-run persistence, intermediate fingerprints are decorative sentinels, and `check` — the highest-frequency operation — bypasses the query system entirely (the project's own ADR-010 documents this and rejects fixing it). "Incremental" in practice = whole-file LLVM-IR-*text* replay + an argv/mtime daemon cache. Meanwhile the C++ codebase is in unusually good *micro* health (zero god-files, modern ownership, clean layer direction) while carrying serious *macro* debt: four generations of codegen/MIR builders (two dead but still compiled), an everything-is-IR-text pipeline, and a per-file QueryContext that shares almost nothing across a run.

---

### L-120 — ADR-002 is undocumented and overclaimed; only 1 of 9 queries is incremental across runs

**Impact:** High · **Confidence:** High · **Layer:** design

Evidence:
- The ADR document does not exist: `docs/adr/` contains only ADR-009 and ADR-010; ADR-001..008 (listed as "Active" in `AGENTS.override.md` T8) have no files anywhere in the repo.
- `compiler/README.md:110` claims "like rustc's `TyCtxt`, 8 memoized stages with red-green incremental" and `:158` "cross-session persistence (red-green)".
- Reality: `compiler/include/query/query_context.hpp:243-254` — previous-session (GREEN) reuse is attempted **only** `if constexpr (std::is_same_v<ResultType, CodegenUnitResult>)`. Parse, typecheck, borrow, HIR, THIR, MIR results are never persisted; `incr.bin` stores only their fingerprints + dep edges (`query_incr.cpp:530-560`), and the IR store persists only CodegenUnit `.ll` text.
- The project already knows: ADR-010 (`docs/adr/ADR-010-check-query-routing.md:22-28`) states "No cross-process typecheck persistence… a one-shot `tml check` starts with an empty in-memory cache… zero memo win."

**Why it conflicts:** a Rust-class compile-speed story depends on rustc-style query reuse (typecheck/metadata reuse, early cutoff). What exists is a Make-style file-level rebuild check. The README/ADR framing hides that the expensive middle of the pipeline is recomputed from scratch for every changed file, every run.

**Recommendation:** write the real ADR-002 documenting what the system *is* (file-granular IR replay cache). Before growing it, decide whether the next investment is (a) persisted typecheck results with structured diagnostics (ADR-010's stated prerequisite) or (b) abandoning query-level granularity and optimizing the file-level path — both defensible; the current in-between is not.

---

### L-121 — Intermediate fingerprints are key-derived sentinels; early cutoff is structurally impossible

**Impact:** Medium-High · **Confidence:** High · **Layer:** implementation

Evidence:
- `compiler/src/query/query_context.cpp:252-264`: for every stage except ReadSource and CodegenUnit, `compute_output_fingerprint` returns a hash of the **serialized query key** + kind — a constant per file path, independent of the output *and* the input. The comment claims "output_fp = input_fp" but the code does neither.
- Consequently `compute_input_fingerprint` (`:202-223`) for e.g. TypecheckModule combines dep output fingerprints that never change. Change detection works *only* because `verify_all_inputs_green` (`:411-492`) ignores non-leaf fingerprints and recursively walks to ReadSource leaves, comparing file-content hashes.
- The 36-line comment at `:444-480` — reasoning about whether lib-env changes are caught, ending with "worst case, we get a false green and the recomputation produces the same result anyway" (a self-contradiction: a false green means *no* recomputation) — shows the authors don't trust the fingerprint design either. (In practice invalidation is saved by `track_source_file(meta_source_path)` at `env_module_loading.cpp:404-406`, which puts stdlib sources into the leaf set even on `.meta` cache hits.)

**Why it conflicts:** red-green's core payoff — recompute one query, observe unchanged output, keep everything downstream green — cannot ever fire, because unchanged outputs are indistinguishable from changed ones. Any whitespace edit reddens the whole file's pipeline. The persisted fingerprints are dead weight that will mislead the next engineer who tries to use them.

**Recommendation:** either hash real outputs (AST/THIR already have binary serializers in `compiler/src/serial/` and `mir/serializer/`) to enable cutoff, or delete the fingerprint fields for intermediate stages and store only the leaf set — honest and smaller.

---

### L-122 — `check` bypasses the query system; the incremental unit for a 1-line change is "everything, again"

**Impact:** High · **Confidence:** High · **Layer:** design

Evidence:
- ADR-010: `run_check` does direct lex → parse → `preload_all_meta_caches` → `check_module`, "constructing **no** QueryContext"; the re-route was formally REJECTED.
- Where the time goes on a 1-line change: daemon result cache (argv CRC + mtime snapshot) misses → full re-lex/re-parse/re-typecheck of the file, plus eager load of all 366 stdlib metas (mtime fast-path skips re-hashing: 419 ms → 285 ms cold preload, `docs/analysis/tooling-performance/03-check-performance.md` F-036/F-015).
- Measured now (release CLI, trivial file): **1.36 s cold, 0.59 s OS-warm** per invocation; warm daemon *unchanged-file* hit is 6.6 ms — but that path replays captured stdout and does zero checking.
- No sub-file, per-function, or per-module typecheck reuse exists anywhere in the codebase.

**Why it conflicts:** the AI-driven edit loop this project optimizes for is dominated by `check` on 1-line changes. Its latency floor is whole-file recheck + whole-stdlib environment, and nothing in the architecture can amortize it — the daemon only removes process/DLL overhead, not the recheck.

**Recommendation:** accept ADR-010 for the CLI, but inside the *daemon* keep a persistent QueryContext per file with real typecheck memoization (the daemon already owns process lifetime, sidestepping ADR-010's "one-shot = zero win" objection). That is the one place query-granular reuse would pay.

---

### L-123 — `.meta` system: sound per-module staleness, no cross-module dependency graph, and lazy loading is permanently blocked by global-scan name resolution

**Impact:** Medium-High · **Confidence:** High · **Layer:** design

Evidence:
- Format: binary serialization of a module's declarations (functions, structs, behavior impls, re-export/private-import *paths*) — `compiler/src/types/module_binary.cpp`.
- Staleness: per-module CRC32C of the module's **own** sources stored in the header (`module_binary.cpp:31-68`), validated per load (`module_binary_read.cpp:1185-1206`); plus a whole-lib signature = `max(mtime):count` over `lib/{core,std,test}/src` gating an F-036 fast path that skips ~734 file re-hashes (`module_binary_read.cpp:1090-1115, 1135-1150`).
- There is **no cross-module dependency graph**: module A's meta never records that it depends on B. Tolerable today because metas store cross-module references by name (resolved at load), but nothing structurally enforces it — the first time a meta embeds resolved data from another module, staleness breaks silently.
- Lazy loading was investigated and rejected as *non-parity*: type/borrow/codegen resolve unqualified names via full-cache `GlobalModuleCache::get_all()` scans (`env_lookups.cpp:156-162,190-196,314-321`), so any subset load changes diagnostics.

**Why it conflicts:** resolution-by-global-scan is the root design decision that couples every `check`/compile startup to O(size of stdlib) forever. The stdlib grows; the floor grows with it. F-013 documented the symptom; this is the cause, and it's load-bearing in the type system, not the cache.

**Recommendation:** longer term, make name resolution import-scoped (prelude + explicit imports) so lazy metadata becomes semantics-preserving. Near term, record explicit inter-meta dependency edges before any meta ever stores resolved foreign data.

---

### L-124 — The entire pipeline's interchange format is LLVM IR *text*: hand-emitted, DLL-crossed as `const char*`, re-parsed every compile, cached by the gigabyte

**Impact:** High · **Confidence:** High · **Layer:** design

Evidence:
- Zero LLVM API includes in either codegen path (`grep 'include <llvm'` over `compiler/src/codegen/llvm/`, `codegen/mir/`, `mir_codegen.cpp` → empty); both emit IR by string building. LLVM appears only in `backend/llvm_backend.cpp`, `backend/jit_engine.cpp`, `testing_compile*`.
- Plugin ABI (ADR-003): `compiler/include/plugin/codegen_api.h:24` — `codegen_compile_ir_to_object(const char* ir_content, …)`.
- Every compile re-parses the text: `LLVMParseIRInContext` at `compiler/src/backend/llvm_backend.cpp:278,509`.
- The incremental cache is therefore a text store: 9,720 `.ll` files / **2.2 GB** (`docs/analysis/incremental-cache/01-cache-inventory.md` layer 3). A GREEN hit (`query_context.cpp:363-409`) loads `.ll` text from disk and still pays print→parse→optimize→object unless the content-keyed object caches hit.

**Why it conflicts (and partly doesn't):** print/parse of IR text is a per-compile tax and forecloses in-memory LLVM module reuse, ThinLTO-style caching, and structured metadata. The compensating benefit is real: the 246K-LOC frontend compiles without LLVM headers (a large C++ build-time win — see L-128's TU times, which would be far worse with LLVM includes). The cost side has quietly grown (2.2 GB text caches, double conversion on the hottest path) while the benefit is fixed.

**Recommendation:** keep the ABI boundary but switch the interchange to LLVM *bitcode* (`LLVMParseBitcodeInContext2` is drop-in on the consumer side; the producer needs one emit-to-bitcode step behind the same C API) — smaller caches, faster parse, no layering change. Rank this above any new optimization pass.

---

### L-125 — Per-file `QueryContext` + the "modern" MIR path copies the entire TypeEnv twice per file

**Impact:** Medium · **Confidence:** High · **Layer:** implementation

Evidence:
- One `QueryContext` per compiled file ("many per-file contexts in a run", `query_context.hpp:150-153`; cache-inventory layer 1: "near-zero cross-file sharing"). Cross-file sharing exists only via the `GlobalModuleCache` singleton and the shared `PrevSessionCache` — both *outside* the query system. `invalidate_file`/`invalidate_dependents` (the in-memory red-green invalidation API, `query_cache.cpp:24-62`) have **zero callers** — dead API.
- `query_core.cpp:585-586` (`provide_hir_lower`): `auto env_copy = *tc.env;` — full copy of TypeEnv (a dozen+ unordered_maps of structs/enums/functions/impls, `env.hpp:841-887`) because HirBuilder wants non-const. `query_core.cpp:708-710` (`provide_mir_build`): a *second* full copy for the pass manager. The legacy AST path takes `*tc.env` by reference (`query_core.cpp:1100`) — no copy.

**Why it conflicts:** the pipeline the project wants to make hot (THIR→MIR) carries two hidden whole-environment copies per file that the legacy path doesn't; and per-file contexts mean a 100-file build re-answers identical stdlib-shaped queries 100 times at the query layer.

**Recommendation:** const-correct `HirBuilder`/`PassManager` (or take a snapshot) to delete both copies — a contained, measurable win on the exact path slated to become default.

---

### L-126 — Four generations of lowering code ship in the DLL; two are dead but still compiled

**Impact:** Medium · **Confidence:** High · **Layer:** implementation

Evidence:
- Live: `ThirMirBuilder` (`thir_mir_builder*.cpp`) and the AST-legacy `LLVMIRGen` (75.5K LOC in `codegen/llvm/`).
- Dead #1 — AST→MIR `MirBuilder`: `mir/mir_builder.cpp` + `mir/builder/{expr,stmt,control,pattern}.cpp` ≈ **1,705 lines**, zero constructor calls anywhere in `compiler/src`.
- Dead #2 — HIR→MIR `HirMirBuilder`: `mir/hir_mir_builder.cpp` + `mir/builder/hir_{expr,expr_control,pattern,stmt}.cpp` ≈ **3,471 lines**; `#include "mir/hir_mir_builder.hpp"` appears *only* in its own implementation files. AGENTS T5 says this path "was removed" — it wasn't; it's compiled into `tml_compiler.dll` (6 `mir/builder` entries in the debug `.ninja_log`).
- Cranelift "backend": a 177-line bridge (`codegen/cranelift/cranelift_codegen_backend.cpp`) with `supports_generics = false` — a stub that nonetheless occupies a branch in the routing gate (`query_core.cpp:991`).

**Why it conflicts:** ~5.2K dead lines cost compile time, DLL size, and — worse — cognitive routing: five "builder" spellings (`MirBuilder`, `HirMirBuilder`, `ThirMirBuilder`, `LLVMIRGen`, `MirCodegen`) make every codegen bug hunt start with "which path am I even on?" — the exact failure mode the codegen-debugger agent memo exists to mitigate.

**Recommendation:** delete `MirBuilder` and `HirMirBuilder` outright (make T5's claim true), and either fund Cranelift or remove the branch.

---

### L-127 — Micro-level C++ health is genuinely good; the caveats are catch-all swallows and a type-erased query core

**Impact:** Medium (positive baseline, two caveats) · **Confidence:** High · **Layer:** implementation

Evidence:
- 283.6K LOC (246,043 src + 37,557 headers). Largest dirs: codegen 83.7K, cli 34.3K, mir 32.6K, types 22.6K.
- **Zero files >3000 lines**; the largest is `mir_codegen.cpp` at 2,209 — decomposition discipline is real and consistent.
- Layer direction is clean: 0 files in `types/`/`parser/` include codegen headers; 0 parser→types includes; codegen's dependence on `parser/ast.hpp` (3 headers) and `types/env.hpp` (7 files) is the *designed* shape of the AST-legacy path, not accidental leakage.
- Ownership: 1,089 `make_unique/make_shared` vs 16 raw `new` and 4 raw `delete` — modern, low crash surface.
- Caveats: **92 `catch (...)`** sites, **34 fully silent `catch (...) {}`** (concentrated in cache I/O, defensible; but `compute_output_fingerprint` returning `{}` on exception silently degrades identity). The query core's result envelope is `std::any` + `any_cast` with a runtime-type-mismatch error path (`query_context.hpp:301-309`) — type-unsafe at the compiler's most centrally-typed junction.

**Why it doesn't conflict:** this is what keeps the codebase workable at 283K lines. The recent `.ilk`-stale-code gotcha is a build-hygiene footnote, not evidence of systemic rot.

**Recommendation:** ban new bare `catch (...) {}` outside cache best-effort code via a lint; replace the `std::any` envelope with a `std::variant` of the 9 result types when the query layer is next touched.

---

### L-128 — Build-time reality and ADR-003's boundary sit in the wrong place for the daily loop

**Impact:** Medium · **Confidence:** High · **Layer:** design

Evidence (measured from `.ninja_log`):
- Full debug build: 684 steps, **58 s wall / 2,939 s CPU**; release: 512 steps, **108 s wall / 3,308 s CPU** (warm zig-cc cache).
- Slowest TUs 15–22 s each — `query_core.cpp` (includes the entire pipeline: 30+ subsystem headers, `query_core.cpp:8-38`), `cli/builder/build.cpp`, `testing_compile.cpp` — because PCH is `if(MSVC)`-guarded (`CMakeLists.txt:916`) while the default toolchain is Zig (ADR-007), and unity builds are disabled for `tml_types`.
- Incremental relink of `tml_compiler.dll` (71.5 MB): 1.9–6.1 s per link.
- ADR-003 today buys: LLVM-free frontend TUs, ABI-versioned mtime-cached loader with zstd + async preload (`plugin/loader.cpp:96-127, 590-656` — well built), daemon-resident DLLs. It costs: 123 MB debug load surface, the text-IR boundary (L-124), and a misplaced seam — **TML→IR codegen lives in `tml_compiler.dll`, not the "codegen" DLL** (`compiler_plugin.cpp:27-28` declares no dependency on it), so the part that changes most (codegen bug fixes) always relinks the big frontend DLL, and the AGENTS T1 warning about rebuilding "the right DLL" exists precisely because the module names lie about the boundary.

**Why it conflicts:** every codegen-fix iteration pays full-TU recompiles of 15-22 s header-giants plus a 71 MB relink; the modular split does not isolate the hot-change surface.

**Recommendation:** enable PCH on the Zig/Clang path (`target_precompile_headers` works fine with Clang); split the IR-emission code into its own target/DLL so codegen fixes relink ~10 MB, not 71 MB.

---

## Verdict

**The incremental-compilation story is real at exactly one granularity and aspirational at every other.** What actually works: (1) whole-file GREEN replay of cached LLVM IR text when no transitive source changed — with honest dependency capture (including `.meta`-loaded stdlib sources) and robust invalidation hardening from phase42a (content-hashed DLL identity, partition pruning, IR GC); (2) content-keyed object/EXE caches downstream; (3) a command-level daemon replay cache. What does not exist despite the "red-green, TyCtxt-like" framing: cross-run reuse of any frontend stage, early cutoff, sub-file granularity, or query participation by `check`. For a *changed* file, TML recompiles everything from bytes to IR, every time, in every mode.

**Top 5 C++ debt items by risk × cost-to-carry:**
1. **Dual live codegen stacks** (75.5K AST-legacy vs ~9.6K THIR/MIR emitters) — every feature ×2, every bug potentially ×2 (F-001; sizes quantified here).
2. **Text-IR interchange everywhere** (L-124) — per-compile parse tax + 2.2 GB caches + forecloses LLVM-level incrementality.
3. **Dead builder generations + stub backend still compiled** (L-126) — 5.2K lines misleading every navigation.
4. **Global-scan name resolution** (L-123) — permanently couples startup latency to stdlib size.
5. **Header-giant TUs with no PCH on the default toolchain** (L-128) — 15-22 s TUs taxing the daily fix loop.

## Keep

- **File decomposition discipline** — zero >3000-line files in 283K LOC is rare and valuable.
- **Ownership hygiene** — 16 raw `new` total; smart pointers everywhere.
- **The plugin loader** (`plugin/loader.cpp`) — mtime-validated handle cache, ABI gate, zstd, async preload: production quality.
- **The phase42a incr-cache hardening** — content-hashed compiler identity with `.bhash` sidecar (`query_incr.cpp:49-147`), position-independent CodegenUnit keys, partition pruning + IR GC, telemetry — the file-level cache is now trustworthy.
- **`.meta` staleness design** — per-module content hash + whole-lib signature fast-path is simple and correct for the current by-name format.
- **Honest self-documentation** — ADR-010 and the `docs/analysis/incremental-cache/` series accurately describe the system's own limits; few codebases do this.

## Top 3 highest-leverage recommendations

1. **Switch the IR interchange from text to bitcode across the plugin ABI and the incr IR store** (L-124). Contained change (emit-side writer + `LLVMParseBitcodeInContext2`), shrinks the 2.2 GB cache several-fold, removes the parse tax from *every* compile including GREEN hits — benefits both codegen paths regardless of how the dual-path split is resolved.
2. **Put real typecheck memoization inside the daemon** (L-122): a persistent per-file QueryContext with structured-diagnostics `TypecheckResult` reuse. The daemon already owns process lifetime, so ADR-010's "one-shot = zero win" objection doesn't apply; this attacks the actual latency of the 1-line-change loop instead of its process overhead.
3. **Delete the dead generations and make the fingerprints honest** (L-126, L-121): remove `MirBuilder`/`HirMirBuilder` (~5.2K lines), the unused invalidation API, and either hash real intermediate outputs (enabling future cutoff) or strip the sentinel fingerprints — so the next incrementality investment starts from truth, not scaffolding that looks load-bearing and isn't.
