# TML Language Reference

Complete index of everything available in the TML language, standard library, and documentation.

---

## Language Syntax & Features

### Keywords & Operators
- **Reference**: [specs/02-LEXICAL.md](specs/02-LEXICAL.md) | [user/appendix-01-keywords.md](user/appendix-01-keywords.md) | [user/appendix-02-operators.md](user/appendix-02-operators.md)

### Variables & Mutability
- `let` (immutable), `var` (mutable), `const` (compile-time constant)
- **Reference**: [user/ch02-01-variables-and-mutability.md](user/ch02-01-variables-and-mutability.md)

### Data Types
- Primitives: `Bool`, `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`, `F32`, `F64`, `Str`, `Char`
- Compound: Tuples `(A, B)`, Arrays `[T; N]`
- Template literals: `` `Hello, {expr}!` `` → returns `Text`
- **Reference**: [specs/04-TYPES.md](specs/04-TYPES.md) | [user/ch02-02-data-types.md](user/ch02-02-data-types.md)

### Functions
- `func name(params) -> ReturnType { ... }`
- `async func` — async functions (codegen WIP)
- **Reference**: [user/ch02-03-functions.md](user/ch02-03-functions.md)

### Control Flow
- `if` / `else if` / `else`
- `loop (condition) { ... }` — unified loop (replaces for/while/loop)
- `for item in iterable { ... }` — iterator-based loop
- `for i in 0 to 10` / `for i in 1 through 10` — range loops
- `when value { pattern -> expr }` — pattern matching
- `if let Just(x) = maybe_value { ... }` — conditional binding
- `break`, `continue`, `return`
- **Reference**: [user/ch02-05-control-flow.md](user/ch02-05-control-flow.md) | [user/ch04-02-pattern-matching.md](user/ch04-02-pattern-matching.md)

### Structs & Methods
- `type Name { field: Type }` — struct definition
- `impl Name { ... }` — method implementation
- `@auto(equal, duplicate, default, debug, hash, order)` — derive macros
- **Reference**: [user/ch03-00-structs.md](user/ch03-00-structs.md) | [user/ch03-01-defining-structs.md](user/ch03-01-defining-structs.md) | [user/ch03-02-methods-and-extend.md](user/ch03-02-methods-and-extend.md)

### Enums & Pattern Matching
- `enum Name { Variant1, Variant2(T) }` — enum definition
- `when value { Variant1 -> ..., Variant2(x) -> ... }` — exhaustive matching
- **Reference**: [user/ch04-00-enums.md](user/ch04-00-enums.md) | [user/ch04-01-defining-enums.md](user/ch04-01-defining-enums.md) | [user/ch04-02-pattern-matching.md](user/ch04-02-pattern-matching.md)

### Behaviors (Traits)
- `behavior Name { func method(this) -> T }` — trait definition
- `impl Behavior for Type { ... }` — implementation
- Bounds: `func f[T: Behavior](x: T)`, `where T: A + B`
- `dyn Behavior` — dynamic dispatch (codegen WIP)
- **Reference**: [user/ch05-00-behaviors.md](user/ch05-00-behaviors.md) | [user/ch05-01-defining-behaviors.md](user/ch05-01-defining-behaviors.md) | [user/ch05-02-common-behaviors.md](user/ch05-02-common-behaviors.md) | [user/ch05-03-behavior-objects.md](user/ch05-03-behavior-objects.md)

### Generics
- Generic types: `type Pair[T] { a: T, b: T }`
- Generic functions: `func identity[T](x: T) -> T`
- Bounds: `T: Equal + Ordered`, `where K: Hash, V: Display`
- Const generics: `Array[T; N]`
- **Reference**: [user/ch06-00-generics.md](user/ch06-00-generics.md) | [user/ch06-01-generic-functions-and-types.md](user/ch06-01-generic-functions-and-types.md) | [user/ch06-02-bounds-and-where.md](user/ch06-02-bounds-and-where.md) | [specs/08-GENERICS.md](rfcs/RFC-0008-GENERICS.md)

### Error Handling
- `Maybe[T]` — `Just(value)` / `Nothing`
- `Outcome[T, E]` — `Ok(value)` / `Err(error)`
- `!` operator — propagate errors
- `else` recovery: `risky()! else default_value`
- `catch { ... } else do(err) { ... }` — catch blocks
- **Reference**: [user/ch07-00-error-handling.md](user/ch07-00-error-handling.md) | [user/ch07-01-maybe-and-outcome.md](user/ch07-01-maybe-and-outcome.md) | [user/ch07-02-propagation-and-recovery.md](user/ch07-02-propagation-and-recovery.md) | [specs/15-ERROR-HANDLING.md](specs/15-ERROR-HANDLING.md)

