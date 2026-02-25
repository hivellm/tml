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
let a: I32 = 42          // I32 (default)
let b: I64 = 42i64       // I64 with suffix
let c: U8 = 255u8        // U8 with suffix
let d: U32 = 0xFF_u32    // U32 hex with suffix
```

**Implicit Coercion for Typed Contexts:**

When a variable or struct field has an explicit type annotation, unsuffixed numeric literals are automatically coerced to the declared type:

```tml
// Variable declarations
var a: U8 = 128          // 128 is coerced to U8 (no suffix needed)
let b: I16 = 1000        // 1000 is coerced to I16
var c: U64 = 4294967296  // large value coerced to U64
let d: F32 = 3.14        // float coerced to F32

// Struct field initializers
type Data {
    id: I64,
    count: U16,
    ratio: F32
}

let data: Data = Data {
    id: 9000000000,      // coerced to I64 (no suffix needed)
    count: 500,          // coerced to U16
    ratio: 0.75          // coerced to F32
}
```

This eliminates the need for explicit casts in simple assignments:
```tml
// Old style (still works)
var a: U8 = 128 as U8

// New style (preferred)
var a: U8 = 128
```

**Explicit conversions between variables:**
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
let pi: F64 = 3.14159    // F64 (default)
let e: F32 = 2.71f32     // F32
let big: F64 = 1.5e10    // F64 with exponent
```

### 2.3 Bool

```tml
let yes: Bool = true
let no: Bool = false

// Operators (keywords, not symbols)
let a: Bool = true and false   // false
let b: Bool = true or false    // true
let c: Bool = not true         // false
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
let multi: String = """
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

### 2.7 Text

Dynamic, growable, heap-allocated string with Small String Optimization (SSO).

```tml
use std::text::Text

// Construction
let t1: Text = Text::new()               // empty
let t2: Text = Text::from("hello")       // from Str
let t3: Text = Text::with_capacity(100)  // pre-allocated
let t4: Text = `Hello, {name}!`          // template literal

// Properties
t2.len()           // 5
t2.capacity()      // >= 5
t2.is_empty()      // false

// Modification (mutates in place)
t1.push('H')              // push single char
t1.push_str("ello")       // push string
t1.clear()                // empty the text

// Conversion
t2.as_str()        // borrow as Str
t2.clone()         // deep copy

// Search
t2.contains("ell")        // true
t2.starts_with("hel")     // true
t2.ends_with("lo")        // true
t2.index_of("l")          // 2

// Transformation (returns new Text)
t2.to_upper_case()        // "HELLO"
t2.to_lower_case()        // "hello"
t2.trim()                 // strip whitespace
t2.substring(0, 3)        // "hel"
t2.replace("l", "L")      // "heLlo"
t2.replace_all("l", "L")  // "heLLo"
t2.repeat(3)              // "hellohellohello"
t2.reverse()              // "olleh"

// Memory management
t2.drop()          // free memory (required!)
```

**SSO (Small String Optimization):**
- Strings ≤23 bytes are stored inline (no heap allocation)
- Larger strings use heap with growth strategy: 2x until 4KB, then 1.5x

**Template Literals:**
Template literals (backtick strings) always produce `Text` type:
```tml
let name: Str = "World"
let greeting: Text = `Hello, {name}!`  // Text, not Str
```

See [02-LEXICAL.md §4.5](02-LEXICAL.md#45-template-literals) for syntax details.

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
let p: Point = Point { x: 1.0, y: 2.0 }
let person: Person = Person {
    name: "Alice",
    age: 30,
    email: Nothing,
}

// Access
let x: F64 = p.x
let name: String = person.name

// Update
let p2 = Point { x: 5.0, ..p }
```

### 3.2 Enums

```tml
// Simple
type Color = Red | Green | Blue

// With data
type Maybe[T] = Just(T) | Nothing

type Outcome[T, E] = Ok(T) | Err(E)

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
let color: Color = Red
let opt: Maybe[I32] = Just(42)
let msg: Message = Text { content: "hi", sender: "alice" }
```

### 3.2.1 Bitflag Enums (`@flags`)

The `@flags` decorator transforms an enum into a type-safe bitflag set. Variants are automatically assigned power-of-2 values (1, 2, 4, 8, ...).

```tml
@flags
type Perms {
    Read,       // = 1
    Write,      // = 2
    Execute,    // = 4
}

// Combine flags with bitwise OR
let rw: Perms = Perms::Read | Perms::Write

// Check flags
rw.has(Perms::Read)       // true
rw.has(Perms::Execute)    // false

// Modify flags
let rwx = rw.add(Perms::Execute)
let r = rwx.remove(Perms::Write)
let toggled = rw.toggle(Perms::Execute)
```

