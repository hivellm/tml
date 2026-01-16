# TML v1.0 — Standard Library Architecture

## 1. Overview

The TML standard library is organized in layers:
- **Core** (`std::core`): Minimal runtime, always linked
- **Alloc** (`std::alloc`): Heap allocation
- **Collections** (`std::collections`): Data structures
- **I/O** (`std::io`): Input/output operations
- **Platform** (`std::platform`): OS-specific functionality

## 2. Module Hierarchy

```
std
├── core           # Always available (no_std compatible)
│   ├── types      # Primitive types, Maybe, Outcome
│   ├── ops        # Operators and behaviors
│   ├── mem        # Memory operations
│   ├── ptr        # Raw pointers
│   ├── slice      # Slice operations
│   └── iter       # Iterator behavior
├── alloc          # Requires allocator
│   ├── string     # String type
│   ├── text       # Text type (dynamic strings with SSO)
│   ├── heap       # Heap[T]
│   ├── shared     # Shared[T], Weak[T]
│   ├── sync       # Sync[T], Weak[T]
│   └── vec        # List[T] (growable array)
├── collections    # Data structures
│   ├── map        # Map[K, V] (HashMap)
│   ├── set        # Set[T] (HashSet)
│   ├── btree      # BTreeMap, BTreeSet
│   ├── deque      # Deque[T]
│   └── heap       # BinaryHeap[T]
├── io             # I/O operations
│   ├── read       # Read behavior
│   ├── write      # Write behavior
│   ├── buf        # Buffered I/O
│   ├── file       # File operations
│   └── net        # Networking
├── ffi            # Foreign function interface
│   ├── c          # C interop types
│   └── os         # OS-specific types
├── sync           # Synchronization
│   ├── mutex      # Mutex[T]
│   ├── rwlock     # RwLock[T]
│   ├── atomic     # Atomic types
│   ├── channel    # Channels
│   └── once       # Once, OnceCell
├── thread         # Threading
│   ├── spawn      # Thread creation
│   ├── join       # JoinHandle
│   └── local      # Thread-local storage
├── time           # Time and duration
│   ├── instant    # Instant (monotonic)
│   ├── duration   # Duration
│   └── system     # SystemTime
├── fmt            # Formatting
│   ├── display    # Display behavior
│   ├── debug      # Debug behavior
│   └── write      # Format writing
├── hash           # Hashing
│   ├── hasher     # Hasher behavior
│   └── sip        # SipHash (default)
├── num            # Numeric utilities
│   ├── int        # Integer extensions
│   ├── float      # Float extensions
│   └── parse      # Parsing
├── env            # Environment
│   ├── args       # Command line args
│   └── vars       # Environment variables
├── path           # Path handling
│   ├── path       # Path type
│   └── pathbuf    # PathBuf type
├── process        # Process management
│   ├── command    # Command builder
│   ├── child      # Child process
│   └── stdio      # Standard I/O
├── panic          # Panic handling
│   ├── hook       # Panic hooks
│   └── catch      # Catch unwind
├── error          # Error types
│   └── error      # Error behavior
├── convert        # Type conversions
│   ├── from       # From/Into behaviors
│   └── try        # TryFrom/TryInto behaviors
├── cmp            # Comparison
│   ├── ord        # Ordering
│   └── eq         # Equality
├── duplicate      # Duplicating
│   └── duplicate  # Duplicate behavior
├── default        # Default values
│   └── default    # Default behavior
├── json           # JSON support (RFC-0015)
│   ├── value      # JsonValue, JsonNumber types
│   ├── parse      # Parser with integer preservation
│   ├── serialize  # Compact and pretty serialization
│   ├── builder    # Fluent builder API
│   ├── schema     # Schema validation
│   └── rpc        # JSON-RPC 2.0 for MCP
├── log            # Logging
│   ├── logger     # Logger behavior
│   └── macros     # log_*, println
└── test           # Testing framework
    ├── assert     # Assertions
    └── runner     # Test runner
```

## 3. Core Module (no_std)

