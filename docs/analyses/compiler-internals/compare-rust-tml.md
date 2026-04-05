# Rust core vs TML — Complete Comparison Report

> Generated: 2026-03-28 | TML v0.2.6 | Rust nightly (main branch)
> Updated after Phase 7 sprint (14/16 tasks complete)

## Summary

| Metric | Value |
|--------|-------|
| Rust core modules | **38** |
| TML core match | **36** (95%) |
| TML core partial | **1** (unit) |
| TML core missing | **1** (unit impls — low priority) |
| TML core extras | **12** (arena, bitset, cache, encoding, pool, reflect, ringbuf, soo, simd, profiler, io, random) |
| TML std extras | **17** (bigint, cli, crypto, events, http, json, math, oop, random, regex, search, semver, sqlite, stream, text, uuid, zlib) |

**Overall: 95%+ parity with Rust core + 29 exclusive TML modules (added core::io, core::random).**

---

## Directory Modules (core)

| Rust core | TML core | TML std | Status | Notes |
|-----------|----------|---------|--------|-------|
| `alloc/` | `alloc/` (6 files, 17 pub) | `alloc/` (5 files) | Complete | Heap, Shared, Sync, Arena allocators |
| `array/` | `array/` (3 files, 49 pub) | — | Complete | Array methods, iterators |
| `ascii/` | `ascii/` (2 files, 13 pub) | — | Mostly complete | 10 is_ascii_* classification functions added; AsciiChar enum still missing |
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
| `io/` | `io.tml` (Read/Write/BufRead behaviors) | `io.tml` (IoError) | Mostly complete | Read, Write, BufRead behaviors in core::io; BorrowedBuf still missing |
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
| `prelude/` | `prelude.tml` (18 re-exports) | — | ✅ Done | core::prelude re-exports Maybe, Outcome, List, HashMap, Str, Text, Iterator, Display, Debug, Clone, Eq, Ord, Hash, Default |
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
| `bool.rs` | (impls spread across files) | Mostly complete | Bool has 18 impls (Eq, Ord, Hash, etc); `then_some()` and `then_with()` now available as standalone functions |
| `borrow.rs` | `borrow.tml` (24 pub) | Complete | Borrow, BorrowMut, ToOwned |
| `contracts.rs` | compiler codegen | Complete | pre:/post: conditions with runtime assertions |
| `default.rs` | `default.tml` (16 pub) | Complete | Default trait + impls for all types |
| `error.rs` | `error.tml` (43 pub) | Extensive | Error, IoError, IoErrorKind, ParseError |
| `escape.rs` | `char/methods.tml` | Exists | escape_unicode, escape_default |
| `option.rs` | `option.tml` (31 pub) | Complete | Maybe[T] = Just/Nothing + 30 methods |
| `panic.rs` | `panic.tml` (14 pub) | Exists | panic, PanicInfo |
| `random.rs` | `random.tml` (Random behavior) | — | ✅ Done | core::random has Random trait; std::random::Rng implements it |
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

### Remaining (Low Priority)

| Gap | Description | Effort |
|-----|-------------|--------|
| `unit.rs` impls | Display, Default, Debug for Unit | Small |
| `ascii/` AsciiChar enum | Full AsciiChar enum + EscapeDefault | Medium |

### Completed in Phase 7 Sprints ✅

| Gap | Status |
|-----|--------|
| `Bool::then_some()` / `then_with()` | ✅ Done (phase7-10) |
| `core::prelude` | ✅ Done (phase7-10) |
| `core::io` traits (Read/Write/BufRead) | ✅ Done (phase7-13) |
| `core::random` trait | ✅ Done (phase7-13) |
| `ascii/` is_ascii_* functions (10) | ✅ Done (phase7-13) |

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

> Generated: 2026-03-28 | TML v0.2.6 | Rust nightly (main branch)
> Updated after Phase 7 sprint (14/16 tasks complete)

## Summary (std)

| Metric | Value |
|--------|-------|
| Rust std modules (non-core) | **19** unique |
| TML full match | **15** (80%) |
| TML partial match | **1** (io traits — split across stream/ and file/) |
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
| `env` (vars, var, set_var, args, current_dir, temp_dir) | `std::env` | ✅ Done | get_var, set_var, remove_var, current_dir, temp_dir, args |

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

### Remaining

