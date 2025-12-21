# TML v1.0 — Standard Library Architecture

## 1. Overview

The TML standard library is organized in layers:
- **Core** (`std.core`): Minimal runtime, always linked
- **Alloc** (`std.alloc`): Heap allocation
- **Collections** (`std.collections`): Data structures
- **I/O** (`std.io`): Input/output operations
- **Platform** (`std.platform`): OS-specific functionality

## 2. Module Hierarchy

```
std
├── core           # Always available (no_std compatible)
│   ├── types      # Primitive types, Option, Result
│   ├── ops        # Operators and traits
│   ├── mem        # Memory operations
│   ├── ptr        # Raw pointers
│   ├── slice      # Slice operations
│   └── iter       # Iterator trait
├── alloc          # Requires allocator
│   ├── string     # String type
│   ├── boxed      # Box[T]
│   ├── rc         # Rc[T], Weak[T]
│   ├── arc        # Arc[T], Weak[T]
│   └── vec        # List[T] (growable array)
├── collections    # Data structures
│   ├── map        # Map[K, V] (HashMap)
│   ├── set        # Set[T] (HashSet)
│   ├── btree      # BTreeMap, BTreeSet
│   ├── deque      # Deque[T]
│   └── heap       # BinaryHeap[T]
├── io             # I/O operations
│   ├── read       # Read trait
│   ├── write      # Write trait
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
│   ├── display    # Display trait
│   ├── debug      # Debug trait
│   └── write      # Format writing
├── hash           # Hashing
│   ├── hasher     # Hasher trait
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
│   └── error      # Error trait
├── convert        # Type conversions
│   ├── from       # From/Into traits
│   └── try        # TryFrom/TryInto
├── cmp            # Comparison
│   ├── ord        # Ordering
│   └── eq         # Equality
├── clone          # Cloning
│   └── clone      # Clone trait
├── default        # Default values
│   └── default    # Default trait
├── json           # JSON support
│   ├── value      # JsonValue type
│   ├── parse      # JSON parser
│   └── serialize  # Serialization
├── log            # Logging
│   ├── logger     # Logger trait
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
public type Option[T] = Some(T) | None
public type Result[T, E] = Ok(T) | Err(E)
public type Ordering = Less | Equal | Greater

// Unit type
public type Unit = unit

// Never type (for functions that don't return)
public type Never = !
```

### 3.2 Traits

```tml
// std.core.ops

public trait Eq {
    func eq(this, other: &This) -> Bool
    func ne(this, other: &This) -> Bool {
        return not this.eq(other)
    }
}

public trait Ord: Eq {
    func cmp(this, other: &This) -> Ordering

    func lt(this, other: &This) -> Bool
    func le(this, other: &This) -> Bool
    func gt(this, other: &This) -> Bool
    func ge(this, other: &This) -> Bool
    func max(this, other: This) -> This
    func min(this, other: This) -> This
}

public trait Clone {
    func clone(this) -> This
}

public trait Default {
    func default() -> This
}

public trait Drop {
    func drop(this)
}

public trait Hash {
    func hash(this, hasher: &mut Hasher)
}

// Operator traits
public trait Add[Rhs = This] {
    type Output
    func add(this, rhs: Rhs) -> This.Output
}

public trait Sub[Rhs = This] {
    type Output
    func sub(this, rhs: Rhs) -> This.Output
}

public trait Mul[Rhs = This] {
    type Output
    func mul(this, rhs: Rhs) -> This.Output
}

public trait Div[Rhs = This] {
    type Output
    func div(this, rhs: Rhs) -> This.Output
}

public trait Neg {
    type Output
    func neg(this) -> This.Output
}

public trait Index[Idx] {
    type Output
    func index(this, idx: Idx) -> &This.Output
}

public trait IndexMut[Idx]: Index[Idx] {
    func index_mut(this, idx: Idx) -> &mut This.Output
}
```

### 3.3 Iterator