### 3.1 Types

```tml
// std.core.types

// Already built-in but defined here for completeness
pub type Maybe[T] = Just(T) | Nothing
pub type Outcome[T, E] = Ok(T) | Err(E)
pub type Ordering = Less | Equal | Greater

// Unit type
pub type Unit = unit

// Never type (for functions that don't return)
pub type Never = !
```

### 3.2 Behaviors

```tml
// std.core.ops

pub behavior Equal {
    func eq(this, other: ref This) -> Bool
    func ne(this, other: ref This) -> Bool {
        return not this.eq(other)
    }
}

pub behavior Ordered: Equal {
    func cmp(this, other: ref This) -> Ordering

    func lt(this, other: ref This) -> Bool
    func le(this, other: ref This) -> Bool
    func gt(this, other: ref This) -> Bool
    func ge(this, other: ref This) -> Bool
    func max(this, other: This) -> This
    func min(this, other: This) -> This
}

pub behavior Duplicate {
    func duplicate(this) -> This
}

pub behavior Default {
    func default() -> This
}

pub behavior Disposable {
    func drop(this)
}

pub behavior Hash {
    func hash(this, hasher: mut ref Hasher)
}

// Operator behaviors
pub behavior Add[Rhs = This] {
    type Output
    func add(this, rhs: Rhs) -> This.Output
}

pub behavior Sub[Rhs = This] {
    type Output
    func sub(this, rhs: Rhs) -> This.Output
}

pub behavior Mul[Rhs = This] {
    type Output
    func mul(this, rhs: Rhs) -> This.Output
}

pub behavior Div[Rhs = This] {
    type Output
    func div(this, rhs: Rhs) -> This.Output
}

pub behavior Neg {
    type Output
    func neg(this) -> This.Output
}

pub behavior Index[Idx] {
    type Output
    func index(this, idx: Idx) -> ref This.Output
}

pub behavior IndexMut[Idx]: Index[Idx] {
    func index_mut(this, idx: Idx) -> mut ref This.Output
}
```

### 3.3 Iterator

```tml
// std.core.iter

pub behavior Iterator {
    type Item

    func next(this) -> Maybe[This.Item]

    // Provided methods (can be overridden for efficiency)
    func count(this) -> U64 {
        var n: U64 = 0
        loop _ in this { n += 1 }
        return n
    }

    func last(this) -> Maybe[This.Item] {
        var last: Maybe[This.Item] = Nothing
        loop item in this { last = Just(item) }
        return last
    }

    func nth(this, n: U64) -> Maybe[This.Item] {
        loop _ in 0 to n {
            if this.next().is_nothing() {
                return Nothing
            }
        }
        return this.next()
    }

    func skip(this, n: U64) -> Skip[This] {
        return Skip { iter: this, n: n }
    }

    func take(this, n: U64) -> Take[This] {
        return Take { iter: this, n: n }
    }

    func map[B, F: Callable[(This.Item), B]](this, f: F) -> Map[This, F] {
        return Map { iter: this, f: f }
    }

    func filter[F: Callable[(ref This.Item), Bool]](this, f: F) -> Filter[This, F] {
        return Filter { iter: this, f: f }
    }

    func fold[B, F: Callable[(B, This.Item), B]](this, init: B, f: F) -> B {
        var acc: T = init
        loop item in this {
            acc = f(acc, item)
        }
        return acc
    }

    func collect[C: FromIterator[This.Item]](this) -> C {
        return C.from_iter(this)
    }

    func any[F: Callable[(ref This.Item), Bool]](this, f: F) -> Bool {
        loop item in this {
            if f(ref item) { return true }
        }
        return false
    }

    func all[F: Callable[(ref This.Item), Bool]](this, f: F) -> Bool {
        loop item in this {
            if not f(ref item) { return false }
        }
        return true
    }

    func find[F: Callable[(ref This.Item), Bool]](this, f: F) -> Maybe[This.Item] {
        loop item in this {
            if f(ref item) { return Just(item) }
        }
        return Nothing
    }
}

pub behavior IntoIterator {
    type Item
    type IntoIter: Iterator[Item = This.Item]

    func into_iter(this) -> This.IntoIter
}

pub behavior FromIterator[A] {
    func from_iter[I: Iterator[Item = A]](iter: I) -> This
}
```

