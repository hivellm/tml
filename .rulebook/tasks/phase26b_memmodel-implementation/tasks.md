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
> The borrow checker already computes the right lattice (`OwnershipState{Owned,Moved,
> Borrowed,MutBorrowed,Dropped}` + init-state merge dataflow, `checker.hpp:330-336,
> 799-816`) and DISCARDS it before codegen (`query_core.cpp:575-593`, F-004). Surface
> those facts to `LLVMIRGen` so drop insertion is ownership-driven, not name-based.
- [ ] 2.1 Thread borrowcheck's per-place ownership/init-state result through the query system into the AST codegen inputs (a new query dependency `provide_codegen_unit` → borrowcheck facts, replacing the discard at `query_core.cpp:575-593`)
- [ ] 2.2 Replace the name-based `consumed_vars_` (`drop.cpp:97-99`, ~30 syntactic sites) with a per-place initialization map keyed on the borrow-checker facts; a place that is moved-from OR uninitialized on a path is not dropped there
- [ ] 2.3 Extend coverage below `lowlevel { *this.ptr }` for the stdlib's own raw-pointer internals — the aliasing the checker is currently blind to (F-004); the read-out sites in shared/heap/collections must be classified as move | borrow | balanced-clone

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
