# Proposal: phase0j_hashmap-simd-stdlib-hotpaths

## Why
The most-used collection is a known 2.25× slower than the code already written
for it (SIMD scan disabled by a codegen bug), sort degrades quadratically on
sorted input, and Text/Buffer hot paths double-allocate or heap-roundtrip per
element (analysis L-081..L-087).

## What Changes
Fix the generic+SIMD codegen bug and re-enable group-scan; tombstone rehash;
introsort; Text uses the O(1) length cache and single-allocation transforms;
Buffer bit-casts floats and bulk-copies bytes.

## Impact
- Affected specs: stdlib docs
- Affected code: compiler SIMD-in-lowlevel codegen; lib/std/src/collections/{hashmap,list,buffer}.tml; lib/std/src/text.tml
- Breaking change: NO (semantics preserved; complexity/perf change)
- User benefit: measured ~2× on HashMap, no more O(n²) sort cliff, materially faster string/serialization paths; SIMD unblocked in all generic code
