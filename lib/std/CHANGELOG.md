# Changelog — TML Standard Library (`lib/std`)

All notable changes to the TML standard library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.5] — 2026-04-06

### Added

- **`std::intern` module** — String interning for efficient deduplication
  - `InternPool[T]` — thread-safe intern pool with HashMap backing
  - `intern(s: Str) -> Str` — returns interned copy, guaranteed unique pointer
  - Methods: `len()`, `is_empty()`, `clear()`, `contains()`, `remove()`
  - Thread-safe: Mutex-wrapped with weak reference tracking
  - Use case: JSON keys, HTML attributes, compiler symbol tables (12 tests)

- **27 Maybe/Outcome methods** — Comprehensive standard library methods
  - Mapping: `map`, `map_or`, `map_or_else`
  - Composition: `and_then`, `or_else`, `flatten`
  - Extraction: `ok_or`, `ok_or_else`, `unwrap_or`, `unwrap_or_else`, `unwrap_or_default`
  - Inspection: `is_ok`, `is_err`, `is_some`, `is_none`, `as_ref`, `as_mut`
  - Error handling: `expect`, `expect_err`, `transpose`

## [0.2.4] — 2026-03-28

### Added
- **`std::bigint::BigInt`** — arbitrary precision integers (975 lines, 47 pub, 26 tests)
- **`std::collections::Trie[V]`** — prefix tree with autocomplete (10 tests)
- **`std::collections::IntervalTree[V]`** — augmented BST range queries (9 tests)
- **`impl Seek for File`** — `file_seek_from` FFI wrapper for fseek/ftell
- **`file_seek_from()` in C runtime** — seek with SEEK_SET/CUR/END, returns new position

### Changed
- **Reorganized loose files into directories**: math/, time/, debug/, events/ (new dirs); glob→file/, url/mime→net/
- **`base` → `base_ptr`** in debug.tml (keyword conflict fix)

## [0.2.3] — 2026-03-25

### Added

- **MinHeap[T]** (`collections/binary_heap.tml`) — min-heap priority queue
  - `new`, `push`, `pop`, `peek`, `len`, `is_empty`, `clear`, `contains`
  - Smallest element always at top
- **BinaryHeap advanced operations** — `from_items`, `into_sorted`, `contains`, `extend`
- **SemaphoreGuard** (`sync/semaphore.tml`) — RAII guard for semaphore permits
  - `acquire_guard()` → auto-release on scope exit
  - `try_acquire_guard() -> Maybe[SemaphoreGuard]` — non-blocking
  - `impl Drop for SemaphoreGuard` — releases permit

### Fixed

- **CString Drop** (`ffi/cstring.tml`) — `impl Drop for CString` now frees heap memory via `mem_free`

### Tests

- 12 BinaryHeap tests (6 original + from_items, into_sorted, contains, MinHeap basic/peek/contains)
- 6 Semaphore tests (4 original + acquire_guard, try_acquire_guard)

## [0.2.2] — 2026-03-22

### Changed

- **HTTP performance** — 183K req/s, beating Node.js cluster by 32%
- **Accept-per-worker architecture** — Each worker thread accepts its own connections
- **HTTP pipelining** — Non-destructive method/path parsing, save/restore byte at boundary
- **lowlevel migration** — 702 blocks across 25 files migrated to typed accessor functions
  - HTTP: `shared_get`/`shared_set`, `node_get`/`node_set`, `table_read`/`table_write`, `rd()`/`wr()` byte accessors
  - Stream: `buffered.tml`, `pipe.tml` → Buffer accessors
  - Runtime: `multi_executor.tml`, `timer_wheel.tml` → typed accessors
  - HTTP dispatch/router/parse/conn_pool/rate_limit/agent → structured accessors

### Added

- **Tracy profiler zones** — 70+ instrumented zones in HTTP hot paths, collections, I/O, option, cell, os, glob, math

## [0.2.1] — 2026-03-21

### Added

- **Chunked transfer-encoding** (RFC 7230 §4.1) — decode_chunked, encode_chunk, recv_chunked_body
- **Expect: 100-continue** — sends `HTTP/1.1 100 Continue` before body reading
- **405 Method Not Allowed + Allow header** — probes all 7 method radix trees
- **501 Not Implemented** — for unrecognized HTTP methods
- **Date header** — RFC 7231 Date via app_build_response
- **Idle timeout enforcement** — SO_RCVTIMEO on keep-alive connections
- **Middleware hooks re-enabled** — onRequest, preHandler, onResponse wired into app_dispatch
- **Custom error handler** — onError hooks called on 404 before default response

### Fixed

- **ServerResponse Bool fields → I64** — fixes i1 layout corruption when passed through fn ptrs

### Tests

- 14 URL percent-decoding tests
- 10 chunked transfer-encoding tests (8 decoder + 3 header detection)

## [0.2.0] — 2026-03-19

### Added

