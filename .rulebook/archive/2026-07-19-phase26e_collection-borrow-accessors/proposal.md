# Proposal: phase26e_collection-borrow-accessors

## Why

The 08-memory-copy-audit's central performance finding (F-021): TML collections have
NO working borrowing accessor — `List/HashMap/BTreeMap/Deque.get` return by value and
deep-clone every handle-bearing element via `ptr_read_clone`. `m.get(i)` on a
`List[List[Str]]` allocates `1 + 2m` times for one logical read; Rust returns `&T`
with zero traffic. The sole interior-reference attempt (`IndexMut::index_mut` →
intrinsic `list_get_mut`) has no codegen implementation. This is THE piece that turns
"correct + leak-free" (from phase26b) into "zero-cost" — the actual Rust-parity
performance thesis. Without it, TML pays heap alloc + memcpy on every non-primitive
container read forever.

## What Changes

New language + codegen surface: interior-pointer codegen into the type-erased backing
buffer yielding `ref T`/`mut ref T`; borrow-checker lifetimes binding the reference to
the container borrow (invalidation on push/rehash = compile error); read accessors
`get_ref`/`get_mut`, borrowing iterators `iter_ref`/`values_ref`; implement the dead
`list_get_mut` intrinsic; migrate hot stdlib sites off `get`(clone). Depends on
phase26b's real move/drop model landing first (a container borrow is only sound once
lifetimes are tracked).

## Impact

- Affected specs: collections API (new accessors), ownership/borrow spec section.
- Affected code: `compiler/src/codegen/**` (interior-pointer emission),
  `compiler/src/borrow/**` (container-lifetime facts), `lib/std/src/collections/**`,
  `compiler/src/codegen/**/intrinsics*` (`list_get_mut`).
- Breaking change: NO (additive accessors; existing `get` stays, or becomes a
  documented clone). Iterator semantics may change (F-019 resolution).
- User benefit: the zero-cost read the whole language thesis depends on — Rust-parity
  heap traffic (zero) on collection reads of strings/objects/handles.

## Source

- docs/analysis/tml-table-analysis/08-memory-copy-audit.md (F-021, F-019).
- ADR-009 flagged this as "largest blast radius, deferred". Depends on phase26b.
