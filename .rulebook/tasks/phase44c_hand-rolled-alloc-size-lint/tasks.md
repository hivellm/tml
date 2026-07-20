# phase44c — Lint for hand-rolled allocation sizes that drift from the real type

> Filed 2026-07-19 out of `phase44b` item 1.2b. Both root causes of the
> core/alloc "floating flaky" (fixed in commit 8c8cff09) were the same shape:
> hand-written memory code whose size assumptions had drifted from the actual
> type. Neither was a compiler bug; both silently corrupted the heap for an
> unrelated test and survived for months as "machine load".

## Motivation (the two specimens)

- `mem_alloc(8)` for a `SharedInner`/`SyncInner` that is
  `{value, strong_count, weak_count}` = 12 bytes. The literal `8` was correct
  when written; `weak_count` was added later and the literal was not.
- `shrink()` delegating to `grow()`, which copies `old_layout.size()` — for a
  shrink that copies MORE than the destination holds (64 into 16).

Both are invisible at the call site and produce a failure in a different test.
The generalisation worth catching: a byte count written as a literal, or
derived from the wrong operand, where the type's real size is knowable.

## 1. Implementation
- [x] 1.1 **SURVEYED (2026-07-20).** 433 alloc/copy occurrences scanned (383 lib
  + 50 compiler, all compiler hits in test files; `compiler-tml/` frozen, out of
  scope). Target subset (literal or source-derived size): ~150.
  - **ACTUALLY-WRONG: 0.** Both specimens already fixed — `shared.tml:94-95` /
    `sync.tml:130-131` now use `size_of[SharedInner/SyncInner[T]]()`; every
    surviving source-derived `mem_copy` is a grow or `min`-clamped path
    (`allocator_ref*` shrink now copies `new_layout.size()`; `global.tml:301`
    clamps; `multi_executor.tml:201` is a grow). No live bug remains.
  - **CORRECT-BUT-FRAGILE: 8** — hand-encoded I64-slot buffer headers
    (`list.tml:61` alloc(32), `hashmap.tml:75` alloc(48), `event_loop.tml:39`
    alloc(24), `async_udp.tml:309` alloc(16), `work_stealing.tml:95` alloc(32),
    `byte_stream.tml:79/98/120` + `buffer.tml:247` buf_mem_alloc(32)). These have
    NO named struct `T` in scope, so rule (a) cannot fire on them (0 FP, but also
    no help — the layout lives only in the literal + offset arithmetic).
  - **SAFE: ~140** (size_of / count*size_of / primitive/algorithm scratch).
  - **Verdict:** the exact class the lint targets (named struct + drifted literal)
    has only ever had 2 instances, both fixed and now on `size_of[T]()`. Nothing
    to clean up today; a lint here is purely a **preventive regression guard**.
    Only rule (a) has acceptable signal/noise (~0 FP); (b) is undecidable
    (grow-vs-shrink is a runtime comparison), (c) is high-noise (fires on the
    intentional raw-buffer headers). See 1.2.
- [x] 1.2 **DECIDED (2026-07-20): rule (a).** Flag an integer-literal size
  argument to `mem_alloc`/`mem_realloc` (and `buf_mem_alloc`) when a concrete
  named struct type `T` with a knowable `size_of[T]()` is in scope at the call
  (the allocation result is cast to / bound as `*T` and `T`'s fields are
  written), and the literal does not match `size_of[T]()`. This is exactly the
  shared/sync specimen-A shape.
  - Rejected (b): "dest is smaller than source" is a runtime layout comparison
    (`old_layout` vs `new_layout`) — statically undecidable; would either flag
    every correct grow or need a `shrink`-name heuristic too narrow to generalize.
  - Rejected (c): the 8 fragile sites deliberately allocate raw hand-indexed
    buffers with no named struct; (c) either can't distinguish them from scratch
    or forces high-churn conversions — high false-positive.
  - **False-positive story for (a):** ~0 on the current tree. It fires only when
    a NAMED struct `T` is in scope AND the size is a bare literal AND it mismatches
    `size_of[T]()`. The 8 fragile raw-buffer headers have no `T` in scope → never
    fire. Genuine primitive/scratch literals (`mem_alloc(8)` for one `I64`) have
    no struct `T` → never fire. Honest caveat recorded: true-positive count on
    today's code is **zero** — this is a preventive regression guard; the 1.4
    fixtures are its only live firings. (User go/no-go: BUILD, 2026-07-20.)
- [ ] 1.3 Implement the chosen rule as a lint diagnostic with a stable code, in
  the same place the existing lints live. Fix every true positive from 1.1.
- [ ] 1.4 Verify: the two specimens above (reconstructed as fixtures, since the
  originals are fixed) are caught; zero diagnostics across `lib/core`,
  `lib/std` and `compiler/` once the true positives are fixed.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation (the lint
  code and its rationale, alongside the other lint codes)
- [ ] 2.2 Write tests covering the new behavior (positive fixtures for each
  caught shape + negative fixtures pinning the accepted-correct patterns, so
  the false-positive boundary is executable)
- [ ] 2.3 Run tests and confirm they pass