### Ownership & References
- Ownership, borrowing, move semantics (Rust-like)
- `ref T` (shared reference), `mut ref T` (mutable reference)
- Smart pointers: `Heap[T]`, `Shared[T]`, `Sync[T]`
- **Reference**: [user/ch08-00-ownership.md](user/ch08-00-ownership.md) | [user/ch08-01-ownership-rules.md](user/ch08-01-ownership-rules.md) | [user/ch08-02-references.md](user/ch08-02-references.md) | [user/ch08-03-smart-pointers.md](user/ch08-03-smart-pointers.md) | [specs/06-MEMORY.md](specs/06-MEMORY.md)

### Closures
- Syntax: `do(x) x * 2`, `do(x: I32) -> I32 { x * 2 }`
- Fn types: `func(I32) -> I32`
- `FnOnce`, `FnMut`, `Fn` behaviors
- **Reference**: [user/ch09-00-closures.md](user/ch09-00-closures.md) | [user/ch09-01-do-syntax.md](user/ch09-01-do-syntax.md) | [user/ch09-02-closures-as-arguments.md](user/ch09-02-closures-as-arguments.md)

### Iterators
- `Iterator[T]` behavior with 30+ adapters
- `.map()`, `.filter()`, `.fold()`, `.take()`, `.skip()`, `.zip()`, `.chain()`, `.flatten()`, `.enumerate()`, `.rev()`, `.cycle()`, `.take_while()`, `.skip_while()`, `.flat_map()`, `.inspect()`, `.scan()`, `.peekable()`, etc.
- **Reference**: [user/ch10-00-iterators.md](user/ch10-00-iterators.md) | [packages/11-ITER.md](packages/11-ITER.md)

### Modules & Imports
- `use module::path::item`
- `pub` visibility
- **Reference**: [specs/07-MODULES.md](specs/07-MODULES.md) | [specs/29-PACKAGES.md](specs/29-PACKAGES.md)

### OOP (Classes & Interfaces)
- `class Name extends Base { ... }` — class hierarchy
- `interface Name { ... }` — interface contracts
- **Reference**: [user/ch15-01-classes.md](user/ch15-01-classes.md) | [user/ch15-02-interfaces.md](user/ch15-02-interfaces.md) | [packages/40-OOP.md](packages/40-OOP.md)

### Concurrency
- Threads, mutexes, channels, atomics
- **Reference**: [user/ch16-00-concurrency.md](user/ch16-00-concurrency.md) | [specs/32-CONCURRENCY.md](specs/32-CONCURRENCY.md)

### FFI & Low-Level
- `@extern("c") func name(...)` — C FFI
- `lowlevel { ... }` — unsafe blocks
- **Reference**: [user/ch17-00-ffi.md](user/ch17-00-ffi.md) | [user/ch17-01-calling-c.md](user/ch17-01-calling-c.md) | [user/ch17-02-lowlevel.md](user/ch17-02-lowlevel.md) | [specs/17-FFI.md](specs/17-FFI.md) | [specs/22-LOW-LEVEL.md](specs/22-LOW-LEVEL.md)

### Conditional Compilation
- `#if`, `#elif`, `#endif`, `#ifdef`, `#ifndef`
- Symbols: `WINDOWS`, `LINUX`, `MACOS`, `X86_64`, `ARM64`, `DEBUG`, `RELEASE`, `TEST`
- **Reference**: [user/ch18-00-conditional-compilation.md](user/ch18-00-conditional-compilation.md)

### Decorators
- `@test`, `@bench`, `@auto(...)`, `@extern(...)`, `@inline`, `@cold`, `@deprecated`
- **Reference**: [specs/25-DECORATORS.md](specs/25-DECORATORS.md)

---

## Core Library (`core::`)

Source: `lib/core/src/`

### Types & Optionals
| Module | Types | Docs |
|--------|-------|------|
| `core::option` | `Maybe[T]` — `Just(T)`, `Nothing` | [user/ch07-01](user/ch07-01-maybe-and-outcome.md) |
| `core::result` | `Outcome[T, E]` — `Ok(T)`, `Err(E)` | [user/ch07-01](user/ch07-01-maybe-and-outcome.md) |
| `core::error` | `Error` behavior, `SimpleError`, `IoError`, `ParseError` | [user/ch07-03](user/ch07-03-error-types.md) |