```tml
// std.core.iter

public trait Iterator {
    type Item

    func next(this) -> Option[This.Item]

    // Provided methods (can be overridden for efficiency)
    func count(this) -> U64 {
        var n: U64 = 0
        loop _ in this { n += 1 }
        return n
    }

    func last(this) -> Option[This.Item] {
        var last: Option[This.Item] = None
        loop item in this { last = Some(item) }
        return last
    }

    func nth(this, n: U64) -> Option[This.Item] {
        loop _ in 0..n {
            if this.next().is_none() {
                return None
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

    func map[B, F: Fn(This.Item) -> B](this, f: F) -> Map[This, F] {
        return Map { iter: this, f: f }
    }

    func filter[F: Fn(&This.Item) -> Bool](this, f: F) -> Filter[This, F] {
        return Filter { iter: this, f: f }
    }

    func fold[B, F: Fn(B, This.Item) -> B](this, init: B, f: F) -> B {
        var acc = init
        loop item in this {
            acc = f(acc, item)
        }
        return acc
    }

    func collect[C: FromIterator[This.Item]](this) -> C {
        return C.from_iter(this)
    }

    func any[F: Fn(&This.Item) -> Bool](this, f: F) -> Bool {
        loop item in this {
            if f(&item) { return true }
        }
        return false
    }

    func all[F: Fn(&This.Item) -> Bool](this, f: F) -> Bool {
        loop item in this {
            if not f(&item) { return false }
        }
        return true
    }

    func find[F: Fn(&This.Item) -> Bool](this, f: F) -> Option[This.Item] {
        loop item in this {
            if f(&item) { return Some(item) }
        }
        return None
    }
}

public trait IntoIterator {
    type Item
    type IntoIter: Iterator[Item = This.Item]

    func into_iter(this) -> This.IntoIter
}

public trait FromIterator[A] {
    func from_iter[I: Iterator[Item = A]](iter: I) -> This
}
```

## 4. Alloc Module

### 4.1 Box

```tml
// std.alloc.boxed

public type Box[T] {
    ptr: *mut T,
}

extend Box[T] {
    public func new(value: T) -> This {
        unsafe {
            let ptr = tml_alloc(size_of[T](), align_of[T]()) as *mut T
            ptr.write(value)
            return This { ptr: ptr }
        }
    }

    public func into_inner(this) -> T {
        unsafe {
            let value = this.ptr.read()
            tml_dealloc(this.ptr as *mut U8, size_of[T](), align_of[T]())
            forget(this)
            return value
        }
    }
}

extend Box[T] with Drop {
    func drop(this) {
        unsafe {
            drop_in_place(this.ptr)
            tml_dealloc(this.ptr as *mut U8, size_of[T](), align_of[T]())
        }
    }
}
```

### 4.2 String

```tml
// std.alloc.string

public type String {
    buf: List[U8],
}

extend String {
    public func new() -> This {
        return This { buf: List.new() }
    }

    public func from(s: &str) -> This {
        var buf = List.with_capacity(s.len())
        buf.extend_from_slice(s.as_bytes())
        return This { buf: buf }
    }

    public func len(this) -> U64 {
        return this.buf.len()
    }

    public func is_empty(this) -> Bool {
        return this.buf.is_empty()
    }

    public func push(this, c: Char) {
        let mut buf: [U8; 4] = [0, 0, 0, 0]
        let len = c.encode_utf8(&mut buf)
        this.buf.extend_from_slice(&buf[0..len])
    }

    public func push_str(this, s: &str) {
        this.buf.extend_from_slice(s.as_bytes())
    }

    public func as_str(this) -> &str {
        unsafe {
            str.from_utf8_unchecked(this.buf.as_slice())
        }
    }

    public func chars(this) -> Chars {
        return Chars { inner: this.as_str() }
    }
}

extend String with Add[&str] {
    type Output = String

    func add(this, other: &str) -> String {
        var result = this.clone()
        result.push_str(other)
        return result
    }
}
```

### 4.3 List (Vec)

