# Rust core vs TML — Complete Comparison Report

> Generated: 2026-03-28 | TML v0.2.1 | Rust nightly (main branch)

## Summary

| Metric | Value |
|--------|-------|
| Rust core modules | **38** |
| TML core match | **33** (87%) |
| TML core partial | **3** (ascii, io, Bool) |
| TML core missing | **2** (prelude, unit) |
| TML core extras | **10** (arena, bitset, cache, encoding, pool, reflect, ringbuf, soo, simd, profiler) |
| TML std extras | **17** (bigint, cli, crypto, events, http, json, math, oop, random, regex, search, semver, sqlite, stream, text, uuid, zlib) |

**Overall: 87% parity with Rust core + 27 exclusive TML modules.**

---

## Directory Modules (core)

| Rust core | TML core | TML std | Status | Notes |
|-----------|----------|---------|--------|-------|
| `alloc/` | `alloc/` (6 files, 17 pub) | `alloc/` (5 files) | Complete | Heap, Shared, Sync, Arena allocators |
| `array/` | `array/` (3 files, 49 pub) | — | Complete | Array methods, iterators |
| `ascii/` | `ascii/` (2 files, 3 pub) | — | Partial | Rust has more (AsciiChar, EscapeDefault) |
| `async_iter/` | `async_iter.tml` (15 pub) | — | Exists | AsyncIterator, adapters |
| `bstr/` | `bstr.tml` (5 pub) | — | Exists | Byte string utilities |
| `cell/` | `cell/` (6 files, 8 pub) | — | Complete | Cell, RefCell, UnsafeCell, OnceCell |
| `char/` | `char/` (4 files, 23 pub) | — | Complete | Char methods, escape, unicode |
| `clone/` | `clone.tml` (23 pub) | — | Complete | Clone, Duplicate, Copy |
| `cmp/` | `cmp.tml` (46 pub) | — | Complete | Eq, Ord, PartialEq, PartialOrd, Ordering, min, max |
| `convert/` | `convert.tml` (49 pub) | — | Complete | From, Into, TryFrom, TryInto |
| `ffi/` | `ffi/` (2 files, 48 pub) | `ffi/` (3 files) | Complete | CStr, CString, c_void, FFI types |
| `fmt/` | `fmt/` (9 files, 24 pub) | — | Complete | Display, Debug, Formatter, write! |
| `future/` | `future/` (3 files, 10 pub) | — | Exists | Future, Join, Select, Ready |
| `hash/` | `hash.tml` (36 pub) | `hash.tml` (20 pub) | Complete | Hash, Hasher, SipHash |
| `hint/` | `hint.tml` (8 pub) | — | Exists | unreachable, assume, likely |
| `intrinsics/` | `intrinsics.tml` (96 pub) | — | Extensive | sin, cos, sqrt, SIMD, memory intrinsics |
| `io/` | — | `io.tml` (IoError only) | Partial | Rust has BorrowedBuf; TML only has IoError |
| `iter/` | `iter/` (2 files, 31 pub) | `iter.tml` (15 pub) | Complete | Iterator, adapters, ranges |
| `macros/` | — | — | N/A | TML uses @decorators instead of macros |
| `marker/` | `marker.tml` (20 pub) | — | Complete | Send, Sync, Sized, Unpin, Copy, PhantomData |
| `mem/` | `mem.tml` (22 pub) | — | Complete | size_of, align_of, swap, replace, take, forget |
| `net/` | — | `net/` (16 files, 202 pub) | Complete | IpAddr, TCP, UDP, TLS, DNS, IOCP |
| `num/` | `num/` (7 files, 13 pub) | — | Exists | Integer traits, NonZero, wrapping |
| `ops/` | `ops/` (12 files, 13 pub) | — | Extensive | Add, Sub, Mul, Index, Fn, Range, Deref, Drop |
| `os/` | — | `os/` (4 files) | Exists | OS-specific APIs |
| `panic/` | `panic.tml` (14 pub) | — | Complete | panic, catch, PanicInfo |
| `pin/` | `pin.tml` (13 pub) | — | Complete | Pin, Unpin |
| `prelude/` | — | — | **Missing** | Rust auto-imports Option, Result, etc |
| `primitive/` | — | — | N/A | Documentation only in Rust |
| `ptr/` | `ptr/` (6 files, 27 pub) | — | Complete | null, read, write, copy, NonNull |
| `range/` | `range.tml` (5 pub) | — | Exists | Range, RangeInclusive |
| `slice/` | `slice/` (4 files, 7 pub) | — | Exists | Slice methods, sort |
| `str/` | `str.tml` (58 pub) | `text.tml` (51 pub) | Extensive | Split, find, replace, trim + Text builder |
| `sync/` | `sync.tml` (24 pub) | `sync/` (14 files) | Extensive | Mutex, Arc, RwLock, Barrier, MPSC, Semaphore |
| `task/` | `task.tml` (33 pub) | — | Complete | Poll, Waker, Context, RawWaker |
| `time/` | `time.tml` (31 pub) | `time/` (2 files) | Complete | Duration, Instant, time_ns + DateTime |
| `tuple/` | `tuple.tml` (32 pub) | — | Complete | Tuple impls, destructuring |
| `unicode/` | `unicode/` (3 files, 13 pub) | — | Exists | Unicode tables, categories |

