# phase26b — Memory-Model Implementation via B1-on-AST (Stabilization ERA 0, Phase B)

> Implements **ADR-009 REVISED — Option B1-on-AST** (user decision 2026-07-15,
> supersedes the same-day B3 acceptance). B3's premise was refuted by Step-2 scoping
> (`specs/step2-scoping/spec.md` + `query_core.cpp:931`): the test framework does NOT
> use MIR — tests AND users run the SAME AST-legacy path for stdlib/generic programs.
> So fix the AST path directly (what 100% of programs run); MIR unification is deferred
> to phases 30–33 (frozen). Evidence: `docs/adr/ADR-009-memory-model-soundness.md`,
> `docs/analysis/tml-table-analysis/08-memory-copy-audit.md` (F-015..F-023).
> Every step gates on: determinism corpus (adversarial ON) + affected suites + K002 verifier.

## 1. Implementation

### Step 1 — Immediate F-013 mitigation (live production bleed) — DONE
- [x] 1.1 `increment_count`/`decrement_count` rewritten to direct field reads (`(*this.ptr).strong_count`) — no `SharedInner[T]` copy, no drop-glue. IR verified (v0.3.55)
- [x] 1.2 Canary `scripts/fixtures/refcount_bleed_probe.tml` via `tml run`, corpus `tml_refcount_bleed_userpath`: 0/N before → 100/100 both modes after; core/alloc 41/41, determinism 5/5

