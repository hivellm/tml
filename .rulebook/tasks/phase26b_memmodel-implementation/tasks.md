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
- [ ] 2.4 The swap (granularity i): in `emit_scope_drops`/`emit_all_drops` (`drop.cpp:1170,1174,1191,1195`) look up `ownership_by_span_[def_span]` — suppress on `moved_out`, drive partial drops from `moved_projections` (not the `"var.field"` prefix scan). Keep `consumed_vars_` as temporary fallback, then retire the ~30 `mark_var_consumed` sites + remove `consumed_vars_`/`has_consumed_fields` (`drop.cpp:97-116`)

### Step 3 — Sound container / smart-pointer read-out on the AST path
- [ ] 3.1 Every value-returning read out of an owning container/smart pointer (`get`/`get_opt`/iterator `next`/`value`) is either a balanced clone (`ptr_read_clone`, bump paired with the returned value's drop) or a tracked move — NEVER a bitwise alias. Close the aggregate-without-Duplicate bitwise fallthrough (`intrinsics.cpp:819-822`)
- [ ] 3.2 The 13 F-016 hazard sites become sound: `Sync::get`/`Heap::get` copies, `ListIter/HashSetIter/HashMapIter` by-value yields, `List::retain`, `Arc::make_mut` — via balanced clone or a borrowing accessor (coordinate with phase26e for the borrow form)
- [ ] 3.3 F-022: `List::destroy`/`HashMap::destroy` run per-element `Drop` (not just `mem_free` of the buffer) so element handles are released
- [ ] 3.4 Remove the `drop.cpp:460-471` leak special-case (Heap/List/HashMap/Buffer/BinaryWriter/BinaryReader field-drop skip) once reads are sound — the leak band-aid is no longer needed and its removal is the proof the class is closed

### Step 4 — Drop-flag elaboration for control-flow-dependent drops
- [ ] 4.1 Where a place's initialization is control-flow-dependent (conditional move/init across branches), emit a drop flag (bool alloca) set on init/cleared on move, and guard the scope-exit drop on it — using the borrow checker's merged init-state (Step 2) to decide where flags are needed vs statically elidable
- [ ] 4.2 The F-015 move-dependent sites become sound by construction: `let y = x` of a handle-owning aggregate suppresses `x`'s drop; RefCell `replace`/`replace_with`/`swap` (`ref_cell.tml:105-166`); partial moves
- [ ] 4.3 Gates: `sig_alone.c` 100/100, `c_essential_repro.c` 100/100 (was 86), `essential.c --emit=ast` 100/100 (was 0), all corpus canaries 100/100 (incl. `tml_refcount_bleed_userpath` + new F-016/F-022 canaries added here), full compiler+core+std suites at/above baseline — all under adversarial allocator + K002 verifier

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
