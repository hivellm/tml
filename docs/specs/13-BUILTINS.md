# TML v1.0 — Builtin Types and Functions

## 1. Primitive Types

### 1.1 Bool
```tml
true, false

// Operations (keywords, not symbols)
not b
a and b
a or b
```

### 1.2 Integers

| Type | Operations |
|------|------------|
| `I8`, `I16`, `I32`, `I64`, `I128` | `+`, `-`, `*`, `/`, `%`, `**` |
| `U8`, `U16`, `U32`, `U64`, `U128` | `&`, `\|`, `^`, `~`, `<<`, `>>` |

```tml
// Common methods
x.abs()              // absolute value (signed)
x.signum()           // sign: -1, 0, or 1
x.pow(n)             // power
x.to_string()        // conversion
x.to_i32()           // conversion between types
x.checked_add(y)     // Maybe[T], Nothing on overflow
x.saturating_add(y)  // saturates at MAX/MIN
x.wrapping_add(y)    // wraps on overflow

// Constants
I32.MIN, I32.MAX
U64.MIN, U64.MAX
```

### 1.3 Floats

```tml
// F32, F64
x.abs()
x.floor()
x.ceil()
x.round()
x.sqrt()
x.sin(), x.cos(), x.tan()
x.ln(), x.log10(), x.log2()
x.exp()
x.is_nan()
x.is_infinite()

// Constants
F64.PI, F64.E
F64.INFINITY, F64.NEG_INFINITY
F64.NAN
```

### 1.4 Char

```tml
'a'.is_alphabetic()
'1'.is_numeric()
'A'.is_uppercase()
'a'.is_lowercase()
'A'.to_lowercase()  // 'a'
'a'.to_uppercase()  // 'A'
'a'.to_digit(10)    // Maybe[U32]
```

### 1.5 String

```tml
// Construction
String.new()
String.from("text")

// Methods
s.len()              // bytes
s.chars()            // Iterable[Char]
s.is_empty()
s.contains("sub")
s.starts_with("pre")
s.ends_with("suf")
s.find("sub")        // Maybe[U64]
s.replace("old", "new")
s.trim()
s.trim_start()
s.trim_end()
s.to_lowercase()
s.to_uppercase()
s.split(",")         // Iterable[String]
s.lines()            // Iterable[String]
s.slice(start, end)  // substring
s + other            // concatenation
```

### 1.6 Bytes

```tml
Bytes.new()
b"literal"

b.len()
b.get(index)         // Maybe[U8]
b.slice(start, end)
b.to_string()        // Outcome[String, Utf8Error]
```

## 2. Collections

### 2.1 List[T]

```tml
List.new()
List.of(1, 2, 3)
List.with_capacity(100)

list.len()
list.is_empty()
list.push(item)
list.pop()           // Maybe[T]
list.get(index)      // Maybe[T]
list.first()         // Maybe[T]
list.last()          // Maybe[T]
list.insert(index, item)
list.remove(index)   // T
list.clear()
list.contains(item)  // requires T: Equal
list.reverse()
list.sort()          // requires T: Ordered
list.duplicate()     // requires T: Duplicate

// Iteration
list.iter()          // Iterable[ref T]
list.iter_mut()      // Iterable[mut ref T]
list.into_iter()     // Iterable[T]

// Functional
list.map(func)
list.filter(func)
list.fold(init, func)
list.find(func)      // Maybe[T]
list.any(func)       // Bool
list.all(func)       // Bool
```

### 2.2 Map[K, V]

```tml
Map.new()
Map.with_capacity(100)

map.len()
map.is_empty()
map.insert(key, value)  // Maybe[V] (old value)
map.get(key)            // Maybe[ref V]
map.get_mut(key)        // Maybe[mut ref V]
map.remove(key)         // Maybe[V]
map.contains(key)       // Bool
map.keys()              // Iterable[ref K]
map.values()            // Iterable[ref V]
map.entries()           // Iterable[(ref K, ref V)]
map.clear()
```

### 2.3 Set[T]

```tml
Set.new()
Set.of(1, 2, 3)

set.len()
set.is_empty()
set.insert(item)     // Bool (true if new)
set.remove(item)     // Bool (true if existed)
set.contains(item)   // Bool
set.union(other)     // Set[T]
set.intersection(other)
set.difference(other)
set.is_subset(other)
set.is_superset(other)
```

## 3. Maybe[T]

```tml
Just(value)
Nothing

opt.is_just()        // Bool
opt.is_nothing()     // Bool
opt.unwrap()         // T (panic if Nothing)
opt.unwrap_or(default)
opt.unwrap_or_else(func)
opt.expect(msg)      // T (panic with msg if Nothing)
opt.map(func)        // Maybe[U]
opt.and_then(func)   // Maybe[U]
opt.or(other)        // Maybe[T]
opt.or_else(func)    // Maybe[T]
opt.filter(func)     // Maybe[T]
opt.to_outcome(err)  // Outcome[T, E]
```

