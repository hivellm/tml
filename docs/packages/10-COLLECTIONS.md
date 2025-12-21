# TML Standard Library: Collections

> `std.collections` — Data structures for storing and organizing data.

## Overview

The collections package provides efficient, generic data structures for common programming patterns. All collections implement standard traits for iteration, comparison, and serialization.

## Import

```tml
import std.collections
import std.collections.{HashMap, Vec, HashSet}
```

---

## Vec[T] — Dynamic Array

A contiguous growable array type with heap-allocated contents.

```tml
public type Vec[T] {
    ptr: *mut T,
    len: U64,
    cap: U64,
}
```

### Constructors

```tml
extend Vec[T] {
    /// Creates an empty Vec
    public func new() -> Vec[T]

    /// Creates a Vec with pre-allocated capacity
    public func with_capacity(capacity: U64) -> Vec[T]

    /// Creates a Vec from a slice
    public func from_slice(slice: &[T]) -> Vec[T]
        where T: Clone

    /// Creates a Vec with n copies of value
    public func repeat(value: T, n: U64) -> Vec[T]
        where T: Clone
}
```

### Basic Operations

```tml
extend Vec[T] {
    /// Returns the number of elements
    public func len(this) -> U64

    /// Returns true if empty
    public func is_empty(this) -> Bool

    /// Returns the capacity
    public func capacity(this) -> U64

    /// Appends an element to the back
    public func push(mut this, value: T)

    /// Removes and returns the last element
    public func pop(mut this) -> Option[T]

    /// Inserts an element at index
    public func insert(mut this, index: U64, value: T)

    /// Removes and returns the element at index
    public func remove(mut this, index: U64) -> T

    /// Clears all elements
    public func clear(mut this)

    /// Reserves capacity for at least `additional` more elements
    public func reserve(mut this, additional: U64)

    /// Shrinks capacity to fit the length
    public func shrink_to_fit(mut this)
}
```

### Access

```tml
extend Vec[T] {
    /// Returns a reference to the element at index
    public func get(this, index: U64) -> Option[&T]

    /// Returns a mutable reference to the element at index
    public func get_mut(mut this, index: U64) -> Option[&mut T]

    /// Returns a reference to the first element
    public func first(this) -> Option[&T]

    /// Returns a reference to the last element
    public func last(this) -> Option[&T]

    /// Returns a slice of the entire vector
    public func as_slice(this) -> &[T]

    /// Returns a mutable slice of the entire vector
    public func as_mut_slice(mut this) -> &mut [T]
}

/// Index operator
implement Index[U64] for Vec[T] {
    type Output = T

    func index(this, idx: U64) -> &T {
        this.get(idx).expect("index out of bounds")
    }
}

implement IndexMut[U64] for Vec[T] {
    func index_mut(mut this, idx: U64) -> &mut T {
        this.get_mut(idx).expect("index out of bounds")
    }
}
```

### Iteration

```tml
extend Vec[T] {
    /// Returns an iterator over references
    public func iter(this) -> VecIter[T]

    /// Returns an iterator over mutable references
    public func iter_mut(mut this) -> VecIterMut[T]

    /// Consumes the Vec and returns an iterator over owned values
    public func into_iter(this) -> VecIntoIter[T]
}

implement IntoIterator for Vec[T] {
    type Item = T
    type Iter = VecIntoIter[T]

    func into_iter(this) -> VecIntoIter[T]
}
```

### Transformations

```tml
extend Vec[T] {
    /// Retains only elements matching the predicate
    public func retain(mut this, predicate: func(&T) -> Bool)

    /// Removes consecutive duplicates
    public func dedup(mut this)
        where T: Eq

    /// Sorts the vector in place
    public func sort(mut this)
        where T: Ord

    /// Sorts with a custom comparator
    public func sort_by(mut this, compare: func(&T, &T) -> Ordering)

    /// Reverses the order of elements
    public func reverse(mut this)

    /// Appends all elements from another Vec
    public func append(mut this, other: mut Vec[T])

    /// Splits off elements from index onwards
    public func split_off(mut this, at: U64) -> Vec[T]

    /// Resizes the Vec to new_len, filling with value if growing
    public func resize(mut this, new_len: U64, value: T)
        where T: Clone
}
```

### Search

```tml
extend Vec[T] {
    /// Returns true if the Vec contains the value
    public func contains(this, value: &T) -> Bool
        where T: Eq

    /// Binary search for a value
    public func binary_search(this, value: &T) -> Result[U64, U64]
        where T: Ord

    /// Finds the first index of a value
    public func position(this, predicate: func(&T) -> Bool) -> Option[U64]
}
```

---

## HashMap[K, V] — Hash Map

