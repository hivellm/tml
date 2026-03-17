# Maps and Sets

Maps store key-value pairs. Sets store unique values. TML provides both hash-based collections (O(1) average operations) and tree-based collections (O(log n) operations, sorted iteration). This section also covers the specialized collections in the standard library: `Deque`, `Queue`, `Stack`, `BinaryHeap`, `LinkedList`, and `Buffer`.

## HashMap[K, V]

`HashMap[K, V]` stores key-value pairs with O(1) average-case lookup, insertion, and deletion. Keys must implement the `Hash` and `Eq` behaviors. The standard types — `I32`, `I64`, `Str`, `Bool`, and others — implement both.

### Creating a HashMap

```tml
use std::collections::HashMap

// Create with an initial capacity hint
let scores: HashMap[Str, I32] = HashMap[Str, I32].new(16)

// Create with the default capacity
let index: HashMap[I32, Str] = HashMap[I32, Str].default()
```

### Inserting and Retrieving

```tml
use std::collections::HashMap

let scores: HashMap[Str, I32] = HashMap[Str, I32].new(16)

scores.set("alice", 100)
scores.set("bob", 85)
scores.set("charlie", 92)

let alice_score = scores.get("alice")  // 100
let bob_score = scores.get("bob")      // 85
```

`get()` panics if the key does not exist. Check with `has()` before calling `get()` when the key may be absent:

```tml
if scores.has("diana") {
    let s = scores.get("diana")
    println(s.to_string())
} else {
    println("diana not found")
}
```

### Removing Entries

```tml
let removed = scores.remove("bob")  // true if key was present, false if not
scores.has("bob")  // false
```

### Updating Values

Calling `set()` with an existing key replaces the value:

```tml
scores.set("alice", 110)      // update alice's score
let updated = scores.get("alice")  // 110
scores.len()                   // still the same number of entries
```

### Iterating Over a HashMap

Use `iter()` to traverse key-value pairs. The iteration order is not guaranteed.

```tml
use std::collections::{HashMap, HashMapIter}

let map: HashMap[Str, I32] = HashMap[Str, I32].new(16)
map.set("x", 10)
map.set("y", 20)
map.set("z", 30)

let iter: HashMapIter[Str, I32] = map.iter()
loop (iter.has_next()) {
    let k: Str = iter.key()
    let v: I32 = iter.value()
    println(k + " => " + v.to_string())
    iter.next()
}
iter.destroy()
map.destroy()
```

Always call `iter.destroy()` when done to release the iterator's resources.

### HashMap Method Reference

| Method | Description |
|---|---|
| `HashMap[K, V].new(capacity: I64)` | Create with initial capacity hint |
| `HashMap[K, V].default()` | Create with default capacity |
| `set(key: K, value: V)` | Insert or update a key-value pair |
| `get(key: K) -> V` | Return value for key (panics if missing) |
| `has(key: K) -> Bool` | True if the key exists |
| `remove(key: K) -> Bool` | Remove key; returns true if it was present |
| `len() -> I64` | Number of entries |
| `is_empty() -> Bool` | True if no entries |
| `clear()` | Remove all entries, retain allocation |
| `iter() -> HashMapIter[K, V]` | Create an iterator over key-value pairs |
| `destroy()` | Free heap memory |

## HashSet[T]

`HashSet[T]` stores a collection of unique values. Inserting a duplicate is a no-op. Membership tests run in O(1) average time.

```tml
use std::collections::HashSet

let seen: HashSet[I32] = HashSet[I32].new(16)

seen.insert(1)
seen.insert(2)
seen.insert(1)  // duplicate — not inserted

seen.len()        // 2
seen.contains(1)  // true
seen.contains(3)  // false

seen.remove(2)
seen.contains(2)  // false

seen.destroy()
```

### Set Operations

```tml
use std::collections::HashSet

let a: HashSet[I32] = HashSet[I32].new(8)
a.insert(1)
a.insert(2)
a.insert(3)

let b: HashSet[I32] = HashSet[I32].new(8)
b.insert(2)
b.insert(3)
b.insert(4)

// Check containment
let all_in_a = a.contains(1) and a.contains(2) and a.contains(3)  // true

a.destroy()
b.destroy()
```

