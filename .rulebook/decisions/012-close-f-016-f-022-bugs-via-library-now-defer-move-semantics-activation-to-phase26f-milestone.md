# 12. Close F-016/F-022 bugs via library now; defer move-semantics activation to phase26f milestone

**Status**: proposed
**Date**: 2026-07-16
**Related Tasks**: phase26b_memmodel-implementation

## Context

phase26b Step 2.3 join-proof found move_value dead. consumed_vars_ (syntactic) currently catches more than the checker, so a moved_out-driven swap would suppress nothing. Full move-semantics is the crux of Rust-like zero-cost but a large migration; the user chose to close real bugs first and schedule moves as a conscious milestone.

## Decision

User decision 2026-07-16. Discovery: BorrowChecker::move_value() (sole writer of OwnershipState::Moved) is dead code — TML never tracked moves; use-after-move compiles clean (root of F-015). Activating strict move-checking has huge blast radius (breaks implicit-copy code, stdlib included). So: (A) NOW — Step 3 closes the concrete double-free/UAF/leak bugs via library/classifier changes with NO move semantics: migrate the 13 F-016 read-out sites (ListIter/HashSetIter/HashMapIter yields, List::retain, Sync/Heap::get) to ptr_read_clone balanced clones, Arc::make_mut to deep-clone, and make List/HashMap::destroy run per-element Drop (F-022). (B) DEFERRED to phase26f milestone — activating move_value/move_projection (making use-after-move an error), the Step 2.4 drop-suppression swap, removing the drop.cpp:460-471 leak special-case (Step 3.4), and drop-flag elaboration (Step 4). The exported borrow facts (phase26b 2.1-2.3, v0.3.57, def-span join 100%) + accurate `initialized` state are the ready foundation. Result: TML becomes SOUND (no corrupt/leak) fast; true zero-cost comes with the moves milestone.

## Alternatives Considered

- Activate full move semantics now (Rust-strict): rejected — huge blast radius before any bug closes
- Suppress-drop-without-move-error (Swift/ARC-like): rejected — marginal over consumed_vars_, not zero-cost

## Consequences

_No consequences documented._
