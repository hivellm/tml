# phase0j — HashMap SIMD probing + stdlib hot-path fixes

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` findings
> L-081/L-082/L-085/L-086/L-087 (05-stdlib-implementation.md). The headline
> item unblocks SIMD in ALL generic library code; the rest are mechanical
> high-ROI constant-factor fixes.

## Motivation

HashMap pays the full Swiss-Table memory cost but probes control bytes one at a
time; its own comment records the SIMD group-scan works and measures **4ns vs
9ns** but is disabled by "generic instantiation has codegen bugs with SIMD
intrinsics inside lowlevel blocks" (`lib/std/src/collections/hashmap.tml:223-225`).
Tombstones are never reclaimed (O(capacity) miss cliff under churn).
`List::sort` is a last-pivot quicksort — O(n²) + O(n) recursion on *sorted*
input. `Text` re-declares raw `strlen` (bypassing the O(1) length cache) and
double-allocates every transform; `Buffer` mallocs+frees per float and
stringifies via a call per byte.

## 1. Implementation
- [ ] 1.1 Root-cause and fix the generic-instantiation × SIMD-intrinsics-in-
  `lowlevel` codegen bug (repro directly from hashmap's disabled group-scan
  path).
- [ ] 1.2 Re-enable SIMD group-scan in `get`/`set`/`has`/`remove`; map
  micro-bench confirms ~2× (project's own 4ns-vs-9ns numbers).
- [ ] 1.3 Tombstone accounting: resize/rehash trigger counts live+deleted
  (`hashmap.tml:118`); churn fixture (insert/remove cycling) no longer
  degrades toward O(capacity) misses.
- [ ] 1.4 `List::sort` → introsort: median-of-three pivot, insertion sort for
  small runs, heapsort past a 2·log2(n) depth limit, recurse smaller half
  first (`list.tml:680-721`). Fixtures: sorted, reverse-sorted, all-equal,
  10M-element sorted input with no stack overflow.
- [ ] 1.5 `Text`: use `core::str::len` (`tml_str_len`) instead of raw `strlen`
  (`text.tml:60-69` + call sites); transforms become single-allocation via a
  `text_from_raw_owned(buf, len)` that adopts the buffer
  (to_upper/to_lower/reverse/repeat/pad_start/pad_end/replace); route substring
  search through the same `memchr`/`memcmp` path Buffer uses.
- [ ] 1.6 `Buffer`: float read/write via stack bitcast, not a heap round-trip
  (`buffer.tml:683-774`); `to/from_string`/`to/from_hex` via bulk copy instead
  of per-byte calls; big-endian integers via the existing bswap helpers.
- [ ] 1.7 Bench each sub-item before/after; record the numbers in this file.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation (HashMap probing/rehash notes; sort complexity
  guarantees; Buffer/Text perf notes)
- [ ] 2.2 Write tests covering the new behavior (churn fixture, sort adversarial
  fixtures, transform correctness incl. non-ASCII pass-through, float
  round-trip)
- [ ] 2.3 Run tests and confirm they pass