## Flat Modules (core)

| Rust core | TML core | Status | Notes |
|-----------|----------|--------|-------|
| `any.rs` | `any.tml` (28 pub) | Complete | TypeId, Any, AnyValue, downcast |
| `bool.rs` | (impls spread across files) | Partial — missing `then()` | Bool has 18 impls (Eq, Ord, Hash, etc) but lacks `then()`/`then_some()` |
| `borrow.rs` | `borrow.tml` (24 pub) | Complete | Borrow, BorrowMut, ToOwned |
| `contracts.rs` | compiler codegen | Complete | pre:/post: conditions with runtime assertions |
| `default.rs` | `default.tml` (16 pub) | Complete | Default trait + impls for all types |
| `error.rs` | `error.tml` (43 pub) | Extensive | Error, IoError, IoErrorKind, ParseError |
| `escape.rs` | `char/methods.tml` | Exists | escape_unicode, escape_default |
| `option.rs` | `option.tml` (31 pub) | Complete | Maybe[T] = Just/Nothing + 30 methods |
| `panic.rs` | `panic.tml` (14 pub) | Exists | panic, PanicInfo |
| `random.rs` | — | Partial | Rust core has trait; TML has `std::random` with impl |
| `result.rs` | `result.tml` (35 pub) | Complete | Outcome[T,E] = Ok/Err + 34 methods |
| `time.rs` | `time.tml` (31 pub) | Complete | Duration, Instant |
| `tuple.rs` | `tuple.tml` (32 pub) | Complete | Tuple traits and methods |
| `unit.rs` | — | **Missing** | Unit has no dedicated impls |

## TML Core Extras (not in Rust core)

| TML core module | Description | Pub Items |
|-----------------|-------------|-----------|
| `arena.tml` | Arena allocator | 3 |
| `bitset.tml` | Bit set operations | 6 |
| `cache.tml` | LRU/expiry cache | 9 |
| `collections.tml` | Collection re-exports | 0 |
| `encoding/` (14 files) | Base64, Hex, UTF-8/16/32 | 14 |
| `pool.tml` | Object pool | 14 |
| `profiler.tml` | Profiling utilities | 2 |
| `reflect.tml` | Runtime reflection (TypeInfo, FieldInfo, vtable dispatch) | 10 |
| `ringbuf.tml` | Ring buffer | 5 |
| `soo.tml` | Small Object Optimization | 2 |
| `simd/` (8 files) | SIMD vectors (f32x4, i32x8, u8x16, etc.) | 8 |

## TML std Extras (beyond Rust core scope)