A hash table providing O(1) average-case lookups.

```tml
public type HashMap[K, V] {
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
    public func new() -> HashMap[K, V]

    /// Creates a HashMap with pre-allocated capacity
    public func with_capacity(capacity: U64) -> HashMap[K, V]

    /// Creates a HashMap from an iterator of key-value pairs
    public func from_iter[I](iter: I) -> HashMap[K, V]
        where I: Iterator[Item = (K, V)]
}
```

### Basic Operations

```tml
extend HashMap[K, V] where K: Hash + Eq {
    /// Returns the number of entries
    public func len(this) -> U64

    /// Returns true if empty
    public func is_empty(this) -> Bool

    /// Inserts a key-value pair, returning the old value if present
    public func insert(mut this, key: K, value: V) -> Option[V]

    /// Removes a key, returning the value if present
    public func remove(mut this, key: &K) -> Option[V]

    /// Clears all entries
    public func clear(mut this)

    /// Returns true if the key is present
    public func contains_key(this, key: &K) -> Bool
}
```

### Access

```tml
extend HashMap[K, V] where K: Hash + Eq {
    /// Returns a reference to the value for the key
    public func get(this, key: &K) -> Option[&V]

    /// Returns a mutable reference to the value for the key
    public func get_mut(mut this, key: &K) -> Option[&mut V]

    /// Returns the value for the key, or inserts a default
    public func get_or_insert(mut this, key: K, default: V) -> &mut V

    /// Returns the value for the key, or inserts using a function
    public func get_or_insert_with(mut this, key: K, f: func() -> V) -> &mut V

    /// Gets a mutable reference or inserts a default
    public func entry(mut this, key: K) -> Entry[K, V]
}

/// Entry API for in-place manipulation
public type Entry[K, V] = Occupied[K, V] | Vacant[K, V]

extend Entry[K, V] {
    /// Returns a mutable reference to the value, inserting if vacant
    public func or_insert(this, default: V) -> &mut V

    /// Returns a mutable reference, inserting using function if vacant
    public func or_insert_with(this, f: func() -> V) -> &mut V

    /// Modifies the value if occupied
    public func and_modify(this, f: func(&mut V)) -> Entry[K, V]
}
```

### Iteration

```tml
extend HashMap[K, V] where K: Hash + Eq {
    /// Iterator over key-value pairs
    public func iter(this) -> HashMapIter[K, V]

    /// Iterator over keys
    public func keys(this) -> HashMapKeys[K, V]

    /// Iterator over values
    public func values(this) -> HashMapValues[K, V]

    /// Mutable iterator over values
    public func values_mut(mut this) -> HashMapValuesMut[K, V]
}
```

---

## HashSet[T] — Hash Set

A set implemented as a HashMap with unit values.

```tml
public type HashSet[T] {
    map: HashMap[T, Unit],
}
```

### Constructors

```tml
extend HashSet[T] where T: Hash + Eq {
    /// Creates an empty HashSet
    public func new() -> HashSet[T]

    /// Creates a HashSet with pre-allocated capacity
    public func with_capacity(capacity: U64) -> HashSet[T]

    /// Creates a HashSet from an iterator
    public func from_iter[I](iter: I) -> HashSet[T]
        where I: Iterator[Item = T]
}
```

### Basic Operations

```tml
extend HashSet[T] where T: Hash + Eq {
    /// Returns the number of elements
    public func len(this) -> U64

    /// Returns true if empty
    public func is_empty(this) -> Bool

    /// Inserts a value, returning true if it was new
    public func insert(mut this, value: T) -> Bool

    /// Removes a value, returning true if it was present
    public func remove(mut this, value: &T) -> Bool

    /// Returns true if the value is present
    public func contains(this, value: &T) -> Bool

    /// Clears all elements
    public func clear(mut this)
}
```

### Set Operations

```tml
extend HashSet[T] where T: Hash + Eq + Clone {
    /// Returns the union of two sets
    public func union(this, other: &HashSet[T]) -> HashSet[T]

    /// Returns the intersection of two sets
    public func intersection(this, other: &HashSet[T]) -> HashSet[T]

    /// Returns the difference (self - other)
    public func difference(this, other: &HashSet[T]) -> HashSet[T]

    /// Returns the symmetric difference
    public func symmetric_difference(this, other: &HashSet[T]) -> HashSet[T]

    /// Returns true if self is a subset of other
    public func is_subset(this, other: &HashSet[T]) -> Bool

    /// Returns true if self is a superset of other
    public func is_superset(this, other: &HashSet[T]) -> Bool

    /// Returns true if sets have no common elements
    public func is_disjoint(this, other: &HashSet[T]) -> Bool
}
```