### Step 2 — Consume borrow-checker move/init facts in the AST codegen
> Scoped concretely in `specs/step2-scoping/wiring-plan.md`. The checker already computes
> `OwnershipState`/`moved_projections`/`is_initialized` (`checker.hpp:330-336,567-614`)
> and DISCARDS it (`provide_borrowcheck_module` returns only success+errors,
> `query_core.cpp:465-502`). **JOIN KEY = the binding's definition `SourceSpan`** (exists
> both sides: `PlaceState.definition` `checker.hpp:588`; `let.span` at codegen, currently
> dropped). Facts already exist → export + join is CHEAP. Land granularity (i) first
> (monotonic "ever moved" set — never worse than today); conditional correctness is Step 4.
- [x] 2.1 Export: added `PlaceOwnershipFact{def_span, name, moved_out, initialized}` in `common.hpp` (neutral home — no new include edges; `moved_projections`/`conditional` deferred to Step 4 per plan) + `ownership` vector on `BorrowcheckResult` (`query_key.hpp`). `BorrowChecker::ownership_facts()` (`checker_core.cpp`) snapshots `env_.all_places()` keyed by `definition.span`; populated in `provide_borrowcheck_module` before the stack-local checker dies. No behavior change, no new query edge.
- [x] 2.2 Plumbed into `LLVMIRGen` (`set_ownership_facts` + `ownership_by_span_`, keyed by packed start/end byte offsets). Wired the codegen_unit site (`query_core.cpp`, facts from cached `bc`) AND the direct-CLI sites (`build.cpp`, `parallel_build.cpp`, `run_profiled.cpp` — each hoists a function-scoped facts vector captured from the NLL checker before it's destroyed). `llvm_codegen_backend.cpp::compile_ast` left unchanged: it has NO borrow-result source and NO live callers (dead interface method) — threading facts there would be dead code.
- [x] 2.3 Carried the key: `DropInfo::def_span` added; `register_for_drop` takes an optional `def_span` (default `{}`, so the distinct MIR `ctx_.register_for_drop` and the when.cpp binding site are unaffected). The 8 `gen_let_stmt` sites thread `let.span`; the const-decl site threads `const_decl.span`.
  - **JOIN-PROOF RESULT (the whole point):** def-span join **CONFIRMED VIABLE**. Debug proof behind `TML_DROP_FACTS_DEBUG=1` in `emit_scope_drops`/`emit_all_drops` (does NOT change the drop decision). 100% of drop-registered user-module `let` bindings that carry a span matched their exported fact by def-span — refcount canary 5/5, explicit-move probe 3/3, **0 MISS-no-fact**. Root reason it works: both phases read the SAME cached parsed module, so `let.span` byte offsets are identical (the checker defines places at `current_location(let.span)`, `checker_stmt.cpp:136`). Only misses are synthesized library-internal bindings with no `let.span` (`digits`@0..0), correctly classified MISS-no-span.
  - **PAYOFF / decision-critical for 2.4:** `moved_out` is currently **uniformly false** (0 `moved_out=1` facts across all probes). `BorrowChecker::move_value()` — the SOLE writer of `OwnershipState::Moved` — is **dead code** (declared `checker.hpp:1115`, defined `checker_ops.cpp:478`, NEVER called), and the partial-move path (`move_projection`/`moved_projections`) does not fire for function-arg or let-init moves either. Verified: `let y = x; use_it(x)` (blatant use-after-move) compiles clean, no B-error. This is the F-015 "no move semantics is systemic" dormancy. Consequence: the payoff is INVERTED vs the plan — `consumed_vars_` currently catches MORE (it caught `use_it(y)` → consumed=1 while the fact said moved_out=0). So granularity-(i) "suppress on moved_out" would suppress NOTHING and be strictly worse than today. **Step 2.4 must FIRST activate the checker's move dataflow at move sites (invoke `move_value`/`move_projection`) before it can drive drop suppression from `moved_out`.** `initialized` IS accurate (from `let.init.has_value()`) and usable now.
- [~] 2.4 **DEFERRED → move-semantics milestone (phase26f).** User decision 2026-07-16: the swap can't drive drop suppression from `moved_out` until the checker's move dataflow is activated (`move_value`/`move_projection` are dead code — Step 2.3 payoff finding). Activating strict move-checking has huge blast radius (breaks implicit-copy code, stdlib included). So this + Step 3.4 + Step 4 move to phase26f, done as a conscious milestone AFTER the concrete bugs are closed (Step 3). `consumed_vars_` stays as the drop-suppression mechanism meanwhile; the exported facts (2.1-2.3) remain the ready foundation.

### Step 3 — Sound container / smart-pointer read-out (DONE 2026-07-16, v0.3.58)
- [x] 3.1 Read-outs → balanced clone (`ptr_read_clone`): `ListIter::next`, `HashSetIter::next` (behaviors.tml), `HashSetIter::current`/`next_item` (class_collections.tml), `HashMapIter::value` (hashmap.tml). `List::retain` rebuilt precisely: predicate BORROWS the slot (`ref (*elem_ptr)` — compiles & passes), kept elements move byte-wise via `mem_copy` (no typed local → no spurious drop), only REMOVED elements are read into an owning local whose drop releases them. `Sync::get` (value = field 0 → `this.ptr as *T`) and `Heap::get` migrated; `Heap::duplicate` simplified (`Heap::new(this.get())` — get is already a balanced clone; the old form double-cloned via a dropping temp, F-016 #9)
- [x] 3.2 `Arc::make_mut`: CoW branch deep-clones via `(*this.ptr).data.duplicate()` (added `where T: Duplicate` — the honest CoW requirement); same sound shape as `Shared::get_clone`
- [x] 3.3 F-022: `List::destroy` drops each live element (0..len); `HashMap::destroy` drops key+value of every occupied slot (ctrl >= 0) — before freeing the buffers. No-op for primitive types
- [ ] 3.4 **DEFERRED → phase26f:** remove the `drop.cpp:460-471` leak special-case — only safe once container COPIES are moves (needs the move dataflow). Until then the leak-over-double-free tradeoff for container copies stays.
- [x] 3.5 Gates: 4 new corpus canaries (`f016_list_iter`, `f016_retain` keep-all/remove-all, `f016_arc_makemut` CoW refcount, `f022_destroy_releases` list+hashmap) all pass; determinism gate **22/22** at floor (adversarial ON); core/alloc 44/44, collections key suites 10/10, std/json 23/23, msgpack 3/3, std/sync 44/45. Four failures found during verification were ALL proven identical at HEAD (pre-existing, now catalogued in known-failures.txt): arraylist K001, once_lock K001, sync_barrier/sync_isolation K001 (undefined DualMutexInner drop — mono-queueing family), mutex X002. NEW phase27a specimen #5: `HashMap[I64, Shared[I64]]` (smart pointer as map value directly) K001

### Step 4 — DEFERRED → move-semantics milestone (phase26f)
Drop-flag elaboration for control-flow-dependent drops; needs the activated move dataflow. `initialized` facts (accurate today) feed it. Gates (`essential.c` 100/100 etc.) belong to the milestone close.

> **Deferred-item disposition (2026-07-16):** 2.4 LANDED as phase26f 1.3 (v0.3.62 —
> union suppression `consumed_vars_ OR moved_out`, fixed the `let b = a` double-drop);
> 3.4 = phase26f 1.4 (in flight); Step 4 = phase26f 1.5. All deferred work has a live
> owner; nothing dangles. This task's own scope (Steps 1–3 + facts foundation) is complete.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [x] 2.1 Docs updated: `docs/adr/ADR-009-memory-model-soundness.md` gained an **Implementation status** table (plan item → landed-as → version, v0.3.55–62, incl. the move_value-dead-code discovery that re-sequenced 2.4 into phase26f); `docs/analysis/tml-table-analysis/08-memory-copy-audit.md` gained a **Closure status** table (F-013..F-023 each with status + closing version/canary, plus the audit-missed `let b = a` double-drop found by the facts). Specs already in-task: `specs/step2-scoping/{spec,wiring-plan}.md`.
- [x] 2.2 Tests = executable canaries in the determinism corpus (regression-guarded forever, adversarial allocator ON): `tml_refcount_bleed_userpath` (Step 1), `f016_list_iter`/`f016_retain`×2/`f016_arc_makemut`/`f022_destroy_releases` (Step 3), `tml_let_move_double_drop` (2.4 via phase26f). Framework tests: `compiler/tests/determinism/*.test.tml` (@test framework). Join-proof harness behind `TML_DROP_FACTS_DEBUG=1`.
- [x] 2.3 Gates run green at each step's landing (recorded per-item above): determinism 22/22 → **23/23** at floor with the new canary; core/alloc 44/44, core/str 32/32, std/json 23/23, collections at baseline; all failures encountered during verification proven pre-existing at HEAD and catalogued in `scripts/known-failures.txt` (shrink-only).