| TML std module | Description | Size |
|----------------|-------------|------|
| `bigint.tml` | Arbitrary precision integers (mod_pow, Miller-Rabin) | 975 lines, 47 pub |
| `cli.tml` | CLI argument parsing | 480 lines, 23 pub |
| `collections/` | List, HashMap, BTreeMap, Trie, IntervalTree, Deque, BinaryHeap, Buffer | 12 files |
| `crypto/` | SHA, AES, HMAC, ECDH, TLS, SecureRandom | 16 files |
| `events/` | EventEmitter, Observable (RxJS-style), Promise | 3 files |
| `file/` | File I/O, glob, path operations | 6 files |
| `http/` | HTTP server/client/router/middleware | 46 files |
| `json/` | JSON parse/stringify | 4 files |
| `math.tml` | Math functions + Complex numbers | 677 lines, 81 pub |
| `net/` | TCP, UDP, TLS, DNS, IOCP, async networking | 16 files, 202 pub |
| `oop/` | C#-style classes, Object base, interfaces | 3 files |
| `random.tml` | xoshiro256** PRNG, Random trait | 535 lines, 42 pub |
| `regex.tml` | Regular expressions (NFA-based) | 1190 lines, 22 pub |
| `search/` | BM25 text search, HNSW vector search | 4 files |
| `semver.tml` | Semantic versioning (parse, compare, ranges) | 563 lines, 18 pub |
| `sqlite/` | SQLite database bindings | 7 files |
| `stream/` | Node.js-style streams (Readable, Writable, Transform, Duplex) | 15 files |
| `sync/` | Mutex, Arc, RwLock, Barrier, MPSC, Semaphore, WaitGroup | 14 files |
| `text.tml` | StringBuilder (Text type) | 1173 lines, 51 pub |
| `thread/` | Thread spawning and management | 3 files |
| `uuid.tml` | UUID v4/v7 generation and parsing | 634 lines, 30 pub |
| `zlib/` | zlib compression/decompression | 10 files |

## Gaps to Address

### High Priority

| Gap | Description | Effort |
|-----|-------------|--------|
| `Bool::then()` / `then_some()` | Popular utility methods | Small (1 file) |
| `core::prelude` | Auto-imported types (Maybe, Outcome, List, etc.) | Medium (compiler change) |

### Medium Priority

| Gap | Description | Effort |
|-----|-------------|--------|
| `core::io` traits | Read, Write, BufRead in core (currently only IoError) | Medium |
| `core::random` trait | Random trait in core (impl in std) | Small |
| `unit.rs` impls | Display, Default, Debug for Unit | Small |
| `ascii/` expansion | AsciiChar enum, is_ascii_* methods | Medium |

### Low Priority / Not Applicable

| Item | Reason |
|------|--------|
| `macros/` | TML uses @decorators, not proc macros |
| `primitive.rs` | Documentation only |
| `unsafe_binder.rs` | Nightly experimental |
| `wtf8.rs` | TML has `encoding/` which covers this |
| `f32`/`f64` modules | TML handles these via `num/` traits + `math.tml` |
| `i8`..`u128` modules | Deprecated in Rust, TML uses `num/` |

---

# Rust std vs TML std — Complete Comparison Report

> Generated: 2026-03-28 | TML v0.2.1 | Rust nightly (main branch)

## Summary (std)

| Metric | Value |
|--------|-------|
| Rust std modules (non-core) | **19** unique |
| TML full match | **13** (68%) |
| TML partial match | **3** (env, io traits, prelude) |
| TML missing | **3** (boxed/rc standalone, autodiff) |
| TML extras beyond Rust | **14** (crypto, http, json, search, sqlite, oop, events, zlib, bigint, regex, semver, uuid, text, cli) |

## Rust std Modules — Full Comparison

### Filesystem & Path

| Rust std | TML | Status | Notes |
|----------|-----|--------|-------|
| `fs` (File, read, write, metadata, permissions) | `std::file/` (6 files) | ✅ Complete | File, BufReader, Path, Dir, Glob + Seek |
| `path` (Path, PathBuf, Component) | `std::file::path` (33 pub) | ✅ Complete | exists, is_file, is_dir, join, parent, filename, extension, absolute, create_dir_all |

### I/O

