# Collections

TML provides built-in support for dynamic collections: Lists (dynamic arrays), HashMaps, Buffers, and more. All collection types are implemented in pure TML using memory intrinsics.

## Arrays and Lists

### Array Literal Syntax

The simplest way to create a list is with array literal syntax:

```tml
// Create a list with initial values
let numbers = [1, 2, 3, 4, 5]

// Create an empty list
let empty = []

// Create a list with repeated values
let zeros = [0; 10]  // 10 zeros
let ones = [1; 5]    // 5 ones
```

### Indexing

Access elements using square bracket notation:

```tml
let arr = [10, 20, 30, 40, 50]

let first = arr[0]   // 10
let third = arr[2]   // 30
let last = arr[4]    // 50
```

### List Methods

TML `List[T]` is a growable array with method-call syntax:

```tml
use std::collections::List

var items: List[I64] = List[I64].new(16)  // Create with capacity 16
items.push(10)
items.push(20)
items.push(30)

let len: I64 = items.len()          // 3
let first: I64 = items.get(0)       // 10
items.set(0, 100)                    // items[0] is now 100
let last: I64 = items.pop()         // returns 30
let has: Bool = items.contains(10)   // false (was replaced with 100)
items.clear()                        // empty the list
```

**Common Methods:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `new` | `new(capacity: I64) -> List[T]` | Create with initial capacity |
| `push` | `push(mut this, item: T)` | Append element |
| `pop` | `pop(mut this) -> T` | Remove and return last |
| `get` | `get(this, index: I64) -> T` | Get element at index |
| `set` | `set(mut this, index: I64, value: T)` | Set element at index |
| `len` | `len(this) -> I64` | Number of elements |
| `is_empty` | `is_empty(this) -> Bool` | True if empty |
| `clear` | `clear(mut this)` | Remove all elements |
| `contains` | `contains(this, item: T) -> Bool` | Linear search |

### Vec[T] — Ergonomic Alias

`Vec[T]` is an alias for `List[T]` with the same API:

```tml
use std::collections::Vec

var v: Vec[I32] = Vec[I32].new(8)
v.push(1)
v.push(2)
v.push(3)
```

### Example: Sum of Elements

```tml
func sum_list() -> I32 {
    let arr = [10, 20, 30, 40, 50]
    var total: I32 = 0
    loop i in 0 to arr.len() {
        total = total + arr.get(i)
    }
    return total  // 150
}
```

## HashMap

HashMaps store key-value pairs with O(1) average lookup time. Implemented in pure TML.

```tml
use std::collections::HashMap

var scores = HashMap[Str, I32]::new(16)
scores.set("Alice", 100)
scores.set("Bob", 85)

let s: I32 = scores.get("Alice")      // 100
let has: Bool = scores.has("Charlie")  // false
scores.remove("Bob")
```

**Common Methods:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `new` | `new(capacity: I64) -> HashMap[K, V]` | Create with capacity |
| `set` | `set(mut this, key: K, value: V)` | Insert or update |
| `get` | `get(this, key: K) -> V` | Get value (panics if missing) |
| `has` | `has(this, key: K) -> Bool` | Check if key exists |
| `remove` | `remove(mut this, key: K) -> Bool` | Remove by key |
| `len` | `len(this) -> I64` | Number of entries |
| `is_empty` | `is_empty(this) -> Bool` | True if empty |
| `clear` | `clear(mut this)` | Remove all entries |

## BTreeMap and BTreeSet

Sorted collections using binary search. O(log n) operations with sorted iteration.

```tml
use std::collections::BTreeMap

var m: BTreeMap = BTreeMap::create()
m.insert(3, 30)
m.insert(1, 10)
m.insert(2, 20)

let v: I64 = m.get(1)          // 10
let min: I64 = m.min_key()     // 1
let max: I64 = m.max_key()     // 3
```

```tml
use std::collections::BTreeSet

var s: BTreeSet = BTreeSet::create()
s.insert(5)
s.insert(2)
s.insert(8)

let has: Bool = s.contains(5)  // true
let min: I64 = s.min()         // 2
```

## Deque

Double-ended queue backed by a ring buffer. O(1) push/pop at both ends.

```tml
use std::collections::Deque

var dq: Deque[I64] = Deque::create[I64]()
dq.push_back(1)
dq.push_front(0)

let front: Maybe[I64] = dq.pop_front()  // Just(0)
let back: Maybe[I64] = dq.pop_back()    // Just(1)
```

## Buffer

Byte buffer for binary data. Implemented in pure TML.

```tml
use std::collections::Buffer

var buf = Buffer::new(1024)

// Write bytes
buf.write_u8(0xFF)
buf.write_i32(42)

// Read bytes (advances read position)
let b: U8 = buf.read_u8()
let val: I32 = buf.read_i32()

// Buffer info
let len: I64 = buf.len()
let cap: I64 = buf.capacity()
let rem: I64 = buf.remaining()

// Reset and clear
buf.reset_read()
buf.clear()
```

## Memory Management

All collection types in TML are implemented in pure TML using memory intrinsics (`mem_alloc`, `mem_free`, `ptr_read`, `ptr_write`). Collections that implement the `Drop` behavior are automatically cleaned up when they go out of scope.

For types without automatic Drop, use the appropriate cleanup method:

```tml
var map = HashMap[Str, I32]::new(16)
// ... use the map ...
map.destroy()  // Free memory when done
```
