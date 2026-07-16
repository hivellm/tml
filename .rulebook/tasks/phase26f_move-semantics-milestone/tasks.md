# phase26f — Move-Semantics Activation Milestone (the zero-cost enabler)

> The deferred, high-blast-radius core of the memory model (user decision 2026-07-16,
> rulebook decision #12). Activating real move semantics is what makes TML genuinely
> Rust-like/zero-cost — but it makes use-after-move a compile error and breaks every
> implicit-copy site (stdlib included), so it is a conscious milestone taken AFTER the
> concrete bugs are closed (phase26b Step 3). Foundation already in place: exported
> borrow facts (phase26b 2.1-2.3, v0.3.57, def-span join 100%) + accurate `initialized`
> state. Prereq: phase26b Step 3 (sound read-outs) landed, so container copies are the
> only remaining copy-double-free source.

## 1. Implementation
- [ ] 1.1 Activate the checker's move dataflow: invoke `BorrowChecker::move_value()` / `move_projection()` (dead today, `checker_ops.cpp:478`) at the real move sites — function-arg moves (ownership-taking params), let-init idents, struct-field moves, returns — so `OwnershipState::Moved` / `moved_projections` carry signal
- [ ] 1.2 Decide + implement the use-after-move policy: emit B-errors on use-after-move (Rust-strict), staged behind a flag first to measure the blast radius across stdlib + tests before making it hard
- [ ] 1.3 Step 2.4 swap: drive drop suppression from the now-live `moved_out`/`moved_projections` (phase26b `emit_scope_drops`/`emit_all_drops`), retire `consumed_vars_`/`has_consumed_fields` + the ~30 `mark_var_consumed` sites
- [ ] 1.4 Step 3.4: remove the `drop.cpp:460-471` container leak special-case — now sound because container copies are moves (source not dropped); its removal is the proof the leak class is closed
- [ ] 1.5 Step 4 drop-flag elaboration: conditional (control-flow-dependent) drops guarded by drop-flag allocas, driven by the checker's merged init-state; RefCell `replace`/`swap` + partial moves sound by construction
- [ ] 1.6 Migrate the fallout: every stdlib/user site that relied on implicit copy-and-reuse (the blast radius from 1.2) — insert explicit `.duplicate()`/borrow where a move now applies
- [ ] 1.7 Gates: `sig_alone.c`/`c_essential_repro.c`/`essential.c --emit=ast` all 100/100; full compiler+core+std suites at/above baseline; determinism adversarial + K002; leak-free under the adversarial allocator

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