### HashSet Method Reference

| Method | Description |
|---|---|
| `HashSet[T].new(capacity: I64)` | Create with initial capacity hint |
| `insert(value: T)` | Add a value; no-op if already present |
| `remove(value: T) -> Bool` | Remove a value; returns true if it was present |
| `contains(value: T) -> Bool` | True if the value is in the set |
| `len() -> I64` | Number of unique values |
| `is_empty() -> Bool` | True if no values |
| `clear()` | Remove all values |
| `destroy()` | Free heap memory |

## BTreeMap[K, V]

`BTreeMap[K, V]` is an ordered map backed by a B-tree. Keys must implement `Ord`. Operations run in O(log n) time, but iteration always produces entries in ascending key order.

Use `BTreeMap` when you need:
- Sorted iteration over keys
- Range queries (find all keys between two bounds)
- Minimum or maximum key in O(log n) time

```tml
use std::collections::BTreeMap

let ages: BTreeMap[Str, I32] = BTreeMap[Str, I32].new()

ages.insert("charlie", 35)
ages.insert("alice", 28)
ages.insert("bob", 31)

let alice_age = ages.get("alice")  // 28
let min_key = ages.min_key()       // "alice" (alphabetically first)
let max_key = ages.max_key()       // "charlie"

ages.destroy()
```

Iteration over a `BTreeMap` yields entries in ascending key order — `"alice"` before `"bob"` before `"charlie"` in the example above.

### BTreeMap Method Reference

| Method | Description |
|---|---|
| `BTreeMap[K, V].new()` | Create an empty sorted map |
| `insert(key: K, value: V)` | Insert or update a key-value pair |
| `get(key: K) -> V` | Return value for key (panics if missing) |
| `has(key: K) -> Bool` | True if the key exists |
| `remove(key: K) -> Bool` | Remove key; returns true if present |
| `min_key() -> K` | The smallest key (panics if empty) |
| `max_key() -> K` | The largest key (panics if empty) |
| `len() -> I64` | Number of entries |
| `is_empty() -> Bool` | True if no entries |
| `destroy()` | Free heap memory |

## BTreeSet[T]

`BTreeSet[T]` is an ordered set backed by a B-tree. Values must implement `Ord`. Iteration produces values in ascending order.

```tml
use std::collections::BTreeSet

let priorities: BTreeSet[I32] = BTreeSet[I32].new()

priorities.insert(5)
priorities.insert(2)
priorities.insert(8)
priorities.insert(1)

let min_val = priorities.min()  // 1
let max_val = priorities.max()  // 8

// Iteration produces: 1, 2, 5, 8

priorities.destroy()
```

## Deque[T]

`Deque[T]` (double-ended queue) supports O(1) push and pop at both the front and the back. It is backed by a ring buffer.

```tml
use std::collections::Deque

let dq: Deque[I32] = Deque[I32].new()

dq.push_back(1)
dq.push_back(2)
dq.push_front(0)
// Contents (front to back): 0, 1, 2

let front = dq.pop_front()  // Just(0)
let back = dq.pop_back()    // Just(2)

dq.len()       // 1
dq.is_empty()  // false

dq.destroy()
```

`pop_front()` and `pop_back()` return `Maybe[T]` — `Just(value)` if the deque is non-empty, `Nothing` if it is empty.

### Deque Method Reference

| Method | Description |
|---|---|
| `Deque[T].new()` | Create an empty deque |
| `push_back(value: T)` | Append to the back |
| `push_front(value: T)` | Prepend to the front |
| `pop_back() -> Maybe[T]` | Remove and return back element |
| `pop_front() -> Maybe[T]` | Remove and return front element |
| `peek_front() -> Maybe[ref T]` | View the front element without removing |
| `peek_back() -> Maybe[ref T]` | View the back element without removing |
| `len() -> I64` | Number of elements |
| `is_empty() -> Bool` | True if no elements |
| `destroy()` | Free heap memory |

