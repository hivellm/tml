# phase26b Step 2 — Borrow-fact → AST-codegen Wiring Plan (concrete)

Read-only scoping, 2026-07-15. Companion to `spec.md` (premise correction).

## The JOIN KEY: definition `SourceSpan`

There is NO shared AST-node id across the two phases. The borrow checker keys on
`PlaceId` (checker-internal `uint64_t`, `checker_env.cpp:80`, never leaves the
instance); AST codegen keys drops on the bare source variable **name string**
(`consumed_vars_` = `unordered_set<string>`, `llvm_ir_gen.hpp:547`). A bare-name key is
free but WRONG — it cannot distinguish shadowed bindings (checker gives `x`@Place0 vs
`x`@Place1 distinct identity via `name_to_place_`, `checker_env.cpp:98`) nor per-branch
state.

**Correct key = the binding's definition `SourceSpan`, present on BOTH sides today:**
- checker: `PlaceState.definition` (`Location` with `SourceSpan`, `checker.hpp:588`).
- codegen: `let.span` at each `register_for_drop` site (`llvm_ir_gen_stmt_let.cpp:422`)
  — currently DROPPED (not stored in `DropInfo`, `llvm_ir_gen.hpp:528-536`).

Small translation layer: key exported facts by def-span; thread `let.span` into
`DropInfo` + `register_for_drop`. Name stays as secondary/debug key.

## Critical caveat — granularity (shapes the two-phase landing)

The checker retains only an **end-of-function snapshot** in `places_` (`checker.hpp:823`)
— `move_value` mutates state in place (`checker_ops.cpp:478-520`); there is NO per-point
history. But drops fire at MANY nested/conditional scope exits. A naive "moved by
end-of-function" export MIS-FIRES on mid-function and conditional moves — trading one
unsoundness for another. Therefore land in two granularities:
- **(i) Monotonic set (Steps 1-5, cheap):** "ever fully/partially moved in the function."
  Strictly better than `consumed_vars_` (checker-authoritative, projection-aware,
  catches moves the ~30 syntactic sites miss), never worse than today.
- **(ii) Per-scope-exit correctness (Step 4, drop flags):** sample the merged init-state
  lattice (`merge_init_states`, `checker_expr.cpp:413-440,469-517`) at each
  `drop_scope_places` (`checker.hpp:1182`); emit runtime drop flags for `conditional`
  places. This is the ONLY piece needing NEW checker work (retain per-drop-point
  snapshots vs the current overwrite).

## lowlevel gap (F-004) is SEPARATE, not a checker extension

Drop-suppression facts concern named local bindings, which the checker DOES track. The
`lowlevel { *this.ptr }` read-out (get/get_opt/iterator next) returns a value that is
not a tracked place at all — a separate problem answered by a **codegen-side classifier**
(Step 3): method calls where receiver base ∈ {Sync,Heap,List,HashMap,HashSet,Arc,Buffer}
× method ∈ {get,get_opt,next,value,...} get a balanced `ptr_read_clone` (already exists,
hardcoded table `instructions_call.cpp:1008-1040`) instead of a bitwise alias. No checker
change for B1-on-AST.

## Dependency-ordered plan (file:line)

1. **[cheap] Export facts.** Add `PlaceOwnershipFact{def_span, name, moved_out,
   initialized, moved_projections, conditional}` + `ownership` vector to
   `BorrowcheckResult` (`query_key.hpp:196-199`). Add `BorrowChecker::ownership_facts()`
   snapshotting `env_.all_places()` (public, `checker.hpp:703`) keyed by
   `definition.span`. Populate in `provide_borrowcheck_module` (`query_core.cpp:488-500`)
   before the stack-local checker is destroyed. No behavior change.
2. **[cheap] Plumb into codegen.** `LLVMIRGen::set_ownership_facts()` +
   `ownership_by_span_` member; call in `provide_codegen_unit` (facts from `bc` at
   `query_core.cpp:722`, gen built at `:1030`). Also wire the direct-CLI sites so
   `tml build`/`run` get facts: `build.cpp:567`, `parallel_build.cpp:676`,
   `run_profiled.cpp:194`, `llvm_codegen_backend.cpp:132`. No new query edge (the
   codegen_unit→borrowcheck dep already exists, `:722`).
3. **[translation] Carry the key.** Add `SourceSpan def_span` to `DropInfo`
   (`llvm_ir_gen.hpp:528`); extend `register_for_drop` (`drop.cpp:128`) + ~15 callers to
   pass `let.span`.
4. **[the swap, granularity (i)] Fact-based suppression.** In `emit_scope_drops`/
   `emit_all_drops` (`drop.cpp:1170,1174,1191,1195`) look up
   `ownership_by_span_[info.def_span]`: suppress on `moved_out`, drive partial drops from
   `moved_projections` (not the `"var.field"` prefix scan). Keep `consumed_vars_` as
   temporary fallback.
5. **[cleanup] Retire** the ~30 `mark_var_consumed`/`mark_field_consumed` sites and
   remove `consumed_vars_`/`has_consumed_fields` (`drop.cpp:97-116`).
6. **[Step 3, codegen classifier] Read-out sites.** Extend `ptr_read_clone` whitelist to
   the F-016 set (Sync/Heap get, List/HashMap accessors, ListIter/HashSetIter/
   HashMapIter next, List::retain, Arc::make_mut).
7. **[Step 4, expensive] Per-scope-exit drop flags** (the only new checker dataflow).

## Scope estimate
- Export facts: ~150 LOC. Plumb: small. Rewire drop.cpp: ~100-200 LOC (mostly mechanical
  caller updates). Join translation: small (span-keyed) + a Projection-path ↔ "var.field"
  normalizer for partial moves.
- **Biggest risk:** the granularity mismatch (end-of-function snapshot vs nested/
  conditional exits) — mitigated by landing (i) first (never worse than today), deferring
  conditional correctness to Step 4. Secondary: shadowing (→ span key), partial-move path
  normalization.
- **Regression gates:** determinism corpus (adversarial) + K002 verifier; `sig_alone.c`/
  `c_essential_repro.c`/`essential.c`; `tml_refcount_bleed_userpath`; core/alloc(41);
  collections; archived automatic-drop-system + drop-raii suites.