| Gap | Description | Effort |
|-----|-------------|--------|
| `BufWriter` | Buffered file writer (BufReader exists, BufWriter doesn't) | Small |
| `io` traits consolidation | Read/Write in core::io but stream/ and file/ not fully unified | Medium |

### Completed in Phase 7 Sprints ✅

| Gap | Status |
|-----|--------|
| `env` module | ✅ Done (phase7-04) |
| `Bool::then_some()` / `then_with()` | ✅ Done (phase7-10) |

### Low Priority

| Gap | Description | Effort |
|-----|-------------|--------|
| `autodiff` | Automatic differentiation | Large — nightly-only in Rust too |
| `Unit` impls | Display, Default for `()` | Small |

---

# Part 3: Function-Level Gap Analysis

> Detailed audit of missing functions per module

---

## 1. Maybe[T] (Rust: Option\<T\>)

**Coverage: ~90%** — 26 methods implemented, 6 missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `is_some_and` | `fn is_some_and(self, f: F) -> bool` | Medium |
| `unzip` | `fn unzip(self) -> (Option<A>, Option<B>)` | Low |
| `get_or_insert` | `fn get_or_insert(&mut self, val: T) -> &mut T` | Medium |
| `get_or_insert_with` | `fn get_or_insert_with(&mut self, f: F) -> &mut T` | Medium |
| `replace` | `fn replace(&mut self, val: T) -> Option<T>` | Low |
| `cloned` / `copied` | `fn cloned(self) -> Option<T>` | Low |

**Note:** TML renames `is_some`→`is_just`, `is_none`→`is_nothing`, `or`→`alt`, `and`→`also`. These are naming differences, not missing functionality.

---

## 2. Outcome[T,E] (Rust: Result\<T,E\>)

**Coverage: ~100%** — All Rust Result methods present (with TML naming)

| Missing Function | Notes |
|------------------|-------|
| (none) | Full parity. TML adds extras: `alt`, `also`, `duplicated`, `iter` |

---

## 3. Str (Rust: str)

**Coverage: ~65%** — 30 methods implemented, 16 missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `splitn` / `rsplitn` | `fn splitn(&self, n: usize, pat: P) -> SplitN` | High |
| `rsplit` | `fn rsplit(&self, pat: P) -> RSplit` | Medium |
| `split_once` / `rsplit_once` | `fn split_once(&self, delim: P) -> Option<(&str, &str)>` | High |
| `trim_matches` | `fn trim_matches(&self, pat: P) -> &str` | Medium |
| `strip_prefix` / `strip_suffix` | `fn strip_prefix(&self, prefix: P) -> Option<&str>` | High |
| `matches` / `rmatches` | `fn matches(&self, pat: P) -> Matches` | Medium |
| `replacen` | `fn replacen(&self, pat: P, to: &str, count: usize) -> String` | Medium |
| `char_indices` | `fn char_indices(&self) -> CharIndices` | Low |
| `bytes` | `fn bytes(&self) -> Bytes` | Low |
| `is_ascii` | `fn is_ascii(&self) -> bool` | Medium |
| `eq_ignore_ascii_case` | `fn eq_ignore_ascii_case(&self, other: &str) -> bool` | Medium |
| `encode_utf8` / `encode_utf16` | `fn encode_utf8(&self, dst: &mut [u8]) -> &str` | Low |
| `make_ascii_uppercase` | in-place mutation | Low |
| `make_ascii_lowercase` | in-place mutation | Low |
| `parse[T]` (generic) | `fn parse<F: FromStr>(&self) -> Result<F, F::Err>` | High |

**Note:** TML has `parse_i32`, `parse_i64`, `parse_f64`, `parse_bool` — type-specific but no generic `parse[T]`.

---

## 4. List[T] (Rust: Vec\<T\>)

**Coverage: ~35%** — 16 methods implemented, 28 missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `insert` | `fn insert(&mut self, index: usize, element: T)` | **Critical** |
| `remove` | `fn remove(&mut self, index: usize) -> T` | **Critical** |
| `contains` | `fn contains(&self, x: &T) -> bool` | **Critical** |
| `sort` / `sort_by` | `fn sort(&mut self) where T: Ord` | **Critical** |
| `reverse` | `fn reverse(&mut self)` | **Critical** |
| `swap` | `fn swap(&mut self, a: usize, b: usize)` | High |
| `swap_remove` | `fn swap_remove(&mut self, index: usize) -> T` | High |
| `binary_search` | `fn binary_search(&self, x: &T) -> Result<usize, usize>` | High |
| `iter` | `fn iter(&self) -> Iter<T>` | High |
| `extend` | `fn extend<I: IntoIterator<Item=T>>(&mut self, iter: I)` | High |
| `reserve` | `fn reserve(&mut self, additional: usize)` | Medium |
| `shrink_to_fit` | `fn shrink_to_fit(&mut self)` | Medium |
| `truncate` | `fn truncate(&mut self, len: usize)` | Medium |
| `dedup` | `fn dedup(&mut self) where T: PartialEq` | Medium |
| `windows` | `fn windows(&self, size: usize) -> Windows<T>` | Medium |
| `chunks` | `fn chunks(&self, chunk_size: usize) -> Chunks<T>` | Medium |
| `split_at` | `fn split_at(&self, mid: usize) -> (&[T], &[T])` | Medium |
| `resize` | `fn resize(&mut self, new_len: usize, value: T)` | Medium |
| `fill` | `fn fill(&mut self, value: T) where T: Clone` | Low |
| `flatten` | `Vec<Vec<T>> -> Vec<T>` | Low |
| `sort_unstable` | `fn sort_unstable(&mut self)` | Low |
| `sort_by_key` | `fn sort_by_key<K, F>(&mut self, f: F)` | Low |
| `dedup_by` / `dedup_by_key` | custom deduplicate | Low |
| `split_off` | `fn split_off(&mut self, at: usize) -> Vec<T>` | Low |
| `splice` | `fn splice<R, I>(&mut self, range: R, replace_with: I)` | Low |
| `repeat` | repeat into new Vec | Low |

**Note:** List[T] is C-backed (via runtime). Adding methods requires either C runtime changes or pure TML wrappers. This is the **biggest gap** in the TML stdlib.

---

## 5. HashMap[K,V]

**Coverage: ~50%** — 10 methods implemented, 14 missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `is_empty` | `fn is_empty(&self) -> bool` | **Critical** |
| `entry` API | `fn entry(&mut self, key: K) -> Entry<K, V>` | **Critical** |
| `keys` | `fn keys(&self) -> Keys<K, V>` | High |
| `values` | `fn values(&self) -> Values<K, V>` | High |
| `contains_key` | `fn contains_key<Q>(&self, k: &Q) -> bool` | High (TML has `has`) |
| `retain` | `fn retain<F>(&mut self, f: F)` | Medium |
| `drain` | `fn drain(&mut self) -> Drain<K, V>` | Medium |
| `extend` | `fn extend<I: IntoIterator<Item=(K,V)>>(&mut self, iter: I)` | Medium |
| `get_or_insert_with` | via Entry API | Medium |
| `capacity` | `fn capacity(&self) -> usize` | Low |
| `reserve` | `fn reserve(&mut self, additional: usize)` | Low |
| `shrink_to_fit` | `fn shrink_to_fit(&mut self)` | Low |
| `get_mut` | `fn get_mut(&mut self, k: &K) -> Option<&mut V>` | Low (value semantics) |
| `iter_mut` | `fn iter_mut(&mut self) -> IterMut<K, V>` | Low (value semantics) |

**Note:** `contains_key` exists as `has()` and `insert` exists as `set()` — naming differences. The Entry API is the biggest missing feature.

---

## 6. Iterator Adapters

**Coverage: ~85%** — Extensive adapter coverage, few missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `collect` | `fn collect<B: FromIterator>(self) -> B` | **Critical** |
| `max` / `min` | `fn max(self) -> Option<T> where T: Ord` | High |
| `max_by_key` / `min_by_key` | `fn max_by_key<B, F>(self, f: F) -> Option<T>` | Medium |
| `partition` | `fn partition<B, F>(self, f: F) -> (B, B)` | Medium |
| `unzip` | `fn unzip<A, B>(self) -> (Vec<A>, Vec<B>)` | Medium |
| `by_ref` | `fn by_ref(&mut self) -> &mut Self` | Low |
| `is_sorted` | `fn is_sorted(self) -> bool` | Low |
| `partial_cmp` | `fn partial_cmp<I>(self, other: I) -> Option<Ordering>` | Low |
| `ne`/`lt`/`le`/`gt`/`ge` | comparison shortcuts | Low |

**Note:** `.collect()` is the single most important missing iterator method. Without it, iterators must be consumed manually with `fold` or `for_each`.

---

## 7. File I/O

**Coverage: ~70%** — Core operations present, advanced features missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `read` (binary) | `fn read(path: P) -> io::Result<Vec<u8>>` | High |
| `write` (binary) | `fn write(path: P, contents: &[u8]) -> io::Result<()>` | High |
| `remove_dir_all` | `fn remove_dir_all(path: P) -> io::Result<()>` | High |
| `read_dir` | `fn read_dir(path: P) -> io::Result<ReadDir>` | High |
| `metadata` | `fn metadata(path: P) -> io::Result<Metadata>` | Medium |
| `canonicalize` | `fn canonicalize(path: P) -> io::Result<PathBuf>` | Medium |
| `set_permissions` | `fn set_permissions(path: P, perm: Permissions)` | Low |
| `read_link` / `symlink` / `hard_link` | symbolic/hard link operations | Low |
| `File::read` (bytes) | `fn read(&mut self, buf: &mut [u8]) -> io::Result<usize>` | High |
| `File::write` (bytes) | `fn write(&mut self, buf: &[u8]) -> io::Result<usize>` | High |
| `File::read_to_string` | instance method (TML has static only) | Medium |

---

## 8. Thread

**Coverage: ~75%** — Core API present, some stubs

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `sleep(Duration)` | `fn sleep(dur: Duration)` | Medium (has `sleep_ms`) |
| `park_timeout` | `fn park_timeout(dur: Duration)` | Low |
| `panicking()` | `fn panicking() -> bool` | Low |
| `Builder::spawn` | currently returns Err (stub) | Medium |
| `park` / `unpark` | real implementation (currently stubs) | Medium |
| `thread_local!` | thread-local storage | Medium |

---

## 9. Sync Primitives

**Coverage: ~85%** — Solid implementation, missing bounded channels and poisoning

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `sync_channel` | `fn sync_channel<T>(bound: usize) -> (SyncSender, Receiver)` | High |
| `Mutex::is_poisoned` | `fn is_poisoned(&self) -> bool` | Low (by design) |
| `Arc::make_mut` | `fn make_mut(this: &mut Arc<T>) -> &mut T` | Medium |
| `Receiver: Iterator` | `impl Iterator for Receiver<T>` | Medium |

**Note:** TML explicitly omits poisoning by design — simpler API.

---

## 10. Net

**Coverage: ~90%** — Exceeds Rust in some areas (TLS, async, IOCP)

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `TcpListener::incoming` | `fn incoming(&self) -> Incoming` | Medium |
| `TcpStream::read_to_end` | `fn read_to_end(&mut self, buf: &mut Vec<u8>)` | Medium |
| `TcpStream::write_all` | `fn write_all(&mut self, buf: &[u8])` | Medium |
| `try_clone` (TCP/UDP) | `fn try_clone(&self) -> io::Result<Self>` | Low |
| IPv6 multicast | `join_multicast_v6`, `leave_multicast_v6` | Low |

---

## Critical Gaps Summary

### Critical (blocks common patterns)

| Gap | Module | Impact |
|-----|--------|--------|
| `List.sort()` | List[T] | Can't sort lists |
| `List.insert()` / `remove()` | List[T] | Can't modify list at arbitrary positions |
| `List.contains()` | List[T] | Can't check membership |
| `List.reverse()` | List[T] | Can't reverse a list |
| `Iterator.collect()` | Iterator | Can't materialize iterators into collections |
| `HashMap.is_empty()` | HashMap | Basic check missing |
| `HashMap.entry()` API | HashMap | No upsert/get-or-insert pattern |
| `env` module | std | No environment variable access |

### High (frequently needed)

| Gap | Module |
|-----|--------|
| `Str.split_once()` / `strip_prefix()` / `strip_suffix()` | Str |
| `Str.splitn()` | Str |
| `Str.is_ascii()` / `eq_ignore_ascii_case()` | Str |
| `List.sort_by()` / `binary_search()` | List[T] |
| `List.iter()` / `extend()` | List[T] |
| `List.swap()` / `swap_remove()` | List[T] |
| `HashMap.keys()` / `values()` | HashMap |
| `Iterator.max()` / `min()` | Iterator |
| `File.read` (binary bytes) | File I/O |
| `read_dir()` (directory listing) | File I/O |
| `remove_dir_all()` | File I/O |
| `sync_channel` (bounded) | Sync |

### Medium

| Gap | Module |
|-----|--------|
| `Maybe.is_some_and()` / `get_or_insert()` | Maybe |
| `Str.replacen()` / `matches()` / `trim_matches()` | Str |
| `List.dedup()` / `truncate()` / `windows()` / `chunks()` | List[T] |
| `HashMap.retain()` / `drain()` | HashMap |
| `Iterator.partition()` / `unzip()` / `max_by_key()` | Iterator |
| `Thread.park_timeout()` / `Builder.spawn` (real impl) | Thread |
| `File.metadata()` / `canonicalize()` | File I/O |
| `prelude` module | Core |
| `Bool.then()` / `then_some()` | Bool |
