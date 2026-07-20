# 09 — Consolidated Priorities & Updated Execution Plan

**Synthesis of:** all eight deep-dives (L-001..L-148) + the prior review (F-001..F-020). This file supersedes `../architecture-performance-review/07-execution-plan.md` where they conflict — the deep-dives re-based several of its premises with measurements.

---

## 1. The cross-cutting patterns

Eight agents audited eight subsystems independently. Five patterns emerged in nearly every report — these, not any single bug, are what "earlier-model debt" actually consists of:

### P1 — Built-but-disconnected (the project's signature failure mode)
The right component almost always *exists*; it is just not wired to the live path. Evidence across dives: the 30-pass MIR optimizer (F-002), the `TraitSolver` (L-006), the THIR exhaustiveness checker (L-043), strict-move checking (`TML_STRICT_MOVES`, off with a measured zero blast radius — L-020), `drop_in_place` (exists, broken, routed around — L-022), the HashMap SIMD group-scan (written, measured 2.25× faster, disabled — L-081), the panic-isolation machinery (in-process today, just not thread-local — L-101), `Builder::spawn[T: Send]` (a stub — L-103), `@repr` (validated, then ignored — L-008), the in-memory invalidation API (zero callers — L-125), HIR inline/closure passes (~2,020 LOC, zero callers — L-048), `mark_moved` on MIR (zero call sites — L-021). **Consequence:** a large fraction of the remediation is "finish and flip the switch," not "build new."

### P2 — The string-typed compiler
The back half of the compiler's real data model is strings: type identity = mangled names that get *re-parsed* into types (L-003); IR = hand-emitted text crossing the plugin ABI as `const char*`, cached as 2.2 GB of `.ll` (L-066, L-124); drop-glue discovery = substring search over *source code* (L-026); layout = a hand-rolled parser of LLVM type strings that disagrees with LLVM (L-007); diagnostics = flattened to strings at query boundaries (L-047); validation = deferred to LLVM's text parser (the K001 treadmill, L-004). Every heap-corruption era and most "flaky" months trace to one of these.

### P3 — Safety purchased at runtime with copies, because ownership facts are missing statically
Clone-on-read on every container access, copy-to-drop to run destructors, eager iterator snapshots of whole collections, per-free runtime string classification, FFI calls per refcount op (L-029, L-084, L-025). The band-aids for the memory model each *added* work; Rust pays none of these. This is why "≤2× Rust" fails even where codegen is fine.

### P4 — Two (or four) of everything
Two codegens (F-001); two Arcs with different soundness (L-102); two borrow checkers, one silently unsound to enable (L-024); **four** type-inference implementations (L-044); four generations of MIR builders, two dead but compiled (L-126); three struct-duplicate emitters (L-065); ~16 method-dispatch emitter files (L-067); two mangling schemes (L-003); two object models (L-046); every piece of sugar lowered twice (L-048). Every fix is paid N times; every divergence is a latent bug.