```tml
// std.alloc.vec

public type List[T] {
    ptr: *mut T,
    len: U64,
    cap: U64,
}

extend List[T] {
    public func new() -> This {
        return This { ptr: null, len: 0, cap: 0 }
    }

    public func with_capacity(cap: U64) -> This {
        if cap == 0 {
            return This.new()
        }
        unsafe {
            let ptr = tml_alloc(cap * size_of[T](), align_of[T]()) as *mut T
            return This { ptr: ptr, len: 0, cap: cap }
        }
    }

    public func len(this) -> U64 { this.len }
    public func capacity(this) -> U64 { this.cap }
    public func is_empty(this) -> Bool { this.len == 0 }

    public func push(this, value: T) {
        if this.len == this.cap {
            this.grow()
        }
        unsafe {
            this.ptr.add(this.len).write(value)
        }
        this.len += 1
    }

    public func pop(this) -> Option[T] {
        if this.len == 0 {
            return None
        }
        this.len -= 1
        unsafe {
            return Some(this.ptr.add(this.len).read())
        }
    }

    public func get(this, index: U64) -> Option[&T] {
        if index >= this.len {
            return None
        }
        unsafe {
            return Some(&*this.ptr.add(index))
        }
    }

    func grow(this) {
        let new_cap = if this.cap == 0 { 4 } else { this.cap * 2 }
        unsafe {
            let new_ptr = tml_alloc(new_cap * size_of[T](), align_of[T]()) as *mut T
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

    func index(this, idx: U64) -> &T {
        when this.get(idx) {
            Some(v) -> v,
            None -> panic("index out of bounds"),
        }
    }
}

extend List[T] with Drop {
    func drop(this) {
        unsafe {
            // Drop all elements
            loop i in 0..this.len {
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

public trait Read {
    func read(this, buf: &mut [U8]) -> Result[U64, IoError]

    func read_exact(this, buf: &mut [U8]) -> Result[Unit, IoError] {
        var filled: U64 = 0
        loop while filled < buf.len() {
            let n = this.read(&mut buf[filled..])!
            if n == 0 {
                return Err(IoError.UnexpectedEof)
            }
            filled += n
        }
        return Ok(unit)
    }

    func read_to_end(this, buf: &mut List[U8]) -> Result[U64, IoError] {
        var read_total: U64 = 0
        var chunk: [U8; 1024] = [0; 1024]
        loop {
            let n = this.read(&mut chunk)!
            if n == 0 {
                break
            }
            buf.extend_from_slice(&chunk[0..n])
            read_total += n
        }
        return Ok(read_total)
    }

    func read_to_string(this, buf: &mut String) -> Result[U64, IoError] {
        var bytes = List.new()
        let n = this.read_to_end(&mut bytes)!
        let s = String.from_utf8(bytes)?
        buf.push_str(&s)
        return Ok(n)
    }
}

public trait Write {
    func write(this, buf: &[U8]) -> Result[U64, IoError]
    func flush(this) -> Result[Unit, IoError]

    func write_all(this, buf: &[U8]) -> Result[Unit, IoError] {
        var written: U64 = 0
        loop while written < buf.len() {
            let n = this.write(&buf[written..])!
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

public type File {
    handle: RawHandle,
}

extend File {
    public func open(path: &Path) -> Result[This, IoError]
    effects: [io.file.read]
    {
        return This.open_options(path, OpenOptions.read())
    }

    public func create(path: &Path) -> Result[This, IoError]
    effects: [io.file.write]
    {
        return This.open_options(path, OpenOptions.write().create(true).truncate(true))
    }

    public func open_options(path: &Path, opts: OpenOptions) -> Result[This, IoError]
    effects: [io.file]
    {
        let handle = platform_open(path, opts)?
        return Ok(This { handle: handle })
    }
}

extend File with Read {
    func read(this, buf: &mut [U8]) -> Result[U64, IoError]
    effects: [io.file.read]
    {
        return platform_read(this.handle, buf)
    }
}

extend File with Write {
    func write(this, buf: &[U8]) -> Result[U64, IoError]
    effects: [io.file.write]
    {
        return platform_write(this.handle, buf)
    }

    func flush(this) -> Result[Unit, IoError]
    effects: [io.file.write]
    {
        return platform_flush(this.handle)
    }
}

extend File with Drop {
    func drop(this) {
        platform_close(this.handle)
    }
}
```

## 6. Sync Module

### 6.1 Mutex

```tml
// std.sync.mutex

public type Mutex[T] {
    inner: RawMutex,
    data: UnsafeCell[T],
}

public type MutexGuard[T] {
    lock: &Mutex[T],
}

extend Mutex[T] {
    public func new(value: T) -> This {
        return This {
            inner: RawMutex.new(),
            data: UnsafeCell.new(value),
        }
    }

    public func lock(this) -> MutexGuard[T] {
        this.inner.lock()
        return MutexGuard { lock: this }
    }

    public func try_lock(this) -> Option[MutexGuard[T]] {
        if this.inner.try_lock() {
            return Some(MutexGuard { lock: this })
        }
        return None
    }
}

extend MutexGuard[T] with Deref {
    type Target = T

    func deref(this) -> &T {
        unsafe { &*this.lock.data.get() }
    }
}

extend MutexGuard[T] with DerefMut {
    func deref_mut(this) -> &mut T {
        unsafe { &mut *this.lock.data.get() }
    }
}

extend MutexGuard[T] with Drop {
    func drop(this) {
        this.lock.inner.unlock()
    }
}
```

