# Spec: Collections Module

## Overview

Extended collection types beyond the basic `List[T]` and `HashMap[K,V]`.

## Types

### Vec[T]

Alias for `List[T]` with extended methods for Rust compatibility.

```tml
pub type Vec[T] = List[T]

extend Vec[T] {
    pub func new() -> Vec[T]
    pub func with_capacity(cap: U64) -> Vec[T]
    pub func from_iter[I: Iterator[Item = T]](iter: I) -> Vec[T]

    pub func push(this, value: T)
    pub func pop(this) -> Maybe[T]
    pub func get(this, index: U64) -> Maybe[ref T]
    pub func get_mut(this, index: U64) -> Maybe[mut ref T]

    pub func len(this) -> U64
    pub func capacity(this) -> U64
    pub func is_empty(this) -> Bool

    pub func extend[I: Iterator[Item = T]](this, iter: I)
    pub func append(this, other: mut ref Vec[T])
    pub func drain(this, range: Range[U64]) -> Drain[T]
    pub func retain[F: Fn(ref T) -> Bool](this, f: F)
    pub func dedup(this) where T: Eq
    pub func sort(this) where T: Ord
    pub func sort_by[F: Fn(ref T, ref T) -> Ordering](this, f: F)

    pub func first(this) -> Maybe[ref T]
    pub func last(this) -> Maybe[ref T]
    pub func clear(this)
    pub func truncate(this, len: U64)
    pub func resize(this, new_len: U64, value: T) where T: Clone
}
```

### HashSet[T]

Hash-based set with O(1) operations.

```tml
pub type HashSet[T] {
    map: HashMap[T, Unit],
}

extend HashSet[T] where T: Hash + Eq {
    pub func new() -> HashSet[T]
    pub func with_capacity(cap: U64) -> HashSet[T]

    pub func insert(this, value: T) -> Bool  // returns true if new
    pub func remove(this, value: ref T) -> Bool
    pub func contains(this, value: ref T) -> Bool
    pub func get(this, value: ref T) -> Maybe[ref T]

    pub func len(this) -> U64
    pub func is_empty(this) -> Bool
    pub func clear(this)

    // Set operations - return new sets
    pub func union(this, other: ref HashSet[T]) -> HashSet[T] where T: Clone
    pub func intersection(this, other: ref HashSet[T]) -> HashSet[T] where T: Clone
    pub func difference(this, other: ref HashSet[T]) -> HashSet[T] where T: Clone
    pub func symmetric_difference(this, other: ref HashSet[T]) -> HashSet[T] where T: Clone

    // Set predicates
    pub func is_subset(this, other: ref HashSet[T]) -> Bool
    pub func is_superset(this, other: ref HashSet[T]) -> Bool
    pub func is_disjoint(this, other: ref HashSet[T]) -> Bool
}

extend HashSet[T] with Iterator where T: Hash + Eq {
    type Item = ref T
    func next(this) -> Maybe[ref T]
}
```

### BTreeMap[K, V]

Ordered map based on B-tree with O(log n) operations.

```tml
pub type BTreeMap[K, V] {
    root: Maybe[Box[Node[K, V]]],
    len: U64,
}

extend BTreeMap[K, V] where K: Ord {
    pub func new() -> BTreeMap[K, V]

    pub func insert(this, key: K, value: V) -> Maybe[V]
    pub func remove(this, key: ref K) -> Maybe[V]
    pub func get(this, key: ref K) -> Maybe[ref V]
    pub func get_mut(this, key: ref K) -> Maybe[mut ref V]
    pub func contains_key(this, key: ref K) -> Bool

    pub func len(this) -> U64
    pub func is_empty(this) -> Bool
    pub func clear(this)

    // Ordered operations
    pub func first_key_value(this) -> Maybe[(ref K, ref V)]
    pub func last_key_value(this) -> Maybe[(ref K, ref V)]
    pub func range[R: RangeBounds[K]](this, range: R) -> Range[(ref K, ref V)]

    pub func keys(this) -> Keys[K, V]
    pub func values(this) -> Values[K, V]
    pub func iter(this) -> Iter[K, V]
}
```

### BTreeSet[T]

Ordered set based on B-tree.

```tml
pub type BTreeSet[T] {
    map: BTreeMap[T, Unit],
}

extend BTreeSet[T] where T: Ord {
    pub func new() -> BTreeSet[T]

    pub func insert(this, value: T) -> Bool
    pub func remove(this, value: ref T) -> Bool
    pub func contains(this, value: ref T) -> Bool

    pub func len(this) -> U64
    pub func is_empty(this) -> Bool
    pub func clear(this)

    // Ordered operations
    pub func first(this) -> Maybe[ref T]
    pub func last(this) -> Maybe[ref T]
    pub func range[R: RangeBounds[T]](this, range: R) -> Range[ref T]

    // Set operations
    pub func union(this, other: ref BTreeSet[T]) -> BTreeSet[T] where T: Clone
    pub func intersection(this, other: ref BTreeSet[T]) -> BTreeSet[T] where T: Clone
    pub func difference(this, other: ref BTreeSet[T]) -> BTreeSet[T] where T: Clone
}
```

### Deque[T]

Double-ended queue (ring buffer).

```tml
pub type Deque[T] {
    buf: Vec[Maybe[T]],
    head: U64,
    tail: U64,
}

extend Deque[T] {
    pub func new() -> Deque[T]
    pub func with_capacity(cap: U64) -> Deque[T]

    pub func push_front(this, value: T)
    pub func push_back(this, value: T)
    pub func pop_front(this) -> Maybe[T]
    pub func pop_back(this) -> Maybe[T]

    pub func front(this) -> Maybe[ref T]
    pub func back(this) -> Maybe[ref T]
    pub func get(this, index: U64) -> Maybe[ref T]

    pub func len(this) -> U64
    pub func is_empty(this) -> Bool
    pub func clear(this)

    pub func make_contiguous(this) -> mut ref [T]
}
```

## Implementation Notes

- **HashSet**: Implemented as `HashMap[T, Unit]` wrapper
- **BTreeMap**: B-tree with order 6 (max 11 keys per node)
- **BTreeSet**: Implemented as `BTreeMap[T, Unit]` wrapper
- **Deque**: Ring buffer with power-of-2 capacity for fast modulo