### Strings & Characters
| Module | Types / Functions | Docs |
|--------|-------------------|------|
| `core::str` | 58 functions: `len`, `char_at`, `substring`, `contains`, `find`, `split`, `trim`, `replace`, `join`, `parse_i32`, `parse_i64`, etc. | [specs/13-BUILTINS.md](specs/13-BUILTINS.md) |
| `core::char` | `Char`, `is_digit`, `is_alphabetic`, `is_whitespace`, `to_uppercase`, `to_lowercase` | — |
| `core::ascii` | `AsciiChar`, `is_ascii`, `ascii_digit_value` | — |
| `core::bstr` | `ByteStr[T]` — non-UTF-8 byte strings | — |
| `core::unicode` | Unicode 15.1.0 classification, case conversion | — |

### Collections & Iteration
| Module | Types | Docs |
|--------|-------|------|
| `core::iter` | `Iterator`, `IntoIterator`, `FromIterator`, `DoubleEndedIterator`, `ExactSizeIterator` + 30 adapters | [user/ch10-00](user/ch10-00-iterators.md), [packages/11-ITER.md](packages/11-ITER.md) |
| `core::slice` | `Slice[T]`, `MutSlice[T]` — zero-copy views with `get`, `split_at`, `binary_search`, `sort` | — |
| `core::array` | `Array[T; N]` — fixed-size arrays with `from_fn`, `map`, `zip` | — |
| `core::range` | `Range`, `RangeInclusive`, `RangeFrom`, `RangeTo`, `RangeFull` | — |
| `core::collections` | Re-exports of collection behaviors | — |

### Behaviors (Traits)
| Module | Behaviors | Docs |
|--------|-----------|------|
| `core::clone` | `Duplicate`, `Copy` | [user/ch05-02](user/ch05-02-common-behaviors.md) |
| `core::cmp` | `PartialEq`, `Eq`, `PartialOrd`, `Ord`, `Ordering` | [user/ch05-02](user/ch05-02-common-behaviors.md) |
| `core::default` | `Default` | [user/ch05-02](user/ch05-02-common-behaviors.md) |
| `core::fmt` | `Display`, `Debug`, `Write`, `Formatter`, `DebugStruct`, `DebugList` | [packages/20-FMT.md](packages/20-FMT.md) |
| `core::hash` | `Hash`, `Hasher` | — |
| `core::convert` | `From`, `Into`, `TryFrom`, `TryInto`, `AsRef`, `AsMut` | — |
| `core::borrow` | `Borrow`, `BorrowMut`, `ToOwned`, `Cow[T]` | — |
| `core::ops` | `Add`, `Sub`, `Mul`, `Div`, `Rem`, `Neg`, `Not`, `Index`, `IndexMut`, `Deref`, `DerefMut`, `Fn`, `FnMut`, `FnOnce` | [user/appendix-02](user/appendix-02-operators.md) |
| `core::marker` | `Send`, `Sync`, `Sized`, `Unpin` | — |

### Memory & Allocation
| Module | Types | Docs |
|--------|-------|------|
| `core::mem` | `size_of`, `align_of`, `swap`, `replace`, `forget`, `ManuallyDrop`, `Discriminant` | — |
| `core::ptr` | `RawPtr[T]`, `RawMutPtr[T]`, `NonNull[T]` | — |
| `core::pin` | `Pin[T]`, `Unpin` | — |
| `core::alloc` | `Heap[T]`, `Shared[T]`, `Sync[T]`, `Layout`, `Allocator` | [user/ch08-03](user/ch08-03-smart-pointers.md), [packages/12-ALLOC.md](packages/12-ALLOC.md) |
| `core::arena` | `Arena` — bump-pointer allocator | [packages/12-ALLOC.md](packages/12-ALLOC.md) |
| `core::pool` | `Pool[T]` — object pool with lock-free free lists | [packages/12-ALLOC.md](packages/12-ALLOC.md) |
| `core::soo` | `SmallVec[T]`, `SmallString`, `SmallBox[T]` — small object optimization | [packages/12-ALLOC.md](packages/12-ALLOC.md) |
| `core::cache` | `CacheAligned[T]`, `Padded[T]`, `SoaVec[T]` — cache-friendly layouts | [packages/12-ALLOC.md](packages/12-ALLOC.md) |

### Interior Mutability
| Module | Types | Docs |
|--------|-------|------|
| `core::cell` | `Cell[T]`, `RefCell[T]`, `OnceCell[T]`, `LazyCell[T]`, `UnsafeCell[T]` | — |