## 4. Alloc Module

### 4.1 Heap

```tml
// std.alloc.heap

pub type Heap[T] {
    ptr: *mut T,
}

extend Heap[T] {
    pub func new(value: T) -> This {
        lowlevel {
            let ptr: ptr T = tml_alloc(size_of[T](), align_of[T]()) as *mut T
            ptr.write(value)
            return This { ptr: ptr }
        }
    }

    pub func into_inner(this) -> T {
        lowlevel {
            let value: T = this.ptr.read()
            tml_dealloc(this.ptr as *mut U8, size_of[T](), align_of[T]())
            forget(this)
            return value
        }
    }
}

extend Heap[T] with Disposable {
    func drop(this) {
        lowlevel {
            drop_in_place(this.ptr)
            tml_dealloc(this.ptr as *mut U8, size_of[T](), align_of[T]())
        }
    }
}
```

### 4.2 String

```tml
// std.alloc.string

pub type String {
    buf: List[U8],
}

extend String {
    pub func new() -> This {
        return This { buf: List.new() }
    }

    pub func from(s: ref str) -> This {
        var buf: List[U8] = List.with_capacity(s.len())
        buf.extend_from_slice(s.as_bytes())
        return This { buf: buf }
    }

    pub func len(this) -> U64 {
        return this.buf.len()
    }

    pub func is_empty(this) -> Bool {
        return this.buf.is_empty()
    }

    pub func push(this, c: Char) {
        var buf: [U8; 4] = [0, 0, 0, 0]
        let len: U64 = c.encode_utf8(mut ref buf)
        this.buf.extend_from_slice(ref buf[0 to len])
    }

    pub func push_str(this, s: ref str) {
        this.buf.extend_from_slice(s.as_bytes())
    }

    pub func as_str(this) -> ref str {
        lowlevel {
            str.from_utf8_unchecked(this.buf.as_slice())
        }
    }

    pub func chars(this) -> Chars {
        return Chars { inner: this.as_str() }
    }
}

extend String with Add[ref str] {
    type Output = String

    func add(this, other: ref str) -> String {
        var result: This = this.duplicate()
        result.push_str(other)
        return result
    }
}
```

### 4.3 Text

Dynamic, growable string with Small String Optimization (SSO).

```tml
// std.text
use std::text::Text

// Constructors
pub func new() -> Text
pub func from(s: Str) -> Text
pub func with_capacity(cap: I64) -> Text

// Template literal syntax produces Text
let greeting: Text = `Hello, {name}!`

// Properties
pub func len(this) -> I64
pub func capacity(this) -> I64
pub func is_empty(this) -> Bool
pub func byte_at(this, idx: I64) -> I32

// Modification
pub func push(this, c: I32)
pub func push_str(this, s: Str)
pub func clear(this)
pub func reserve(this, additional: I64)

// Conversion
pub func as_str(this) -> Str
pub func clone(this) -> Text
pub func drop(this)

// Search
pub func contains(this, search: Str) -> Bool
pub func starts_with(this, prefix: Str) -> Bool
pub func ends_with(this, suffix: Str) -> Bool
pub func index_of(this, search: Str) -> I64
pub func last_index_of(this, search: Str) -> I64

// Transformation (returns new Text)
pub func to_upper_case(this) -> Text
pub func to_lower_case(this) -> Text
pub func trim(this) -> Text
pub func trim_start(this) -> Text
pub func trim_end(this) -> Text
pub func substring(this, start: I64, end: I64) -> Text
pub func replace(this, search: Str, replacement: Str) -> Text
pub func replace_all(this, search: Str, replacement: Str) -> Text
pub func repeat(this, count: I64) -> Text
pub func reverse(this) -> Text
pub func pad_start(this, len: I64, pad: I32) -> Text
pub func pad_end(this, len: I64, pad: I32) -> Text

// Concatenation
pub func concat(this, other: Str) -> Text
pub func concat_str(this, s: Str) -> Text

// Comparison
pub func compare(this, other: ref Text) -> I32
pub func equals(this, other: ref Text) -> Bool

// Output
pub func print(this)
pub func println(this)

// Static constructors
pub func from_i64(n: I64) -> Text
pub func from_f64(n: F64) -> Text
pub func from_f64_precision(n: F64, precision: I32) -> Text
pub func from_bool(b: Bool) -> Text
```