| Rust std | TML | Status | Notes |
|----------|-----|--------|-------|
| `io` (Read, Write, BufRead, Cursor, stdin/stdout/stderr) | `std::io` (IoError only) + `std::stream/` (15 files) | ⚠️ Split | IoError in `io.tml`; Read/Write traits in `stream::Readable`/`stream::Writable`; BufReader in `file::bufio` |
| `io::BufReader/BufWriter` | `std::file::bufio` (BufReader) | ⚠️ Partial | BufReader exists; BufWriter missing |

### Networking

| Rust std | TML | Status | Notes |
|----------|-----|--------|-------|
| `net` (TcpListener, TcpStream, UdpSocket, IpAddr, SocketAddr) | `std::net/` (16 files, 202 pub) | ✅ Exceeds Rust | TCP, UDP, TLS, DNS, IOCP, async variants, IpAddr, Ipv4Addr, Ipv6Addr |

### Process & Environment

| Rust std | TML | Status | Notes |
|----------|-----|--------|-------|
| `process` (Command, Child, Stdio, exit) | `std::os::subprocess` | ✅ Exists | Command builder, Stdio redirect, output capture |
| `env` (vars, var, set_var, args, current_dir, temp_dir) | — | ❌ Missing | No env var access. `std::cli` handles args but not env vars |

### Concurrency

| Rust std | TML | Status | Notes |
|----------|-----|--------|-------|
| `thread` (spawn, sleep, JoinHandle, Builder) | `std::thread/` (3 files) | ✅ Complete | Thread spawn, join, sleep |
| `sync` (Mutex, Arc, RwLock, Condvar, Barrier, mpsc) | `std::sync/` (14 files) | ✅ Exceeds Rust | Mutex, Arc, RwLock, Barrier, Condvar, MPSC, Semaphore, WaitGroup, Once, AtomicQueue, AtomicStack |

### Memory & Smart Pointers

| Rust std | TML | Status | Notes |
|----------|-----|--------|-------|
| `boxed` (Box\<T\>) | `core::alloc::Heap[T]` | ✅ Equivalent | `Heap[T]` = heap-allocated owned value |
| `rc` (Rc\<T\>, Weak\<T\>) | `core::alloc::Shared[T]` | ✅ Equivalent | `Shared[T]` = reference-counted pointer |
| `sync::Arc` | `std::sync::Arc[T]` | ✅ Complete | Atomic reference counting |
| `vec` (Vec\<T\>) | `std::collections::List[T]` | ✅ Equivalent | Dynamic array with push/pop/get/set/iter |
| `string` (String) | `Str` (builtin) + `std::text::Text` | ✅ Equivalent | `Str` is immutable string; `Text` is growable StringBuilder |

### Collections

| Rust std | TML | Status | Notes |
|----------|-----|--------|-------|
| `HashMap` | `std::collections::HashMap[K,V]` | ✅ Complete | Swiss-table style, open addressing |
| `BTreeMap` | `std::collections::BTreeMap[K,V]` | ✅ Complete | Balanced tree map |
| `BTreeSet` | `std::collections::BTreeSet[T]` | ✅ Complete | Balanced tree set |
| `HashSet` | `std::collections::HashSet[T]` (class) | ✅ Complete | Hash-based set |
| `VecDeque` | `std::collections::Deque[T]` | ✅ Complete | Double-ended queue |
| `BinaryHeap` | `std::collections::BinaryHeap[T]` | ✅ Complete | Priority queue |
| `LinkedList` | `std::collections::LinkedList[T]` (class) | ✅ Complete | Doubly-linked list |

### Error Handling

| Rust std | TML | Status | Notes |
|----------|-----|--------|-------|
| `error` (Error trait) | `core::error` (43 pub) | ✅ Complete | Error, IoError, ParseError, IoErrorKind |
| `backtrace` (Backtrace) | `lib/backtrace/` (4 files) + C runtime | ✅ Complete | Stack capture, symbol resolution, formatting |
| `panic` / `panicking` | `core::panic` + `essential.c` | ✅ Complete | panic, catch_unwind equivalent, PanicInfo |

### Time

| Rust std | TML | Status | Notes |
|----------|-----|--------|-------|
| `time` (Duration, Instant, SystemTime) | `core::time` (31 pub) + `std::time/` (2 files) | ✅ Complete | Duration, Instant, time_ns, DateTime, formatting |

