# Smart Pointers

The basic ownership model — one owner, automatic drop — covers most situations.
But some programs require patterns that do not map neatly onto single ownership:

- A recursive data structure (tree, linked list) where a node must own its children
  but the size of the children is not known at compile time.
- Data that needs to be shared among several independent parts of a program, where
  no single part should outlive the others.
- Shared mutable data with borrow-checking deferred to runtime rather than compile
  time.

Smart pointer types address these situations. They are ordinary TML structs that
implement `Drop` to clean up their allocations, and they carry additional semantics
on top of raw ownership.

## Heap[T] — Single Owner on the Heap

`Heap[T]` places a single value on the heap and gives you unique ownership of it.
It is the equivalent of Rust's `Box<T>`.

The most common use case is a recursive type. If a struct contains itself directly,
the compiler cannot compute its size (it would be infinite). Wrapping the
self-referential field in `Heap` breaks the cycle by placing the inner value at
a fixed-size heap pointer:

```tml
use std::alloc::Heap

type Node {
    value: I32,
    next: Maybe[Heap[Node]],   // Heap[Node] has a known size: one pointer
}

func main() {
    let list = Node {
        value: 1,
        next: Just(Heap::new(Node {
            value: 2,
            next: Nothing,
        })),
    }
    println(list.value.to_string())              // 1
    println(list.next.unwrap().deref().value.to_string())  // 2
}
// Both nodes are freed when list goes out of scope
```

### Creating and Using Heap[T]

```tml
use std::alloc::Heap

// Allocate a single I32 on the heap
let boxed: Heap[I32] = Heap::new(42)

// Access the inner value
let v: ref I32 = boxed.deref()
println(v.to_string())   // 42

// Move the value back off the heap
let raw: I32 = boxed.into_inner()
// boxed is consumed; raw owns the I32 value on the stack
```

`Heap[T]` implements `Drop`: when the `Heap` goes out of scope, the heap allocation
is freed. There is never a need to call a free function manually.

## Shared[T] — Reference-Counted Sharing

`Shared[T]` provides shared ownership through reference counting. Each call to
`.duplicate()` increments the count; each drop decrements it. When the count
reaches zero, the allocation is freed. `Shared[T]` is the equivalent of Rust's
`Rc<T>`.

`Shared[T]` is **not thread-safe**. Use it only when shared data lives within
a single thread.

```tml
use std::sync::Shared

let a: Shared[I32] = Shared::new(42)
let b: Shared[I32] = a.duplicate()   // refcount: 2
let c: Shared[I32] = a.duplicate()   // refcount: 3

println(a.deref().to_string())   // 42
println(b.deref().to_string())   // 42 — same allocation
println(c.deref().to_string())   // 42

// c goes out of scope here — refcount: 2
// b goes out of scope here — refcount: 1
// a goes out of scope here — refcount: 0 — heap allocation freed
```

### Reference Cycles

Reference counting cannot free cycles. If two `Shared[T]` values hold references
to each other, neither will ever reach a count of zero. Use `Weak[T]` (described
below) to break cycles.

## Sync[T] — Atomic Reference-Counted Sharing

`Sync[T]` is the thread-safe version of `Shared[T]`. It uses atomic operations
to update the reference count, making it safe to share across threads. It is the
equivalent of Rust's `Arc<T>`.

```tml
use std::sync::Sync
use std::thread

let arc: Sync[I32] = Sync::new(100)
let arc2: Sync[I32] = arc.duplicate()   // safe to send to another thread

let handle = thread::spawn(do() {
    println(arc2.deref().to_string())    // may run concurrently
})

println(arc.deref().to_string())
handle.join()
// Both arc and arc2 are dropped — the allocation is freed
```

The atomic increment and decrement cost slightly more than `Shared[T]`'s plain
integer operations. Prefer `Shared[T]` when the data is confined to one thread
and `Sync[T]` when it crosses thread boundaries.

### Shared Mutable Data Across Threads

`Sync[T]` on its own provides only shared read access. To mutate the inner value
safely, combine it with a `Mutex[T]`:

```tml
use std::sync::{Sync, Mutex}
use std::thread

let shared_counter: Sync[Mutex[I32]] = Sync::new(Mutex::new(0))
let clone: Sync[Mutex[I32]] = shared_counter.duplicate()

let handle = thread::spawn(do() {
    let guard = clone.deref().lock()
    *guard.get_mut() = *guard.get_mut() + 1
})

handle.join()

let guard = shared_counter.deref().lock()
println(guard.get().to_string())   // 1
```

See the [Concurrency](ch16-00-concurrency.md) chapter for full coverage of
`Mutex`, `RwLock`, and other synchronization primitives.

## Weak[T] — Non-Owning References

`Weak[T]` is a non-owning reference created from a `Shared[T]` or `Sync[T]`.
A `Weak[T]` does not contribute to the reference count, so holding a `Weak[T]`
does not prevent the allocation from being freed.

To use the value behind a `Weak[T]`, call `.upgrade()`. It returns
`Just(Shared[T])` if the allocation is still alive, or `Nothing` if it has
already been freed.

```tml
use std::sync::Shared

let strong: Shared[I32] = Shared::new(42)
let weak: Weak[I32] = strong.downgrade()

when weak.upgrade() {
    Just(s) => println("alive: " + s.deref().to_string())  // alive: 42
    Nothing => println("freed")
}

// Now let strong drop
drop(strong)

when weak.upgrade() {
    Just(s) => println("alive: " + s.deref().to_string())
    Nothing => println("freed")   // freed — allocation is gone
}
```

