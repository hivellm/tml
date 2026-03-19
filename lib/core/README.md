# TML Core Library

The `core` library provides fundamental behaviors and types for the TML language. This is the foundation that other libraries build upon, similar to Rust's `core` crate.

**Status**: 830+ tests passing | [Changelog](CHANGELOG.md)

## Modules

### Fundamental Behaviors

#### `clone` — Duplication and Copying
- **`Duplicate`** — Behavior for types that can be duplicated (Rust's `Clone`)
- **`Copy`** — Marker behavior for types that can be bitwise copied

```tml
use core::clone::{Duplicate, Copy}

let x: I32 = 42
let y: I32 = x.duplicate()  // Explicit duplication
```

#### `cmp` — Comparison
- **`Ordering`** — Less, Equal, Greater enum
- **`PartialEq`** — Partial equality (`eq`, `ne`)
- **`Eq`** — Marker for full equality
- **`PartialOrd`** — Partially ordered types
- **`Ord`** — Totally ordered types (`cmp`, `min`, `max`, `clamp`)

```tml
use core::cmp::{Ordering, Ord}

let a: I32 = 5
let b: I32 = 10
when a.cmp(ref b) {
    Less => print("a < b"),
    Equal => print("a == b"),
    Greater => print("a > b")
}
```

#### `ops` — Operator Overloading
- **Arithmetic**: `Add`, `Sub`, `Mul`, `Div`, `Rem`, `Neg`
- **Bitwise**: `BitAnd`, `BitOr`, `BitXor`, `Shl`, `Shr`, `Not`
- **Indexing**: `Index`, `IndexMut`
- **Compound assignment**: `AddAssign`, `SubAssign`, `MulAssign`, etc.
- **Range**: `Range`, `RangeInclusive`, `RangeTo`, `RangeFrom`
- **Function traits**: `Fn`, `FnMut`, `FnOnce`
- **Coroutine**: `Coroutine`, `CoroutineState`
- **Drop**: `Drop` for custom destructors

```tml
use core::ops::Add

type Point { x: I32, y: I32 }

impl Add for Point {
    type Output = Point
    pub func add(this, rhs: Point) -> Point {
        return Point { x: this.x + rhs.x, y: this.y + rhs.y }
    }
}
```

#### `default` — Default Values
- **`Default`** — Behavior for types with a default value

#### `fmt` — Formatting
- **`Display`** — Human-readable formatting (`to_string`)
- **`Debug`** — Debug formatting (`debug_string`)
- **`Formatter`** — Format state and buffer management
- **`Write`** — Behavior for writable buffers

### Type Conversion

#### `convert` — Type Conversions
- **`From[T]`** / **`Into[T]`** — Infallible conversion
- **`TryFrom[T]`** / **`TryInto[T]`** — Fallible conversion (returns `Outcome`)
- **`AsRef[T]`** / **`AsMut[T]`** — Borrow as reference

### Memory and Safety

#### `alloc` — Memory Allocation
- **`Heap[T]`** — Heap-allocated box (Rust's `Box`)
- **`Shared[T]`** — Reference-counted pointer (Rust's `Rc`)
- **`Sync[T]`** — Atomic reference-counted pointer (Rust's `Arc`)
- **`Weak[T]`** — Weak reference to `Shared[T]`
- `alloc(size)` / `dealloc(ptr)` — Raw allocation functions

```tml
use core::alloc::{Heap, Shared, Sync}

let boxed: Heap[I32] = Heap::new(42)
let shared: Shared[I32] = Shared::new(100)
let synced: Sync[I32] = Sync::new(42)
let weak: Weak[I32] = shared.downgrade()
```

#### `arena` — Arena Allocation
- **`Arena`** — Bump allocator for fast allocation of same-lifetime objects

#### `pool` — Object Pooling
- **`Pool[T]`** — Reusable object pool
- **`PooledObject[T]`** — RAII handle to pooled object

#### `mem` — Memory Utilities
- `size_of[T]()`, `align_of[T]()`, `swap(a, b)`, `replace(dest, src)`, `take(dest)`, `forget(value)`
- **`ManuallyDrop[T]`** — Prevent automatic dropping
- **`MaybeUninit[T]`** — Possibly uninitialized memory

#### `cell` — Interior Mutability
- **`Cell[T]`** — Single-threaded interior mutability for `Copy` types
- **`RefCell[T]`** — Runtime borrow checking with `Ref[T]`/`RefMut[T]`
- **`OnceCell[T]`** — Write-once cell
- **`LazyCell[T]`** — Lazy initialization

#### `marker` — Marker Behaviors
- **`Send`**, **`Sync`**, **`Sized`**, **`Unpin`**, **`PhantomData[T]`**

#### `borrow` — Borrowing
- **`Borrow[T]`** / **`BorrowMut[T]`** — Borrow data as type T
- **`ToOwned`** — Create owned data from borrowed
- **`Cow[T]`** — Clone-on-write smart pointer

#### `pin` — Pinning
- **`Pin[P]`** — Pinned pointer that guarantees stability

### Collections Support

#### `iter` — Iteration
- **`Iterator`** — Core iteration with `next()`
- **`IntoIterator`** / **`FromIterator`** / **`Extend`**
- **`DoubleEndedIterator`** / **`ExactSizeIterator`**
- Adapters: `Map`, `Filter`, `Take`, `Skip`, `Chain`, `Zip`, `Enumerate`, `Peekable`, `TakeWhile`, `SkipWhile`, `Flatten`, `FlatMap`, `Cycle`, `Fuse`, `Rev`, `Cloned`, `Copied`, `Chunks`, `Windows`, `StepBy`

```tml
use core::iter::Iterator

let sum: I32 = (1 through 10)
    .iter()
    .filter(do(x: ref I32) *x % 2 == 0)
    .map(do(x: I32) x * 2)
    .sum()
```

#### `async_iter` — Async Iteration
- **`AsyncIterator`** — Async iteration with `poll_next()`

#### `slice` — Slice Operations
- **`Slice[T]`** — Immutable view into contiguous memory
- Sorting, searching, manipulation, chunking

#### `array` — Fixed-Size Arrays
- Methods for `[T; N]` types: `len()`, `is_empty()`, `get()`, `iter()`

#### `hash` — Hashing
- **`Hash`** / **`Hasher`** / **`BuildHasher`**

### Enhanced Types

#### `option` — Maybe[T] Methods
- Extracting: `unwrap()`, `expect()`, `unwrap_or()`, `unwrap_or_else()`
- Transforming: `map()`, `map_or()`, `and_then()`, `or_else()`, `filter()`
- Converting: `ok_or()`, `transpose()`, `zip()`, `flatten()`

#### `result` — Outcome[T, E] Methods
- Extracting: `unwrap()`, `expect()`, `unwrap_err()`, `unwrap_or_else()`
- Transforming: `map()`, `map_err()`, `and_then()`, `or_else()`
- Converting: `ok()`, `err()`, `transpose()`, `flatten()`

#### `range` — Range Types
- **`Range[T]`**, **`RangeInclusive[T]`**, **`RangeFrom[T]`**, **`RangeTo[T]`**

#### `tuple` — Tuple Operations
- Methods for tuple types (up to 12 elements)

### Strings and Text

#### `str` — String Utilities
- `len()`, `is_empty()`, `char_at()`, `trim()`, `starts_with()`, `ends_with()`, `contains()`
- `split()`, `lines()`, `chars()`, `to_uppercase()`, `to_lowercase()`
- `find()`, `replace()`, `repeat()`

#### `ascii` — ASCII Operations
- Character classification and case conversion
- `AsciiChar` — Single ASCII character type

#### `char` — Unicode Characters
- **`Char`** — Unicode scalar value
- UTF-8/UTF-16 encoding/decoding, character properties

#### `bstr` — Byte Strings
- **`BStr`** — Byte string slice (may not be valid UTF-8)

#### `unicode` — Unicode Support
- Unicode categories, properties, normalization

#### `encoding` — Encoding Utilities
- Base64, hex, and other encoding/decoding functions

### Error Handling

#### `error` — Error Types
- **`Error`** — Base behavior for error types
- **`SimpleError`** — Basic string error
- **`ChainedError[E]`** — Error with underlying cause
- **`BoxedError`** — Type-erased error
- **`ParseError`**, **`IoError`**, **`TryFromIntError`**

### Low-Level

#### `ptr` — Raw Pointers
- **`RawPtr[T]`**, **`RawMutPtr[T]`**, **`NonNull[T]`**
- `copy()`, `copy_nonoverlapping()`, `write_bytes()`

#### `intrinsics` — Compiler Intrinsics
- `type_id[T]()`, `type_name[T]()`, `likely()`, `unlikely()`, `unreachable()`

#### `sync` — Synchronization Primitives (core)
- Generic atomics: `atomic_load`, `atomic_store`, `atomic_add`, `atomic_sub`, `atomic_exchange`, `atomic_cas`
- Typed atomic FFI: `atomic_fetch_add_i32`, `atomic_load_i32`, etc.
- Memory fences: `atomic_fence`, `atomic_fence_acquire`, `atomic_fence_release`
- Spinlock: `spin_lock`, `spin_unlock`, `spin_trylock`

#### `any` — Type Erasure
- **`Any`** — Type-erased value with runtime type checking
- `downcast[T]()` — Safe downcasting

### Specialized

#### `soo` — Small Object Optimization
- **`SmallBox[T, N]`** — Stack-allocated box with fallback to heap

#### `cache` — Caching
- **`Cache[K, V]`** — LRU cache implementation

#### `reflect` — Reflection
- Runtime type introspection via `@derive(Reflect)`
- `variant_name()`, `variant_tag()` for enums

#### `ringbuf` — Ring Buffer
- Lock-free ring buffer for concurrent producers/consumers

#### `bitset` — Bit Sets
- Fixed-size and dynamic bit set operations

#### `simd` — SIMD Intrinsics
- Native SSE2: `sse2_cmpeq_epi8`, `sse2_movemask_epi8`
- `simd_splat`, `simd_load_ptr`, `cttz`
- Guarded with `#if X86_64`

### Async/Concurrency

#### `future` — Futures
- **`Future`** — Async computation
- **`Poll`** — Future poll result

#### `task` — Task Management
- **`Context`** — Task context
- **`Waker`** — Task waker

#### `time` — Time Utilities
- **`Duration`** — Time duration
- **`Instant`** — Point in time
- `now()`, `elapsed()`, `sleep()`

## Design Philosophy

The core library follows TML's design principles:

1. **Self-documenting names**: `Duplicate` instead of `Clone`, `Maybe` instead of `Option`
2. **Words over symbols**: `ref T` instead of `&T`, `and`/`or` instead of `&&`/`||`
3. **Explicit over implicit**: Clear behavior contracts with explicit type constraints
4. **Rust compatibility**: Familiar patterns adapted to TML syntax
5. **Minimal C dependencies**: Smart pointers, iterators, fmt — all pure TML
