# TML v1.0 — Type System

## 1. Overview

TML has a static and strong type system:
- **Local inference** — types inferred within functions
- **Explicit at boundaries** — parameters and returns annotated
- **No null** — Maybe[T] for optional values
- **No exceptions** — Outcome[T, E] for errors

## 2. Primitive Types

### 2.1 Integers

| Type | Bits | Range | Use |
|------|------|-------|-----|
| `I8` | 8 | -128 to 127 | Signed bytes |
| `I16` | 16 | -32768 to 32767 | Short |
| `I32` | 32 | -2³¹ to 2³¹-1 | Default for integers |
| `I64` | 64 | -2⁶³ to 2⁶³-1 | Timestamps, IDs |
| `I128` | 128 | -2¹²⁷ to 2¹²⁷-1 | Crypto, big numbers |
| `U8` | 8 | 0 to 255 | Bytes, ASCII |
| `U16` | 16 | 0 to 65535 | Ports, small counts |
| `U32` | 32 | 0 to 2³²-1 | Indices, IDs |
| `U64` | 64 | 0 to 2⁶⁴-1 | Sizes, hashes |
| `U128` | 128 | 0 to 2¹²⁸-1 | UUIDs |

**Literals:**
```tml
let a = 42          // I32 (default)
let b = 42i64       // I64
let c = 255u8       // U8
let d = 0xFF_u32    // U32 hex
```

**Explicit conversions:**
```tml
let x: I32 = 100
let y: I64 = x.to_i64()   // explicit, never implicit
let z: U8 = x.to_u8()     // may truncate - checked in debug
```

### 2.2 Floats

| Type | Bits | Precision | Use |
|------|------|-----------|-----|
| `F32` | 32 | ~7 digits | Graphics, ML |
| `F64` | 64 | ~15 digits | Default, calculations |

**Literals:**
```tml
let pi = 3.14159    // F64 (default)
let e = 2.71f32     // F32
let big = 1.5e10    // F64 with exponent
```

### 2.3 Bool

```tml
let yes: Bool = true
let no: Bool = false

// Operators (keywords, not symbols)
let a = true and false   // false
let b = true or false    // true
let c = not true         // false
```

### 2.4 Char

Unicode scalar value (4 bytes).

```tml
let c: Char = 'A'
let emoji: Char = '🎉'
let newline: Char = '\n'
```

### 2.5 String

UTF-8, immutable, heap-allocated.

```tml
let s: String = "hello"
let multi = """
    Multi-line
    string
"""

// Operations
s.len()           // bytes
s.chars()         // Iterable[Char]
s + " world"      // concatenation
s.slice(0, 5)     // substring
```

### 2.6 Bytes

Byte sequence, immutable.

```tml
let data: Bytes = b"hello"
let raw: Bytes = b"\x00\xFF\x42"

data.len()        // 5
data.get(0)       // Just(104)  (ASCII 'h')
```

## 3. Composite Types

### 3.1 Structs

```tml
type Point {
    x: F64,
    y: F64,
}

type Person {
    name: String,
    age: U32,
    email: String?,   // optional (Maybe[String])
}

// Construction
let p = Point { x: 1.0, y: 2.0 }
let person = Person {
    name: "Alice",
    age: 30,
    email: Nothing,
}

// Access
let x = p.x
let name = person.name

// Update
let p2 = Point { x: 5.0, ..p }
```

### 3.2 Enums

```tml
// Simple
type Color = Red | Green | Blue

// With data
type Maybe[T] = Just(T) | Nothing

type Outcome[T, E] = Success(T) | Failure(E)

type JsonValue =
    | Null
    | Bool(Bool)
    | Number(F64)
    | Text(String)
    | Array(List[JsonValue])
    | Object(Map[String, JsonValue])

// With named fields
type Message =
    | Text { content: String, sender: String }
    | Image { url: String, width: U32, height: U32 }
    | File { path: String, size: U64 }

// Usage
let color = Red
let opt = Just(42)
let msg = Text { content: "hi", sender: "alice" }
```

### 3.3 Tuples

```tml
let pair: (I32, String) = (42, "answer")
let triple: (F64, F64, F64) = (1.0, 2.0, 3.0)

// Access by index
let first = pair.0    // 42
let second = pair.1   // "answer"

// Destructuring
let (x, y, z) = triple
```

### 3.4 Arrays (Fixed Size)

