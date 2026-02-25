# TML Standard Library: Collections

> `std::collections` — Data structures for storing and organizing data.

> **Implementation Status (2026-02-24)**: `List[T]`, `HashMap[K,V]`, `HashSet[T]`, `Buffer`, `BTreeMap[K,V]`, `BTreeSet[T]`, `Deque[T]`, `Vec[T]` (alias for List), `ArrayList[T]`, `RingBuf[T]`, `BitSet` are all implemented in **pure TML** (no C runtime dependency). HashMap uses FNV-1a hashing with **content-based comparison** for Str keys (deep copy on insert, byte-by-byte equality). Validated with 10K+ entry scale tests.

## Overview

The collections package provides efficient, generic data structures for common programming patterns. All collections are implemented in pure TML using `core::intrinsics` (ptr_read, ptr_write, mem_alloc, mem_free).

## Import

```tml
use std::collections
use std::collections.{HashMap, Vec, HashSet}
```

---

## Vec[T] — Dynamic Array

A contiguous growable array type with heap-allocated contents.

```tml
pub type Vec[T] {
    ptr: *mut T,
    len: U64,
    cap: U64,
}
```

### Constructors

```tml
extend Vec[T] {
    /// Creates an empty Vec
    pub func new() -> Vec[T]

    /// Creates a Vec with pre-allocated capacity
    pub func with_capacity(capacity: U64) -> Vec[T]

    /// Creates a Vec from a slice
    pub func from_slice(slice: ref [T]) -> Vec[T]
        where T: Duplicate

    /// Creates a Vec with n copies of value
    pub func repeat(value: T, n: U64) -> Vec[T]
        where T: Duplicate
}
```

### Basic Operations

```tml
extend Vec[T] {
    /// Returns the number of elements
    pub func len(this) -> U64

    /// Returns true if empty
    pub func is_empty(this) -> Bool

    /// Returns the capacity
    pub func capacity(this) -> U64

    /// Appends an element to the back
    pub func push(mut this, value: T)

    /// Removes and returns the last element
    pub func pop(mut this) -> Maybe[T]

    /// Inserts an element at index
    pub func insert(mut this, index: U64, value: T)

    /// Removes and returns the element at index
    pub func remove(mut this, index: U64) -> T

    /// Clears all elements
    pub func clear(mut this)

    /// Reserves capacity for at least `additional` more elements
    pub func reserve(mut this, additional: U64)

    /// Shrinks capacity to fit the length
    pub func shrink_to_fit(mut this)
}
```

### Access

```tml
extend Vec[T] {
    /// Returns a reference to the element at index
    pub func get(this, index: U64) -> Maybe[ref T]

    /// Returns a mutable reference to the element at index
    pub func get_mut(mut this, index: U64) -> Maybe[mut ref T]

    /// Returns a reference to the first element
    pub func first(this) -> Maybe[ref T]

    /// Returns a reference to the last element
    pub func last(this) -> Maybe[ref T]

    /// Returns a slice of the entire vector
    pub func as_slice(this) -> ref [T]

    /// Returns a mutable slice of the entire vector
    pub func as_mut_slice(mut this) -> mut ref [T]
}

/// Index operator
extend Vec[T] with Index[U64] {
    type Output = T

    func index(this, idx: U64) -> ref T {
        this.get(idx).expect("index out of bounds")
    }
}

extend Vec[T] with IndexMut[U64] {
    func index_mut(mut this, idx: U64) -> mut ref T {
        this.get_mut(idx).expect("index out of bounds")
    }
}
```

### Iteration

```tml
extend Vec[T] {
    /// Returns an iterator over references
    pub func iter(this) -> VecIter[T]

    /// Returns an iterator over mutable references
    pub func iter_mut(mut this) -> VecIterMut[T]

    /// Consumes the Vec and returns an iterator over owned values
    pub func into_iter(this) -> VecIntoIter[T]
}

extend Vec[T] with IntoIterator {
    type Item = T
    type Iter = VecIntoIter[T]

    func into_iter(this) -> VecIntoIter[T]
}
```

### Transformations

