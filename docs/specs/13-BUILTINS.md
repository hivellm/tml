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
// Static methods (Type::method syntax)
I32::default()       // returns 0
I32::from(x)         // type conversion from other numeric types

// Common methods
x.abs()              // absolute value (signed)
x.signum()           // sign: -1, 0, or 1
x.pow(n)             // power
x.to_string()        // conversion to string
x.to_i32()           // conversion between types
x.duplicate()        // returns a copy
x.hash()             // returns I64 hash value
x.cmp(other)         // returns Ordering (Less, Equal, Greater)
x.checked_add(y)     // Maybe[T], Nothing on overflow
x.saturating_add(y)  // saturates at MAX/MIN
x.wrapping_add(y)    // wraps on overflow

// Constants
I32.MIN, I32.MAX
U64.MIN, U64.MAX
```

**Type Conversions:**

```tml
// Using Type::from() for explicit conversions
let a: I32 = I32::from(42i8)     // widen I8 to I32
let b: I64 = I64::from(100i32)   // widen I32 to I64
let c: F64 = F64::from(42)       // int to float
let d: I32 = I32::from(3.14f64)  // float to int (truncates)

// Using as keyword for inline conversions
let x: I64 = 42 as I64
let y: F32 = 3.14 as F32
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

// Querying
opt.is_just()        // Bool
opt.is_nothing()     // Bool
opt.contains(ref value)  // Bool (true if Just and contains value)

// Extracting values
opt.unwrap()         // T (panic if Nothing)
opt.unwrap_or(default)   // T (returns default if Nothing)
opt.unwrap_or_else(func) // T (calls func if Nothing)
opt.unwrap_or_default()  // T (uses T::default() if Nothing)
opt.expect(msg)      // T (panic with msg if Nothing)

// Transforming
opt.map(func)        // Maybe[U]
opt.map_or(default, func)      // U (returns default if Nothing)
opt.map_or_else(default_fn, func)  // U

// Chaining
opt.and_then(func)   // Maybe[U]
opt.alt(other)       // Maybe[T] (returns other if Nothing)
opt.or_else(func)    // Maybe[T]
opt.xor(other)       // Maybe[T] (returns Just if exactly one is Just)
opt.also(other)      // Maybe[U] (returns other if self is Just)

// Filtering
opt.filter(func)     // Maybe[T]

// Converting
opt.ok_or(err)       // Outcome[T, E]
opt.ok_or_else(err_fn)   // Outcome[T, E]

// Combining
opt.zip(other)       // Maybe[(T, U)]
opt.zip_with(other, func)  // Maybe[V]

// Flattening
opt.flatten()        // Maybe[T] (when opt is Maybe[Maybe[T]])

// Iteration
opt.iter()           // Iterator over 0 or 1 elements
```

## 4. Outcome[T, E]

```tml
Ok(value)
Err(error)

// Querying
res.is_ok()          // Bool
res.is_err()         // Bool
res.is_ok_and(func)  // Bool (true if Ok and func returns true)
res.is_err_and(func) // Bool (true if Err and func returns true)

// Extracting values
res.unwrap()         // T (panic if Err)
res.unwrap_err()     // E (panic if Ok)
res.unwrap_or(default)   // T (returns default if Err)
res.unwrap_or_else(func) // T (calls func(err) if Err)
res.unwrap_or_default()  // T (uses T::default() if Err)
res.expect(msg)      // T (panic with msg if Err)
res.expect_err(msg)  // E (panic with msg if Ok)

// Transforming
res.map(func)        // Outcome[U, E]
res.map_err(func)    // Outcome[T, F]
res.map_or(default, func)      // U (returns default if Err)
res.map_or_else(err_fn, ok_fn) // U

// Chaining
res.and_then(func)   // Outcome[U, E]
res.or(other)        // Outcome[T, E]
res.or_else(func)    // Outcome[T, F]

// Converting to Maybe
res.ok()             // Maybe[T]
res.err()            // Maybe[E]

// Iteration
res.iter()           // Iterator over 0 or 1 Ok values