## Queue[T]

`Queue[T]` is a FIFO (first-in, first-out) queue. Elements are enqueued at the back and dequeued from the front.

```tml
use std::collections::Queue

let jobs: Queue[I32] = Queue[I32].new()

jobs.enqueue(1)
jobs.enqueue(2)
jobs.enqueue(3)

let first = jobs.dequeue()  // Just(1)
let second = jobs.dequeue() // Just(2)

jobs.len()  // 1
jobs.destroy()
```

## Stack[T]

`Stack[T]` is a LIFO (last-in, first-out) stack. Elements are pushed and popped from the top.

```tml
use std::collections::Stack

let history: Stack[Str] = Stack[Str].new()

history.push("page-1")
history.push("page-2")
history.push("page-3")

let top = history.pop()   // Just("page-3")
let next = history.peek() // Just(ref "page-2") — not removed

history.len()  // 2
history.destroy()
```

## BinaryHeap[T]

`BinaryHeap[T]` is a max-heap priority queue. The element with the highest value (by `Ord`) is always at the top and can be extracted in O(log n) time. Use it when you need to repeatedly process the highest-priority item.

```tml
use std::collections::BinaryHeap

let heap: BinaryHeap[I32] = BinaryHeap[I32].new()

heap.push(3)
heap.push(1)
heap.push(4)
heap.push(1)
heap.push(5)

let top = heap.pop()   // Just(5) — always the max
let next = heap.peek() // Just(ref 4)

heap.len()  // 4
heap.destroy()
```

## LinkedList[T]

`LinkedList[T]` is a doubly-linked list. It provides O(1) insertion and removal at arbitrary positions using cursors, but O(n) random access. Use it when you need frequent insertions and deletions in the middle of a sequence.

```tml
use std::collections::LinkedList

let list: LinkedList[I32] = LinkedList[I32].new()

list.push_back(1)
list.push_back(2)
list.push_back(3)
list.push_front(0)

let front = list.pop_front()  // Just(0)
let back = list.pop_back()    // Just(3)

list.len()  // 2
list.destroy()
```

For most use cases, `List[T]` is faster than `LinkedList[T]` due to better cache locality. Prefer `LinkedList[T]` only when you have a measured need for O(1) mid-sequence insertions.

## Buffer

`Buffer` is a byte-level read/write buffer designed for binary data serialization and protocol handling. It maintains separate read and write positions.

```tml
use std::collections::Buffer

let buf = Buffer.new(1024)

// Write typed values
buf.write_u8(0xFF)
buf.write_i32(42)
buf.write_i64(1234567890)

// Read typed values (advances the read position)
let b: U8 = buf.read_u8()    // 0xFF
let i: I32 = buf.read_i32()  // 42
let l: I64 = buf.read_i64()  // 1234567890

// Query state
let length = buf.len()        // number of written bytes
let capacity = buf.capacity() // allocated capacity
let remaining = buf.remaining()  // bytes available to read

// Reset and reuse
buf.reset_read()  // move read position back to start
buf.clear()       // reset both positions, keep allocation

buf.destroy()
```

### Buffer Method Reference

| Method | Description |
|---|---|
| `Buffer.new(capacity: I64)` | Create with initial capacity |
| `write_u8(v: U8)` | Write one unsigned byte |
| `write_i32(v: I32)` | Write four bytes (little-endian) |
| `write_i64(v: I64)` | Write eight bytes (little-endian) |
| `read_u8() -> U8` | Read one unsigned byte |
| `read_i32() -> I32` | Read four bytes as I32 |
| `read_i64() -> I64` | Read eight bytes as I64 |
| `len() -> I64` | Number of written bytes |
| `capacity() -> I64` | Allocated capacity |
| `remaining() -> I64` | Bytes available to read |
| `reset_read()` | Reset read position to start |
| `clear()` | Reset both positions, keep allocation |
| `destroy()` | Free heap memory |
