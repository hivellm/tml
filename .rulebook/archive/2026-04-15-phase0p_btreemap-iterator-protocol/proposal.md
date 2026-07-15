# Proposal: phase0p_btreemap-iterator-protocol

## Why

`BTreeMap[K,V].iter()` returns `BTreeMapIter[K,V]` which does NOT implement the
`Iterator` behavior. This means `for entry in map.iter()` fails with T050 ("For loop
requires slice or collection type"). Developers expect map iteration to work like
list iteration. Currently the only way to iterate is the cursor API
(`iter.has_next()` / `iter.key()` / `iter.value()` / `iter.next()`), which is not
discoverable and is more verbose than necessary. Reported by an external AI agent
as a significant friction point when building database-adjacent code.

## What Changes

- `lib/std/src/collections/btreemap.tml`: implement `Iterator[Pair[K,V]]` behavior
  on `BTreeMapIter[K,V]` with `next() -> Maybe[Pair[K,V]]`.
- `lib/core/src/`: add or verify `Pair[A,B]` type (or use tuple `(K,V)`) for the
  yielded item.
- The existing cursor API is preserved for backward compatibility.
- `for entry in map.iter() { let k = entry.key; let v = entry.value }` should work.

## Impact

- Affected specs: `lib/std/src/collections/btreemap.tml`
- Affected code: `BTreeMapIter` struct + `Iterator` behavior impl
- Breaking change: NO (additive)
- User benefit: Map iteration matches the rest of the stdlib; AI agents and Rust
  developers can iterate maps using the familiar for-in pattern.