### P5 — Docs/specs/plans that assert what the code doesn't do
Phantom ADRs cited as active (ADR-001..008 don't exist as files — L-040, L-120); spec quick-starts that don't parse (L-140); a fictional macro system (L-141); `@inline(never)` doing the opposite of its docs (L-143); "red-green incremental" claims vs 1-of-9 persisted queries (L-120); the "~100 MB DLL / ~500 ms spawn" test-cost premise that turned out to be 0.3-0.7 MB / 11-30 ms (L-105). Planning built on these premises mis-allocates effort; L-105 alone re-bases the entire C1 business case.

---

## 2. Corrections to the prior review

| Prior claim | Correction | Source |
|---|---|---|
| F-007: subprocess tests each load ~100 MB DLLs; spawn is the structural cost | Test EXEs are 0.3–0.7 MB static binaries; spawn ≈ 11–30 ms. The structural cost is the **~176 codegen+link cycles**. C1's value is unlocking the mega-binary (1 link), not deleting spawn. | L-105 |
| C1 (panic isolation) is a large language/runtime project | The mechanism exists and runs in-process per-test today; the gap is ~15 process-global statics → `_Thread_local` + a pool dispatcher. **≈ 1.5–2.5 engineer-weeks.** Prerequisite: real atomics (L-102), or parallel in-process tests are unsound. | L-101, L-102 |
| Phase B option (ii) "port essential MIR passes to the AST path" | Refined: for **release** IR, 4 additive fixes (attributes, TBAA, sret/insertvalue, less spilling) get there without porting passes. For **O0/debug/test** IR — the felt pain — option (ii) cannot work without rebuilding the optimizer in a worse substrate. | L-060..L-068 |
| Phase B option (i) "MIR unification" is primarily a codegen project | It is a **whole-pipeline** project: the MIR path today double-drops on `let b = a` (L-021), drops `?.` semantics (L-042), mis-widths integer literals (L-001), and receives no ownership facts. Unification = the "semantic spine" (below), not just codegen routing. | L-001, L-021, L-042, L-044 |
| Tests are slow but trustworthy post-phase44a | Deeper trust holes: exhaustiveness never enforced anywhere users see (L-043), atomics fake (L-102), moves unchecked by default (L-020), smart pointers leak payloads (L-022) — several "passing" behaviors are silently wrong today. | L-020, L-022, L-043, L-102 |

---

## 3. Consolidated priority tiers

Ordering rule: correctness cliffs first (they poison every measurement), then shipped-path performance, then the test-speed endgame, then the strategic consolidation that subsumes the rest. Items marked ⚡ are small (hours-to-days).

### Tier 0 — Correctness cliffs (days each; do immediately, no design decisions needed)

| # | Item | Findings | Why now |
|---|------|----------|---------|
| 0.1 | ⚡ Flip `TML_STRICT_MOVES` default-on | L-020 | Measured zero blast radius at v0.3.65; converts internal-error crashes into real B001 diagnostics |
| 0.2 | Fix the `drop_in_place` intrinsic | L-022 | Smart pointers currently never run payload destructors; closes leak class in `Heap`/`Shared`/`Sync`, deletes copy-to-drop |
| 0.3 | Real atomics: all widths, honor `Ordering`; collapse the two Arcs | L-102 | `std::sync::atomic` is fake today — silent UB; prerequisite for any parallel execution (incl. Tier 2) |
| 0.4 | Enum deep-clone + finish phase44b/44c (clone/drop symmetry, alloc-size lint) | L-065, F-016, F-017, L-080 | The open heap-corruption tail; phase44b/44c tasks already filed |
| 0.5 | ⚡ Exhaustiveness as a real `check` diagnostic (deny-by-default) | L-043 | The language's core idiom is unchecked everywhere users look |
| 0.6 | ⚡ Fix `@inline(never)` inversion; honor `@repr` on enums | L-143, L-008 | Tiny, surgical; both currently do nothing or the opposite |
| 0.7 | ⚡ Kill the integer-literal split-brain (one default, conflict = error) | L-001 | Live miscompilation demonstrated; also deletes the T6 annotation tax |

**Gate:** probe corpus from dives 01/02 (move probes, drop probes, literal-width probes, exhaustiveness probes) all behave; `std/collections` standalone 20/20 reruns.

### Tier 1 — Performance on the path that ships (weeks; independent of the big decision, none wasted later)

| # | Item | Findings | Expected effect |
|---|------|----------|-----------------|
| 1.1 | Run mem2reg/SROA/always-inline/DCE at O0-semantics (decouple checked-math from SSA cleanup) | L-060, L-068 | The single biggest felt-perf change: tests and default builds stop shipping raw naive IR; measure compile-time delta |
| 1.2 | Pointer-parameter attributes (`noalias`/`readonly`/`dereferenceable`/`align`) + minimal TBAA tree | L-062, L-063 | Unblocks `-O2` recovery on aggregate/collection code — the release-parity gap |
| 1.3 | ⚡ Link mimalloc into `tml_runtime.lib` | L-104 | Cheapest runtime win given allocation-heavy codegen; also de-serializes the CRT heap lock for Tier 2 |
| 1.4 | Fix generic+SIMD-in-`lowlevel` codegen → re-enable HashMap group-scan; tombstone rehash | L-081 | Known 2.25× on the most-used collection; unblocks SIMD in all generic code |
| 1.5 | ⚡ Stdlib mechanical fixes: `Text`→`tml_str_len`; single-allocation transforms; `Buffer` float bitcast + memcpy stringifiers; introsort | L-082, L-085, L-086, L-087 | Hot-path constant factors; each is hours |
| 1.6 | Iterators by borrow: `List` cursor/`Duplicate`, retire clone-snapshots; container reads return `ref T` | L-084, L-028, L-029 | Deletes hidden O(n) allocation per iteration + the per-read clone tax; converges with 0.4 |
| 1.7 | sret + insertvalue enum construction; reduce param spilling | L-064, L-061 | Narrows the O0 gap independent of LLVM |

**Gate:** `/compare-ir` corpus at ≤2× Rust *at -O2* on aggregate/collection probes (with generator provenance verified per F-004); benchmark the `.filter().map().sum()` idiom (L-145) before republishing any zero-cost claims.

### Tier 2 — Test-speed endgame (after 0.3; ~2-4 weeks total)

| # | Item | Findings |
|---|------|----------|
| 2.1 | TLS-ify panic/catch state (~15 statics) + threaded suite dispatcher; POSIX side in the same change | L-101, L-108 |
| 2.2 | Mega-binary endgame: 1 link instead of ~176 suite links, intra-run thread parallelism | L-105, F-005..F-008 |
| 2.3 | Daemon-resident typecheck memoization (persistent QueryContext where process lifetime exists) | L-122 |
| 2.4 | IR interchange text→bitcode across plugin ABI + incr store (2.2 GB cache shrinks; parse tax gone) | L-124 |

### Tier 3 — The strategic consolidation (the real "Phase B", reframed)

The dives converged from four directions on one program — **build the semantic spine, then retire the duplicates**:

1. **Stable node IDs at parse time; checker results persisted and consumed downstream** (kills the 4 inference engines and the `void*` map — L-001, L-044).
2. **HIR = the single desugaring point** (`?.`, let-else, for-in, templates lowered once — L-042, L-048).
3. **Structural type IDs end-to-end; mangling one-way; layout from `llvm::DataLayout`** (kills the K001 treadmill and the string data-model — L-003, L-004, L-007, L-026).
4. **Ownership facts on MIR (CFG-based init/move/liveness), `DropInst` first-class** (L-021, L-024, L-027 — where ADR-009 always said it should end).
5. **One codegen** consuming the above via LLVM's builder API (bitcode out), absorbing the Tier 1 attribute/TBAA work; then delete the AST-legacy path, the dead builders, `method_maybe`/`method_outcome`, the three duplicate emitters (F-001, L-066, L-067, L-126).

This is a staged multi-month program, but **every tier above feeds it rather than fighting it** — attributes/TBAA, allocator, stdlib contracts, atomics, and the spine's node-ID work are all keep-forever assets. Recommended sequencing: land Tiers 0–2 first (they de-risk and speed up the iteration loop the consolidation itself needs), then execute the spine stages behind the existing routing gate, migrating construct-by-construct with A/B IR diffing.

**Decisions this tier needs from the user (flagged, not made):**
- **D1:** Commit to the semantic-spine consolidation as the strategic direction (recommended), or continue indefinitely on the hardened AST path (viable for release IR after Tier 1, but permanently pays P4's N× tax and never fixes O0/debug).
- **D2:** The OOP layer (L-046): freeze behind a gate, or specify its lowering to behaviors and delete the parallel branches. (Recommended: freeze now, decide lowering during the spine work.)
- **D3:** Vapor-spec items (L-141 decorator/quote, L-146 `@simd` decorator): implement or formally cut. (Recommended: cut decorator/quote — it contradicts the no-macros principle; re-document SIMD as `core::simd`.)
- **D4:** Naming window (08's verdict): keep ambiguity-driven renames (`[T]`, `do(x)`); decide soon whether the cosmetic renames (`Maybe`/`behavior`/`duplicate`/`Heap`) are worth their permanent LLM-translation tax. The window is closing, not open.

### Tier 4 — Hygiene & trust (cheap, parallel-anytime)

| # | Item | Findings |
|---|------|----------|
| 4.1 | ⚡ Delete dead builders (~5.2K LOC), orphaned HIR passes (~2K), Polonius-or-parity | L-126, L-048, L-024 |
| 4.2 | ⚡ Doc-example CI gate (`tml check` every fenced block); fix `module`/`loop` quick-starts | L-140 |
| 4.3 | Port the 7 `.claude/rules/` idiom rules to `tml lint` (`I001-I007`, `--fix`) | L-147 |
| 4.4 | ⚡ Write the missing ADRs as they *are* (grammar policy, incremental reality); one `STATUS:` convention for aspirational specs | L-040, L-120, L-148 |
| 4.5 | Quarantine `compiler-tml/` from default build/test/docs | F-020 |
| 4.6 | Lexer terminator-insertion rule + doc-comment side table (retires 148 `skip_newlines` sites) | L-041 |
| 4.7 | ⚡ Update stale premises in code/docs (`cmd_test.cpp:299` "100 MB", F-007 text) | L-105 |

---

## 4. What NOT to do (explicit anti-recommendations)

- **Don't build full unwinding/EH** for panic — the noreturn design is the right perf default; C1 needs TLS-ification, not landingpads (L-100, L-101).
- **Don't build the decorator/quote macro system** to match the spec — cut the spec instead (L-141).
- **Don't add new soundness mechanisms to the AST codegen side** — that substrate is scheduled for demolition; new dataflow belongs on MIR (L-021 verdict).
- **Don't hand-port MIR optimization passes into the string emitter** — that was the old option (ii) reading; the probe data shows attributes+TBAA+LLVM-at-O0 gets the same outcome without throwaway work (L-060..L-068).
- **Don't trust green tests as a perf or soundness signal yet** — Tier 0 items mean several wrong behaviors currently pass (L-020, L-022, L-043, L-102).
- **Don't optimize `check` startup further via caching tweaks** — the floor is the global-scan name-resolution design (L-123); daemon memoization (2.3) is the only lever that moves it.

---

## 5. Suggested immediate next steps

1. File Tier 0 as rulebook tasks (0.4 extends the open phase44b/44c work; 0.1/0.5/0.6/0.7 are each ⚡).
2. Take D1–D4 to the user as the one decision batch this review actually requires.
3. After Tier 0 lands, re-run the dive-01/02 probe corpus + `std/collections` standalone×20 as the trust gate for everything after.