```tml
extend Vec[T] {
    /// Retains only elements matching the predicate
    pub func retain(mut this, predicate: func(ref T) -> Bool)

    /// Removes consecutive duplicates
    pub func dedup(mut this)
        where T: Eq

    /// Sorts the vector in place
    pub func sort(mut this)
        where T: Ord

    /// Sorts with a custom comparator
    pub func sort_by(mut this, compare: func(ref T, ref T) -> Ordering)

    /// Reverses the order of elements
    pub func reverse(mut this)

    /// Appends all elements from another Vec
    pub func append(mut this, other: mut Vec[T])

    /// Splits off elements from index onwards
    pub func split_off(mut this, at: U64) -> Vec[T]

    /// Resizes the Vec to new_len, filling with value if growing
    pub func resize(mut this, new_len: U64, value: T)
        where T: Duplicate
}
```

### Search

```tml
extend Vec[T] {
    /// Returns true if the Vec contains the value
    pub func contains(this, value: ref T) -> Bool
        where T: Eq

    /// Binary search for a value
    pub func binary_search(this, value: ref T) -> Outcome[U64, U64]
        where T: Ord

    /// Finds the first index of a value
    pub func position(this, predicate: func(ref T) -> Bool) -> Maybe[U64]
}
```

---

## HashMap[K, V] — Hash Map

A hash table providing O(1) average-case lookups.

```tml
pub type HashMap[K, V] {
    buckets: Vec[Bucket[K, V]],
    len: U64,
}

type Bucket[K, V] = Vec[Entry[K, V]]

type Entry[K, V] {
    key: K,
    value: V,
    hash: U64,
}
```

### Constructors

```tml
extend HashMap[K, V] where K: Hash + Eq {
    /// Creates an empty HashMap
    pub func new() -> HashMap[K, V]

    /// Creates a HashMap with pre-allocated capacity
    pub func with_capacity(capacity: U64) -> HashMap[K, V]

    /// Creates a HashMap from an iterator of key-value pairs
    pub func from_iter[I](iter: I) -> HashMap[K, V]
        where I: Iterator[Item = (K, V)]
}
```

### Basic Operations

```tml
extend HashMap[K, V] where K: Hash + Eq {
    /// Returns the number of entries
    pub func len(this) -> U64

    /// Returns true if empty
    pub func is_empty(this) -> Bool

    /// Inserts a key-value pair, returning the old value if present
    pub func insert(mut this, key: K, value: V) -> Maybe[V]

    /// Removes a key, returning the value if present
    pub func remove(mut this, key: ref K) -> Maybe[V]

    /// Clears all entries
    pub func clear(mut this)

    /// Returns true if the key is present
    pub func contains_key(this, key: ref K) -> Bool
}
```

### Access

```tml
extend HashMap[K, V] where K: Hash + Eq {
    /// Returns a reference to the value for the key
    pub func get(this, key: ref K) -> Maybe[ref V]

    /// Returns a mutable reference to the value for the key
    pub func get_mut(mut this, key: ref K) -> Maybe[mut ref V]

    /// Returns the value for the key, or inserts a default
    pub func get_or_insert(mut this, key: K, default: V) -> mut ref V

    /// Returns the value for the key, or inserts using a function
    pub func get_or_insert_with(mut this, key: K, f: func() -> V) -> mut ref V

    /// Gets a mutable reference or inserts a default
    pub func entry(mut this, key: K) -> Entry[K, V]
}

/// Entry API for in-place manipulation
pub type Entry[K, V] = Occupied[K, V] | Vacant[K, V]

extend Entry[K, V] {
    /// Returns a mutable reference to the value, inserting if vacant
    pub func or_insert(this, default: V) -> mut ref V

    /// Returns a mutable reference, inserting using function if vacant
    pub func or_insert_with(this, f: func() -> V) -> mut ref V

    /// Modifies the value if occupied
    pub func and_modify(this, f: func(mut ref V)) -> Entry[K, V]
}
```

### Iteration

```tml
extend HashMap[K, V] where K: Hash + Eq {
    /// Iterator over key-value pairs
    pub func iter(this) -> HashMapIter[K, V]

    /// Iterator over keys
    pub func keys(this) -> HashMapKeys[K, V]

    /// Iterator over values
    pub func values(this) -> HashMapValues[K, V]

    /// Mutable iterator over values
    pub func values_mut(mut this) -> HashMapValuesMut[K, V]
}
```

---

## HashSet[T] — Hash Set

A set implemented as a HashMap with unit values.

```tml
pub type HashSet[T] {
    map: HashMap[T, Unit],
}
```

### Constructors