```tml
let arr: [I32; 5] = [1, 2, 3, 4, 5]
let zeros: [U8; 256] = [0; 256]

// Access
let first = arr[0]
arr[1] = 10        // if mutable

// Length is part of the type
func process(data: [U8; 16]) { ... }
```

## 4. Collection Types

### 4.1 List[T]

Dynamic array, heap-allocated.

```tml
let nums: List[I32] = List.new()
let items = List.of(1, 2, 3)

// Operations
items.push(4)
items.pop()
items.get(0)      // Maybe[I32]
items.len()
items.is_empty()

// Iteration
loop item in items {
    print(item)
}

// Functional
items.map(do(x) x * 2)
items.filter(do(x) x > 0)
items.fold(0, do(acc, x) acc + x)
```

### 4.2 Map[K, V]

Hash map.

```tml
let scores: Map[String, I32] = Map.new()

scores.insert("alice", 100)
scores.insert("bob", 85)

scores.get("alice")    // Maybe[I32]
scores.contains("bob") // Bool
scores.remove("bob")

loop (key, value) in scores {
    print(key + ": " + value.to_string())
}
```

### 4.3 Set[T]

Hash set.

```tml
let tags: Set[String] = Set.new()

tags.insert("rust")
tags.insert("tml")

tags.contains("rust")  // true
tags.remove("rust")

let other = Set.of("go", "tml")
tags.union(other)
tags.intersection(other)
```

## 5. Special Types

### 5.1 Maybe[T]

```tml
type Maybe[T] = Just(T) | Nothing

let maybe: Maybe[I32] = Just(42)
let empty: Maybe[I32] = Nothing

// Pattern matching
when maybe {
    Just(x) -> use(x),
    Nothing -> default(),
}

// Methods
maybe.unwrap()           // panic if Nothing
maybe.unwrap_or(0)       // default value
maybe.map(do(x) x * 2)   // Maybe[I32]
maybe.and_then(do(x) validate(x))

// Sugar: T? = Maybe[T]
let name: String? = get_name()
```

### 5.2 Outcome[T, E]

```tml
type Outcome[T, E] = Success(T) | Failure(E)

func divide(a: F64, b: F64) -> Outcome[F64, String] {
    if b == 0.0 then return Failure("division by zero")
    return Success(a / b)
}

// Pattern matching
when divide(10.0, 2.0) {
    Success(result) -> print(result),
    Failure(msg) -> print("Error: " + msg),
}

// ! propagates errors
func calculate() -> Outcome[F64, String] {
    let x = divide(10.0, 2.0)!
    let y = divide(x, 3.0)!
    return Success(y)
}

// else for fallback
let result = divide(10.0, 0.0)! else 0.0
```

### 5.3 Unit

Type with a single value, equivalent to void.

```tml
func log(msg: String) -> Unit {
    print(msg)
    return unit
}

// Implicit if no return
func log(msg: String) {
    print(msg)
}
```

### 5.4 Never

Type that never returns (panic, infinite loop).

```tml
func panic(msg: String) -> Never {
    // never returns
}

func infinite() -> Never {
    loop {
        work()
    }
}
```

## 6. Memory Types

### 6.1 Heap[T]

Heap-allocated, single owner.

```tml
let data: Heap[LargeStruct] = Heap.new(create_large())
```

### 6.2 Shared[T]

Reference-counted, shared ownership (single-threaded).

```tml
let cache: Shared[Map[String, Data]] = Shared.new(Map.new())
let copy = cache.duplicate()  // increments ref count
```

### 6.3 Sync[T]

Atomic reference-counted, thread-safe shared ownership.

```tml
let global: Sync[Config] = Sync.new(load_config())
// Can be safely shared across threads
```

## 7. References

### 7.1 Immutable: ref T

```tml
func print_point(p: ref Point) {
    print(p.x.to_string())
}

let point = Point { x: 1.0, y: 2.0 }
print_point(ref point)
```

### 7.2 Mutable: mut ref T

```tml
func increment(counter: mut ref I32) {
    counter += 1
}

var count = 0
increment(mut ref count)
```

### 7.3 Borrowing Rules

1. Multiple immutable references OK
2. One exclusive mutable reference
3. Cannot have ref T and mut ref T simultaneously

```tml
var data = List.of(1, 2, 3)

// OK: multiple reads
let a = ref data
let b = ref data
print(a.len())
print(b.len())

// OK: one exclusive write
let c = mut ref data
c.push(4)

// ERROR: simultaneous read and write
let d = ref data
let e = mut ref data  // error: already has immutable reference
```

## 8. Function Types

