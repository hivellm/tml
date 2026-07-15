# phase26b — Memory-Model Implementation via B3 (Stabilization ERA 0, Phase B)

> Implements **ADR-009 Option B3** (ACCEPTED 2026-07-15): unify user builds onto
> the query/MIR pipeline the test framework already validates, then land
> move/drop-flag elaboration ONCE. Evidence: `docs/adr/ADR-009-memory-model-soundness.md`
> + `../phase26a_memmodel-adr-decision/specs/groundwork/spec.md` (archived at
> `archive/2026-07-15-phase26a_memmodel-adr-decision/`).
> Every step gates on: determinism corpus (adversarial ON) + affected suites +
> K002 verifier green.

## 1. Implementation

### Step 1 — Immediate F-013 mitigation (live production bleed)
- [x] 1.1 `increment_count`/`decrement_count` rewritten to direct field reads (`(*this.ptr).strong_count`) — no `SharedInner[T]` copy, no drop-glue. IR verified: GEP+load i32 only, zero `load %struct.SharedInner`, zero drop calls. Audit: the only two whole-inner copies in shared.tml were these; `try_unwrap`'s `(*this.ptr).value` is an intentional unique-owner move-out (left as-is); `Heap::into_inner` same intentional shape (deferred to step 4.4 semantics work)
- [x] 1.2 Canary added: `scripts/fixtures/refcount_bleed_probe.tml` run via `tml run` (USER/AST path — test-framework exes take query/MIR and mask the class), registered in the corpus as `tml_refcount_bleed_userpath`. Before fix: bleeds (2→1→−1, exit 1). After: **100/100 normal + 100/100 adversarial**; core/alloc 41/41 and determinism 5/5 clean. Baseline doc updated (footnote ¹)

### Step 2 — MIR pipeline gap closure + flip
- [ ] 2.1 Inventory exactly why `build.cpp:320-407` forces the AST fallback (imported-module functions, generic instantiation, generic enum construct/destructure) and map each to the query pipeline's existing solution (the test framework compiles the same programs via `testing_compile.cpp` — reuse that machinery)
- [ ] 2.2 Wire `tml build`/`tml run` to the query/MIR pipeline behind `TML_PIPELINE=query` env/flag; determinism corpus + bleed probes must pass under the flag
- [ ] 2.3 Run the full compiler + core + std suites under the flag; triage deltas (differences = latent MIR-path holes currently masked — fix or file with owner)
- [ ] 2.4 Flip the default (`tml build`/`run` use query/MIR); keep `TML_PIPELINE=legacy` escape for one release; gates ×100 green

### Step 3 — AST-legacy path retirement
- [ ] 3.1 Make `--emit-ir` emit from the LIVE pipeline (today it lies: emits AST-path IR that users run but tests don't — after the flip it must show query/MIR output)
- [ ] 3.2 Remove the AST-fallback selection from `build.cpp`/`parallel_build.cpp` and delete/deprecate the dead emission path; record removed-LOC and any @-risk features in the patch notes

### Step 4 — Move/init-state + drop-flag elaboration (B1 mechanics, once)
- [ ] 4.1 First-class `DropInst` in MIR (instruction set, builder, printer, validator) replacing eager `CallInst("Type::drop")` lowering; migrate the string-matching passes (`remove_unneeded_drops`, `destructor_hoist`, `batch_destruction`)
- [ ] 4.2 Wire `BuildContext::mark_moved` (currently dead) at every move site in the THIR→MIR builder; moved-from locals are never dropped
- [ ] 4.3 Port the borrow checker's init-state merge dataflow (checker.hpp:799-816) to MIR drop elaboration; conditional drops via flags only where control-flow-dependent
- [ ] 4.4 Extend the discipline below `lowlevel` for stdlib internals (container read-out = borrow or explicit clone; `.get()` semantics well-defined; supersedes `ptr_read_clone` conservative detection)
- [ ] 4.5 Gates: `sig_alone.c` 100/100, `c_essential_repro.c` 100/100 (was 86), `essential.c --emit=ast` 100/100 (was 0), corpus canaries 100/100, full suites at/above baseline — all under adversarial allocator

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