`Weak[T]` is most useful for parent-child relationships where the child must be
able to refer to the parent without preventing the parent from being freed.
The parent holds a `Shared[T]` to the child; the child holds a `Weak[T]` back
to the parent, breaking the reference cycle.

## Cell[T] — Interior Mutability for Copy Types

The borrowing rules normally prevent mutating a value through a shared reference.
`Cell[T]` provides a safe escape hatch for Copy types by using a different
access model: instead of borrowing the contents, you move values in and out.

```tml
use std::cell::Cell

let cell: Cell[I32] = Cell::new(0)

cell.set(42)
let v: I32 = cell.get()   // 42 — get() copies the value out
cell.set(v + 1)
println(cell.get().to_string())   // 43
```

Because `cell.get()` returns a copy rather than a reference, there is no risk
of data races: you cannot hold a reference to the interior at the same time as
a mutable operation.

`Cell[T]` is restricted to Copy types precisely because returning a reference to
the interior would be unsafe. For non-Copy types, use `RefCell[T]`.

## RefCell[T] — Runtime Borrow Checking

`RefCell[T]` moves the borrow rules from compile time to runtime. It maintains
an internal borrow count and panics if you attempt to violate the rules at runtime.

```tml
use std::cell::RefCell
use std::collections::List

let rc: RefCell[List[I32]] = RefCell::new(List[I32].new(8))

// Immutable borrow
{
    let borrow = rc.borrow()        // returns Ref[List[I32]]
    println(borrow.get().len().to_string())
}   // borrow is released here

// Mutable borrow
{
    let mut_borrow = rc.borrow_mut()   // returns RefMut[List[I32]]
    mut_borrow.get_mut().push(1)
    mut_borrow.get_mut().push(2)
}   // mut_borrow is released here

println(rc.borrow().get().len().to_string())   // 2
```

If you call `.borrow_mut()` while an active `.borrow()` still exists,
`RefCell[T]` panics with a message describing the conflict. This is still
safe — it cannot produce undefined behavior — but it is a runtime failure
rather than a compile-time error.

Use `RefCell[T]` when:

- You know at the call site that only one mutable borrow will be active at
  a time, but the compiler cannot prove it from the code structure.
- You are implementing a graph, tree, or other data structure where nodes
  need to mutate their neighbors.
- You are writing mock objects or test doubles that need to record calls
  made through a shared reference.

Prefer compile-time borrowing wherever possible. `RefCell[T]` is a tool for
cases where the static analysis is too conservative, not a replacement for it.

## Combining Smart Pointers

Smart pointers compose. Common combinations:

| Combination | Meaning |
|-------------|---------|
| `Shared[RefCell[T]]` | Shared ownership with interior mutability (single-threaded) |
| `Sync[Mutex[T]]` | Shared ownership with guarded mutability (multi-threaded) |
| `Heap[dyn Behavior]` | Heap-allocated behavior object (dynamic dispatch) |
| `Shared[T]` + `Weak[T]` | Parent/child graph without reference cycles |

```tml
use std::sync::Shared
use std::cell::RefCell

// Shared mutable configuration object passed to many components
type Config { debug: Bool, max_retries: I32 }

let cfg: Shared[RefCell[Config]] = Shared::new(RefCell::new(Config {
    debug: false,
    max_retries: 3,
}))

let cfg2 = cfg.duplicate()

// Component A enables debug mode
cfg.deref().borrow_mut().get_mut().debug = true

// Component B sees the change through the same allocation
println(cfg2.deref().borrow().get().debug.to_string())  // true
```

## The Drop Behavior

All smart pointer types implement `Drop`. You can implement `Drop` for your own
types to perform cleanup when they go out of scope:

```tml
behavior Drop {
    func drop(mut this)
}

type TempFile {
    path: Str,
}

extend TempFile with Drop {
    func drop(mut this) {
        // Remove the temporary file from disk
        fs::remove(ref this.path)
        println("Removed temp file: " + this.path)
    }
}

func main() {
    let tmp = TempFile { path: "/tmp/work-123.bin" }
    do_work(ref tmp)
}
// TempFile::drop() is called automatically here — the file is deleted
```

`Drop` is called in all exit paths from a scope: normal returns, early returns
within the scope, and panics. You cannot opt out of it for a value you own.

## Memory Intrinsics

For situations where none of the above smart pointers are appropriate — writing
custom allocators, interoperating with C libraries that hand ownership of raw
pointers to TML, or implementing new collection types in the standard library —
TML provides raw memory intrinsics. These require a `lowlevel` block:

```tml
lowlevel {
    let ptr = mem_alloc(1024)              // allocate 1024 bytes
    ptr_write[I32](ptr, 0, 42)            // write I32 at byte offset 0
    let val = ptr_read[I32](ptr, 0)       // read I32 from byte offset 0
    copy_nonoverlapping(ptr, dest, 1024)  // copy 1024 bytes (no overlap)
    mem_free(ptr)                          // free the allocation
}
```

Raw memory operations bypass the ownership system and the borrow checker. The
compiler cannot verify their correctness. Use them only when you have a clear
reason that the safe alternatives are insufficient, and document the invariants
your code maintains. See [Lowlevel Blocks and Intrinsics](ch17-02-lowlevel.md)
for the full API reference.