```tml
// Function type
type Predicate = func(I32) -> Bool
type BinaryOp = func(I32, I32) -> I32
type Handler = func(Request) -> Response

// Usage
func apply(f: func(I32) -> I32, x: I32) -> I32 {
    return f(x)
}

let double = do(x) x * 2
let result = apply(double, 21)  // 42
```

## 9. Generics

### 9.1 Generic Functions

```tml
func identity[T](x: T) -> T {
    return x
}

func swap[T](a: T, b: T) -> (T, T) {
    return (b, a)
}

// Usage (type inferred)
let x = identity(42)        // I32
let y = identity("hello")   // String
```

### 9.2 Generic Types

```tml
type Pair[T] {
    first: T,
    second: T,
}

type Entry[K, V] {
    key: K,
    value: V,
}

let pair = Pair { first: 1, second: 2 }
let entry = Entry { key: "name", value: "Alice" }
```

### 9.3 Bounds (Constraints)

```tml
// T must implement Addable
func sum[T: Addable](items: List[T]) -> T {
    return items.fold(T.zero(), do(acc, x) acc + x)
}

// Multiple bounds
func sorted[T: Ordered + Duplicate](items: List[T]) -> List[T] {
    // ...
}

// Where clause for complex bounds
func merge[K, V](a: Map[K, V], b: Map[K, V]) -> Map[K, V]
where K: Equal + Hashable, V: Duplicate
{
    // ...
}
```

## 10. Behaviors

### 10.1 Definition

```tml
behavior Equal {
    func eq(this, other: This) -> Bool

    // Default implementation
    func ne(this, other: This) -> Bool {
        return not this.eq(other)
    }
}

behavior Ordered: Equal {
    func compare(this, other: This) -> Ordering
}

behavior Duplicate {
    func duplicate(this) -> This
}

behavior Default {
    func default() -> This
}
```

### 10.2 Implementation

```tml
extend Point with Equal {
    func eq(this, other: This) -> Bool {
        return this.x == other.x and this.y == other.y
    }
}

extend Point with Duplicate {
    func duplicate(this) -> This {
        return This { x: this.x, y: this.y }
    }
}

extend Point with Default {
    func default() -> This {
        return This { x: 0.0, y: 0.0 }
    }
}
```

### 10.3 Automatic Generation

```tml
@auto(equal, duplicate, default, debug)
type Point {
    x: F64,
    y: F64,
}
```

Auto-generatable behaviors:
- `equal` — structural equality
- `order` — ordering (requires equal)
- `duplicate` — deep copy
- `default` — default value
- `debug` — debug representation
- `hash` — hash for Map/Set

## 11. Type Aliases

```tml
type UserId = U64
type Email = String
type Handler = func(Request) -> Response
type StringMap[V] = Map[String, V]

// Usage
let id: UserId = 12345
let users: StringMap[User] = Map.new()
```

## 12. Type Inference

### 12.1 Where It Works

```tml
// Local variables
let x = 42          // I32 inferred
let y = 3.14        // F64 inferred
let z = "hello"     // String inferred

// Closures
let add = do(x, y) x + y    // types inferred from usage

// Generics
let list = List.of(1, 2, 3) // List[I32] inferred
```

### 12.2 Where It's Required

```tml
// Function parameters
func add(a: I32, b: I32) -> I32

// Struct fields
type Point { x: F64, y: F64 }

// Public constants
public const MAX: U32 = 1000
```

## 13. Coercions

### 13.1 No Implicit Coercion

```tml
let x: I32 = 42
let y: I64 = x        // ERROR: different types

let y: I64 = x.to_i64()  // OK: explicit conversion
```

### 13.2 Conversions

```tml
// Numbers
x.to_i8()   x.to_i16()  x.to_i32()  x.to_i64()
x.to_u8()   x.to_u16()  x.to_u32()  x.to_u64()
x.to_f32()  x.to_f64()

// Strings
42.to_string()
"42".parse[I32]()    // Outcome[I32, ParseError]

// Checked (returns Maybe)
x.checked_add(y)
x.checked_mul(y)
```

## 14. Subtyping

TML has no data type subtyping, only via behaviors:

```tml
// No struct inheritance

// Polymorphism via behaviors
func print_all[T: Debug](items: List[T]) {
    loop item in items {
        print(item.debug())
    }
}
```

---

*Previous: [03-GRAMMAR.md](./03-GRAMMAR.md)*
*Next: [05-SEMANTICS.md](./05-SEMANTICS.md) — Semantics*