**Explicit discriminant values** allow composite flags:

```tml
@flags
type Style {
    Bold = 1,
    Italic = 2,
    Underline = 4,
    BoldItalic = 3,   // Bold | Italic
}
```

**Underlying type** defaults to `U32`, configurable via `@flags(U8)`, `@flags(U16)`, `@flags(U64)`:

```tml
@flags(U8)
type SmallFlags { A, B, C, D, E, F, G, H }  // max 8 flags
```

**Built-in methods:**

| Method | Description |
|--------|-------------|
| `.has(flag)` | Returns `Bool`: true if flag bit(s) are set |
| `.add(flag)` | Returns `Self`: bitwise OR |
| `.remove(flag)` | Returns `Self`: bitwise AND NOT |
| `.toggle(flag)` | Returns `Self`: bitwise XOR |
| `.is_empty()` | Returns `Bool`: true if zero |
| `.bits()` | Returns underlying integer value |
| `::from_bits(n)` | Creates flags from raw integer |
| `::none()` | Returns zero value |
| `::all()` | Returns all valid bits set |

**Bitwise operators:** `|`, `&`, `^`, `~` return the flags type. `~` masks to valid bits only.

**Restrictions:**
- Variants must be unit variants (no data fields)
- Variant count must not exceed the bit width of the underlying type
- No generic parameters allowed

### 3.3 Unions (C-style)

C-style unions where all fields share the same memory location. Only one field can meaningfully hold a value at a time.

```tml
// Declaration
union IntOrPtr {
    int_val: I32,
    ptr_val: I64,
}

union Value {
    a: I32,
    b: I32,
    c: I32,
}
```

**Construction (Single Field Only):**
```tml
// Union literals initialize exactly one field
let v1: IntOrPtr = IntOrPtr { int_val: 42 }
let v2: IntOrPtr = IntOrPtr { ptr_val: 1000000 }

// ERROR: cannot initialize multiple fields
let bad: IntOrPtr = IntOrPtr { int_val: 42, ptr_val: 100 }  // error
```

**Field Access:**
```tml
let v: IntOrPtr = IntOrPtr { int_val: 42 }

// Access the initialized field
let x: I32 = v.int_val  // 42

// Reading wrong field: bits reinterpreted (UNSAFE!)
let ptr: I64 = v.ptr_val  // undefined - reads int_val bits as I64
```

**Memory Sharing:**
All union fields overlap at offset 0. The union size equals the largest field:

```tml
union IntOrPtr {
    int_val: I32,  // 4 bytes at offset 0
    ptr_val: I64,  // 8 bytes at offset 0
}
// sizeof(IntOrPtr) = 8 (max of 4, 8)
```

**Function Parameters and Returns:**
```tml
func process_union(u: IntOrPtr) -> I32 {
    u.int_val
}

func create_union(val: I32) -> IntOrPtr {
    IntOrPtr { int_val: val }
}

let v: IntOrPtr = IntOrPtr { int_val: 100 }
let result: I32 = process_union(v)  // 100
```

**Mutable Unions:**
```tml
var v: IntOrPtr = IntOrPtr { int_val: 10 }
v = IntOrPtr { ptr_val: 2000 }  // switch to different field
```

**Safety Considerations:**

| Aspect | Notes |
|--------|-------|
| **No tag** | Unlike enums, unions have no runtime discriminant |
| **Wrong field read** | Reading uninitialized field returns garbage bits |
| **Use case** | FFI interop, low-level memory layout, bit reinterpretation |
| **Alternative** | For safe tagged unions, use `enum` with data variants |

**When to Use:**
- Interfacing with C code that uses unions
- Memory-constrained scenarios where fields are mutually exclusive
- Low-level bit manipulation (e.g., accessing float bits as integer)

For type-safe alternatives, prefer enums:
```tml
// Preferred: safe tagged union
type SafeValue = IntVal(I32) | PtrVal(I64)

// C-style: when you need raw memory layout
union IntOrPtr {
    int_val: I32,
    ptr_val: I64,
}
```

### 3.4 Tuples

```tml
let pair: (I32, String) = (42, "answer")
let triple: (F64, F64, F64) = (1.0, 2.0, 3.0)

// Access by index
let first: I32 = pair.0    // 42
let second: String = pair.1   // "answer"

// Destructuring
let (x, y, z) = triple
```

### 3.5 Arrays (Fixed Size)