**SSO (Small String Optimization):**
- Strings ≤23 bytes are stored inline without heap allocation
- Threshold: 23 bytes (fits in 24-byte struct minus length byte)
- Growth strategy: 2x until 4KB, then 1.5x

**Example:**
```tml
use std::text::Text

func greet(name: Str) -> Text {
    let greeting: Text = `Hello, {name}!`
    greeting.push_str(" Welcome!")
    return greeting
}

func main() {
    let msg: Text = greet("World")
    msg.println()  // "Hello, World! Welcome!"
    msg.drop()     // Required: free memory
}
```

### 4.4 List (Vec)

```tml
// std.alloc.vec

pub type List[T] {
    ptr: *mut T,
    len: U64,
    cap: U64,
}

extend List[T] {
    pub func new() -> This {
        return This { ptr: null, len: 0, cap: 0 }
    }

    pub func with_capacity(cap: U64) -> This {
        if cap == 0 {
            return This.new()
        }
        lowlevel {
            let ptr: ptr T = tml_alloc(cap * size_of[T](), align_of[T]()) as *mut T
            return This { ptr: ptr, len: 0, cap: cap }
        }
    }

    pub func len(this) -> U64 { this.len }
    pub func capacity(this) -> U64 { this.cap }
    pub func is_empty(this) -> Bool { this.len == 0 }

    pub func push(this, value: T) {
        if this.len == this.cap {
            this.grow()
        }
        lowlevel {
            this.ptr.add(this.len).write(value)
        }
        this.len += 1
    }

    pub func pop(this) -> Maybe[T] {
        if this.len == 0 {
            return Nothing
        }
        this.len -= 1
        lowlevel {
            return Just(this.ptr.add(this.len).read())
        }
    }

    pub func get(this, index: U64) -> Maybe[ref T] {
        if index >= this.len {
            return Nothing
        }
        lowlevel {
            return Just(ref *this.ptr.add(index))
        }
    }

    func grow(this) {
        let new_cap: U64 = if this.cap == 0 { 4 } else { this.cap * 2 }
        lowlevel {
            let new_ptr: ptr T = tml_alloc(new_cap * size_of[T](), align_of[T]()) as *mut T
            if this.ptr != null {
                tml_memcpy(new_ptr as *mut U8, this.ptr as *const U8, this.len * size_of[T]())
                tml_dealloc(this.ptr as *mut U8, this.cap * size_of[T](), align_of[T]())
            }
            this.ptr = new_ptr
            this.cap = new_cap
        }
    }
}

extend List[T] with Index[U64] {
    type Output = T

    func index(this, idx: U64) -> ref T {
        when this.get(idx) {
            Just(v) -> v,
            Nothing -> panic("index out of bounds"),
        }
    }
}

extend List[T] with Disposable {
    func drop(this) {
        lowlevel {
            // Drop all elements
            loop i in 0 to this.len {
                drop_in_place(this.ptr.add(i))
            }
            // Free buffer
            if this.ptr != null {
                tml_dealloc(this.ptr as *mut U8, this.cap * size_of[T](), align_of[T]())
            }
        }
    }
}
```

## 5. I/O Module

### 5.1 Read/Write Traits