### 6.2 Atomic Types

```tml
// std.sync.atomic

public type AtomicBool { inner: U8 }
public type AtomicI32 { inner: I32 }
public type AtomicI64 { inner: I64 }
public type AtomicU32 { inner: U32 }
public type AtomicU64 { inner: U64 }
public type AtomicPtr[T] { inner: *mut T }

public type Ordering = Relaxed | Acquire | Release | AcqRel | SeqCst

extend AtomicI64 {
    public func new(value: I64) -> This {
        return This { inner: value }
    }

    public func load(this, order: Ordering) -> I64 {
        intrinsic_atomic_load(&this.inner, order)
    }

    public func store(this, value: I64, order: Ordering) {
        intrinsic_atomic_store(&this.inner, value, order)
    }

    public func swap(this, value: I64, order: Ordering) -> I64 {
        intrinsic_atomic_swap(&this.inner, value, order)
    }

    public func compare_exchange(
        this,
        current: I64,
        new: I64,
        success: Ordering,
        failure: Ordering
    ) -> Result[I64, I64] {
        intrinsic_atomic_cmpxchg(&this.inner, current, new, success, failure)
    }

    public func fetch_add(this, value: I64, order: Ordering) -> I64 {
        intrinsic_atomic_fetch_add(&this.inner, value, order)
    }

    public func fetch_sub(this, value: I64, order: Ordering) -> I64 {
        intrinsic_atomic_fetch_sub(&this.inner, value, order)
    }
}
```

## 7. Thread Module

```tml
// std.thread

public type JoinHandle[T] {
    handle: RawThread,
    result: Arc[Mutex[Option[T]]],
}

public func spawn[F: FnOnce() -> T + Send, T: Send](f: F) -> JoinHandle[T]
effects: [io.sync]
{
    let result = Arc.new(Mutex.new(None))
    let result_clone = result.clone()

    let handle = platform_spawn(do() {
        let value = f()
        *result_clone.lock() = Some(value)
    })

    return JoinHandle { handle: handle, result: result }
}

extend JoinHandle[T] {
    public func join(this) -> Result[T, JoinError]
    effects: [io.sync]
    {
        platform_join(this.handle)?

        let guard = this.result.lock()
        when guard.take() {
            Some(value) -> Ok(value),
            None -> Err(JoinError.Panicked),
        }
    }

    public func is_finished(this) -> Bool {
        platform_is_finished(this.handle)
    }
}
```

## 8. Extension Points

### 8.1 Plugin Traits

```tml
// Libraries can implement these to extend functionality

// Custom allocator
public trait GlobalAlloc {
    unsafe func alloc(this, layout: Layout) -> *mut U8
    unsafe func dealloc(this, ptr: *mut U8, layout: Layout)
    unsafe func realloc(this, ptr: *mut U8, old: Layout, new: Layout) -> *mut U8
}

// Custom hasher
public trait BuildHasher {
    type Hasher: Hasher
    func build_hasher(this) -> This.Hasher
}

// Custom error
public trait Error: Debug + Display {
    func source(this) -> Option[&dyn Error] { None }
}

// Custom serialization
public trait Serialize {
    func serialize[S: Serializer](this, serializer: &mut S) -> Result[Unit, S.Error]
}

public trait Deserialize {
    func deserialize[D: Deserializer](deserializer: &mut D) -> Result[This, D.Error]
}
```

### 8.2 Library Extension

```tml
// Example: custom Map implementation

import std.collections.Map

// Third-party library can extend Map
extend Map[K, V] with custom_lib.Serialize where K: Serialize, V: Serialize {
    func serialize[S: Serializer](this, s: &mut S) -> Result[Unit, S.Error] {
        s.serialize_map_start(this.len())?
        loop (k, v) in this.entries() {
            k.serialize(s)?
            v.serialize(s)?
        }
        s.serialize_map_end()
    }
}
```

---

*Previous: [19-RUNTIME.md](./19-RUNTIME.md)*
*Next: [21-TARGETS.md](./21-TARGETS.md) — Cross-Compilation Targets*
