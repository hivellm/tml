# phase0b — Fix `drop_in_place`; smart pointers run payload destructors

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` finding L-022
> (02-memory-model-borrow.md). Highest single soundness+perf return in the
> memory model: one intrinsic fix closes the payload-leak class in `Heap`,
> `Shared` and `Sync`, deletes the copy-to-drop pattern, and removes two
> documented deliberate leaks in enum drop-glue.

## Motivation

Runtime probes proved `Heap::new(R)` / `Shared::new(R)` with `impl Drop for R`
never print R's drop message: all three smart pointers `mem_free` the
allocation without invoking the payload destructor, because the one primitive
they need — `drop_in_place[T]` (`lib/core/src/ops/drop.tml:150-157`) — is
codegen-broken (self-recursion / invalid IR; both its test files are stubbed
out). The library routes around it with copy-to-drop: a full `memcpy` of each
element into a local so scope-exit glue fires.

## 1. Implementation
- [ ] 1.1 Root-cause the intrinsic from the two stubbed test files
  (`lib/core/tests/ops/drop_in_place.test.tml`,
  `lib/core/tests/drop/drop_in_place.test.tml`): fix codegen so
  `drop_in_place[T]` expands to T's drop glue at the monomorphized type (no
  self-recursion). Spec reference: docs/specs/22-LOW-LEVEL.md:119.
- [ ] 1.2 Wire payload destruction: `Heap[T]::drop` calls `drop_in_place`
  before `mem_free` (`lib/core/src/alloc/heap.tml:264-271`); `Shared`/`Sync`
  `decrement_count` drops the value when strong hits 0
  (`lib/core/src/alloc/shared.tml:334-353`, sync.tml equivalent).
- [ ] 1.3 Delete copy-to-drop: `List::destroy` / `retain` drop elements via
  `drop_in_place` instead of memcpy-into-local
  (`lib/std/src/collections/list.tml:296-299,354-357`).
- [ ] 1.4 Remove now-redundant compiler special cases: the enum drop-glue
  `Heap__X` hardcode and both documented-leak paths
  (`compiler/src/codegen/llvm/core/drop.cpp:1131-1208,1198-1204,1239-1243`).
- [ ] 1.5 Re-enable both stubbed test files. Probe gate: `Heap::new(R)` and
  `Shared::new(R)` print `drop R payload` exactly once;
  `mcp__tml__debug(check_leaks=true)` clean on the probe corpus;
  `std/collections` standalone 20/20 reruns.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation (drop semantics through pointer indirection;
  remove the "fields may leak" caveats)
- [ ] 2.2 Write tests covering the new behavior (payload-drop fixtures for
  Heap/Shared/Sync incl. nested and enum-payload cases)
- [ ] 2.3 Run tests and confirm they pass