- **HTTP Module** (2026-02-24) — `std::http` with full server and client support
  - Router with radix tree, request/response, headers, cookies, encoding, multipart
  - `HttpClient` with `get()`, `post()`, `put()`, `delete()`, `send()`
  - Connection management with DNS + TCP + optional TLS
  - 14 source files, 12 test files
  - **HTTP Date Header** (2026-03-19) — RFC 7231 compliant `Date:` header
  - **HTTP URL Percent-Decoding** (2026-03-19) — `app_percent_decode()` in `parse.tml`
  - **HTTP Latency Measurement** (2026-03-19) — `time_ns()` per-worker stats
  - **HTTP Comparative Analysis** (2026-03-19) — nginx, Tokio/Hyper, Node.js, Go analysis

- **Crypto Module** (2026-02-02) — Comprehensive cryptography suite
  - Random: `random_bytes()`, `random_int()`, `random_uuid()`, `SecureRandom`
  - Hash: SHA-256, SHA-512, MD5, BLAKE3 via `std::crypto::hash`
  - HMAC: Message authentication via `std::crypto::hmac`
  - Cipher: AES-GCM, ChaCha20 via `std::crypto::cipher`
  - KDF: PBKDF2, HKDF, scrypt via `std::crypto::kdf`
  - Signatures: ECDSA, Ed25519 via `std::crypto::sign`
  - Key management, DH, ECDH, RSA, X.509 certificates
  - Windows BCrypt + Linux OpenSSL backends

- **Zlib Module** (2026-02-02) — Compression and decompression
  - Deflate/Inflate, Gzip, Brotli, Zstd compression
  - CRC32 checksums
  - Streaming API: `DeflateStream`, `InflateStream`
  - Configurable compression level, window bits, memory level

- **SQLite Module** (2026-02-24) — `std::sqlite` with FFI bindings
  - `Database` — open, exec, prepare, transactions
  - `Statement` — bind, step, execute, typed column accessors
  - `Row`, `Value` types
  - 7 test files with 77+ tests

- **Regex Module** (2026-02-17) — `std::regex` Thompson's NFA engine
  - `Regex` with `is_match()`, `find()`, `find_all()`, `replace()`, `replace_all()`, `split()`
  - O(n*m) worst case, no exponential backtracking
  - Syntax: `.`, `*`, `+`, `?`, `|`, `()`, `[a-z]`, `[^0-9]`, `\d`, `\w`, `\s`, `^`, `$`
  - 22 tests across 2 files

- **Search Module** (2026-02-11) — Vector search and text indexing
  - `std::search::bm25` — BM25Index with document scoring
  - `std::search::hnsw` — HNSW approximate nearest neighbor, TfIdfVectorizer
  - `std::search::distance` — SIMD distance functions (dot product, L2, cosine)
  - 27 tests across HNSW and BM25

- **Glob Module** (2026-02-11) — `std::glob` pattern matching and directory walking
  - Patterns: `*`, `?`, `**`, `[abc]`/`[a-z]`, `{a,b}` alternation
  - Windows and POSIX support, path normalization
  - 64 tests across 8 test files

- **Stream Module** (2026-02-24) — `std::stream` with composable streams
  - `Readable` and `Writable` behaviors
  - `BufferedReader`, `BufferedWriter` with configurable buffers
  - `ByteStream` — in-memory read/write stream
  - `DuplexStream`, `PassThroughStream`, `PipelineStream`, `TransformStream`
  - `pipe()` for fluent reader → transform → writer chains

- **Async I/O Module** (2026-02-25) — `std::aio` for event-driven I/O
  - `Poller` — Platform I/O polling (epoll/WSAPoll) with token-based dispatch
  - `TimerWheel` — Hashed 2-level timer wheel (O(1) operations)
  - `EventLoop` — Single-threaded event loop (Node.js/libuv-style)
  - 28 tests across 3 test files

- **Async Runtime** (2026-02-23) — Real executor, timer, yield, and channel
  - `Executor`, `TimerState`, `YieldState`, `Channel` types
  - 12 tests across 5 files

- **Math Module** (2026-02-16) — `std::math` comprehensive math functions
  - Trigonometric, hyperbolic, exponential, rounding, utility functions
  - Constants: PI, E, TAU, SQRT_2, LN_2, LN_10, etc.
  - 30 tests across 4 files

- **DateTime Module** (2026-02-16) — `std::datetime` date/time manipulation
  - `DateTime::now()`, `from_timestamp()`, `from_parts()`
  - Calendar: `weekday()`, `day_of_year()`, `is_leap_year()`
  - Formatting: `to_iso8601()`, `to_rfc2822()`
  - Parsing: `parse_iso8601()`, `parse_date()`, `parse()` (2026-02-17)

- **OS Module** (2026-02-16) — `std::os` environment and process interaction
  - Environment variables, CWD, command-line args
  - System info: `cpu_count()`, `total_memory()`, `process_id()`
  - Process execution: `os::exec()`, `os::exec_status()` (2026-02-17)

- **Time Module** (2026-02-16) — `std::time` Instant and SystemTime
  - `Instant::now()`, `elapsed()`, `duration_since()` via `QueryPerformanceCounter`
  - `SystemTime::now()`, `as_secs()`, `duration_since_epoch()`