### OS

| Rust std | TML | Status | Notes |
|----------|-----|--------|-------|
| `os` (platform-specific) | `std::os/` (4 files) | ✅ Exists | Subprocess, Signal, Pipe |
| `os::unix` / `os::windows` | Conditional compilation | ✅ Exists | `#if WINDOWS` / `#if LINUX` directives |

### Other Rust std

| Rust std | TML | Status | Notes |
|----------|-----|--------|-------|
| `prelude` | — | ❌ Missing | No auto-imports |
| `random` | `std::random` (42 pub) | ✅ Complete | xoshiro256**, Random trait, Rng |
| `alloc` | `core::alloc/` + `std::alloc/` | ✅ Complete | Heap, Shared, Sync, GlobalAlloc |
| `ffi` (OsStr, OsString, CStr, CString) | `core::ffi/` + `std::ffi/` | ✅ Complete | CStr, CString, FFI types |
| `num` | `core::num/` (7 files) | ✅ Exists | NonZero, Wrapping, traits |
| `autodiff` | — | ❌ Missing | Automatic differentiation (nightly Rust) |

## TML Extras Beyond Rust std

| TML module | Description | Rust equivalent |
|------------|-------------|-----------------|
| `std::crypto/` (16 files) | SHA, AES, HMAC, ECDH, TLS, DH | External crate (`ring`, `rustls`) |
| `std::http/` (46 files) | Full HTTP server/client/router/middleware | External crate (`hyper`, `axum`) |
| `std::json/` (4 files) | JSON parse/stringify | External crate (`serde_json`) |
| `std::sqlite/` (7 files) | SQLite database bindings | External crate (`rusqlite`) |
| `std::search/` (4 files) | BM25 text search, HNSW vector search | No equivalent |
| `std::zlib/` (10 files) | Compression/decompression | External crate (`flate2`) |
| `std::regex` (1190 lines) | Regular expressions | External crate (`regex`) |
| `std::bigint` (975 lines) | Arbitrary precision integers | External crate (`num-bigint`) |
| `std::semver` (563 lines) | Semantic versioning | External crate (`semver`) |
| `std::uuid` (634 lines) | UUID v4/v7 | External crate (`uuid`) |
| `std::text` (1173 lines) | StringBuilder (Text type) | `String` in Rust (builtin) |
| `std::cli` (480 lines) | CLI argument parsing | External crate (`clap`) |
| `std::events/` (3 files) | EventEmitter, Observable, Promise | No direct equivalent |
| `std::oop/` (3 files) | C#-style classes, interfaces | Not in Rust |
| `std::stream/` (15 files) | Node.js-style streams | External crate (`tokio::io`) |
| `core::reflect` | Runtime reflection, TypeInfo, vtable dispatch | No equivalent (Rust has no reflection) |
| `core::encoding/` (14 files) | Base64, Hex, UTF-8/16/32 codecs | External crate (`base64`, `hex`) |
| `core::simd/` (8 files) | Portable SIMD (f32x4, i32x8...) | `std::simd` (nightly) |

## Gaps to Address (std)

### High Priority

| Gap | Description | Effort |
|-----|-------------|--------|
| `env` module | Environment variable access (getenv, setenv, args, cwd, temp_dir) | Small — FFI to C getenv/setenv |
| `BufWriter` | Buffered file writer (BufReader exists, BufWriter doesn't) | Small |
| `io` traits consolidation | Unify Read/Write traits (currently split across stream/ and file/) | Medium |

### Medium Priority

| Gap | Description | Effort |
|-----|-------------|--------|
| `prelude` | Auto-import common types (Maybe, Outcome, List, HashMap, Str) | Medium — compiler change |
| `Bool::then()` / `then_some()` | Popular utility methods | Small |

### Low Priority

| Gap | Description | Effort |
|-----|-------------|--------|
| `autodiff` | Automatic differentiation | Large — nightly-only in Rust too |
| `Unit` impls | Display, Default for `()` | Small |
