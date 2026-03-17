# Ownership Rules

TML's ownership system rests on three rules that the compiler enforces at every
point in a program:

1. Every value has exactly one owner.
2. When the owner goes out of scope, the value is dropped.
3. Ownership can be transferred — moved — from one variable to another, but after
   a move the original variable can no longer be used.

These rules interact with a fourth concept — Copy types — that carves out an
exception for values cheap enough to duplicate implicitly.

## Drop: Automatic Cleanup

When a variable goes out of scope, TML calls its `drop` method if it implements
the `Drop` behavior, then reclaims the storage. This happens unconditionally, at
a point determined by the compiler. There is no garbage collector deciding when
cleanup happens: cleanup is always at the closing brace of the block that owns
the value.

```tml
func process() {
    let data = List[I32].new(16)
    data.push(1)
    data.push(2)
    data.push(3)
    // work with data ...
}
// `data` goes out of scope here — its heap allocation is freed immediately
```

Multiple values in the same scope are dropped in reverse declaration order,
mirroring the LIFO structure of the call stack.

```tml
func multi_drop() {
    let a = List[I32].new(4)   // created first
    let b = List[I32].new(4)   // created second

    // b is dropped first, then a
}
```

## Move Semantics

Assignment does not automatically copy heap-allocated values. Instead, it moves
ownership: the source variable is invalidated, and the destination variable becomes
the new owner.

```tml
let s1 = "hello"
let s2 = s1      // ownership moves from s1 to s2

println(s2)      // ok: s2 owns the string
// println(s1)   // ERROR: s1 has been moved — it no longer holds a valid value
```

The same rule applies when passing a value to a function:

```tml
func consume(s: Str) {
    println(s)
}   // s is dropped here

func main() {
    let greeting = "hello"
    consume(greeting)
    // println(greeting)  // ERROR: greeting was moved into consume()
}
```

And when returning a value from a function:

```tml
func make_greeting(name: Str) -> Str {
    let result = "Hello, " + name   // name is moved into the concatenation
    return result                    // result is moved to the caller
}

func main() {
    let g = make_greeting("Alice")  // g now owns the returned string
    println(g)                       // "Hello, Alice"
}
```

### Why Move Instead of Copy?

Implicit copying of heap-allocated values is expensive and makes it hard to reason
about where memory is being allocated. Making moves the default keeps the performance
contract clear: assigning a `Str` or `List[T]` never silently allocates.

## Copy Types

Some types are so cheap to duplicate that implicit copying is the right default.
These are called Copy types. When a Copy type is assigned or passed to a function,
the value is bitwise-copied and both the source and destination are valid afterwards.

The built-in Copy types are:

| Type | Description |
|------|-------------|
| `Bool` | Boolean |
| `I8`, `I16`, `I32`, `I64` | Signed integers |
| `U8`, `U16`, `U32`, `U64` | Unsigned integers |
| `F32`, `F64` | Floating-point numbers |
| `Char` | Unicode scalar value |
| `()` | Unit type |
| Arrays of Copy types | e.g., `[I32; 4]` |
| Tuples of Copy types | e.g., `(I32, Bool)` |

```tml
let x: I32 = 42
let y = x          // x is COPIED — both x and y are valid
println(x)         // 42
println(y)         // 42

let a: F64 = 3.14
let b = a          // b is a copy of a
println(a + b)     // 6.28
```

References (`ref T`) are also Copy: copying a reference produces a second reference
to the same value, not a copy of the value.

### Structs and Copy

A struct implements Copy automatically only when all of its fields are Copy types.
If any field is a non-Copy type (such as `Str` or `List[T]`), the struct is a move
type.

```tml
// This struct is automatically Copy — all fields are primitive
type Point {
    x: F64,
    y: F64,
}

// This struct is NOT Copy — it contains a Str
type Named {
    name: Str,
    value: I32,
}

func demo() {
    let p1 = Point { x: 1.0, y: 2.0 }
    let p2 = p1     // p1 is copied; both are valid
    println(p1.x)   // ok

    let n1 = Named { name: "Alice", value: 10 }
    let n2 = n1     // n1 is moved; n1 is no longer valid
    // println(n1.name)  // ERROR: n1 has been moved
}
```

## Explicit Deep Copy

When you need a full copy of a non-Copy value, call `.duplicate()`. This is an
explicit deep clone that allocates new heap storage:

```tml
let original = "hello"
let copy = original.duplicate()   // new heap allocation, independent string

println(original)  // still valid
println(copy)      // also valid
```

The `.duplicate()` method is defined by the `Clone` behavior. Standard library types
— `Str`, `List[T]`, `HashMap[K, V]` — all implement it. The cost of `.duplicate()`
is visible at the call site, which is intentional: you can always see where deep
copies occur.

## Ownership in Pattern Matching

Pattern matching with `when` also participates in the ownership system. Matching
a non-Copy value moves it into the pattern:

```tml
func classify(value: Maybe[Str]) {
    when value {
        Just(s) => {
            // s owns the Str that was inside value
            println("Got: " + s)
        }
        Nothing => println("Nothing")
    }
    // value has been moved — do not use it after the when expression
}
```

To borrow from a `Maybe` without consuming it, use a reference pattern:

```tml
func peek(value: ref Maybe[Str]) {
    when value {
        Just(ref s) => println("Contains: " + s)
        Nothing     => println("Empty")
    }
    // value is still valid — we only borrowed
}
```

## Drop Order and Resource Management

Because drop order is deterministic and tied to scope, TML provides RAII
(Resource Acquisition Is Initialization) — a pattern where resource lifetime
is tied to an object's lifetime.

```tml
behavior Drop {
    func drop(mut this)
}

type FileHandle {
    path: Str,
    // internal OS handle ...
}

extend FileHandle with Drop {
    func drop(mut this) {
        this.close()
        println("Closed: " + this.path)
    }
}

func process_file() {
    let handle = FileHandle::open("data.txt")
    // use handle ...
}
// drop() is called automatically here — file is guaranteed to be closed
```

This pattern extends to any resource: database connections, network sockets,
mutex guards, and anything else that has a paired acquire/release lifecycle. In
TML, the release always happens — there is no way to forget it.

### Dropping Early

If you want to release a resource before the end of its enclosing scope, introduce
a nested block:

```tml
func main() {
    {
        let lock = mutex.lock()
        // critical section
    }
    // lock is released here, before the rest of main() runs
    do_more_work()
}
```

## Ownership Across Collections

When you insert a value into a collection, the collection becomes its owner:

```tml
var items: List[Str] = List[Str].new(4)
let label = "item-one"
items.push(label)        // label is moved into items
// println(label)        // ERROR: label has been moved

items.push("item-two")   // string literal — also moved
```

Iterating over a collection can either move the elements out or borrow them,
depending on how the loop is written:

```tml
// Moving iteration — consumes the list
for item in items {
    println(item)   // item is owned here
}
// items has been consumed — do not use it after this loop

// Borrowing iteration — borrows each element
for ref item in items {
    println(item)   // item is a reference
}
// items is still valid
```