- **Collections Expansion** (2026-02-17) — New collection types
  - `BTreeMap[K,V]` — Ordered map with binary search
  - `BTreeSet[T]` — Ordered set
  - `Deque[T]` — Double-ended queue with ring buffer
  - `Vec[T]` — Ergonomic alias for `List[T]`

- **Buffer Migration to Pure TML** (2026-02-18) — Buffer fully in TML, no C runtime
  - All collection types (List, HashMap, Buffer) now pure TML

- **Buffered I/O** (2026-02-17) — `std::file::bufio`
  - `BufReader`, `BufWriter`, `LineWriter`
  - 9 tests across 3 categories

- **Random Enhancements** (2026-02-17) — Extended `std::random`
  - `random_i64()`, `random_f64()`, `random_bool()`, `random_range()`
  - `shuffle_i64()`, `shuffle_i32()` Fisher-Yates shuffle

- **Fast Hash Module** (2026-02-04) — `std::hash` non-cryptographic hashes
  - FNV-1a: `fnv1a32()`, `fnv1a64()` — Fast hashes
  - MurmurHash2: `murmur2_32()`, `murmur2_64()` — Seeded hashes
  - ETag helpers for HTTP caching
  - 22 tests

- **TLS Module** (2026-02-10) — TLS/SSL support in `std::net::tls`
  - `TlsConfig`, `TlsStream` with connect/read/write/close
  - Windows certificate store integration (2026-02-23)
  - TLS 1.2/1.3 version constraint tests (2026-02-23)

- **Networking Expansion** (2026-02-22) — TCP/UDP/DNS test coverage 30% → 91%+
  - TCP echo, UDP datagram, socket options, E2E framework
  - 42+ new tests across 15 files

- **Text Type** (2026-01-15) — `std::text` heap-allocated growable strings with SSO
  - 40+ methods: `len`, `push`, `concat`, `substring`, `trim`, `replace`, etc.
  - Template literal syntax: `` `Hello, {name}!` `` produces `Text` type

### Changed

- **IOCP Dynamic Recv Buffers** (2026-03-19) — Per-connection recv buffer from 64KB to 8KB initial
  - Grows dynamically (2x) up to 64KB max, saves 56KB per connection

### Fixed

- **HashMap Content-Based Hashing** (2026-02-24) — Hashes string content, not pointer addresses
  - Key comparison uses byte-by-byte equality, deep copies of Str keys

- **HashMap Overflow Panic with Str Keys** (2026-02-24) — Wrapping multiplication for FNV-1a hashing

- **HashMap Scale Tests** (2026-02-24) — Validates 10K+ entries for self-hosting readiness

- **HTTP Headers String Comparison** (2026-02-24) — Rewrote from HashMap to linear arrays

- **Memory Leaks in Derive and Flags Enum** (2026-02-24) — Fixed Str temporary leaks

- **Drop Double-Free in Crypto** (2026-02-23) — Removed manual `destroy()` conflicting with Drop

- **DH/DHX Key Type Mismatch** (2026-02-23) — Detect local key type for peer key construction

- **Idempotent `destroy()` Across Collections** (2026-02-23) — Null-check guards prevent double-free

- **`os::get_cwd` Static Buffer Corruption** (2026-02-17) — Dynamic allocation instead of static buffer

- **x509 verify_chain List Access** (2026-02-17) — Use `list_len`/`list_get` for TML list handles

- **Crypto Cipher Integer Overflow** (2026-02-16) — `INT_MAX` bounds checks on OpenSSL operations

- **Windows Crypto Build** (2026-02-04) — Fixed include order (`windows.h` before `bcrypt.h`)

### Performance

- **Text and Encoding Optimizations** (2026-02-24) — Batched writes, BCrypt caching, lookup tables

### Test Coverage

- **Comprehensive Expansion** (2026-02-10 through 2026-03-07)
  - std::collections — Buffer swap, HashMap edge cases, List grow
  - std::exception — 15 tests (basics, IO/file, subclasses, formatting)
  - std::file — 12 tests (dir ops, file IO, path basics/components)
  - std::json — 18 tests (arrays, constructors, nested, objects, parse, serialize)
  - std::net — 15 tests (Ipv4Addr, Ipv6Addr, IpAddr, NetError)
  - std::os — 21 tests (CPU/memory, env vars, process info, system info)
  - std::profiler — 3 tests
  - std::sync — 6 tests (Ordering semantics)
  - std::text — 18 tests (basics, constructors, modify, operations, search, transform)
  - std::types — 12 tests (exception format, Outcome, unwrap)

## [0.1.0] — 2025-12-22

### Added
- Initial release with collections (List, HashMap, Buffer)
- File I/O, networking (TCP, UDP, DNS)
- Synchronization (Mutex, RwLock, Condvar, channels)
- Threading (spawn, join, sleep)
- JSON parsing and serialization
- Text processing and formatting