## 4. Outcome[T, E]

```tml
Ok(value)
Err(error)

res.is_ok()          // Bool
res.is_err()         // Bool
res.unwrap()         // T (panic if Err)
res.unwrap_err()     // E (panic if Ok)
res.unwrap_or(default)
res.expect(msg)
res.map(func)        // Outcome[U, E]
res.map_err(func)    // Outcome[T, F]
res.and_then(func)   // Outcome[U, E]
res.or(other)        // Outcome[T, E]
res.or_else(func)    // Outcome[T, F]
res.to_maybe()       // Maybe[T]
res.err()            // Maybe[E]
```

## 5. Ranges

```tml
0 to 10              // Range[I32] (0 to 9)
0 through 10         // RangeInclusive (0 to 10)

range.contains(5)    // Bool
range.is_empty()     // Bool

loop i in 0 to 10 {
    // i = 0, 1, 2, ..., 9
}

loop i in 1 through 5 {
    // i = 1, 2, 3, 4, 5
}
```

## 6. Iterables

```tml
behavior Iterable {
    type Item
    func next(this) -> Maybe[This.Item]
}

// Iterable methods
iter.count()
iter.last()
iter.nth(n)
iter.skip(n)
iter.take(n)
iter.step_by(n)
iter.chain(other)
iter.zip(other)
iter.enumerate()     // Iterable[(U64, T)]
iter.map(func)
iter.filter(func)
iter.filter_map(func)
iter.flat_map(func)
iter.flatten()
iter.fold(init, func)
iter.reduce(func)
iter.find(func)
iter.position(func)
iter.any(func)
iter.all(func)
iter.max()           // requires Ordered
iter.min()
iter.sum()           // requires Addable
iter.product()       // requires Multipliable
iter.collect()       // to List, Set, etc.
```

## 7. Global Functions

### 7.1 I/O

```tml
print(msg)           // prints without newline
println(msg)         // prints with newline
eprint(msg)          // stderr
eprintln(msg)        // stderr with newline
```

**Polymorphic Functions:**

The `print` and `println` functions are **polymorphic** - they accept multiple types with the same function name. This is implemented through compiler-level type resolution, not runtime polymorphism.

```tml
// All of these use the same function name 'print'
print(42)           // I32
print(-100)         // I32
print(3.14)         // F64
print(true)         // Bool
print(false)        // Bool
print("hello")      // Str

// The compiler resolves the correct runtime function based on argument type:
// - I32    → tml_print_i32()
// - I64    → tml_print_i64()
// - F64    → tml_print_f64()
// - Bool   → tml_print_bool()
// - Str    → tml_print_str()
```

**Type-Specific Print Functions:**

While `print()` handles most cases automatically, you can use type-specific variants if needed:

```tml
// These are the underlying functions (prefer polymorphic print())
print_i32(42)        // explicit I32 print
print_i64(100)       // explicit I64 print
print_f64(3.14)      // explicit F64 print
print_bool(true)     // explicit Bool print
print_str("hello")   // explicit Str print
```

**Note:** The polymorphic `print()` is recommended over type-specific variants as it provides a cleaner, more ergonomic API.

**Format Strings:**

Both `print` and `println` support format strings with placeholders:

```tml
// Basic placeholder {}
println("Hello, {}!", name)
println("Values: {} and {}", x, y)

// Precision format specifiers {:.N} for floats
let pi: F64 = 3.14159265359
println("Pi: {:.2}", pi)        // "Pi: 3.14"
println("Pi: {:.5}", pi)        // "Pi: 3.14159"

// Multiple values with mixed formats
let name: Str = "benchmark"
let time: F64 = 0.266
let runs: I64 = 3
println("{}: {:.3} ms (avg of {} runs)", name, time, runs)
// Output: "benchmark: 0.266 ms (avg of 3 runs)"

// Type conversion for precision
let x: I32 = 42
println("{:.2}", x)  // "42.00" (converts to double for display)
```

**Supported Format Specifiers:**
- `{}` - Default formatting (works with any type)
- `{:.N}` - Floating-point precision (N decimal places)
  - Works with F32, F64
  - Automatically converts integers to double when precision is specified
  - Common values: `{:.0}`, `{:.1}`, `{:.2}`, `{:.3}`, `{:.6}`

**Type Support:**
- Str: Direct output
- I8, I16, I32, I64, I128: Integer formatting
- U8, U16, U32, U64, U128: Unsigned integer formatting
- F32, F64: Float formatting (supports precision)
- Bool: "true" or "false"

### 7.2 Control

```tml
panic(msg)           // terminates with error
unreachable()        // marks unreachable code
todo()               // placeholder for implementation
unimplemented()      // unimplemented feature
```

### 7.3 Debug

```tml
dbg(expr)            // prints and returns value
assert(cond)
assert(cond, msg)
assert_eq(a, b)
assert_ne(a, b)
debug_assert(cond)   // only in debug
```