```tml
let arr: [I32; 5] = [1, 2, 3, 4, 5]
let zeros: [U8; 256] = [0; 256]

// Access
let first: I32 = arr[0]
arr[1] = 10        // if mutable

// Length is part of the type
func process(data: [U8; 16]) { ... }
```

## 4. Collection Types

### 4.1 List[T]

Dynamic array, heap-allocated.

```tml
let nums: List[I32] = List.new()
let items: List[I32] = List.of(1, 2, 3)

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

let other: Set[String] = Set.of("go", "tml")
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
type Outcome[T, E] = Ok(T) | Err(E)

func divide(a: F64, b: F64) -> Outcome[F64, String] {
    if b == 0.0 then return Err("division by zero")
    return Ok(a / b)
}

// Pattern matching
when divide(10.0, 2.0) {
    Ok(result) -> print(result),
    Err(msg) -> print("Error: " + msg),
}

// ! propagates errors
func calculate() -> Outcome[F64, String] {
    let x: F64 = divide(10.0, 2.0)!
    let y: F64 = divide(x, 3.0)!
    return Ok(y)
}

// else for fallback
let result: F64 = divide(10.0, 0.0)! else 0.0
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

### 6.1 Ptr[T] (Raw Pointer)

Raw pointer type for low-level memory operations. Only usable inside `lowlevel` blocks.

```tml
lowlevel {
    var x: I32 = 42
    let p: *I32 = &x           // Pointer to I32

    // Read value through pointer
    let value: I32 = p.read()  // 42

    // Write value through pointer
    p.write(100)               // x is now 100

    // Pointer arithmetic
    let next: *I32 = p.offset(1)

    // Null check
    let valid: Bool = not p.is_null()
}
```

**Pointer Methods:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `read` | `func read(this) -> T` | Read value at pointer location |
| `write` | `func write(this, value: T)` | Write value to pointer location |
| `offset` | `func offset(this, n: I64) -> *T` | Return pointer offset by n elements |
| `is_null` | `func is_null(this) -> Bool` | Check if pointer is null |

**Note:** See [06-MEMORY.md](./06-MEMORY.md#9-low-level-operations) for complete lowlevel and pointer documentation.

### 6.2 Heap[T]

Heap-allocated, single owner.

```tml
let data: Heap[LargeStruct] = Heap.new(create_large())
```

### 6.3 Shared[T]

Reference-counted, shared ownership (single-threaded).

```tml
let cache: Shared[Map[String, Data]] = Shared.new(Map.new())
let copy: Shared[Map[String, Data]] = cache.duplicate()  // increments ref count
```

### 6.4 Sync[T]

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

let point: Point = Point { x: 1.0, y: 2.0 }
print_point(ref point)
```

### 7.2 Mutable: mut ref T

```tml
func increment(counter: mut ref I32) {
    counter += 1
}

var count: I32 = 0
increment(mut ref count)
```

### 7.3 Borrowing Rules

1. Multiple immutable references OK
2. One exclusive mutable reference
3. Cannot have ref T and mut ref T simultaneously

```tml
var data: List[I32] = List.of(1, 2, 3)

// OK: multiple reads
let a: ref List[I32] = ref data
let b: ref List[I32] = ref data
print(a.len())
print(b.len())

// OK: one exclusive write
let c: mut ref List[I32] = mut ref data
c.push(4)

// ERROR: simultaneous read and write
let d: ref List[I32] = ref data
let e: mut ref List[I32] = mut ref data  // error: already has immutable reference
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

let double: func(I32) -> I32 = do(x) x * 2
let result: I32 = apply(double, 21)  // 42
```

## 9. Generics

TML uses **monomorphization** for generics (like Rust). Each unique instantiation
generates specialized code: `Pair[I32]` and `Pair[Bool]` become separate types.

### 9.1 Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| Generic Structs | ✅ Complete | `Pair[T]`, `Entry[K, V]`, `Triple[A, B, C]` |
| Generic Enums | ✅ Complete | `Maybe[T]`, `Outcome[T, E]` |
| Pattern Matching | ✅ Complete | `when` expressions with generic enums |
| Multi-param Generics | ✅ Complete | Unlimited type parameters |
| Generic Functions | ✅ Complete | Type inference at call sites |
| Bounds/Constraints | ✅ Complete | `T: Addable` syntax with where clauses |
| Where Clauses | ✅ Complete | `where T: Ordered + Hashable` |

### 9.2 Generic Structs