```tml
// std.io

pub behavior Read {
    func read(this, buf: mut ref [U8]) -> Outcome[U64, IoError]

    func read_exact(this, buf: mut ref [U8]) -> Outcome[Unit, IoError] {
        var filled: U64 = 0
        loop while filled < buf.len() {
            let n: Outcome[U64, Error] = this.read(mut ref buf[filled to buf.len()])!
            if n == 0 {
                return Err(IoError.UnexpectedEof)
            }
            filled += n
        }
        return Ok(unit)
    }

    func read_to_end(this, buf: mut ref List[U8]) -> Outcome[U64, IoError] {
        var read_total: U64 = 0
        var chunk: [U8; 1024] = [0; 1024]
        loop {
            let n: Outcome[U64, Error] = this.read(mut ref chunk)!
            if n == 0 {
                break
            }
            buf.extend_from_slice(ref chunk[0 to n])
            read_total += n
        }
        return Ok(read_total)
    }

    func read_to_string(this, buf: mut ref String) -> Outcome[U64, IoError] {
        var bytes: List[T] = List.new()
        let n: U64 = this.read_to_end(mut ref bytes)!
        let s: String = String.from_utf8(bytes)!
        buf.push_str(ref s)
        return Ok(n)
    }
}

pub behavior Write {
    func write(this, buf: ref [U8]) -> Outcome[U64, IoError]
    func flush(this) -> Outcome[Unit, IoError]

    func write_all(this, buf: ref [U8]) -> Outcome[Unit, IoError] {
        var written: U64 = 0
        loop while written < buf.len() {
            let n: Outcome[U64, Error] = this.write(ref buf[written to buf.len()])!
            if n == 0 {
                return Err(IoError.WriteZero)
            }
            written += n
        }
        return Ok(unit)
    }
}
```

### 5.2 File

```tml
// std.io.file

pub type File {
    handle: RawHandle,
}

extend File {
    pub func open(path: ref Path) -> Outcome[This, IoError]
    effects: [io::file::read]
    {
        return This.open_options(path, OpenOptions.read())
    }

    pub func create(path: ref Path) -> Outcome[This, IoError]
    effects: [io::file::write]
    {
        return This.open_options(path, OpenOptions.write().create(true).truncate(true))
    }

    pub func open_options(path: ref Path, opts: OpenOptions) -> Outcome[This, IoError]
    effects: [io::file]
    {
        let handle: Outcome[Handle, Error] = platform_open(path, opts)!
        return Ok(This { handle: handle })
    }
}

extend File with Read {
    func read(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io::file::read]
    {
        return platform_read(this.handle, buf)
    }
}

extend File with Write {
    func write(this, buf: ref [U8]) -> Outcome[U64, IoError]
    effects: [io::file::write]
    {
        return platform_write(this.handle, buf)
    }

    func flush(this) -> Outcome[Unit, IoError]
    effects: [io::file::write]
    {
        return platform_flush(this.handle)
    }
}

extend File with Disposable {
    func drop(this) {
        platform_close(this.handle)
    }
}
```

## 6. Sync Module

### 6.1 Mutex

```tml
// std.sync.mutex

pub type Mutex[T] {
    inner: RawMutex,
    data: LowlevelCell[T],
}

pub type MutexGuard[T] {
    lock: ref Mutex[T],
}

extend Mutex[T] {
    pub func new(value: T) -> This {
        return This {
            inner: RawMutex.new(),
            data: LowlevelCell.new(value),
        }
    }

    pub func lock(this) -> MutexGuard[T] {
        this.inner.lock()
        return MutexGuard { lock: this }
    }

    pub func try_lock(this) -> Maybe[MutexGuard[T]] {
        if this.inner.try_lock() {
            return Just(MutexGuard { lock: this })
        }
        return Nothing
    }
}

extend MutexGuard[T] {
    pub func get(this) -> ref T {
        lowlevel { ref *this.lock.data.get() }
    }

    pub func get_mut(this) -> mut ref T {
        lowlevel { mut ref *this.lock.data.get() }
    }
}

extend MutexGuard[T] with Disposable {
    func drop(this) {
        this.lock.inner.unlock()
    }
}
```

### 6.2 Atomic Types

