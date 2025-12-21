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

### 3.3 Clone

For explicit copy of non-Copy types:

```tml
let a = String.from("hello")
let b = a.clone()  // deep copy

print(a)  // OK: a still valid
print(b)  // OK: b is independent copy
```

## 4. Borrowing

### 4.1 Immutable References (&T)

```tml
func print_len(s: &String) {
    print(s.len().to_string())
}

let s = String.from("hello")
print_len(&s)   // borrow s
print(s)        // s still valid
```

### 4.2 Mutable References (&mut T)

```tml
func append(s: &mut String, suffix: String) {
    s.push(suffix)
}

var s = String.from("hello")
append(&mut s, " world")
print(s)  // "hello world"
```

### 4.3 Borrowing Rules

| Scenario | Allowed |
|----------|---------|
| Multiple `&T` | Yes |
| One `&mut T` | Yes |
| `&T` and `&mut T` together | No |
| Use owner during borrow | Depends |

```tml
var data = List.of(1, 2, 3)

// OK: multiple immutable
let r1 = &data
let r2 = &data
print(r1.len())

// OK: one mutable
let r3 = &mut data
r3.push(4)

// ERROR: mixing
let r4 = &data
let r5 = &mut data  // error: r4 still exists
```

## 5. Smart Pointers

### 5.1 Box[T] — Heap Allocation

```tml
// Allocate on heap
let boxed: Box[I32] = Box.new(42)

// Access
print(*boxed)

// Useful for recursive types
type Tree[T] = Leaf(T) | Node(Box[Tree[T]], Box[Tree[T]])
```

### 5.2 Rc[T] — Reference Counting

```tml
import std.rc.Rc

let a = Rc.new(List.of(1, 2, 3))
let b = a.clone()  // increments counter, doesn't copy data

// a and b point to same data
// data released when last Rc is dropped
```

### 5.3 Arc[T] — Atomic Reference Counting

```tml
import std.sync.Arc

let shared = Arc.new(data)

// Can be sent between threads
spawn(do() {
    let local = shared.clone()
    process(local)
})
```

### 5.4 Weak[T] — Weak Reference

```tml
import std.rc.{Rc, Weak}

let strong = Rc.new(Node { value: 42 })
let weak: Weak[Node] = Rc.downgrade(&strong)

// weak doesn't keep alive
when weak.upgrade() {
    Some(rc) -> use(rc),
    None -> print("already released"),
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
    func increment(this) {  // note: this, not &mut this
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

### 7.2 Drop Trait

```tml
trait Drop {
    func drop(this);
}

extend Connection with Drop {
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

## 9. Unsafe

For low-level operations:

```tml
#[unsafe]
func raw_pointer_example() {
    let x: I32 = 42
    let ptr: *I32 = &x as *I32

    // Manual dereference
    let value = *ptr  // unsafe!
}
```

Unsafe operations:
- Raw pointer dereference
- FFI calls
- Access to static mut
- Implement unsafe traits

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

// Fix: clone if you need both
let s = String.from("hello")
let s2 = s.clone()
print(s)   // OK
print(s2)  // OK
```

### 11.2 Dangling Reference

```tml
func dangling() -> &String {
    let s = String.from("hello")
    return &s  // ERROR: s will be destroyed
}

// Fix: return owned
func valid() -> String {
    let s = String.from("hello")
    return s  // move, not reference
}
```

### 11.3 Borrow of Temporary

```tml
let r = &String.from("temp")  // ERROR: temporary destroyed

// Fix: bind first
let s = String.from("temp")
let r = &s
```

---

*Previous: [05-SEMANTICS.md](./05-SEMANTICS.md)*
*Next: [07-MODULES.md](./07-MODULES.md) — Module System*