// Flattening
res.flatten()        // Outcome[T, E] (when res is Outcome[Outcome[T, E], E])
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
assert_true(a)       // asserts a == true
assert_false(a)      // asserts a == false
assert_lt(a, b)      // asserts a < b
assert_gt(a, b)      // asserts a > b
assert_lte(a, b)     // asserts a <= b
assert_gte(a, b)     // asserts a >= b
assert_in_range(val, min, max)  // asserts min <= val <= max
assert_str_len(s, expected_len) // asserts s.len() == expected_len
assert_str_empty(s)             // asserts s.is_empty()
assert_str_not_empty(s)         // asserts not s.is_empty()
debug_assert(cond)   // only in debug
```

### 7.3.1 Compile-Time Constants

```tml
__FILE__             // Str: source file path (forward-slash normalized)
__DIRNAME__          // Str: directory of source file
__LINE__             // I64: current line number
```

### 7.4 Memory

```tml
size_of[T]()         // size in bytes
align_of[T]()        // alignment
drop(value)          // explicitly destroys value
forget(value)        // doesn't call destructor

// Low-level allocation (maps to malloc/free)
alloc(size: I32) -> *Unit       // Allocate size bytes
dealloc(ptr: *Unit) -> Unit     // Free allocated memory

// Higher-level memory functions
mem_alloc(size: I64) -> *Unit           // Allocate with I64 size
mem_alloc_zeroed(size: I64) -> *Unit    // Allocate zeroed memory
mem_realloc(ptr: *Unit, size: I64) -> *Unit  // Reallocate
mem_free(ptr: *Unit) -> Unit            // Free memory
mem_copy(dest: *Unit, src: *Unit, size: I64) -> Unit   // Copy memory
mem_move(dest: *Unit, src: *Unit, size: I64) -> Unit   // Move memory (overlapping safe)
mem_set(ptr: *Unit, value: I32, size: I64) -> Unit     // Fill memory
mem_zero(ptr: *Unit, size: I64) -> Unit                // Zero memory
mem_compare(a: *Unit, b: *Unit, size: I64) -> I32      // Compare memory
mem_eq(a: *Unit, b: *Unit, size: I64) -> Bool          // Memory equality
```

### 7.5 Atomic Operations

Thread-safe atomic operations for lock-free programming. All atomic operations use sequentially consistent ordering by default.

```tml
// Load and Store (type-specific: I32 and I64 variants)
atomic_load_i32(ptr: *Unit) -> I32           // Thread-safe read (I32)
atomic_load_i64(ptr: *Unit) -> I64           // Thread-safe read (I64)
atomic_store_i32(ptr: *Unit, val: I32) -> Unit  // Thread-safe write (I32)
atomic_store_i64(ptr: *Unit, val: I64) -> Unit  // Thread-safe write (I64)

// Arithmetic (fetch-and-op, returns old value)
atomic_fetch_add_i32(ptr: *Unit, val: I32) -> I32  // Atomic add (I32)
atomic_fetch_add_i64(ptr: *Unit, val: I64) -> I64  // Atomic add (I64)
atomic_fetch_sub_i32(ptr: *Unit, val: I32) -> I32  // Atomic sub (I32)
atomic_fetch_sub_i64(ptr: *Unit, val: I64) -> I64  // Atomic sub (I64)

// Exchange
atomic_swap_i32(ptr: *Unit, val: I32) -> I32  // Atomic swap (I32)
atomic_swap_i64(ptr: *Unit, val: I64) -> I64  // Atomic swap (I64)

// Compare-and-Exchange
atomic_compare_exchange_i32(ptr: *Unit, expected: I32, new: I32) -> I32  // CAS (I32)
atomic_compare_exchange_i64(ptr: *Unit, expected: I64, new: I64) -> I64  // CAS (I64)
// Returns the previous value. Compare with expected to check success.

// Memory Fences
atomic_fence()                 // Full sequentially consistent fence
atomic_fence_acquire()         // Acquire fence
atomic_fence_release()         // Release fence
```

**Example:**

```tml
let counter: *Unit = alloc(4)
atomic_store(counter, 0)

// Increment atomically
let old: I32 = atomic_add(counter, 1)
println("Old value: {}", old)  // 0

// Compare-and-swap
let success: Bool = atomic_cas(counter, 1, 10)
if success {
    println("Swapped 1 -> 10")
}