### Encoding
| Module | Formats | Docs |
|--------|---------|------|
| `core::encoding` | `base64`, `base64url`, `base32`, `base36`, `base45`, `base58`, `base62`, `base85`, `base91`, `hex`, `base16`, `base8`, `ascii85`, `percent` | [packages/04-ENCODING.md](packages/04-ENCODING.md) |

### Async Primitives
| Module | Types | Docs |
|--------|-------|------|
| `core::task` | `Poll[T]` (`Ready`/`Pending`), `Context`, `Waker` | [packages/14-ASYNC.md](packages/14-ASYNC.md) |
| `core::future` | `Future` behavior, `IntoFuture` | [packages/14-ASYNC.md](packages/14-ASYNC.md) |
| `core::async_iter` | `AsyncIterator` behavior | [packages/14-ASYNC.md](packages/14-ASYNC.md) |

### Numeric & Utility
| Module | Types | Docs |
|--------|-------|------|
| `core::num` | `Zero`, `One`, `Bounded`, `Saturating`, `Wrapping`, `NonZero[T]` | — |
| `core::time` | `Duration` | — |
| `core::any` | `TypeId`, `Any` behavior | — |
| `core::reflect` | `TypeInfo`, `FieldInfo`, `Reflect` behavior | — |
| `core::simd` | `I32x4`, `F32x4`, `I64x2`, `F64x2`, `U8x16` — SIMD vectors | — |
| `core::bitset` | `BitSet`, `BitArray` | — |
| `core::ringbuf` | `RingBuffer[T]` — circular buffer | — |
| `core::sync` | Raw atomic operations, memory fences, spinlocks | — |
| `core::panic` | `PanicInfo`, `catch_unwind_fn`, `set_hook`, `clear_hook`, `resume_unwind`, `CatchResult` | — |
| `core::hint` | `likely`, `unlikely`, `black_box_i64/bool/f64`, `spin_loop_hint`, `assume`, `unreachable_unchecked` | — |
| `core::ffi` | `c_void`, `c_int`, `c_uint`, `c_long`, `c_ulong`, `c_size_t`, `c_ssize_t`, `CStr` | — |
| `core::intrinsics` | `ptr_read`, `ptr_write`, `mem_alloc`, `mem_free`, `copy_nonoverlapping` | [specs/23-INTRINSICS.md](specs/23-INTRINSICS.md) |

---

## Standard Library (`std::`)

Source: `lib/std/src/`

### Text & Strings
| Module | Types | Docs |
|--------|-------|------|
| `std::text` | `Text` — mutable string with `push_str`, `push_i64`, `as_str`, auto-grow | [packages/36-TEXT.md](packages/36-TEXT.md) |

### Collections
| Module | Types | Docs |
|--------|-------|------|
| `std::collections::list` | `List[T]` — dynamic array: `push`, `pop`, `get`, `len` | [packages/10-COLLECTIONS.md](packages/10-COLLECTIONS.md) |
| `std::collections::hashmap` | `HashMap[K,V]` — hash table: `set`, `get`, `has`, `remove`, `iter` | [packages/10-COLLECTIONS.md](packages/10-COLLECTIONS.md) |
| `std::collections::btreemap` | `BTreeMap[K,V]` — sorted map | [packages/10-COLLECTIONS.md](packages/10-COLLECTIONS.md) |
| `std::collections::btreeset` | `BTreeSet[T]` — sorted set | [packages/10-COLLECTIONS.md](packages/10-COLLECTIONS.md) |
| `std::collections::deque` | `Deque[T]` — double-ended queue | [packages/10-COLLECTIONS.md](packages/10-COLLECTIONS.md) |
| `std::collections::binary_heap` | `BinaryHeap[T]` — max-heap: `push`, `pop`, `peek`, `from_items`, `into_sorted`, `contains` | [packages/10-COLLECTIONS.md](packages/10-COLLECTIONS.md) |
| `std::collections::binary_heap` | `MinHeap[T]` — min-heap: `push`, `pop`, `peek`, `contains` | [packages/10-COLLECTIONS.md](packages/10-COLLECTIONS.md) |
| `std::collections::buffer` | `Buffer` — byte buffer: `get`, `set`, `write_byte`, `read_byte`, endian read/write, `to_hex` | [packages/03-BUFFER.md](packages/03-BUFFER.md) |