```tml
// std.sync.atomic

pub type AtomicBool { inner: U8 }
pub type AtomicI32 { inner: I32 }
pub type AtomicI64 { inner: I64 }
pub type AtomicU32 { inner: U32 }
pub type AtomicU64 { inner: U64 }
pub type AtomicPtr[T] { inner: *mut T }

pub type MemoryOrdering = Relaxed | Acquire | Release | AcqRel | SeqCst

extend AtomicI64 {
    pub func new(value: I64) -> This {
        return This { inner: value }
    }

    pub func load(this, order: MemoryOrdering) -> I64 {
        intrinsic_atomic_load(ref this.inner, order)
    }

    pub func store(this, value: I64, order: MemoryOrdering) {
        intrinsic_atomic_store(ref this.inner, value, order)
    }

    pub func swap(this, value: I64, order: MemoryOrdering) -> I64 {
        intrinsic_atomic_swap(ref this.inner, value, order)
    }

    pub func compare_exchange(
        this,
        current: I64,
        new: I64,
        success: MemoryOrdering,
        failure: MemoryOrdering
    ) -> Outcome[I64, I64] {
        intrinsic_atomic_cmpxchg(ref this.inner, current, new, success, failure)
    }

    pub func fetch_add(this, value: I64, order: MemoryOrdering) -> I64 {
        intrinsic_atomic_fetch_add(ref this.inner, value, order)
    }

    pub func fetch_sub(this, value: I64, order: MemoryOrdering) -> I64 {
        intrinsic_atomic_fetch_sub(ref this.inner, value, order)
    }
}
```

## 7. Thread Module

```tml
// std.thread

pub type JoinHandle[T] {
    handle: RawThread,
    result: Sync[Mutex[Maybe[T]]],
}

pub func spawn[F, T](f: F) -> JoinHandle[T]
where F: Callable[(), T] + Sendable,
      T: Sendable
effects: [io::sync]
{
    let result: Sync[Mutex[Maybe[T]]] = Sync.new(Mutex.new(Nothing))
    let result_copy: Sync[Mutex[Maybe[T]]] = result.duplicate()

    let handle: Handle = platform_spawn(do() {
        let value: T = f()
        *result_copy.lock().get_mut() = Just(value)
    })

    return JoinHandle { handle: handle, result: result }
}

extend JoinHandle[T] {
    pub func join(this) -> Outcome[T, JoinError]
    effects: [io::sync]
    {
        platform_join(this.handle)!

        let guard: MutexGuard[Maybe[T]] = this.result.lock()
        when guard.get_mut().take() {
            Just(value) -> Ok(value),
            Nothing -> Err(JoinError.Panicked),
        }
    }

    pub func is_finished(this) -> Bool {
        platform_is_finished(this.handle)
    }
}
```

## 8. JSON Module

> **See Also:** [RFC-0015-JSON.md](../rfcs/RFC-0015-JSON.md) for full specification.

Native JSON implementation with integer precision preservation for MCP integration.

### 8.1 JsonNumber

Discriminated union preserving numeric precision:

```tml
// std.json.value

pub type JsonNumber {
    Int(I64),       // Integer without decimal
    Uint(U64),      // Large unsigned integer
    Float(F64),     // Number with decimal/exponent
}
```

### 8.2 JsonValue

```tml
pub type JsonValue {
    Null,
    Bool(Bool),
    Number(JsonNumber),
    String(String),
    Array(List[JsonValue]),
    Object(Map[String, JsonValue]),
}

extend JsonValue {
    // Type queries
    pub func is_null(this) -> Bool
    pub func is_bool(this) -> Bool
    pub func is_number(this) -> Bool
    pub func is_integer(this) -> Bool  // Int64 or Uint64
    pub func is_float(this) -> Bool    // Float64
    pub func is_string(this) -> Bool
    pub func is_array(this) -> Bool
    pub func is_object(this) -> Bool

    // Value accessors
    pub func as_bool(this) -> Maybe[Bool]
    pub func as_i64(this) -> Maybe[I64]
    pub func as_u64(this) -> Maybe[U64]
    pub func as_f64(this) -> Maybe[F64]
    pub func as_str(this) -> Maybe[ref String]
    pub func as_array(this) -> Maybe[ref List[JsonValue]]
    pub func as_object(this) -> Maybe[ref Map[String, JsonValue]]

    // Object access
    pub func get(this, key: ref String) -> Maybe[ref JsonValue]

    // Serialization
    pub func to_string(this) -> String
    pub func to_string_pretty(this, indent: I32) -> String
}
```