dealloc(counter)
```

### 7.6 Memory Fences

Memory barriers for controlling memory ordering between threads.

```tml
fence() -> Unit          // Full memory barrier (SeqCst)
fence_acquire() -> Unit  // Acquire barrier (loads/stores after can't move before)
fence_release() -> Unit  // Release barrier (loads/stores before can't move after)
```

### 7.7 Spinlock Primitives

Low-level spinlock operations for building synchronization primitives.

```tml
spin_lock(lock: *Unit) -> Unit     // Acquire lock (spins until acquired)
spin_unlock(lock: *Unit) -> Unit   // Release lock
spin_trylock(lock: *Unit) -> Bool  // Try to acquire (returns immediately)
```

**Example:**

```tml
let lock: *Unit = alloc(4)
atomic_store(lock, 0)  // Initialize to unlocked

// Try to acquire
if spin_trylock(lock) {
    // Critical section
    spin_unlock(lock)
}

// Blocking acquire
spin_lock(lock)
// Critical section
spin_unlock(lock)

dealloc(lock)
```

### 7.8 Thread Primitives

Basic thread management functions.

```tml
thread_yield() -> Unit   // Yield execution to other threads
thread_id() -> I64       // Get current thread ID
```

### 7.9 String Utilities

These are low-level string utility functions available as global builtins:

```tml
str_len(s: Str) -> I32               // Length of string in bytes
str_eq(a: Str, b: Str) -> Bool       // Compare two strings for equality
str_hash(s: Str) -> I32              // Compute hash of a string
str_concat(a: Str, b: Str) -> Str    // Concatenate two strings
str_substring(s: Str, start: I32, len: I32) -> Str  // Extract substring
str_contains(s: Str, sub: Str) -> Bool              // Check if contains substring
str_starts_with(s: Str, prefix: Str) -> Bool        // Check prefix
str_ends_with(s: Str, suffix: Str) -> Bool          // Check suffix
str_to_upper(s: Str) -> Str          // Convert to uppercase
str_to_lower(s: Str) -> Str          // Convert to lowercase
str_trim(s: Str) -> Str              // Remove leading/trailing whitespace
str_char_at(s: Str, index: I32) -> I32              // Get char code at index
```

**Examples:**

```tml
let len: I32 = str_len("hello")           // 5
let same: Bool = str_eq("a", "a")         // true
let diff: Bool = str_eq("a", "b")         // false
let hash: I32 = str_hash("key")           // some I32 hash value

// String manipulation
let full: Str = str_concat("Hello", " World")   // "Hello World"
let sub: Str = str_substring("Hello", 0, 2)     // "He"
let has: Bool = str_contains("Hello", "ell")    // true
let upper: Str = str_to_upper("hello")          // "HELLO"

// str_hash is useful for implementing custom hash-based data structures
// Same strings always produce the same hash
assert(str_hash("test") == str_hash("test"))
```

**Note:** For most string operations, prefer using the `String` type methods (see section 1.5). These low-level functions are provided for cases where you need direct, simple operations without the overhead of method calls.

### 7.10 Synchronization Primitives

Higher-level synchronization primitives for concurrent programming.

```tml
// Mutex (mutual exclusion lock)
mutex_create() -> *Unit         // Create a new mutex
mutex_destroy(m: *Unit) -> Unit // Destroy mutex
mutex_lock(m: *Unit) -> Unit    // Acquire lock (blocks)
mutex_unlock(m: *Unit) -> Unit  // Release lock
mutex_trylock(m: *Unit) -> Bool // Try to acquire (non-blocking)

// Channels (message passing)
channel_create() -> *Unit       // Create a new channel
channel_destroy(c: *Unit) -> Unit  // Destroy channel
channel_send(c: *Unit, val: I32) -> Unit   // Send value (blocks if full)
channel_recv(c: *Unit) -> I32              // Receive value (blocks if empty)
channel_try_send(c: *Unit, val: I32) -> Bool  // Non-blocking send
channel_try_recv(c: *Unit) -> Maybe[I32]   // Non-blocking receive
channel_len(c: *Unit) -> I32    // Number of items in channel