### 7.4 Memory

```tml
size_of[T]()         // size in bytes
align_of[T]()        // alignment
drop(value)          // explicitly destroys value
forget(value)        // doesn't call destructor
```

### 7.5 String Utilities

These are low-level string utility functions available as global builtins:

```tml
str_len(s: Str) -> I32       // Length of string in bytes
str_eq(a: Str, b: Str) -> Bool  // Compare two strings for equality
str_hash(s: Str) -> I32      // Compute hash of a string
```

**Examples:**

```tml
let len: I32 = str_len("hello")      // 5
let same: Bool = str_eq("a", "a")    // true
let diff: Bool = str_eq("a", "b")    // false
let hash: I32 = str_hash("key")      // some I32 hash value

// str_hash is useful for implementing custom hash-based data structures
// Same strings always produce the same hash
assert(str_hash("test") == str_hash("test"))
```

**Note:** For most string operations, prefer using the `String` type methods (see section 1.5). These low-level functions are provided for cases where you need direct, simple operations without the overhead of method calls.

### 7.6 Time and Benchmarking

```tml
// ⚠️ DEPRECATED: Use Instant API instead
@deprecated(since: "v1.2", use: "Instant::now()")
time_ms() -> I32     // Current time in milliseconds

@deprecated(since: "v1.2", use: "Instant::now()")
time_us() -> I64     // Current time in microseconds

@deprecated(since: "v1.2", use: "Instant::now()")
time_ns() -> I64     // Current time in nanoseconds

// ✅ STABLE: Preferred API (like Rust's std::time::Instant)
Instant::now() -> I64                      // High-resolution timestamp (μs)
Instant::elapsed(start: I64) -> I64        // Duration since start (μs)
Duration::as_secs_f64(us: I64) -> F64      // Duration in seconds as float
Duration::as_millis_f64(us: I64) -> F64    // Duration in milliseconds as float

// Example: Benchmarking with format specifiers
let start: I64 = Instant::now()
expensive_computation()
let elapsed: I64 = Instant::elapsed(start)
let ms: F64 = Duration::as_millis_f64(elapsed)
println("Time: {:.3} ms", ms)  // e.g., "Time: 0.266 ms"

// Example: Multiple runs averaging
let mut total: I64 = 0
for _ in 0 to 10 {
    let start: I64 = Instant::now()
    some_function()
    total += Instant::elapsed(start)
}
let avg_ms: F64 = Duration::as_millis_f64(total / 10)
println("Average: {:.3} ms (10 runs)", avg_ms)
```

**Stability Notes:**
- `time_ms()`, `time_us()`, `time_ns()` are deprecated in favor of the `Instant` API
- The `Instant` API provides better ergonomics and is consistent with Rust's time API
- Compiler will emit warnings when using deprecated time functions
- Use `--allow-unstable` flag to suppress stability warnings during migration

## 8. Fundamental Behaviors

### 8.1 Equal

```tml
behavior Equal {
    func eq(this, other: This) -> Bool
    func ne(this, other: This) -> Bool {
        return not this.eq(other)
    }
}

// Usage: a == b, a != b
```

### 8.2 Ordered

```tml
type Ordering = Less | Equal | Greater

behavior Ordered: Equal {
    func compare(this, other: This) -> Ordering

    func lt(this, other: This) -> Bool
    func le(this, other: This) -> Bool
    func gt(this, other: This) -> Bool
    func ge(this, other: This) -> Bool
    func max(this, other: This) -> This
    func min(this, other: This) -> This
}

// Usage: a < b, a <= b, a > b, a >= b
```

### 8.3 Duplicate

```tml
behavior Duplicate {
    func duplicate(this) -> This
}

let copy: T = original.duplicate()
```

### 8.4 Default

```tml
behavior Default {
    func default() -> This
}

let x: I32 = I32.default()    // 0
let s: String = String.default() // ""
let l: List[I32] = List.default()   // []
```

### 8.5 Debug

```tml
behavior Debug {
    func debug(this) -> String
}

print(value.debug())
```

### 8.6 Hashable

```tml
behavior Hashable {
    func hash(this, hasher: mut ref Hasher)
}

// Required to use as key in Map/Set
```

### 8.7 Addable, Subtractable, Multipliable, Divisible

```tml
behavior Addable[Rhs = This] {
    type Output
    func add(this, rhs: Rhs) -> This.Output
}

// Similar for Subtractable, Multipliable, Divisible, Remainder
```

### 8.8 From / Into

```tml
behavior From[T] {
    func from(value: T) -> This
}

behavior Into[T] {
    func into(this) -> T
}

let s: String = String.from(42)
let n: I32 = "42".parse().unwrap()
```

---

*Previous: [12-ERRORS.md](./12-ERRORS.md)*
*Next: [14-EXAMPLES.md](./14-EXAMPLES.md) — Complete Examples*
