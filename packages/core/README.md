# TML Core Library

The `core` library provides fundamental behaviors and types for the TML language. This is the foundation that other libraries build upon, similar to Rust's `core` and `std` crates.

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
- **`Add`**, **`Sub`**, **`Mul`**, **`Div`**, **`Rem`** - Arithmetic operators
- **`Neg`**, **`Not`** - Unary operators
- **`BitAnd`**, **`BitOr`**, **`BitXor`**, **`Shl`**, **`Shr`** - Bitwise operators
- **`Index`**, **`IndexMut`** - Indexing operators
- Compound assignment: `AddAssign`, `SubAssign`, etc.

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
- **`TryFrom[T]`** - Fallible conversion from T
- **`TryInto[T]`** - Fallible conversion into T
- **`AsRef[T]`** - Borrow as reference to T
- **`AsMut[T]`** - Borrow as mutable reference to T

```tml
use core::convert::From

// I8 can be converted to I32
let x: I32 = I32::from(42 as I8)
```

### Memory and Safety

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

```tml
use core::cell::RefCell

let cell: RefCell[I32] = RefCell::new(42)
{
    let r: Ref[I32] = cell.borrow()
    print((*r).to_string())  // 42
}
{
    let mut_r: RefMut[I32] = cell.borrow_mut()
    *mut_r = 100
}
```

#### `marker` - Marker Behaviors
- **`Send`** - Types safe to send between threads
- **`Sync`** - Types safe to share between threads
- **`Sized`** - Types with known size at compile time
- **`Unpin`** - Types that can be moved after pinning

#### `borrow` - Borrowing
- **`Borrow[T]`** - Borrow data as type T
- **`BorrowMut[T]`** - Mutably borrow data as type T
- **`ToOwned`** - Create owned data from borrowed
- **`Cow[T]`** - Clone-on-write smart pointer

### Collections Support

#### `iter` - Iteration
- **`Iterator`** - Core iteration behavior with `next()`
- **`IntoIterator`** - Types convertible to iterators
- **`FromIterator`** - Types constructible from iterators
- **`Extend`** - Types extendable from iterators

Iterator adapters:
- `Map`, `Filter`, `Take`, `Skip`, `Chain`, `Zip`
- `Enumerate`, `Peekable`, `TakeWhile`, `SkipWhile`

```tml
use core::iter::Iterator

let sum: I32 = (1 through 10)
    .iter()
    .filter(do(x: ref I32) *x % 2 == 0)
    .map(do(x: I32) x * 2)
    .sum()
```

#### `slice` - Slice Operations
- **`Slice[T]`** - Immutable view into contiguous memory
- **`MutSlice[T]`** - Mutable view into contiguous memory
- Sorting: `sort()`, `sort_by()`, `sort_by_key()`
- Searching: `binary_search()`, `contains()`
- Manipulation: `reverse()`, `rotate_left()`, `rotate_right()`
- Iteration: `chunks()`, `windows()`

#### `hash` - Hashing
- **`Hash`** - Behavior for hashable types
- **`Hasher`** - Hash state accumulator
- `combine_hashes(h1, h2)` - Combine hash values

### Enhanced Types

#### `option` - Maybe[T] Methods
Enhanced methods for `Maybe[T]` (Rust's `Option`):
- Extracting: `unwrap()`, `expect()`, `unwrap_or()`, `unwrap_or_else()`
- Transforming: `map()`, `map_or()`, `and_then()`, `or_else()`, `filter()`
- Converting: `ok_or()`, `ok_or_else()`, `transpose()`
- Combining: `zip()`, `zip_with()`, `flatten()`

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

```tml
let value: I32 = parse_int("42")
    .map(do(x: I32) x * 2)
    .unwrap_or(0)  // 84
```

### Low-Level

#### `ptr` - Raw Pointers
- **`RawPtr[T]`** - Raw immutable pointer
- **`RawMutPtr[T]`** - Raw mutable pointer
- **`NonNull[T]`** - Non-null pointer guarantee
- `copy()`, `copy_nonoverlapping()`, `write_bytes()`

#### `str` - String Utilities
String manipulation functions:
- `len()`, `is_empty()`, `char_at()`
- `trim()`, `trim_start()`, `trim_end()`
- `starts_with()`, `ends_with()`, `contains()`
- `split()`, `lines()`, `chars()`
- `to_uppercase()`, `to_lowercase()`
- `find()`, `replace()`, `repeat()`

#### `error` - Error Handling
- **`Error`** - Base behavior for error types
- **`SimpleError`** - Basic string error
- **`ChainedError[E]`** - Error with underlying cause
- **`BoxedError`** - Type-erased error
- **`ParseError`** - Parsing errors
- **`IoError`** / **`IoErrorKind`** - I/O errors

```tml
use core::error::{Error, SimpleError}

func do_something() -> Outcome[I32, SimpleError] {
    return Err(SimpleError::new("something went wrong"))
}

// Error chaining
let result = do_something()
    .context("while doing something important")
```

## Design Philosophy

The core library follows TML's design principles:

1. **Self-documenting names**: `Duplicate` instead of `Clone`, `Maybe` instead of `Option`
2. **Words over symbols**: `ref T` instead of `&T`, `and`/`or` instead of `&&`/`||`
3. **Explicit over implicit**: Clear behavior contracts with explicit type constraints
4. **Rust compatibility**: Familiar patterns for Rust developers, adapted to TML syntax
