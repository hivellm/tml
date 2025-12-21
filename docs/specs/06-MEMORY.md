# TML v1.0 — Memory Management

## 1. Philosophy

TML doesn't use Garbage Collector. Memory is managed via:
- **Ownership** — each value has an owner
- **Borrowing** — temporary loans
- **RAII** — resources released automatically

## 2. Stack vs Heap

### 2.1 Stack (Automatic)

```tml
func example() {
    let x: I32 = 42           // stack
    let point = Point { x: 1.0, y: 2.0 }  // stack (if small)
    let arr: [U8; 16] = [0; 16]  // stack (fixed size)
}  // all released automatically
```

### 2.2 Heap (Managed)

```tml
func example() {
    let s = String.from("hello")  // heap
    let list = List.of(1, 2, 3)   // heap
    let map = Map.new()           // heap
}  // heap released when owner goes out of scope
```

## 3. Ownership

### 3.1 Single Rule

Every value has exactly one owner:

```tml
let s1 = String.from("hello")
// s1 is owner of "hello"

let s2 = s1
// s2 is now owner
// s1 is no longer valid

print(s1)  // ERROR: s1 moved
print(s2)  // OK
```

### 3.2 Move vs Copy

**Move** (heap types):
```tml
let a = String.from("hello")
let b = a   // move
// a invalid
```

**Copy** (small types):
```tml
let a: I32 = 42
let b = a   // copy
// both valid
print(a)    // OK
print(b)    // OK
```

Copy types:
- Primitives: `Bool`, `I8`-`I128`, `U8`-`U128`, `F32`, `F64`, `Char`
- Tuples of Copy types
- Arrays of Copy types with fixed size

### 3.3 Duplicate

For explicit copy of non-Copy types:

```tml
let a = String.from("hello")
let b = a.duplicate()  // deep copy

print(a)  // OK: a still valid
print(b)  // OK: b is independent copy
```

## 4. Borrowing

### 4.1 Immutable References (ref T)

```tml
func print_len(s: ref String) {
    print(s.len().to_string())
}

let s = String.from("hello")
print_len(ref s)   // borrow s
print(s)           // s still valid
```

### 4.2 Mutable References (mut ref T)

```tml
func append(s: mut ref String, suffix: String) {
    s.push(suffix)
}

var s = String.from("hello")
append(mut ref s, " world")
print(s)  // "hello world"
```

### 4.3 Borrowing Rules

| Scenario | Allowed |
|----------|---------|
| Multiple `ref T` | Yes |
| One `mut ref T` | Yes |
| `ref T` and `mut ref T` together | No |
| Use owner during borrow | Depends |

```tml
var data = List.of(1, 2, 3)

// OK: multiple immutable
let r1 = ref data
let r2 = ref data
print(r1.len())

// OK: one mutable
let r3 = mut ref data
r3.push(4)

// ERROR: mixing
let r4 = ref data
let r5 = mut ref data  // error: r4 still exists
```

## 5. Smart Pointers

### 5.1 Heap[T] — Heap Allocation

```tml
// Allocate on heap
let boxed: Heap[I32] = Heap.new(42)

// Access
print(boxed.get())

// Useful for recursive types
type Tree[T] = Leaf(T) | Node(Heap[Tree[T]], Heap[Tree[T]])
```

### 5.2 Shared[T] — Reference Counting

```tml
import std.shared.Shared

let a = Shared.new(List.of(1, 2, 3))
let b = a.duplicate()  // increments counter, doesn't copy data

// a and b point to same data
// data released when last Shared is dropped
```

### 5.3 Sync[T] — Atomic Reference Counting

```tml
import std.sync.Sync

let shared = Sync.new(data)

// Can be sent between threads
spawn(do() {
    let local = shared.duplicate()
    process(local)
})
```

### 5.4 Weak[T] — Weak Reference

```tml
import std.shared.{Shared, Weak}

let strong = Shared.new(Node { value: 42 })
let weak: Weak[Node] = Shared.downgrade(ref strong)

// weak doesn't keep alive
when weak.upgrade() {
    Just(shared) -> use(shared),
    Nothing -> print("already released"),
}
```

## 6. Interior Mutability

### 6.1 Cell[T] — For Copy Types

```tml
import std.cell.Cell

type Counter {
    value: Cell[I32],
}

extend Counter {
    func increment(this) {  // note: this, not mut ref this
        let current = this.value.get()
        this.value.set(current + 1)
    }
}
```

### 6.2 RefCell[T] — With Runtime Borrow Checking

