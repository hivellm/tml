# Collections

A collection is a data structure that holds multiple values under a single name. TML's collection types cover the most common needs: growable lists, key-value maps, sets with fast membership tests, double-ended queues, priority queues, and more. All collection types in the standard library are implemented in pure TML using memory intrinsics.

## Choosing a Collection

The right collection depends on what operations you need to perform efficiently:

| Collection | Use when... |
|---|---|
| `[T; N]` (array) | Size is fixed at compile time; values live on the stack |
| `List[T]` | You need a growable sequence with O(1) append and O(1) indexed access |
| `HashMap[K, V]` | You need fast lookup, insertion, and deletion by key |
| `HashSet[T]` | You need fast membership tests and deduplication |
| `BTreeMap[K, V]` | You need key-value storage with sorted iteration |
| `BTreeSet[T]` | You need sorted membership with range queries |
| `Deque[T]` | You need O(1) push and pop at both the front and the back |
| `Queue[T]` | You need FIFO ordering (first in, first out) |
| `Stack[T]` | You need LIFO ordering (last in, first out) |
| `BinaryHeap[T]` | You need to repeatedly extract the maximum (or minimum) element |
| `LinkedList[T]` | You need O(1) insertion and removal at arbitrary positions |
| `Buffer` | You need a byte-level read/write buffer for binary data |

When in doubt, start with `List[T]`. It is the most versatile general-purpose collection, and you can switch to a more specialized structure later if profiling shows a need.

## Memory Management

Collection types allocate heap memory. When you are finished with a collection, call its `destroy()` method to release that memory. Collections that implement the `Drop` behavior will be released automatically when they go out of scope, but most standard library collections require an explicit call:

```tml
use std::collections::List

func count_words(text: Str) -> I64 {
    let words = List[Str].new(16)
    // ... populate words ...
    let count = words.len()
    words.destroy()   // release memory before returning
    return count
}
```

If you return a collection from a function, the caller is responsible for calling `destroy()`.

## What This Chapter Covers

- **Lists and Arrays** (ch11-01) — fixed-size arrays, `List[T]`, iteration patterns, and common algorithms
- **Maps and Sets** (ch11-02) — `HashMap`, `HashSet`, `BTreeMap`, `BTreeSet`, and the remaining specialized collections