// WaitGroup (barrier synchronization)
waitgroup_create() -> *Unit     // Create a new waitgroup
waitgroup_destroy(wg: *Unit) -> Unit  // Destroy waitgroup
waitgroup_add(wg: *Unit, n: I32) -> Unit  // Add n tasks
waitgroup_done(wg: *Unit) -> Unit         // Signal task complete
waitgroup_wait(wg: *Unit) -> Unit         // Wait for all tasks

// Thread control
thread_sleep(ms: I32) -> Unit   // Sleep for milliseconds
thread_yield() -> Unit          // Yield to scheduler
thread_id() -> I64              // Get current thread ID
```

**Example:**

```tml
// Using mutex for thread-safe counter
let mutex: *Unit = mutex_create()
let counter: *Unit = alloc(4)
atomic_store(counter, 0)

mutex_lock(mutex)
let val: I32 = atomic_load(counter)
atomic_store(counter, val + 1)
mutex_unlock(mutex)

mutex_destroy(mutex)
dealloc(counter)
```

### 7.11 Time and Benchmarking

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

## 8. Intrinsics (lowlevel)

Intrinsics are low-level operations that map directly to LLVM instructions. They are declared with `@intrinsic` in `lib/core/src/intrinsics.tml` and used inside `lowlevel { }` blocks.

### 8.1 Slice Intrinsics

For implementing slice operations in the core library:

```tml
// Get element reference at index (no bounds check)
@intrinsic
pub func slice_get[T](data: ref T, index: I64) -> ref T

// Get mutable element reference at index (no bounds check)
@intrinsic
pub func slice_get_mut[T](data: mut ref T, index: I64) -> mut ref T

// Set element at index (no bounds check)
@intrinsic
pub func slice_set[T](data: mut ref T, index: I64, value: T)

// Compute offset pointer (for slice splitting)
@intrinsic
pub func slice_offset[T](data: ref T, count: I64) -> ref T

// Swap two elements (no bounds check)
@intrinsic
pub func slice_swap[T](data: mut ref T, a: I64, b: I64)
```

**Example Usage:**

```tml
impl[T] Slice[T] {
    pub func get(this, index: I64) -> Maybe[ref T] {
        if index < 0 or index >= this.len {
            return Nothing
        }
        lowlevel {
            Just(slice_get(this.data, index))
        }
    }

    pub func set(mut this, index: I64, value: T) -> Bool {
        if index < 0 or index >= this.len {
            return false
        }
        lowlevel {
            slice_set(this.data, index, value)
        }
        true
    }
}
```

### 8.2 Memory Intrinsics

```tml
@intrinsic pub func ptr_read[T](ptr: Ptr[T]) -> T
@intrinsic pub func ptr_write[T](ptr: Ptr[T], value: T)
@intrinsic pub func ptr_offset[T](ptr: Ptr[T], offset: I64) -> Ptr[T]
@intrinsic pub func ptr_to_int[T](ptr: Ptr[T]) -> I64
@intrinsic pub func int_to_ptr[T](addr: I64) -> Ptr[T]
```

### 8.3 Arithmetic Intrinsics (with overflow)

```tml
@intrinsic pub func checked_add[T](a: T, b: T) -> (T, Bool)   // (result, overflow)
@intrinsic pub func checked_sub[T](a: T, b: T) -> (T, Bool)
@intrinsic pub func checked_mul[T](a: T, b: T) -> (T, Bool)
@intrinsic pub func wrapping_add[T](a: T, b: T) -> T
@intrinsic pub func wrapping_sub[T](a: T, b: T) -> T
@intrinsic pub func wrapping_mul[T](a: T, b: T) -> T
@intrinsic pub func saturating_add[T](a: T, b: T) -> T
@intrinsic pub func saturating_sub[T](a: T, b: T) -> T
@intrinsic pub func saturating_mul[T](a: T, b: T) -> T
```

### 8.4 Bit Manipulation Intrinsics

```tml
@intrinsic pub func rotate_left[T](val: T, amount: U32) -> T
@intrinsic pub func rotate_right[T](val: T, amount: U32) -> T
@intrinsic pub func ctlz[T](val: T) -> T        // Count leading zeros
@intrinsic pub func cttz[T](val: T) -> T        // Count trailing zeros
@intrinsic pub func ctpop[T](val: T) -> T       // Population count (count 1 bits)
@intrinsic pub func bitreverse[T](val: T) -> T
@intrinsic pub func bswap[T](val: T) -> T       // Byte swap
```

### 8.5 Compiler Hints

```tml
@intrinsic pub func unreachable()               // Mark unreachable code
@intrinsic pub func assume(cond: Bool)          // Assume condition is true
@intrinsic pub func likely(cond: Bool) -> Bool  // Branch prediction hint
@intrinsic pub func unlikely(cond: Bool) -> Bool
```

### 8.6 Reflection Intrinsics (Compile-Time)

These intrinsics provide compile-time reflection over struct types:

```tml
use core::intrinsics::{field_count, field_name, field_offset, field_type_id, type_id}