```tml
import std.cell.RefCell

let data = RefCell.new(List.of(1, 2, 3))

// Immutable borrow
{
    let borrowed = data.borrow()
    print(borrowed.len())
}

// Mutable borrow
{
    let mut_borrowed = data.borrow_mut()
    mut_borrowed.push(4)
}

// Panic if try borrow_mut while borrow exists
```

## 7. Destruction

### 7.1 Automatic Drop

```tml
func example() {
    let file = File.open("data.txt")
    // use file...
}  // file.drop() called automatically
   // file closed
```

### 7.2 Disposable Behavior

```tml
behavior Disposable {
    func drop(this);
}

extend Connection with Disposable {
    func drop(this) {
        this.close()
        log("connection closed")
    }
}
```

### 7.3 Drop Order

Reverse order of declaration:

```tml
func example() {
    let a = Resource.new("a")
    let b = Resource.new("b")
    let c = Resource.new("c")
}
// Drop order: c, b, a
```

## 8. Arenas

For bulk allocation:

```tml
import std.arena.Arena

func process_batch(items: List[Data]) {
    let arena = Arena.new()

    loop item in items {
        // Allocate in arena, very fast
        let processed = arena.alloc(transform(item))
        use(processed)
    }
}  // Arena and all allocations released at once
```

## 9. Low-Level Operations

For low-level operations:

```tml
@lowlevel
func raw_pointer_example() {
    let x: I32 = 42
    let ptr: ptr I32 = ref x as ptr I32

    // Manual dereference
    let value = ptr.read()  // lowlevel!
}
```

Low-level operations:
- Raw pointer dereference
- FFI calls
- Access to static mut
- Implement lowlevel behaviors

## 10. Optimizations

### 10.1 Small String Optimization

```tml
// Small strings (< 24 bytes) don't use heap
let s = "hello"  // stack, not heap
```

### 10.2 Move Elision

```tml
func create() -> BigStruct {
    let result = BigStruct { ... }
    return result  // move elided, built in-place
}
```

### 10.3 Zero-Cost Abstractions

```tml
// Iterator chains compile to simple loops
let sum = items
    .filter(do(x) x > 0)
    .map(do(x) x * 2)
    .sum()

// Equivalent to:
var sum = 0
loop x in items {
    if x > 0 then sum += x * 2
}
```

## 11. Common Errors

### 11.1 Use After Move

```tml
let s = String.from("hello")
let s2 = s
print(s)  // ERROR: s moved

// Fix: duplicate if you need both
let s = String.from("hello")
let s2 = s.duplicate()
print(s)   // OK
print(s2)  // OK
```

### 11.2 Dangling Reference

```tml
func dangling() -> ref String {
    let s = String.from("hello")
    return ref s  // ERROR: s will be destroyed
}

// Fix: return owned
func valid() -> String {
    let s = String.from("hello")
    return s  // move, not reference
}
```

### 11.3 Borrow of Temporary

```tml
let r = ref String.from("temp")  // ERROR: temporary destroyed

// Fix: bind first
let s = String.from("temp")
let r = ref s
```

## 12. Complex Borrowing Scenarios

This section documents advanced borrowing patterns for LLM reference.

### 12.1 Closure Capture

Closures capture variables from their environment:

```tml
func closure_capture_example() {
    var data = List.new()
    data.push(1)

    // Closure captures mut ref data implicitly
    let modifier = do() {
        data.push(2)  // Mutable borrow captured
    }

    // ERROR: Cannot borrow data while closure holds mut ref
    // print(data.len())

    modifier()  // Closure executes, releases borrow
    print(data.len())  // OK: borrow released, prints "2"
}

// Move into closure with explicit capture
func move_into_closure() {
    let data = String.from("owned")

    // 'move' transfers ownership to closure
    let consumer = move do() {
        print(data)  // data moved into closure
    }

    // ERROR: data moved
    // print(data)

    consumer()  // OK
}
```

### 12.2 Struct Field Borrowing

Different struct fields can be borrowed independently:

```tml
type Container {
    items: List[I32],
    metadata: String,
}

func process(c: mut ref Container) {
    // Can borrow different fields simultaneously
    let items_ref = mut ref c.items   // Borrows items field
    let meta_ref = ref c.metadata     // Borrows metadata field (OK - different fields)

    items_ref.push(42)
    print(meta_ref)
}

func conflict_example(c: mut ref Container) {
    let items_ref = mut ref c.items

    // ERROR: Cannot borrow c.items again
    // let items_ref2 = ref c.items

    // But this is OK: different field
    let meta_ref = ref c.metadata
}
```

