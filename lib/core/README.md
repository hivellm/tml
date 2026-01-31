# TML Core Library

The `core` library provides fundamental behaviors and types for the TML language. This is the foundation that other libraries build upon, similar to Rust's `core` crate.

**Status**: 657 tests passing

## Modules

### Fundamental Behaviors

#### `clone` - Duplication and Copying
- **`Duplicate`** - Behavior for types that can be duplicated (Rust's `Clone`)
- **`Copy`** - Marker behavior for types that can be bitwise copied

```tml
use core::clone::{Duplicate, Copy}

let x: I32 = 42
let y: I32 = x.duplicate()  // Explicit duplication
```

#### `cmp` - Comparison
- **`Ordering`** - Less, Equal, Greater enum
- **`PartialEq`** - Behavior for types with partial equality (`eq`, `ne`)
- **`Eq`** - Marker for types with full equality
- **`PartialOrd`** - Behavior for partially ordered types
- **`Ord`** - Behavior for totally ordered types (`cmp`, `min`, `max`, `clamp`)

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

#### `ops` - Operator Overloading
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

#### `default` - Default Values
- **`Default`** - Behavior for types with a default value

```tml
use core::default::Default

let x: I32 = I32::default()  // 0
let m: Maybe[I32] = Maybe[I32]::default()  // Nothing
```

#### `fmt` - Formatting
- **`Display`** - Human-readable formatting (`to_string`)
- **`Debug`** - Debug formatting (`debug_string`)
- **`Formatter`** - Format state and buffer management
- **`Write`** - Behavior for writable buffers

```tml
use core::fmt::{Display, Debug}

type User { name: Str, age: I32 }

impl Display for User {
    pub func to_string(this) -> Str {
        return this.name + " (" + this.age.to_string() + ")"
    }
}
```

### Type Conversion

#### `convert` - Type Conversions
- **`From[T]`** - Convert from type T
- **`Into[T]`** - Convert into type T
- **`TryFrom[T]`** - Fallible conversion from T (returns `Outcome`)
- **`TryInto[T]`** - Fallible conversion into T
- **`AsRef[T]`** - Borrow as reference to T
- **`AsMut[T]`** - Borrow as mutable reference to T

```tml
use core::convert::{From, TryFrom}

// Infallible conversion
let x: I32 = I32::from(42 as I8)

// Fallible conversion (may overflow)
when I32::try_from(9999999999 as I64) {
    Ok(n) => print("Converted: " + n.to_string()),
    Err(e) => print("Overflow!")
}
```

### Memory and Safety

#### `alloc` - Memory Allocation
- **`Heap[T]`** - Heap-allocated box (Rust's `Box`)
- **`Shared[T]`** - Reference-counted pointer (Rust's `Rc`)
- **`Weak[T]`** - Weak reference to `Shared[T]`
- `alloc(size)` / `dealloc(ptr)` - Raw allocation functions

```tml
use core::alloc::{Heap, Shared}

let boxed: Heap[I32] = Heap::new(42)
let shared: Shared[I32] = Shared::new(100)
let weak: Weak[I32] = shared.downgrade()
```

#### `arena` - Arena Allocation
- **`Arena`** - Bump allocator for fast allocation
- Efficient for allocating many objects with same lifetime

#### `pool` - Object Pooling
- **`Pool[T]`** - Reusable object pool
- **`PooledObject[T]`** - RAII handle to pooled object

#### `mem` - Memory Utilities
- `size_of[T]()` - Size of a type in bytes
- `align_of[T]()` - Alignment of a type
- `swap(a, b)` - Swap two values
- `replace(dest, src)` - Replace value, returning old
- `take(dest)` - Take value, leaving default
- `forget(value)` - Leak a value without dropping
- **`ManuallyDrop[T]`** - Prevent automatic dropping
- **`MaybeUninit[T]`** - Possibly uninitialized memory

#### `cell` - Interior Mutability
- **`Cell[T]`** - Single-threaded interior mutability for `Copy` types
- **`RefCell[T]`** - Single-threaded interior mutability with runtime borrow checking
- **`Ref[T]`**, **`RefMut[T]`** - Smart references from RefCell
- **`OnceCell[T]`** - Write-once cell
- **`LazyCell[T]`** - Lazy initialization

```tml
use core::cell::RefCell

let cell: RefCell[I32] = RefCell::new(42)
{
    let r: Ref[I32] = cell.borrow()
    print((*r).to_string())  // 42
}
{
    var mut_r: RefMut[I32] = cell.borrow_mut()
    *mut_r = 100
}
```

#### `marker` - Marker Behaviors
- **`Send`** - Types safe to send between threads
- **`Sync`** - Types safe to share between threads
- **`Sized`** - Types with known size at compile time
- **`Unpin`** - Types that can be moved after pinning
- **`PhantomData[T]`** - Zero-sized type for variance

#### `borrow` - Borrowing
- **`Borrow[T]`** - Borrow data as type T
- **`BorrowMut[T]`** - Mutably borrow data as type T
- **`ToOwned`** - Create owned data from borrowed
- **`Cow[T]`** - Clone-on-write smart pointer

#### `pin` - Pinning
- **`Pin[P]`** - Pinned pointer that guarantees stability

### Collections Support

#### `iter` - Iteration
- **`Iterator`** - Core iteration behavior with `next()`
- **`IntoIterator`** - Types convertible to iterators
- **`FromIterator`** - Types constructible from iterators
- **`Extend`** - Types extendable from iterators
- **`DoubleEndedIterator`** - Iteration from both ends
- **`ExactSizeIterator`** - Iterators with known length

Iterator adapters:
- `Map`, `Filter`, `Take`, `Skip`, `Chain`, `Zip`
- `Enumerate`, `Peekable`, `TakeWhile`, `SkipWhile`
- `Flatten`, `FlatMap`, `Cycle`, `Fuse`
- `Rev`, `Cloned`, `Copied`
- `Chunks`, `Windows`, `StepBy`

```tml
use core::iter::Iterator

let sum: I32 = (1 through 10)
    .iter()
    .filter(do(x: ref I32) *x % 2 == 0)
    .map(do(x: I32) x * 2)
    .sum()
```

#### `async_iter` - Async Iteration
- **`AsyncIterator`** - Async iteration with `poll_next()`

#### `slice` - Slice Operations
- **`Slice[T]`** - Immutable view into contiguous memory
- **`SliceIter[T]`** - Iterator over slice elements
- Sorting: `sort()`, `sort_by()`, `sort_by_key()`
- Searching: `binary_search()`, `contains()`
- Manipulation: `reverse()`, `rotate_left()`, `rotate_right()`
- Iteration: `chunks()`, `windows()`

#### `array` - Fixed-Size Arrays
- Methods for `[T; N]` types
- `len()`, `is_empty()`, `get()`, `iter()`

#### `collections` - Collection Traits
- **`IntoIterator`** - Convert to iterator
- **`FromIterator`** - Build from iterator
- **`Extend`** - Extend collection

#### `hash` - Hashing
- **`Hash`** - Behavior for hashable types
- **`Hasher`** - Hash state accumulator
- **`BuildHasher`** - Hasher factory
- `combine_hashes(h1, h2)` - Combine hash values

### Enhanced Types

#### `option` - Maybe[T] Methods
Enhanced methods for `Maybe[T]` (Rust's `Option`):
- Extracting: `unwrap()`, `expect()`, `unwrap_or()`, `unwrap_or_else()`
- Transforming: `map()`, `map_or()`, `and_then()`, `or_else()`, `filter()`
- Converting: `ok_or()`, `ok_or_else()`, `transpose()`
- Combining: `zip()`, `zip_with()`, `flatten()`
- Checking: `is_just()`, `is_nothing()`

```tml
let result: I32 = Just(5)
    .map(do(x: I32) x * 2)
    .filter(do(x: ref I32) *x > 5)
    .unwrap_or(0)  // 10
```

#### `result` - Outcome[T, E] Methods
Enhanced methods for `Outcome[T, E]` (Rust's `Result`):
- Extracting: `unwrap()`, `expect()`, `unwrap_err()`, `unwrap_or_else()`
- Transforming: `map()`, `map_err()`, `and_then()`, `or_else()`
- Converting: `ok()`, `err()`, `transpose()`
- Chaining: `and()`, `or()`, `flatten()`
- Checking: `is_ok()`, `is_err()`

```tml
let value: I32 = parse_int("42")
    .map(do(x: I32) x * 2)
    .unwrap_or(0)  // 84
```

#### `range` - Range Types
- **`Range[T]`** - Half-open range `a to b`
- **`RangeInclusive[T]`** - Closed range `a through b`
- **`RangeFrom[T]`** - Unbounded start
- **`RangeTo[T]`** - Unbounded end

#### `tuple` - Tuple Operations
- Methods for tuple types (up to 12 elements)
- `first()`, `second()`, etc.

### Strings and Text

#### `str` - String Utilities
- `len()`, `is_empty()`, `char_at()`
- `trim()`, `trim_start()`, `trim_end()`
- `starts_with()`, `ends_with()`, `contains()`
- `split()`, `lines()`, `chars()`
- `to_uppercase()`, `to_lowercase()`
- `find()`, `replace()`, `repeat()`

#### `ascii` - ASCII Operations
- Character classification: `is_digit()`, `is_alpha()`, `is_alphanumeric()`
- Case conversion: `to_uppercase()`, `to_lowercase()`
- `AsciiChar` - Single ASCII character type

#### `char` - Unicode Characters
- **`Char`** - Unicode scalar value
- UTF-8/UTF-16 encoding/decoding
- Character properties and classification

#### `bstr` - Byte Strings
- **`BStr`** - Byte string slice (may not be valid UTF-8)
- Binary string operations

#### `unicode` - Unicode Support
- Unicode categories and properties
- Normalization forms

### Error Handling

#### `error` - Error Types
- **`Error`** - Base behavior for error types
- **`SimpleError`** - Basic string error
- **`ChainedError[E]`** - Error with underlying cause
- **`BoxedError`** - Type-erased error
- **`ParseError`** - Parsing errors
- **`IoError`** / **`IoErrorKind`** - I/O errors
- **`TryFromIntError`** - Integer conversion errors

```tml
use core::error::{Error, SimpleError}

func do_something() -> Outcome[I32, SimpleError] {
    return Err(SimpleError::new("something went wrong"))
}
```

### Low-Level

#### `ptr` - Raw Pointers
- **`RawPtr[T]`** - Raw immutable pointer
- **`RawMutPtr[T]`** - Raw mutable pointer
- **`NonNull[T]`** - Non-null pointer guarantee
- `copy()`, `copy_nonoverlapping()`, `write_bytes()`

#### `intrinsics` - Compiler Intrinsics
- `type_id[T]()` - Get type ID
- `type_name[T]()` - Get type name
- `likely()`, `unlikely()` - Branch hints
- `unreachable()` - Unreachable code marker
- Atomic operations

#### `sync` - Synchronization Primitives (core)
- **`AtomicBool`**, **`AtomicI32`**, **`AtomicI64`**, etc.
- Atomic operations and memory ordering

#### `any` - Type Erasure
- **`Any`** - Type-erased value with runtime type checking
- `downcast[T]()` - Safe downcasting

#### `soo` - Small Object Optimization
- **`SmallBox[T, N]`** - Stack-allocated box with fallback to heap

#### `cache` - Caching
- **`Cache[K, V]`** - LRU cache implementation

### Async/Concurrency

#### `future` - Futures
- **`Future`** - Async computation
- **`Poll`** - Future poll result

#### `task` - Task Management
- **`Context`** - Task context
- **`Waker`** - Task waker

#### `time` - Time Utilities
- **`Duration`** - Time duration
- **`Instant`** - Point in time
- `now()`, `elapsed()`, `sleep()`

## Design Philosophy

The core library follows TML's design principles:

1. **Self-documenting names**: `Duplicate` instead of `Clone`, `Maybe` instead of `Option`
2. **Words over symbols**: `ref T` instead of `&T`, `and`/`or` instead of `&&`/`||`
3. **Explicit over implicit**: Clear behavior contracts with explicit type constraints
4. **Rust compatibility**: Familiar patterns for Rust developers, adapted to TML syntax