```tml
extend HashSet[T] where T: Hash + Eq {
    /// Creates an empty HashSet
    pub func new() -> HashSet[T]

    /// Creates a HashSet with pre-allocated capacity
    pub func with_capacity(capacity: U64) -> HashSet[T]

    /// Creates a HashSet from an iterator
    pub func from_iter[I](iter: I) -> HashSet[T]
        where I: Iterator[Item = T]
}
```

### Basic Operations

```tml
extend HashSet[T] where T: Hash + Eq {
    /// Returns the number of elements
    pub func len(this) -> U64

    /// Returns true if empty
    pub func is_empty(this) -> Bool

    /// Inserts a value, returning true if it was new
    pub func insert(mut this, value: T) -> Bool

    /// Removes a value, returning true if it was present
    pub func remove(mut this, value: ref T) -> Bool

    /// Returns true if the value is present
    pub func contains(this, value: ref T) -> Bool

    /// Clears all elements
    pub func clear(mut this)
}
```

### Set Operations

```tml
extend HashSet[T] where T: Hash + Eq + Duplicate {
    /// Returns the union of two sets
    pub func union(this, other: ref HashSet[T]) -> HashSet[T]

    /// Returns the intersection of two sets
    pub func intersection(this, other: ref HashSet[T]) -> HashSet[T]

    /// Returns the difference (self - other)
    pub func difference(this, other: ref HashSet[T]) -> HashSet[T]

    /// Returns the symmetric difference
    pub func symmetric_difference(this, other: ref HashSet[T]) -> HashSet[T]

    /// Returns true if self is a subset of other
    pub func is_subset(this, other: ref HashSet[T]) -> Bool

    /// Returns true if self is a superset of other
    pub func is_superset(this, other: ref HashSet[T]) -> Bool

    /// Returns true if sets have no common elements
    pub func is_disjoint(this, other: ref HashSet[T]) -> Bool
}
```

---

## BTreeMap[K, V] — Ordered Map

A map based on a B-tree, providing O(log n) operations with sorted keys.

```tml
pub type BTreeMap[K, V] {
    root: Maybe[Heap[Node[K, V]]],
    len: U64,
}

type Node[K, V] {
    keys: Vec[K],
    values: Vec[V],
    children: Vec[Heap[Node[K, V]]],
}
```

### Constructors

```tml
extend BTreeMap[K, V] where K: Ord {
    /// Creates an empty BTreeMap
    pub func new() -> BTreeMap[K, V]

    /// Creates from an iterator of key-value pairs
    pub func from_iter[I](iter: I) -> BTreeMap[K, V]
        where I: Iterator[Item = (K, V)]
}
```

### Operations

```tml
extend BTreeMap[K, V] where K: Ord {
    /// Returns the number of entries
    pub func len(this) -> U64

    /// Inserts a key-value pair
    pub func insert(mut this, key: K, value: V) -> Maybe[V]

    /// Removes a key
    pub func remove(mut this, key: ref K) -> Maybe[V]

    /// Returns a reference to the value
    pub func get(this, key: ref K) -> Maybe[ref V]

    /// Returns true if the key exists
    pub func contains_key(this, key: ref K) -> Bool

    /// Returns the first (minimum) key-value pair
    pub func first(this) -> Maybe[(ref K, ref V)]

    /// Returns the last (maximum) key-value pair
    pub func last(this) -> Maybe[(ref K, ref V)]

    /// Returns a range iterator
    pub func range(this, range: Range[K]) -> BTreeMapRange[K, V]
}
```

---

## BTreeSet[T] — Ordered Set

A set based on a B-tree, providing O(log n) operations with sorted elements.

```tml
pub type BTreeSet[T] {
    map: BTreeMap[T, Unit],
}
```

### Operations

```tml
extend BTreeSet[T] where T: Ord {
    /// Creates an empty BTreeSet
    pub func new() -> BTreeSet[T]

    /// Inserts a value
    pub func insert(mut this, value: T) -> Bool

    /// Removes a value
    pub func remove(mut this, value: ref T) -> Bool

    /// Returns true if the value exists
    pub func contains(this, value: ref T) -> Bool

    /// Returns the first (minimum) element
    pub func first(this) -> Maybe[ref T]

    /// Returns the last (maximum) element
    pub func last(this) -> Maybe[ref T]

    /// Returns a range iterator
    pub func range(this, range: Range[T]) -> BTreeSetRange[T]
}
```

