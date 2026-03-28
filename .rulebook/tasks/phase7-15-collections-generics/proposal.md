# Proposal: Collections Generics + Missing Methods — BTreeMap, Deque, BinaryHeap

## Why

BTreeMap and BTreeSet are hardcoded to I64 — not generic. Deque missing insert/remove/iter (15 methods). BinaryHeap missing iter/drain. These collections exist but are incomplete compared to Rust equivalents.

## What Changes

1. Make BTreeMap[K,V] and BTreeSet[T] generic (or document I64-only limitation and add type aliases).
2. Add missing methods to Deque and BinaryHeap.
3. Add iterators to all collections that lack them.

## Impact
- Affected specs: std::collections (BTreeMap, BTreeSet, Deque, BinaryHeap)
- Affected code: `lib/std/src/collections/btreemap.tml`, `deque.tml`, `binary_heap.tml`
- Breaking change: POSSIBLE if BTreeMap signature changes
- User benefit: Generic sorted collections, complete deque operations, iterable heaps