---

## BTreeMap[K, V] — Ordered Map

A map based on a B-tree, providing O(log n) operations with sorted keys.

```tml
public type BTreeMap[K, V] {
    root: Option[Box[Node[K, V]]],
    len: U64,
}

type Node[K, V] {
    keys: Vec[K],
    values: Vec[V],
    children: Vec[Box[Node[K, V]]],
}
```

### Constructors

```tml
extend BTreeMap[K, V] where K: Ord {
    /// Creates an empty BTreeMap
    public func new() -> BTreeMap[K, V]

    /// Creates from an iterator of key-value pairs
    public func from_iter[I](iter: I) -> BTreeMap[K, V]
        where I: Iterator[Item = (K, V)]
}
```

### Operations

```tml
extend BTreeMap[K, V] where K: Ord {
    /// Returns the number of entries
    public func len(this) -> U64

    /// Inserts a key-value pair
    public func insert(mut this, key: K, value: V) -> Option[V]

    /// Removes a key
    public func remove(mut this, key: &K) -> Option[V]

    /// Returns a reference to the value
    public func get(this, key: &K) -> Option[&V]

    /// Returns true if the key exists
    public func contains_key(this, key: &K) -> Bool

    /// Returns the first (minimum) key-value pair
    public func first(this) -> Option[(&K, &V)]

    /// Returns the last (maximum) key-value pair
    public func last(this) -> Option[(&K, &V)]

    /// Returns a range iterator
    public func range(this, range: Range[K]) -> BTreeMapRange[K, V]
}
```

---

## BTreeSet[T] — Ordered Set

A set based on a B-tree, providing O(log n) operations with sorted elements.

```tml
public type BTreeSet[T] {
    map: BTreeMap[T, Unit],
}
```

### Operations

```tml
extend BTreeSet[T] where T: Ord {
    /// Creates an empty BTreeSet
    public func new() -> BTreeSet[T]

    /// Inserts a value
    public func insert(mut this, value: T) -> Bool

    /// Removes a value
    public func remove(mut this, value: &T) -> Bool

    /// Returns true if the value exists
    public func contains(this, value: &T) -> Bool

    /// Returns the first (minimum) element
    public func first(this) -> Option[&T]

    /// Returns the last (maximum) element
    public func last(this) -> Option[&T]

    /// Returns a range iterator
    public func range(this, range: Range[T]) -> BTreeSetRange[T]
}
```

---

## LinkedList[T] — Doubly-Linked List

A doubly-linked list with O(1) insertion at both ends.

```tml
public type LinkedList[T] {
    head: Option[Box[Node[T]]],
    tail: *mut Node[T],
    len: U64,
}

type Node[T] {
    value: T,
    next: Option[Box[Node[T]]],
    prev: *mut Node[T],
}
```

### Operations

```tml
extend LinkedList[T] {
    /// Creates an empty LinkedList
    public func new() -> LinkedList[T]

    /// Returns the number of elements
    public func len(this) -> U64

    /// Adds an element to the front
    public func push_front(mut this, value: T)

    /// Adds an element to the back
    public func push_back(mut this, value: T)

    /// Removes and returns the front element
    public func pop_front(mut this) -> Option[T]

    /// Removes and returns the back element
    public func pop_back(mut this) -> Option[T]

    /// Returns a reference to the front element
    public func front(this) -> Option[&T]

    /// Returns a reference to the back element
    public func back(this) -> Option[&T]

    /// Appends another list to the back
    public func append(mut this, other: mut LinkedList[T])

    /// Splits the list at the given index
    public func split_off(mut this, at: U64) -> LinkedList[T]
}
```

---

## VecDeque[T] — Double-Ended Queue

A ring buffer providing O(1) operations at both ends.

```tml
public type VecDeque[T] {
    buf: Vec[T],
    head: U64,
    len: U64,
}
```

### Operations

```tml
extend VecDeque[T] {
    /// Creates an empty VecDeque
    public func new() -> VecDeque[T]

    /// Creates with pre-allocated capacity
    public func with_capacity(capacity: U64) -> VecDeque[T]

    /// Returns the number of elements
    public func len(this) -> U64

    /// Adds an element to the front
    public func push_front(mut this, value: T)

    /// Adds an element to the back
    public func push_back(mut this, value: T)

    /// Removes and returns the front element
    public func pop_front(mut this) -> Option[T]

    /// Removes and returns the back element
    public func pop_back(mut this) -> Option[T]

    /// Returns a reference to the element at index
    public func get(this, index: U64) -> Option[&T]

    /// Rotates elements left by n positions
    public func rotate_left(mut this, n: U64)

    /// Rotates elements right by n positions
    public func rotate_right(mut this, n: U64)

    /// Makes the deque contiguous
    public func make_contiguous(mut this) -> &mut [T]
}
```