---

## LinkedList[T] — Doubly-Linked List

A doubly-linked list with O(1) insertion at both ends.

```tml
pub type LinkedList[T] {
    head: Maybe[Heap[Node[T]]],
    tail: *mut Node[T],
    len: U64,
}

type Node[T] {
    value: T,
    next: Maybe[Heap[Node[T]]],
    prev: *mut Node[T],
}
```

### Operations

```tml
extend LinkedList[T] {
    /// Creates an empty LinkedList
    pub func new() -> LinkedList[T]

    /// Returns the number of elements
    pub func len(this) -> U64

    /// Adds an element to the front
    pub func push_front(mut this, value: T)

    /// Adds an element to the back
    pub func push_back(mut this, value: T)

    /// Removes and returns the front element
    pub func pop_front(mut this) -> Maybe[T]

    /// Removes and returns the back element
    pub func pop_back(mut this) -> Maybe[T]

    /// Returns a reference to the front element
    pub func front(this) -> Maybe[ref T]

    /// Returns a reference to the back element
    pub func back(this) -> Maybe[ref T]

    /// Appends another list to the back
    pub func append(mut this, other: mut LinkedList[T])

    /// Splits the list at the given index
    pub func split_off(mut this, at: U64) -> LinkedList[T]
}
```

---

## VecDeque[T] — Double-Ended Queue

A ring buffer providing O(1) operations at both ends.

```tml
pub type VecDeque[T] {
    buf: Vec[T],
    head: U64,
    len: U64,
}
```

### Operations

```tml
extend VecDeque[T] {
    /// Creates an empty VecDeque
    pub func new() -> VecDeque[T]

    /// Creates with pre-allocated capacity
    pub func with_capacity(capacity: U64) -> VecDeque[T]

    /// Returns the number of elements
    pub func len(this) -> U64

    /// Adds an element to the front
    pub func push_front(mut this, value: T)

    /// Adds an element to the back
    pub func push_back(mut this, value: T)

    /// Removes and returns the front element
    pub func pop_front(mut this) -> Maybe[T]

    /// Removes and returns the back element
    pub func pop_back(mut this) -> Maybe[T]

    /// Returns a reference to the element at index
    pub func get(this, index: U64) -> Maybe[ref T]

    /// Rotates elements left by n positions
    pub func rotate_left(mut this, n: U64)

    /// Rotates elements right by n positions
    pub func rotate_right(mut this, n: U64)

    /// Makes the deque contiguous
    pub func make_contiguous(mut this) -> mut ref [T]
}
```

---

## BinaryHeap[T] — Priority Queue

A max-heap providing O(log n) push and O(1) peek of the maximum.

```tml
pub type BinaryHeap[T] {
    data: Vec[T],
}
```

### Operations

```tml
extend BinaryHeap[T] where T: Ord {
    /// Creates an empty BinaryHeap
    pub func new() -> BinaryHeap[T]

    /// Creates from a Vec
    pub func from_vec(vec: Vec[T]) -> BinaryHeap[T]

    /// Returns the number of elements
    pub func len(this) -> U64

    /// Pushes an element onto the heap
    pub func push(mut this, value: T)

    /// Removes and returns the maximum element
    pub func pop(mut this) -> Maybe[T]

    /// Returns a reference to the maximum element
    pub func peek(this) -> Maybe[ref T]

    /// Pushes a value and pops the maximum
    pub func push_pop(mut this, value: T) -> T

    /// Pops the maximum and pushes a value
    pub func replace(mut this, value: T) -> Maybe[T]

    /// Consumes the heap and returns a sorted Vec
    pub func into_sorted_vec(this) -> Vec[T]
}
```

---

## Traits

### Hash

```tml
/// Trait for types that can be hashed
pub behavior Hash {
    func hash(this, hasher: mut ref Hasher)
}

/// Standard hasher interface
pub behavior Hasher {
    func write(mut this, bytes: ref [U8])
    func finish(this) -> U64
}

/// Default hasher using SipHash
pub type DefaultHasher {
    // SipHash state
}

extend DefaultHasher with Hasher {
    func write(mut this, bytes: ref [U8]) { ... }
    func finish(this) -> U64 { ... }
}
```

### FromIterator