// Type identification
type_id[T]() -> I64           // Unique hash for type T

// Struct field introspection
field_count[T]() -> I64       // Number of fields in struct T
field_name[T](index) -> Str   // Name of field at index
field_offset[T](index) -> I64 // Byte offset of field at index
field_type_id[T](index) -> I64 // Type ID of field's type
```

**Compile-Time Field Iteration:**

When iterating with `for i in 0 to field_count[T]()`, the compiler automatically unrolls the loop at compile time, allowing `field_name[T](i)` to work with the compile-time constant index:

```tml
type Point {
    x: I32,
    y: I32,
}

func print_field_names[T]() {
    for i in 0 to field_count[T]() {
        // Loop is unrolled at compile time
        // Each iteration, i is a compile-time constant
        let name: Str = field_name[T](i)
        println(name)
    }
}

// Usage:
print_field_names[Point]()  // Prints "x", then "y"
```

**Example: Generic Debug Formatter:**

```tml
func debug_struct[T](value: ref T) -> Str {
    var result: Str = type_name[T]() + " { "
    for i in 0 to field_count[T]() {
        if i > 0 {
            result = result + ", "
        }
        result = result + field_name[T](i) + ": ..."
    }
    result = result + " }"
    return result
}
```

## 9. Fundamental Behaviors

### 9.1 Equal

```tml
behavior Equal {
    func eq(this, other: This) -> Bool
    func ne(this, other: This) -> Bool {
        return not this.eq(other)
    }
}

// Usage: a == b, a != b
```

### 9.2 Ordering Enum

```tml
// Builtin enum for comparison results
enum Ordering {
    Less,
    Equal,
    Greater,
}

// Methods
ord.is_less() -> Bool
ord.is_equal() -> Bool
ord.is_greater() -> Bool
ord.to_string() -> Str      // "Less", "Equal", or "Greater"
ord.debug_string() -> Str   // "Ordering::Less", etc.
```

### 9.3 Ordered Behavior

```tml
behavior Ordered: Equal {
    func cmp(this, other: This) -> Ordering

    func lt(this, other: This) -> Bool
    func le(this, other: This) -> Bool
    func gt(this, other: This) -> Bool
    func ge(this, other: This) -> Bool
    func max(this, other: This) -> This
    func min(this, other: This) -> This
}

// Usage: a < b, a <= b, a > b, a >= b
// Using cmp() directly:
let order: Ordering = a.cmp(b)
when order {
    Less -> println("a < b"),
    Equal -> println("a == b"),
    Greater -> println("a > b"),
}
```

### 9.4 Duplicate

```tml
behavior Duplicate {
    func duplicate(this) -> This
}

let copy: T = original.duplicate()
```

### 9.5 Default

```tml
behavior Default {
    func default() -> This
}

let x: I32 = I32.default()    // 0
let s: String = String.default() // ""
let l: List[I32] = List.default()   // []
```

### 9.6 Debug

```tml
behavior Debug {
    func debug(this) -> String
}

print(value.debug())
```

### 9.7 Hashable

```tml
behavior Hashable {
    func hash(this, hasher: mut ref Hasher)
}

// Required to use as key in Map/Set
```

### 9.8 Addable, Subtractable, Multipliable, Divisible

```tml
behavior Addable[Rhs = This] {
    type Output
    func add(this, rhs: Rhs) -> This.Output
}

// Similar for Subtractable, Multipliable, Divisible, Remainder
```

### 9.9 From / Into

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
