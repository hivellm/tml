# Proposal: phase26f_move-semantics-milestone

## Why

phase26b Step 2.3 proved the borrow checker never tracked moves: `move_value()` (sole
writer of `OwnershipState::Moved`) is dead code, and `let y = x; use_it(x)` compiles
clean. This is the root of F-015 ("no move semantics is systemic"). Activating real move
semantics is the ONLY path to genuine Rust-like zero-cost (no defensive clones, no
copy-everywhere), and it's what lets the `drop.cpp:460-471` container leak special-case
be removed. But it makes use-after-move a compile error and breaks every implicit-copy
site across the stdlib and user code — a large migration. Per user decision (#12) it is
a conscious milestone taken AFTER the concrete bugs (F-016/F-022) are closed in
phase26b Step 3.

## What Changes

Activate `move_value`/`move_projection` at real move sites; decide + stage the
use-after-move error policy (measure blast radius behind a flag first); land the Step 2.4
drop-suppression swap driven by live `moved_out`; remove the leak special-case (Step 3.4);
add drop-flag elaboration for conditional drops (Step 4); migrate the fallout.

## Impact

- Affected specs: ownership/borrow/move sections of the language spec.
- Affected code: `compiler/src/borrow/**` (activate move dataflow + errors),
  `compiler/src/codegen/llvm/core/drop.cpp` (consume facts, remove leak special-case,
  drop flags), and broad stdlib fallout (`.duplicate()`/borrow insertions).
- Breaking change: YES — use-after-move becomes an error; code relying on implicit copy
  must change. Staged behind a flag to measure first.
- User benefit: true zero-cost memory (Rust-parity), leak-free, the special-case band-aids
  gone — the language's core thesis delivered.

## Source

- docs/adr/ADR-009-memory-model-soundness.md, decision #12,
  docs/analysis/tml-table-analysis/08-memory-copy-audit.md (F-015).
- Foundation: phase26b 2.1-2.3 (exported facts, v0.3.57). Prereq: phase26b Step 3.