```tml
// Single type parameter
type Pair[T] {
    first: T,
    second: T,
}

// Two type parameters
type Entry[K, V] {
    key: K,
    value: V,
}

// Three type parameters
type Triple[A, B, C] {
    a: A,
    b: B,
    c: C,
}

// Usage
let p: Pair[I32] = Pair { first: 10, second: 20 }
print(p.first)   // 10
print(p.second)  // 20

let e: Entry[I32, I32] = Entry { key: 1, value: 100 }
print(e.key)     // 1
print(e.value)   // 100
```

### 9.3 Generic Enums

```tml
// Maybe (Option) type
type Maybe[T] {
    Just(T),
    Nothing,
}

// Outcome (Result) type
type Outcome[T, E] {
    Ok(T),
    Err(E),
}

// Usage with pattern matching
let m: Maybe[I32] = Just(42)
when m {
    Just(v) => print(v),     // 42
    Nothing => print(0),
}

let r: Outcome[I32, I32] = Ok(200)
when r {
    Ok(v) => print(v),       // 200
    Err(e) => print(e),
}

// Unit variant
let n: Maybe[I32] = Nothing
when n {
    Just(v) => print(v),
    Nothing => print(0),     // 0
}
```

### 9.4 Monomorphization

The compiler generates specialized code for each unique type instantiation:

| TML Type | Mangled Name | LLVM Type |
|----------|--------------|-----------|
| `Pair[I32]` | `Pair__I32` | `%struct.Pair__I32` |
| `Entry[I32, Str]` | `Entry__I32__Str` | `%struct.Entry__I32__Str` |
| `Maybe[Bool]` | `Maybe__Bool` | `%struct.Maybe__Bool` |
| `Outcome[I32, I32]` | `Outcome__I32__I32` | `%struct.Outcome__I32__I32` |

### 9.5 Generic Functions

```tml
func identity[T](x: T) -> T {
    return x
}

func swap[T](a: T, b: T) -> (T, T) {
    return (b, a)
}

// Usage (type inferred at call site)
let x: I32 = identity(42)        // I32
let y: String = identity("hello")   // String
```

### 9.6 Bounds and Where Clauses

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

Where clauses are fully checked at call sites. If a type doesn't satisfy the constraints, the compiler produces an error with suggestions.

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

### 10.3 Associated Types

Behaviors can declare associated types that implementors must define:

```tml
behavior Iterator {
    type Item                    // Associated type declaration

    func next(mut ref this) -> Maybe[This::Item]
}

behavior Container {
    type Element
    type Index

    func get(this, idx: This::Index) -> Maybe[This::Element]
    func len(this) -> I32
}
```

Implementations provide concrete types:

```tml
impl Iterator for NumberRange {
    type Item = I32              // Associated type binding

    func next(mut ref this) -> Maybe[I32] {
        if this.current >= this.end {
            return Nothing
        }
        let val: I32 = this.current
        this.current = this.current + 1
        return Just(val)
    }
}

impl Container for List[T] {
    type Element = T
    type Index = I32

    func get(this, idx: I32) -> Maybe[T] {
        // ...
    }

    func len(this) -> I32 {
        return this.length
    }
}
```

Associated types with bounds:

```tml
behavior Sortable {
    type Item: Ordered           // Must implement Ordered behavior

    func sort(mut ref this) -> Unit
}
```

### 10.4 Automatic Generation

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

## 12. Explicit Type Annotations (Mandatory)

TML requires **explicit type annotations** on all variable declarations. This design choice prioritizes LLM code generation clarity over brevity.

### 12.1 Why Mandatory Types?

| Benefit | Description |
|---------|-------------|
| **Zero ambiguity** | LLMs never guess types - no generation errors |
| **Self-documenting** | Code is immediately clear without analysis |
| **Deterministic parsing** | Parser knows types at declaration, not inference |
| **Safer patches** | LLMs can modify code without inferring context |

### 12.2 All Declarations Require Types

```tml
// Variables - type annotation required
let x: I32 = 42
let y: F64 = 3.14
let z: String = "hello"

// Mutable variables
var count: I32 = 0
var name: String = "default"

// Constants
const PI: F64 = 3.14159
const MAX_SIZE: I32 = 1024

// Closures - parameter types in signature
let add: func(I32, I32) -> I32 = do(x, y) x + y

// Collections
let list: List[I32] = List.of(1, 2, 3)
let map: Map[String, I32] = Map.new()
```

### 12.3 Function Signatures

```tml
// Parameters and return types always explicit
func add(a: I32, b: I32) -> I32 {
    return a + b
}

// Struct fields always explicit
type Point { x: F64, y: F64 }

// Behavior methods always explicit
behavior Addable {
    func add(this, other: This) -> This
}
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