### Concurrency & Synchronization
| Module | Types | Docs |
|--------|-------|------|
| `std::sync::mutex` | `Mutex[T]`, `MutexGuard[T]` | [user/ch16-02](user/ch16-02-sync.md), [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::rwlock` | `RwLock[T]`, `RwLockReadGuard[T]`, `RwLockWriteGuard[T]` | [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::Arc` | `Arc[T]`, `Weak[T]` — thread-safe reference counting | [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::atomic` | `AtomicBool`, `AtomicI32`, `AtomicI64`, `AtomicU32`, `AtomicU64`, `AtomicPtr[T]` | [user/ch16-04](user/ch16-04-atomics.md), [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::mpsc` | `Sender[T]`, `Receiver[T]` — channels | [user/ch16-03](user/ch16-03-channels.md), [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::barrier` | `Barrier` | [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::condvar` | `CondVar` | [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::once` | `Once`, `OnceLock` | [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::ordering` | `Ordering` — memory ordering | [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::queue` | `LockFreeQueue[T]` | [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::stack` | `LockFreeStack[T]` | [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::semaphore` | `Semaphore`, `SemaphoreGuard` — counting semaphore with RAII guard | [packages/13-SYNC.md](packages/13-SYNC.md) |
| `std::sync::wait_group` | `WaitGroup` — wait for N tasks: `add`, `done`, `wait` | [packages/13-SYNC.md](packages/13-SYNC.md) |

### FFI (Foreign Function Interface)
| Module | Types | Docs |
|--------|-------|------|
| `std::ffi::cstring` | `CString` — owned heap-allocated C string with Drop, `new`, `from_raw`, `into_raw`, `as_cstr` | — |
| `std::ffi::os_str` | `OsStr`, `OsString` — platform byte strings, `from_str`, `to_str`, `push` | — |

### Threading
| Module | Types | Docs |
|--------|-------|------|
| `std::thread` | `spawn()`, `Builder`, `JoinHandle`, `current()`, `sleep()`, `yield_now()`, `park()` | [user/ch16-01](user/ch16-01-threads.md), [packages/31-THREAD.md](packages/31-THREAD.md) |

### Networking
| Module | Types | Docs |
|--------|-------|------|
| `std::net::tcp` | `TcpStream`, `TcpListener` — full TCP with timeouts, shutdown | [packages/02-NET.md](packages/02-NET.md) |
| `std::net::socket` | `SocketAddr`, `SocketAddrV4`, `SocketAddrV6`, `Ipv4Addr`, `Ipv6Addr`, `IpAddr` | [packages/02-NET.md](packages/02-NET.md) |
| `std::net::eventloop` | `NetEventLoop`, `Interest`, `Event` | [packages/02-NET.md](packages/02-NET.md) |

### HTTP
| Module | Types | Docs |
|--------|-------|------|
| `std::http::app` | `App` — Express-like framework: `get`, `post`, `put`, `delete`, `listen` | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::router` | `Router`, `RouteMatch` — radix tree routing | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::incoming` | `IncomingMessage` — parsed request | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::server_response` | `ServerResponse` — response builder | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::headers` | `Headers` — header map | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::method` | `Method` — HTTP methods | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::status` | `Status` — status codes (1xx–5xx) | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::client` | `HttpClient` — HTTP client | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::chunked` | Chunked transfer encoding/decoding | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::cors` | CORS handling | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::security` | Security headers (CSP, HSTS, X-Frame-Options) | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::etag` | ETag generation and conditional requests | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::compression` | Response compression (gzip, deflate, brotli, zstd) | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::rate_limit` | Token bucket rate limiting | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::static_server` | Static file serving with MIME types | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::cookie` | Cookie parsing and generation | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::multipart` | Multipart form-data parsing | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::stream` | SSE, NDJSON streaming | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::range` | HTTP range requests | [packages/07-HTTP.md](packages/07-HTTP.md) |
| `std::http::cache_control` | Cache-Control header management | [packages/07-HTTP.md](packages/07-HTTP.md) |

### File I/O
| Module | Types | Docs |
|--------|-------|------|
| `std::file` | `File`, `Dir`, `Path`, `PathBuf`, `BufReader`, `BufWriter`, `LineWriter`, `Lines` | [packages/01-FS.md](packages/01-FS.md) |

### JSON
| Module | Types | Docs |
|--------|-------|------|
| `std::json` | `Json`, `JsonObject`, `JsonArray`, `parse()`, `to_string()`, `ToJson`, `FromJson` behaviors, `Builder` | [user/ch21-00](user/ch21-00-json.md), [packages/09-JSON.md](packages/09-JSON.md) |

### Cryptography
| Module | Types | Docs |
|--------|-------|------|
| `std::crypto::hash` | `sha1`, `sha256`, `sha384`, `sha512`, `sha3_256`, `sha3_384`, `sha3_512`, `md5`, `blake2b`, `blake2s`, `blake3` | [user/ch22-00](user/ch22-00-crypto.md), [packages/05-CRYPTO.md](packages/05-CRYPTO.md) |
| `std::crypto::hmac` | `Hmac`, `hmac_sha256`, `hmac_sha512` | [packages/05-CRYPTO.md](packages/05-CRYPTO.md) |
| `std::crypto::cipher` | `Cipher`, `Decipher`, AES, ChaCha20-Poly1305 | [packages/05-CRYPTO.md](packages/05-CRYPTO.md) |
| `std::crypto::sign` | `Signer`, `Verifier`, RSA, ECDSA, Ed25519, Ed448 | [packages/05-CRYPTO.md](packages/05-CRYPTO.md) |
| `std::crypto::key` | `SecretKey`, `PrivateKey`, `PublicKey`, `KeyPair`, `generate_key`, `generate_key_pair` | [packages/05-CRYPTO.md](packages/05-CRYPTO.md) |
| `std::crypto::kdf` | `pbkdf2`, `scrypt`, `hkdf`, `argon2` | [packages/05-CRYPTO.md](packages/05-CRYPTO.md) |
| `std::crypto::dh` | Diffie-Hellman key exchange | [packages/05-CRYPTO.md](packages/05-CRYPTO.md) |
| `std::crypto::ecdh` | Elliptic curve Diffie-Hellman | [packages/05-CRYPTO.md](packages/05-CRYPTO.md) |
| `std::crypto::x509` | X.509 certificate parsing and validation | [packages/05-CRYPTO.md](packages/05-CRYPTO.md) |
| `std::crypto::random` | Cryptographically secure random generation | [packages/05-CRYPTO.md](packages/05-CRYPTO.md) |

### Compression
| Module | Algorithms | Docs |
|--------|-----------|------|
| `std::zlib` | gzip, deflate, brotli, zstd | [user/ch23-00](user/ch23-00-compression.md), [packages/08-COMPRESS.md](packages/08-COMPRESS.md) |

### Streams
| Module | Types | Docs |
|--------|-------|------|
| `std::stream` | `ReadableStream`, `WritableStream`, `BufferedReader`, `pipe` | [packages/23-STREAM.md](packages/23-STREAM.md) |

### I/O
| Module | Types | Docs |
|--------|-------|------|
| `std::io` | `IoError`, `IoErrorKind` | — |

### OS & Process
| Module | Types | Docs |
|--------|-------|------|
| `std::os` | OS info, subprocess, signals, pipes, env vars, args, cwd | [packages/30-OS.md](packages/30-OS.md), [packages/21-ENV.md](packages/21-ENV.md) |

### Async I/O
| Module | Types | Docs |
|--------|-------|------|
| `std::aio` | Event loop, poller, timer wheel | [packages/27-AIO.md](packages/27-AIO.md) |
| `std::runtime` | Executor, timer, yield, channel | [packages/39-RUNTIME.md](packages/39-RUNTIME.md) |

### OOP
| Module | Types | Docs |
|--------|-------|------|
| `std::object` | `Object` base class | [packages/40-OOP.md](packages/40-OOP.md) |
| `std::oop` | Class hierarchy support | [packages/40-OOP.md](packages/40-OOP.md) |
| `std::interfaces` | Interface system | [packages/40-OOP.md](packages/40-OOP.md) |
| `std::exception` | Exception class hierarchy | [packages/32-EXCEPTION.md](packages/32-EXCEPTION.md) |

### Utilities
| Module | Types | Docs |
|--------|-------|------|
| `std::math` | Trig, log, exp, sqrt, abs, constants (PI, E, etc.) | [packages/26-MATH.md](packages/26-MATH.md) |
| `std::random` | PRNG, distributions, `random_i64`, `random_f64` | [packages/27-RANDOM.md](packages/27-RANDOM.md) |
| `std::time` | `Instant`, `sleep()` | — |
| `std::datetime` | `DateTime`, UTC from timestamps | [packages/16-DATETIME.md](packages/16-DATETIME.md) |
| `std::uuid` | `uuid_v4()`, `uuid_v7()`, parsing | [packages/17-UUID.md](packages/17-UUID.md) |
| `std::url` | URL parsing (RFC 3986) | [packages/34-URL.md](packages/34-URL.md) |
| `std::mime` | MIME type detection from extension | [packages/35-MIME.md](packages/35-MIME.md) |
| `std::semver` | Semantic version parsing, comparison, ranges | [packages/37-SEMVER.md](packages/37-SEMVER.md) |
| `std::regex` | Pattern matching | [packages/15-REGEX.md](packages/15-REGEX.md) |
| `std::glob` | File pattern matching | [packages/33-GLOB.md](packages/33-GLOB.md) |
| `std::log` | Structured logging with levels | [packages/18-LOG.md](packages/18-LOG.md) |
| `std::cli` | Argument parser | [packages/19-ARGS.md](packages/19-ARGS.md) |
| `std::hash` | FNV-1a, Murmur2 (non-crypto) | [packages/28-HASH.md](packages/28-HASH.md) |
| `std::search` | BM25, HNSW, SIMD distance | [packages/29-SEARCH.md](packages/29-SEARCH.md) |
| `std::events` | Event emitter/listener | [packages/25-EVENTS.md](packages/25-EVENTS.md) |
| `std::sqlite` | SQLite3 embedded database | [packages/24-SQLITE.md](packages/24-SQLITE.md) |
| `std::profiler` | Performance profiling | [packages/38-PROFILER.md](packages/38-PROFILER.md) |
| `std::types` | Helpers for `Maybe[T]`, `Outcome[T, E]` | — |
| `std::traits` | Re-exports from core | — |
| `std::iter` | Re-exports and extras | — |
| `std::alloc` | Specialized allocators | [packages/12-ALLOC.md](packages/12-ALLOC.md) |

---

## Test Framework (`test::`)

Source: `lib/test/src/`

| Module | Purpose | Docs |
|--------|---------|------|
| `test::assertions` | `assert`, `assert_eq`, `assert_ne`, `assert_gt`, `assert_lt`, `assert_contains`, etc. | [specs/10-TESTING.md](specs/10-TESTING.md) |
| `test::bench` | `@bench` decorator, benchmarking | [specs/10-TESTING.md](specs/10-TESTING.md) |
| `test::mock` | Mock framework | — |
| `test::property` | Property-based testing (QuickCheck-style) | — |
| `test::e2e` | End-to-end network testing | — |
| `test::coverage` | Code coverage tracking | — |
| `test::report` | Test result formatting | — |
| `test::runner` | Test discovery and execution | — |

---

## Documentation Index

### Language Specification (`docs/specs/`)
| File | Topic |
|------|-------|
| [01-OVERVIEW.md](specs/01-OVERVIEW.md) | Language overview |
| [02-LEXICAL.md](specs/02-LEXICAL.md) | Tokens, keywords, literals |
| [03-GRAMMAR.md](specs/03-GRAMMAR.md) | Formal grammar |
| [04-TYPES.md](specs/04-TYPES.md) | Type system |
| [05-SEMANTICS.md](specs/05-SEMANTICS.md) | Semantic rules |
| [06-MEMORY.md](specs/06-MEMORY.md) | Ownership, borrowing, lifetimes |
| [07-MODULES.md](specs/07-MODULES.md) | Module system |
| [08-IR.md](specs/08-IR.md) | Intermediate representation |
| [09-CLI.md](specs/09-CLI.md) | Command-line interface |
| [10-TESTING.md](specs/10-TESTING.md) | Test framework |
| [11-DEBUG.md](specs/11-DEBUG.md) | Debugging |
| [12-ERRORS.md](specs/12-ERRORS.md) | Error codes |
| [13-BUILTINS.md](specs/13-BUILTINS.md) | Built-in functions |
| [14-EXAMPLES.md](specs/14-EXAMPLES.md) | Code examples |
| [15-ERROR-HANDLING.md](specs/15-ERROR-HANDLING.md) | Error handling design |
| [16-COMPILER-ARCHITECTURE.md](specs/16-COMPILER-ARCHITECTURE.md) | Compiler pipeline |
| [17-FFI.md](specs/17-FFI.md) | Foreign function interface |
| [18-ABI.md](specs/18-ABI.md) | Application binary interface |
| [19-RUNTIME.md](specs/19-RUNTIME.md) | Runtime system |
| [20-STDLIB.md](specs/20-STDLIB.md) | Standard library design |
| [21-TARGETS.md](specs/21-TARGETS.md) | Target platforms |
| [22-LOW-LEVEL.md](specs/22-LOW-LEVEL.md) | Low-level operations |
| [23-INTRINSICS.md](specs/23-INTRINSICS.md) | Compiler intrinsics |
| [24-SYSCALL.md](specs/24-SYSCALL.md) | System calls |
| [25-DECORATORS.md](specs/25-DECORATORS.md) | Decorator system |
| [26-FORMATTER.md](specs/26-FORMATTER.md) | Code formatter |
| [27-AST.md](specs/27-AST.md) | Abstract syntax tree |
| [28-CHECKER.md](specs/28-CHECKER.md) | Type checker |
| [29-PACKAGES.md](specs/29-PACKAGES.md) | Package system |
| [30-MIR.md](specs/30-MIR.md) | Mid-level IR |
| [31-HIR.md](specs/31-HIR.md) | High-level IR |
| [32-CONCURRENCY.md](specs/32-CONCURRENCY.md) | Concurrency model |

### User Guide (`docs/user/`)
| Chapter | Topic |
|---------|-------|
| [ch01](user/ch01-00-getting-started.md) | Getting started |
| [ch02](user/ch02-00-common-programming-concepts.md) | Variables, types, functions, control flow |
| [ch03](user/ch03-00-structs.md) | Structs and methods |
| [ch04](user/ch04-00-enums.md) | Enums and pattern matching |
| [ch05](user/ch05-00-behaviors.md) | Behaviors (traits) |
| [ch06](user/ch06-00-generics.md) | Generics |
| [ch07](user/ch07-00-error-handling.md) | Error handling |
| [ch08](user/ch08-00-ownership.md) | Ownership and references |
| [ch09](user/ch09-00-closures.md) | Closures |
| [ch10](user/ch10-00-iterators.md) | Iterators |
| [ch15](user/ch15-01-classes.md) | OOP (classes, interfaces) |
| [ch16](user/ch16-00-concurrency.md) | Concurrency (threads, sync, channels, atomics) |
| [ch17](user/ch17-00-ffi.md) | FFI and low-level |
| [ch18](user/ch18-00-conditional-compilation.md) | Conditional compilation |
| [ch19](user/ch19-00-bitwise-operations.md) | Bitwise operations |
| [ch20](user/ch20-00-standard-library.md) | Standard library overview |
| [ch21](user/ch21-00-json.md) | JSON |
| [ch22](user/ch22-00-crypto.md) | Cryptography |
| [ch23](user/ch23-00-compression.md) | Compression |
| [ch24](user/ch24-00-networking.md) | Networking |

### Package Documentation (`docs/packages/`)
40 package-level API guides — see [packages/00-INDEX.md](packages/00-INDEX.md)

### RFCs (`docs/rfcs/`)
| RFC | Topic |
|-----|-------|
| [RFC-0001](rfcs/RFC-0001-CORE.md) | Core types |
| [RFC-0002](rfcs/RFC-0002-SYNTAX.md) | Syntax design |
| [RFC-0003](rfcs/RFC-0003-CONTRACTS.md) | Function contracts |
| [RFC-0004](rfcs/RFC-0004-ERRORS.md) | Error handling |
| [RFC-0005](rfcs/RFC-0005-MODULES.md) | Module system |
| [RFC-0006](rfcs/RFC-0006-OO.md) | Object-oriented features |
| [RFC-0007](rfcs/RFC-0007-IR.md) | Intermediate representation |
| [RFC-0008](rfcs/RFC-0008-GENERICS.md) | Generics |
| [RFC-0010](rfcs/RFC-0010-TESTING.md) | Testing framework |
| [RFC-0011](rfcs/RFC-0011-FFI.md) | Foreign function interface |
| [RFC-0012](rfcs/RFC-0012-MIR.md) | Mid-level IR |
| [RFC-0013](rfcs/RFC-0013-HIR.md) | High-level IR |
| [RFC-0014](rfcs/RFC-0014-OOP-CLASSES.md) | OOP classes |
| [RFC-0015](rfcs/RFC-0015-JSON.md) | JSON support |

---

## Known Codegen Bugs

These features are fully parsed and type-checked but produce invalid LLVM IR:

| Bug | Symptom | Workaround |
|-----|---------|------------|
| Bool in structs + fn pointers | SEGFAULT at runtime | Use `I64` instead of `Bool` in structs passed through fn ptrs |
| `dyn Behavior` dispatch | Invalid IR: undefined value | Use monomorphized generics instead |
| `async func` + `.await` chain | IR type mismatch (i64 vs i32) | Use threads + manual state machines |
