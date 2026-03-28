# TML Core Library

Foundation types and behaviors for the TML language. Everything here is available without `std` — similar to Rust's `core` crate.

[Changelog](CHANGELOG.md)

## Module Index

### Traits & Behaviors (`traits/`)

| Module | Path | Description |
|--------|------|-------------|
| clone | `core::traits::clone` | `Duplicate`, `Copy` — type duplication |
| cmp | `core::traits::cmp` | `PartialEq`, `Eq`, `PartialOrd`, `Ord`, `Ordering` |
| convert | `core::traits::convert` | `From`, `Into`, `TryFrom`, `TryInto`, `AsRef`, `AsMut` |
| default | `core::traits::default` | `Default` — types with a default value |
| hash | `core::traits::hash` | `Hash`, `Hasher`, `BuildHasher` |
| marker | `core::traits::marker` | `Send`, `Sync`, `Sized`, `Unpin`, `Copy`, `PhantomData` |
| borrow | `core::traits::borrow` | `Borrow`, `BorrowMut`, `ToOwned`, `Cow` |

### Core Types (`types/`)

| Module | Path | Description |
|--------|------|-------------|
| option | `core::types::option` | `Maybe[T]` — `Just(T)` / `Nothing` with 30 methods |
| result | `core::types::result` | `Outcome[T,E]` — `Ok(T)` / `Err(E)` with 34 methods |
| tuple | `core::types::tuple` | Tuple impls up to 12 elements |
| range | `core::types::range` | `Range`, `RangeInclusive`, `RangeFrom`, `RangeTo` |
| any | `core::types::any` | `TypeId`, `AnyValue`, runtime type checking, `downcast` |

### Runtime & Compiler (`runtime/`)

| Module | Path | Description |
|--------|------|-------------|
| error | `core::runtime::error` | `Error`, `IoError`, `IoErrorKind`, `ParseError`, `SimpleError` |
| panic | `core::runtime::panic` | `panic()`, `PanicInfo`, catch support |
| intrinsics | `core::runtime::intrinsics` | Compiler intrinsics — math, memory, SIMD, type info |
| mem | `core::runtime::mem` | `size_of`, `align_of`, `swap`, `replace`, `take`, `forget`, `ManuallyDrop`, `MaybeUninit` |
| pin | `core::runtime::pin` | `Pin[P]` — pinned pointer for self-referential types |
| hint | `core::runtime::hint` | `unreachable`, `assume`, `likely`, `unlikely` |
| profiler | `core::runtime::profiler` | Profiling utilities |

### Data Structures (`data/`)

| Module | Path | Description |
|--------|------|-------------|
| arena | `core::data::arena` | Arena bump allocator |
| bitset | `core::data::bitset` | Fixed-size and dynamic bit sets |
| cache | `core::data::cache` | LRU cache with TTL expiry |
| pool | `core::data::pool` | `Pool[T]` — reusable object pool |
| ringbuf | `core::data::ringbuf` | Lock-free ring buffer |
| soo | `core::data::soo` | `SmallBox` — small object optimization |
| collections | `core::data::collections` | Collection re-exports |

### Async (`async/`)

| Module | Path | Description |
|--------|------|-------------|
| async_iter | `core::async::async_iter` | `AsyncIterator` — async iteration with `poll_next` |
| task | `core::async::task` | `Poll`, `Waker`, `Context`, `RawWaker` |

### Memory (`alloc/`)

| Module | Path | Description |
|--------|------|-------------|
| heap | `core::alloc` | `Heap[T]` — owned heap allocation (Rust's `Box`) |
| shared | `core::alloc` | `Shared[T]` — reference-counted pointer (Rust's `Rc`) |
| sync | `core::alloc` | `Sync[T]` — atomic reference-counted pointer (Rust's `Arc`) |
| weak | `core::alloc` | `Weak[T]` — weak reference |

### Cell (`cell/`)

| Module | Path | Description |
|--------|------|-------------|
| cell | `core::cell` | `Cell[T]` — interior mutability for Copy types |
| ref_cell | `core::cell` | `RefCell[T]` — runtime borrow checking |
| once | `core::cell` | `OnceCell[T]` — write-once cell |
| lazy | `core::cell` | `LazyCell[T]` — lazy initialization |
| unsafe_cell | `core::cell` | `UnsafeCell[T]` — raw interior mutability |

### Iterators (`iter/`)

| Module | Path | Description |
|--------|------|-------------|
| traits | `core::iter` | `Iterator`, `IntoIterator`, `FromIterator`, `DoubleEndedIterator` |
| adapters | `core::iter` | `Map`, `Filter`, `Take`, `Skip`, `Chain`, `Zip`, `Enumerate`, `Peekable`, `Flatten`, `FlatMap`, `Cycle`, `Rev`, `StepBy`, `Intersperse` |
| sources | `core::iter` | `empty`, `once`, `repeat`, `from_fn`, `successors` |
| accumulators | `core::iter` | `sum`, `product` |

### Strings & Text

| Module | Path | Description |
|--------|------|-------------|
| str | `core::str` | String methods — split, find, replace, trim, parse, chars |
| char | `core::char` | Unicode scalar — properties, escape, case conversion |
| ascii | `core::ascii` | ASCII character classification and conversion |
| encoding | `core::encoding` | Base64, hex, UTF-8/16/32 codecs, BStr |

### Operators (`ops/`)

| Module | Path | Description |
|--------|------|-------------|
| arith | `core::ops` | `Add`, `Sub`, `Mul`, `Div`, `Rem`, `Neg` |
| bit | `core::ops` | `BitAnd`, `BitOr`, `BitXor`, `Shl`, `Shr`, `Not` |
| index | `core::ops` | `Index`, `IndexMut` |
| assign | `core::ops` | `AddAssign`, `SubAssign`, `MulAssign`, etc. |
| range | `core::ops` | `Range`, `RangeInclusive`, `RangeTo`, `RangeFrom` |
| function | `core::ops` | `Fn`, `FnMut`, `FnOnce` |
| deref | `core::ops` | `Deref`, `DerefMut` |
| drop | `core::ops` | `Drop` — custom destructors |
| try_trait | `core::ops` | `Try`, `FromResidual` — `?` operator support |
| coroutine | `core::ops` | `Coroutine`, `CoroutineState` |

### Other

| Module | Path | Description |
|--------|------|-------------|
| fmt | `core::fmt` | `Display`, `Debug`, `Formatter`, `Write` |
| reflect | `core::reflect` | `Reflect`, `TypeInfo`, `FieldInfo`, vtable dispatch |
| ptr | `core::ptr` | `RawPtr`, `NonNull`, `copy`, `write_bytes` |
| ffi | `core::ffi` | `CStr`, `CString`, FFI types |
| slice | `core::slice` | Slice methods — sort, search, chunks |
| array | `core::array` | Fixed-size array methods |
| num | `core::num` | Integer traits, `NonZero`, wrapping arithmetic |
| future | `core::future` | `Future`, `Join`, `Select`, `Ready` |
| unicode | `core::unicode` | Unicode tables and categories |
| simd | `core::simd` | SIMD vectors — `F32x4`, `I32x8`, `U8x16` |
| sync | `core::sync` | Atomic operations, fences, spinlocks |
| time | `core::time` | `Duration`, `Instant`, `time_ns` |