### 12.3 Reborrowing

Temporary borrows from existing references:

```tml
func reborrow_example() {
    var data = String.from("hello")
    let r1 = mut ref data

    // Reborrow: temporarily create new reference from existing
    helper(r1)     // r1 is reborrowed as mut ref inside helper
    r1.push("!")   // OK: reborrow ended, r1 still valid
}

func helper(s: mut ref String) {
    s.push(" world")
}

// Reborrow coercion
func reborrow_coercion() {
    var data = String.from("test")
    let r: mut ref String = mut ref data

    // mut ref T can be reborrowed as ref T
    let len = get_len(r)  // r reborrowed as ref String

    r.push("!")  // OK: mutable access restored
}

func get_len(s: ref String) -> U64 {
    return s.len()
}
```

### 12.4 Return Reference Lifetime

When returning references, lifetime must be unambiguous:

```tml
// ERROR: Ambiguous lifetime - which input does output reference?
func longest_error(a: ref String, b: ref String) -> ref String {
    if a.len() > b.len() then a else b
}

// SOLUTION 1: Return owned value (always works)
func longest_owned(a: ref String, b: ref String) -> String {
    if a.len() > b.len() then a.duplicate() else b.duplicate()
}

// SOLUTION 2: Single source reference (lifetime unambiguous)
func longest_from_list(items: ref List[String]) -> ref String {
    // Lifetime tied to items - unambiguous
    var longest = ref items[0]
    loop item in items.iter() {
        if item.len() > longest.len() then {
            longest = item
        }
    }
    return longest
}

// SOLUTION 3: Return index instead of reference
func longest_index(a: ref String, b: ref String) -> U8 {
    if a.len() > b.len() then 0 else 1
}
```

### 12.5 Non-Lexical Lifetimes (NLL)

Borrows end at last use, not at scope end:

```tml
func nll_example() {
    var data = List.of(1, 2, 3)

    let r = ref data[0]
    print(r)          // Last use of r

    // In lexical lifetimes: ERROR (r still in scope)
    // In NLL (TML default): OK (r not used after this point)
    data.push(4)      // Mutable access allowed

    print(data.len()) // Prints 4
}

func nll_conditional() {
    var data = List.of(1, 2, 3)

    let r = ref data[0]

    if some_condition() then {
        print(r)      // r used in this branch
    }

    // OK even though r is "in scope" - NLL sees it's not used after if
    data.push(4)
}
```

### 12.6 Self-Referential Structures

Self-referential structures require special handling:

```tml
// ERROR: Cannot create self-referential struct directly
type SelfRef {
    data: String,
    // ptr: ref String,  // Would reference data - not allowed
}

// SOLUTION: Use indices instead of references
type Document {
    paragraphs: List[String],
    current_index: U64,  // Index instead of reference
}

extend Document {
    func current(this) -> ref String {
        return ref this.paragraphs[this.current_index]
    }
}

// SOLUTION 2: Use Pin for async/generator contexts
import std.pin.Pin

type Generator {
    // Pin allows controlled self-reference for async
}
```

### 12.7 Iterator Invalidation Prevention

TML's borrow checker prevents iterator invalidation:

```tml
func safe_iteration() {
    var list = List.of(1, 2, 3)

    // ERROR: Cannot mutate while iterating
    // loop item in list.iter() {
    //     list.push(item * 2)  // Would invalidate iterator
    // }

    // SOLUTION 1: Collect first
    let doubled: List[I32] = list.iter().map(do(x) x * 2).collect()
    list.extend(doubled)

    // SOLUTION 2: Index-based iteration
    let len = list.len()
    loop i in 0 to len {
        let val = list[i]
        list.push(val * 2)
    }
}
```

### 12.8 Two-Phase Borrow

Method chains with temporary borrows:

```tml
type Builder {
    value: String,
}

extend Builder {
    func append(this, s: ref String) -> mut ref This {
        this.value.push(s)
        return this  // Returns mut ref This
    }

    func build(this) -> String {
        return this.value.duplicate()
    }
}

func builder_chain() {
    var builder = Builder { value: String.new() }

    // Two-phase borrow enables method chaining
    let result = builder
        .append(ref "hello")
        .append(ref " ")
        .append(ref "world")
        .build()

    print(result)  // "hello world"
}
```

---

*Previous: [05-SEMANTICS.md](./05-SEMANTICS.md)*
*Next: [07-MODULES.md](./07-MODULES.md) — Module System*