```tml
/// Trait for creating a collection from an iterator
pub behavior FromIterator[A] {
    func from_iter[I](iter: I) -> This
        where I: IntoIterator[Item = A]
}

extend Vec[T] with FromIterator[T] {
    func from_iter[I](iter: I) -> Vec[T]
        where I: IntoIterator[Item = T]
    {
        let result = Vec.new()
        loop item in iter {
            result.push(item)
        }
        return result
    }
}
```

### Extend

```tml
/// Trait for extending a collection with an iterator
pub behavior Extend[A] {
    func extend[I](mut this, iter: I)
        where I: IntoIterator[Item = A]
}

extend Vec[T] with Extend[T] {
    func extend[I](mut this, iter: I)
        where I: IntoIterator[Item = T]
    {
        loop item in iter {
            this.push(item)
        }
    }
}
```

---

## Examples

### Using Vec

```tml
use std::collections.Vec

func example_vec() {
    // Create and populate
    var numbers = Vec.new()
    numbers.push(1)
    numbers.push(2)
    numbers.push(3)

    // Iterate
    loop n in numbers.iter() {
        print(n)
    }

    // Transform
    let doubled: Vec[I32] = numbers.iter()
        .map(do(n) n * 2)
        .collect()

    // Sort
    numbers.sort()

    // Binary search
    when numbers.binary_search(ref 2) {
        Ok(index) -> print("Found at index: " + index.to_string()),
        Err(index) -> print("Would insert at: " + index.to_string()),
    }
}
```

### Using HashMap

```tml
use std::collections.HashMap

func example_hashmap() {
    var scores = HashMap[Str, I32].new(16)

    // Insert key-value pairs
    scores.set("Alice", 100)
    scores.set("Bob", 85)
    scores.set("Charlie", 92)

    // Access (get returns V directly, zero value if missing)
    let alice_score = scores.get("Alice")
    print("Alice: " + alice_score.to_string())

    // Check existence
    if scores.has("Bob") {
        print("Bob is in the map")
    }

    // Remove
    scores.remove("Charlie")

    // Iterate using iterator
    var it = scores.iter()
    loop it.has_next() {
        print(it.key() as Str + ": " + (it.value() as I32).to_string())
        it.next()
    }
    it.destroy()

    // Cleanup
    scores.destroy()
}
```

### Using BTreeMap for Ranges

```tml
use std::collections.BTreeMap

func example_btreemap() {
    var events: BTreeMap[DateTime, Str] = BTreeMap.new()

    events.insert(DateTime.parse("2024-01-01"), "New Year")
    events.insert(DateTime.parse("2024-07-04"), "Independence Day")
    events.insert(DateTime.parse("2024-12-25"), "Christmas")

    // Get events in date range
    let start = DateTime.parse("2024-06-01")
    let end = DateTime.parse("2024-12-31")

    loop (date, name) in events.range(start to end) {
        print(date.to_string() + ": " + name)
    }
}
```

### Using BinaryHeap

```tml
use std::collections.BinaryHeap

func example_heap() {
    var heap: BinaryHeap[I32] = BinaryHeap.new()

    heap.push(3)
    heap.push(1)
    heap.push(4)
    heap.push(1)
    heap.push(5)

    // Elements come out in descending order
    loop heap.len() > 0 {
        let max = heap.pop().unwrap()
        print(max)  // 5, 4, 3, 1, 1
    }
}
```

---

## Implementation Notes

### Memory Layout

All collections use the global allocator by default. Custom allocators can be specified:

```tml
use std::alloc.Arena

let arena = Arena.new(1024 * 1024)  // 1MB arena
var vec: Vec[I32, Arena] = Vec.new_in(ref arena)
```

### Thread Safety

Collections are not thread-safe by default. For concurrent access, use:
- `Arc[Mutex[HashMap[K, V]]]` for exclusive access
- `Arc[RwLock[HashMap[K, V]]]` for read-heavy workloads
- Concurrent collections from `std::sync.concurrent`

### Capacity Growth

Dynamic collections grow by doubling capacity when full:
- Vec: 0 → 4 → 8 → 16 → 32 → ...
- HashMap: Load factor of 0.75 triggers resize

---

## See Also

- [std.iter](./11-ITER.md) — Iterator traits and adapters
- [std.alloc](./12-ALLOC.md) — Memory allocators
- [22-LOW-LEVEL.md](../specs/22-LOW-LEVEL.md) — Raw memory operations