---

## BinaryHeap[T] — Priority Queue

A max-heap providing O(log n) push and O(1) peek of the maximum.

```tml
public type BinaryHeap[T] {
    data: Vec[T],
}
```

### Operations

```tml
extend BinaryHeap[T] where T: Ord {
    /// Creates an empty BinaryHeap
    public func new() -> BinaryHeap[T]

    /// Creates from a Vec
    public func from_vec(vec: Vec[T]) -> BinaryHeap[T]

    /// Returns the number of elements
    public func len(this) -> U64

    /// Pushes an element onto the heap
    public func push(mut this, value: T)

    /// Removes and returns the maximum element
    public func pop(mut this) -> Option[T]

    /// Returns a reference to the maximum element
    public func peek(this) -> Option[&T]

    /// Pushes a value and pops the maximum
    public func push_pop(mut this, value: T) -> T

    /// Pops the maximum and pushes a value
    public func replace(mut this, value: T) -> Option[T]

    /// Consumes the heap and returns a sorted Vec
    public func into_sorted_vec(this) -> Vec[T]
}
```

---

## Traits

### Hash

```tml
/// Trait for types that can be hashed
public trait Hash {
    func hash(this, hasher: &mut Hasher)
}

/// Standard hasher interface
public trait Hasher {
    func write(mut this, bytes: &[U8])
    func finish(this) -> U64
}

/// Default hasher using SipHash
public type DefaultHasher {
    // SipHash state
}

implement Hasher for DefaultHasher {
    func write(mut this, bytes: &[U8]) { ... }
    func finish(this) -> U64 { ... }
}
```

### FromIterator

```tml
/// Trait for creating a collection from an iterator
public trait FromIterator[A] {
    func from_iter[I](iter: I) -> This
        where I: IntoIterator[Item = A]
}

implement FromIterator[T] for Vec[T] {
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
public trait Extend[A] {
    func extend[I](mut this, iter: I)
        where I: IntoIterator[Item = A]
}

implement Extend[T] for Vec[T] {
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
import std.collections.Vec

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
    when numbers.binary_search(&2) {
        Ok(index) -> print("Found at index: " + index.to_string()),
        Err(index) -> print("Would insert at: " + index.to_string()),
    }
}
```

### Using HashMap

```tml
import std.collections.HashMap

func example_hashmap() {
    var scores: HashMap[String, I32] = HashMap.new()

    // Insert
    scores.insert("Alice", 100)
    scores.insert("Bob", 85)
    scores.insert("Charlie", 92)

    // Access
    when scores.get(&"Alice") {
        Some(score) -> print("Alice: " + score.to_string()),
        None -> print("Alice not found"),
    }

    // Entry API
    scores.entry("Dave")
        .or_insert(0)
        .add_assign(10)

    // Iterate
    loop (name, score) in scores.iter() {
        print(name + ": " + score.to_string())
    }
}
```

### Using BTreeMap for Ranges

```tml
import std.collections.BTreeMap

func example_btreemap() {
    var events: BTreeMap[DateTime, String] = BTreeMap.new()

    events.insert(DateTime.parse("2024-01-01"), "New Year")
    events.insert(DateTime.parse("2024-07-04"), "Independence Day")
    events.insert(DateTime.parse("2024-12-25"), "Christmas")

    // Get events in date range
    let start = DateTime.parse("2024-06-01")
    let end = DateTime.parse("2024-12-31")

    loop (date, name) in events.range(start..end) {
        print(date.to_string() + ": " + name)
    }
}
```

### Using BinaryHeap

```tml
import std.collections.BinaryHeap

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
import std.alloc.Arena

let arena = Arena.new(1024 * 1024)  // 1MB arena
var vec: Vec[I32, Arena] = Vec.new_in(&arena)
```

### Thread Safety

Collections are not thread-safe by default. For concurrent access, use:
- `Arc[Mutex[HashMap[K, V]]]` for exclusive access
- `Arc[RwLock[HashMap[K, V]]]` for read-heavy workloads
- Concurrent collections from `std.sync.concurrent`

### Capacity Growth

Dynamic collections grow by doubling capacity when full:
- Vec: 0 → 4 → 8 → 16 → 32 → ...
- HashMap: Load factor of 0.75 triggers resize

---

## See Also

- [std.iter](./11-ITER.md) — Iterator traits and adapters
- [std.alloc](./12-ALLOC.md) — Memory allocators
- [22-LOW-LEVEL.md](../specs/22-LOW-LEVEL.md) — Raw memory operations