### 8.3 Parser

```tml
// std.json.parse

pub func parse(input: ref String) -> Outcome[JsonValue, JsonError]

pub type JsonError {
    message: String,
    line: U64,
    column: U64,
}
```

Features:
- RFC 8259 compliant
- Integer detection (no `.` or `e/E` → stored as Int64/Uint64)
- Max depth limit: 1000 levels
- Line/column error tracking

### 8.4 Builder API

```tml
// std.json.builder

let user = object()
    .set("name", "Alice")
    .set("age", 30)
    .set("roles", array()
        .push("admin")
        .push("user")
        .build())
    .build()
```

### 8.5 JSON-RPC 2.0

For MCP (Model Context Protocol) integration:

```tml
// std.json.rpc

pub type JsonRpcRequest {
    method: String,
    params: Maybe[JsonValue],
    id: Maybe[JsonValue],  // null for notifications
}

pub type JsonRpcResponse {
    result: Maybe[JsonValue],
    error: Maybe[JsonRpcError],
    id: JsonValue,
}

pub type JsonRpcError {
    code: I32,
    message: String,
    data: Maybe[JsonValue],
}

// Standard error codes
pub const PARSE_ERROR: I32 = -32700
pub const INVALID_REQUEST: I32 = -32600
pub const METHOD_NOT_FOUND: I32 = -32601
pub const INVALID_PARAMS: I32 = -32602
pub const INTERNAL_ERROR: I32 = -32603
```

### 8.6 Schema Validation

```tml
// std.json.schema

let schema = JsonSchema.object()
    .required("name", JsonSchema.string())
    .required("age", JsonSchema.integer())
    .optional("email", JsonSchema.string())

let result = schema.validate(ref json)
when result {
    Ok(()) => println("Valid"),
    Err(error) => println("Invalid: {error.message} at {error.path}"),
}
```

## 9. Extension Points

### 9.1 Plugin Behaviors

```tml
// Libraries can implement these to extend functionality

// Custom allocator
pub behavior GlobalAlloc {
    lowlevel func alloc(this, layout: Layout) -> *mut U8
    lowlevel func dealloc(this, ptr: *mut U8, layout: Layout)
    lowlevel func realloc(this, ptr: *mut U8, old: Layout, new: Layout) -> *mut U8
}

// Custom hasher
pub behavior BuildHasher {
    type Hasher: Hasher
    func build_hasher(this) -> This.Hasher
}

// Custom error
pub behavior Error: Debug + Display {
    func source(this) -> Maybe[ref any Error] { Nothing }
}

// Custom serialization
pub behavior Serialize {
    func serialize[S: Serializer](this, serializer: mut ref S) -> Outcome[Unit, S.Error]
}

pub behavior Deserialize {
    func deserialize[D: Deserializer](deserializer: mut ref D) -> Outcome[This, D.Error]
}
```

### 9.2 Library Extension

```tml
// Example: custom Map implementation

use std::collections::Map

// Third-party library can extend Map
extend Map[K, V] with custom_lib::Serialize where K: Serialize, V: Serialize {
    func serialize[S: Serializer](this, s: mut ref S) -> Outcome[Unit, S.Error] {
        s.serialize_map_start(this.len())!
        loop (k, v) in this.entries() {
            k.serialize(s)!
            v.serialize(s)!
        }
        s.serialize_map_end()
    }
}
```

---

*Previous: [19-RUNTIME.md](./19-RUNTIME.md)*
*Next: [21-TARGETS.md](./21-TARGETS.md) — Cross-Compilation Targets*
