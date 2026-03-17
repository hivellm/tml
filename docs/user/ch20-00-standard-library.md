# Chapter 20 — Standard Library

TML ships with two layered libraries: **core** (`lib/core`) and **std** (`lib/std`).
Core provides the primitives the rest of the language and standard library build
on — allocation, iterators, formatting, and error handling — with no operating
system dependencies. Std layers I/O, networking, cryptography, collections, and
every other domain-specific capability on top of core.

All collection and algorithm modules in both libraries are implemented in pure TML
using memory intrinsics (`ptr_read`, `ptr_write`, `mem_alloc`, `mem_free`,
`copy_nonoverlapping`). There are no hidden C wrappers.

## The Prelude

A small set of types and functions is automatically in scope in every TML program:

```tml
// Always available — no import needed
Maybe[T]   // Just(x) or Nothing
Outcome[T, E]  // Ok(x) or Err(e)
print(s: Str)
println(s: Str)
```

Everything else requires an explicit `use` statement.

---

## Core Library (`lib/core`)

### `core::alloc` — Memory Allocation

Smart pointer types that manage heap memory:

```tml
use core::alloc::{Heap, Shared, Sync, Arena, Pool}

// Box equivalent: single-owner heap allocation
let boxed: Heap[I32] = Heap::new(42)
println(*boxed)  // 42

// Rc equivalent: reference-counted, single-thread
let shared: Shared[I32] = Shared::new(42)
let copy: Shared[I32] = shared.duplicate()

// Arc equivalent: atomic reference-counted, thread-safe
let arc: Sync[I32] = Sync::new(42)

// Arena: bump allocator — allocate many objects, free all at once
let arena = Arena::new(4096)
let ptr = arena.alloc[I32](10)

// Pool: fixed-size object pool for repeated allocation patterns
let pool = Pool::new[I32](64)
```

### `core::cell` — Interior Mutability

Shared mutable state without a mutable reference:

```tml
use core::cell::{Cell, RefCell, UnsafeCell, Lazy}

// Cell: copy types only, no runtime cost
let c: Cell[I32] = Cell::new(0)
c.set(42)
println(c.get())  // 42

// RefCell: dynamic borrow checking at runtime
let rc: RefCell[List[I32]] = RefCell::new(List::new())
{
    let guard = rc.borrow_mut()
    guard.push(1)
}

// Lazy: initialize exactly once on first access
let config: Lazy[Config] = Lazy::new(do() load_config())
let cfg = config.get()  // initializes on first call
```

### `core::cmp` — Comparison

Behaviors that define ordering:

```tml
use core::cmp::{PartialEq, Eq, PartialOrd, Ord, Ordering}

// Ordering enum returned by comparison methods
let o: Ordering = 3.cmp(5)  // Ordering.Less
when o {
    Ordering.Less    => println("less"),
    Ordering.Equal   => println("equal"),
    Ordering.Greater => println("greater"),
}

// Derive for custom types
@derive(PartialEq, Eq, PartialOrd, Ord)
type Version { major: I32, minor: I32, patch: I32 }
```

### `core::clone` — Duplication

```tml
use core::clone::{Duplicate, Copy}

// Duplicate: explicit cloning (like Rust's Clone)
@derive(Duplicate)
type Config { host: Str, port: I32 }

let a = Config { host: "localhost", port: 8080 }
let b = a.duplicate()

// Copy: implicit bitwise copy (like Rust's Copy)
// Automatically derived for all primitive types
let x: I32 = 5
let y: I32 = x  // copy, not move
```

### `core::convert` — Type Conversions

```tml
use core::convert::{From, Into, TryFrom, TryInto, AsRef, AsMut}

// From / Into: infallible conversions
let n: I64 = I64::from(42 as I32)
let s: Str = Str::from(n)

// TryFrom / TryInto: fallible conversions
let result: Outcome[I32, _] = I32::try_from(9999999999 as I64)
when result {
    Ok(v)  => println("converted: {v}"),
    Err(_) => println("overflow"),
}
```

### `core::default` — Default Values

```tml
use core::default::Default

@derive(Default)
type Config {
    host: Str,   // ""
    port: I32,   // 0
    debug: Bool, // false
}

let cfg = Config::default()
```

### `core::error` — Error Handling Behaviors

```tml
use core::error::{Error, SimpleError, IoError}

// Error behavior: implemented by all error types
behavior Error {
    func message(ref self) -> Str
    func source(ref self) -> Maybe[ref dyn Error]
}

// SimpleError: quick one-off errors
let e = SimpleError::new("something went wrong")
println(e.message())

// IoError: wraps OS I/O errors with a kind tag
```

### `core::fmt` — Formatting

```tml
use core::fmt::{Display, Debug, Binary, Octal, Hex, Formatter}

// Implement Display for human-readable output
extend Point : Display {
    func fmt(ref self, f: mut ref Formatter) {
        f.write("({}, {})", self.x, self.y)
    }
}

// Use format string interpolation (calls Display or Debug)
let p = Point { x: 3, y: 4 }
println("{p}")       // calls Display
println("{p:?}")     // calls Debug
println("{42:b}")    // binary: 101010
println("{42:o}")    // octal: 52
println("{42:x}")    // hex: 2a
```

### `core::hash` — Hashing

```tml
use core::hash::{Hash, DefaultHasher, combine_hashes}

@derive(Hash)
type Point { x: I32, y: I32 }

// Manual hashing
let mut hasher = DefaultHasher::new()
42.hash(mut ref hasher)
"hello".hash(mut ref hasher)
let digest: U64 = hasher.finish()

// Combine multiple hashes (useful in Hash impls)
let h = combine_hashes(h1, h2)
```

### `core::iter` — Iterators

The iterator system is covered in depth in Chapter 10. Key types:

```tml
use core::iter::{Iterator, IntoIterator}

// Iterator behavior
behavior Iterator {
    type Item
    func next(mut ref self) -> Maybe[Self::Item]
}

// Adapters: map, filter, flat_map, take, skip, zip, enumerate, chain, ...
let sum: I32 = (1..=100)
    .filter(do(n) n % 2 == 0)
    .map(do(n) n * n)
    .fold(0, do(acc, x) acc + x)
```

### `core::marker` — Marker Behaviors

```tml
use core::marker::{Send, Sync, Sized, Unpin}

// Send: safe to transfer to another thread
// Sync: safe to share between threads via references
// Both are auto-derived by the compiler when safe
// Sized: type has a known size at compile time (default for all concrete types)
// Unpin: type can be safely moved after being pinned
```

### `core::mem` — Memory Utilities

```tml
use core::mem::{size_of, align_of, swap, replace, ManuallyDrop}

let sz: USize = size_of[I64]()       // 8
let al: USize = align_of[I64]()      // 8

var a: I32 = 1
var b: I32 = 2
swap(mut ref a, mut ref b)           // a=2, b=1

let old: I32 = replace(mut ref a, 99)  // a=99, old=2

// ManuallyDrop: suppress automatic drop
let v: ManuallyDrop[List[I32]] = ManuallyDrop::new(List::new())
// v's destructor will NOT run automatically
```

### `core::num` — Numeric Behaviors

```tml
use core::num::{Zero, One, Bounded}

// Zero / One: additive and multiplicative identities
let z: I32 = I32::zero()  // 0
let o: I32 = I32::one()   // 1

// Bounded: minimum and maximum values
let max: I32 = I32::MAX   // 2147483647
let min: I32 = I32::MIN   // -2147483648
```

### `core::ops` — Operator Behaviors

Implement operators for custom types:

```tml
use core::ops::{Add, Sub, Mul, Div, Neg, Index, IndexMut, Deref, DerefMut, Drop}
use core::ops::{Range, RangeInclusive, Fn, FnMut, FnOnce}

extend Vector2 : Add {
    type Output = Vector2
    func add(self, other: Vector2) -> Vector2 {
        Vector2 { x: self.x + other.x, y: self.y + other.y }
    }
}

let v1 = Vector2 { x: 1.0, y: 2.0 }
let v2 = Vector2 { x: 3.0, y: 4.0 }
let v3 = v1 + v2  // calls Vector2::add
```

### `core::pin` — Pinning

```tml
use core::pin::{Pin, Unpin}

// Pin[P]: guarantees the pointee will not be moved
// Required by async/await and self-referential structures
let pinned: Pin[Heap[I32]] = Pin::new(Heap::new(42))
```

### `core::ptr` — Raw Pointer Utilities

```tml
use core::ptr

// Low-level pointer operations (used in library internals)
// Regular TML code should prefer safe abstractions
let p: *I32 = ptr::null()
let is_null: Bool = ptr::is_null(p)
```

### `core::slice` — Slices

```tml
use core::slice::{Slice, MutSlice}

// Slice[T]: immutable view into a contiguous sequence
// MutSlice[T]: mutable view into a contiguous sequence

let arr: [I32; 5] = [1, 2, 3, 4, 5]
let s: Slice[I32] = arr[1..4]  // [2, 3, 4]

println(s.len())            // 3
println(s[0])               // 2
println(s.contains(ref 3))  // true

// Sort a mutable slice
var data: [I32; 4] = [4, 2, 3, 1]
let ms: MutSlice[I32] = mut ref data[..]
ms.sort()
```

### `core::str` — String Operations

`Str` is TML's immutable string slice type, analogous to `&str` in Rust. Core
provides the low-level string operations:

```tml
use core::str

let s: Str = "hello, world"
println(str::len(s))             // 12
println(str::contains(s, "world"))  // true
let upper: Str = str::to_upper(s)
let parts: List[Str] = str::split(s, ",")
let trimmed: Str = str::trim("  hello  ")
let sub: Str = str::substring(s, 0, 5)  // "hello"
```

### `core::time` — Duration

```tml
use core::time::Duration

let d: Duration = Duration::from_secs(5)
let ms: I64 = d.as_millis()
let ns: I64 = d.as_nanos()
let secs: F64 = d.as_secs_f64()

let total: Duration = Duration::from_secs(3) + Duration::from_millis(500)
```

### `core::borrow` — Borrow Behaviors

```tml
use core::borrow::{Borrow, BorrowMut, Cow}

// Cow: clone-on-write — avoid copying until mutation is needed
let s: Cow[Str] = Cow::Borrowed("hello")
let owned: Cow[Str] = s.into_owned()  // clones only if needed
```

---

## Standard Library (`lib/std`)

### `std::collections` — Data Structures

See Chapter 11 for detailed coverage. The full set of collection types:

```tml
use std::collections::{List, Vec, HashMap, HashSet, BTreeMap, BTreeSet,
                        Deque, Queue, Stack, LinkedList, BinaryHeap}
```

| Type | Description | Complexity |
|------|-------------|------------|
| `List[T]` / `Vec[T]` | Growable array | O(1) amortized push |
| `HashMap[K, V]` | Hash table | O(1) average lookup |
| `HashSet[T]` | Hash-based set | O(1) average contains |
| `BTreeMap[K, V]` | Sorted tree map | O(log n) all operations |
| `BTreeSet[T]` | Sorted tree set | O(log n) all operations |
| `Deque[T]` | Double-ended queue (ring buffer) | O(1) push/pop both ends |
| `Queue[T]` | FIFO queue | O(1) enqueue/dequeue |
| `Stack[T]` | LIFO stack | O(1) push/pop |
| `LinkedList[T]` | Doubly-linked list | O(1) insert/remove with cursor |
| `BinaryHeap[T]` | Max-heap priority queue | O(log n) push/pop |

### `std::crypto` — Cryptography

See Chapter 22 for detailed coverage:

```tml
use std::crypto::hash::{sha256, sha512}
use std::crypto::hmac::hmac_sha256
use std::crypto::cipher::{Cipher, CipherAlgorithm}
use std::crypto::kdf::{pbkdf2, hkdf, argon2}
use std::crypto::sign::{sign, verify}
use std::crypto::random::random_bytes
```

Supports: MD5, SHA-1, SHA-2 family, HMAC, AES-GCM, AES-CBC, ChaCha20-Poly1305,
PBKDF2, HKDF, Scrypt, Argon2id, Ed25519, ECDSA (P-256, P-384), RSA, CSPRNG.

### `std::encoding` — Binary Encoding

```tml
use std::encoding::{hex, base64, base32, percent, ascii85}

// Hex
let encoded: Str = hex::encode(ref bytes)
let decoded: Buffer = hex::decode("48656c6c6f")!

// Base64
let b64: Str = base64::encode(ref bytes)
let raw: Buffer = base64::decode(b64)!

// URL percent encoding
let safe: Str = percent::encode("hello world?")  // "hello%20world%3F"
let orig: Str = percent::decode("hello%20world")!
```

### `std::file` — File I/O

```tml
use std::file::{File, Dir, Path, BufReader, BufWriter}

// Read entire file
let content: Str = File::read_to_string("config.toml")!

// Write a file
File::write_string("output.txt", "hello")!

// Streaming read
let f = File::open("large.csv")!
let reader = BufReader::new(f)
loop line in reader.lines() {
    println(line!)
}

// Directory operations
let entries: List[Path] = Dir::list("src/")!
let exists: Bool = Path::exists("build/debug/bin/tml.exe")
let stem: Str = Path::stem("hello.tml")  // "hello"
```

### `std::json` — JSON

See Chapter 21 for detailed coverage:

```tml
use std::json::{parse, stringify, stringify_pretty, Json}
use std::json::{Serialize, Deserialize}
use std::json::builder::{object, array}
use std::json::stream::{JsonReader, JsonWriter}
```

### `std::net` — Networking

See Chapter 24 for detailed coverage:

```tml
use std::net::tcp::{TcpListener, TcpStream}
use std::net::udp::UdpSocket
use std::net::dns
use std::net::{SocketAddr, Ipv4Addr, Ipv6Addr, IpAddr}
```

### `std::http` — HTTP Client and Server

See Chapter 24 for detailed coverage:

```tml
use std::http::client::HttpClient
use std::http::server::HttpServer
use std::http::request::Request
use std::http::response::Response
use std::http::router::Router
use std::http::{Method, Status, Headers}
use std::http::cors::Cors
use std::http::rate_limit::RateLimiter
```

### `std::compress` — Compression

See Chapter 23 for detailed coverage:

```tml
use std::zlib::deflate::{deflate, inflate}
use std::zlib::gzip::{gzip, gunzip}
use std::zlib::brotli::{brotli_compress, brotli_decompress}
use std::zlib::zstd::{zstd_compress, zstd_decompress}
use std::zlib::crc32::{crc32, adler32}
```

### `std::search` — Full-Text and Vector Search

```tml
use std::search::bm25::BM25
use std::search::hnsw::HNSW
use std::search::distance::{cosine, euclidean, dot_product}

// BM25 full-text search
var index = BM25::new()
index.add_document(0, "the quick brown fox")
index.add_document(1, "jumps over the lazy dog")
let results: List[I64] = index.search("quick fox", 10)

// HNSW approximate nearest-neighbor search
var graph = HNSW::new(128)  // 128-dimensional vectors
graph.insert(0, my_vector)
let neighbors: List[(I64, F64)] = graph.search(query_vec, 5)
```

### `std::sync` — Synchronization Primitives

```tml
use std::sync::{Mutex, RwLock, Arc, Barrier, Once, OnceLock}
use std::sync::mpsc::{channel, Sender, Receiver}
use std::sync::atomic::{AtomicI32, AtomicI64, AtomicBool, AtomicUsize, Ordering}
use std::sync::{LockFreeQueue, LockFreeStack}

// Mutex: mutual exclusion
let m: Mutex[I32] = Mutex::new(0)
{
    let guard = m.lock()
    *guard.get_mut() += 1
}  // lock released

// RwLock: multiple readers, one writer
let rw: RwLock[HashMap[Str, I32]] = RwLock::new(HashMap::new())
let reader = rw.read()     // shared read lock
let writer = rw.write()    // exclusive write lock

// Arc: thread-safe shared ownership
let shared: Arc[Mutex[I32]] = Arc::new(Mutex::new(0))
let clone: Arc[Mutex[I32]] = shared.clone()

// MPSC channel: message passing
let (tx, rx) = channel[I32]()
thread::spawn(do() { tx.send(42) })
let val: I32 = rx.recv()!

// Atomics
let counter: AtomicI64 = AtomicI64::new(0)
counter.fetch_add(1, Ordering::SeqCst)
let n: I64 = counter.load(Ordering::SeqCst)

// Barrier: rendezvous point
let barrier: Barrier = Barrier::new(4)
barrier.wait()  // blocks until 4 threads have called wait()

// Once / OnceLock: one-time initialization
let once: Once = Once::new()
once.call_once(do() { initialize() })
```

### `std::thread` — Thread Management

```tml
use std::thread
use std::thread::{Builder, ThreadLocal, JoinHandle}

// Spawn a thread
let handle: JoinHandle[I32] = thread::spawn(do() {
    // runs in a new thread
    42
})
let result: I32 = handle.join()!

// Named thread with custom stack size
let h = Builder::new()
    .name("worker")
    .stack_size(2 * 1024 * 1024)
    .spawn(do() { do_work() })!

// Scoped threads: borrow from parent scope
thread::scope(do(s) {
    s.spawn(do() { access_parent_data() })
    s.spawn(do() { access_parent_data() })
})  // all scoped threads joined here

// Thread-local storage
let MY_COUNTER: ThreadLocal[I32] = ThreadLocal::new(do() 0)
let val: I32 = MY_COUNTER.get()

// Control
thread::sleep(Duration::from_millis(100))
thread::yield_now()
println(thread::current_id())
```

### `std::regex` — Regular Expressions

Thompson's NFA-based engine — O(n) worst case, no backtracking:

```tml
use std::regex::Regex

let re = Regex::new("^(\\d{4})-(\\d{2})-(\\d{2})$")!

// Test for match
println(re.is_match("2026-03-17"))  // true

// Find a match with position
when re.find("Today is 2026-03-17") {
    Just(m) => println("at {m.start}..{m.end}: {m.text}"),
    Nothing => println("no match"),
}

// Extract capture groups
when re.captures("2026-03-17") {
    Just(caps) => {
        println(caps.get(1))  // "2026"
        println(caps.get(2))  // "03"
        println(caps.get(3))  // "17"
    },
    Nothing => {},
}

// Find all non-overlapping matches
let all = re.find_all("2026-01-01 and 2026-03-17")
```

### `std::datetime` — Date and Time

```tml
use std::datetime::{DateTime, SystemTime}
use std::time::{Instant, Duration}

// Current time
let now: SystemTime = SystemTime::now()
let dt: DateTime = DateTime::from_timestamp(now)

println("{dt.year()}-{dt.month()}-{dt.day()}")       // 2026-03-17
println("{dt.hour()}:{dt.minute()}:{dt.second()}")    // 14:30:05

// Parse from string
let d = DateTime::parse("2026-03-17T14:30:05Z")!

// Format
let s: Str = dt.format("%Y-%m-%d %H:%M:%S")

// High-resolution monotonic clock
let start: Instant = Instant::now()
do_work()
let elapsed: Duration = start.elapsed()
println("elapsed: {elapsed.as_millis()} ms")
```

### `std::uuid` — UUID Generation

```tml
use std::uuid::Uuid

let id_v4: Uuid = Uuid::v4()   // random
let id_v7: Uuid = Uuid::v7()   // time-ordered (sortable)
let id_v1: Uuid = Uuid::v1()   // time-based + MAC

// Name-based (deterministic)
let ns = Uuid::NAMESPACE_URL
let id_v5: Uuid = Uuid::v5(ns, "https://example.com")

println("{id_v4}")                  // e.g. 550e8400-e29b-41d4-a716-446655440000
println(id_v4.to_hex_no_dashes())  // without dashes

let parsed: Uuid = Uuid::parse("550e8400-e29b-41d4-a716-446655440000")!
```

### `std::log` — Logging

```tml
use std::log::{log, debug, info, warn, error, Level, Logger}

// Simple logging macros
debug("processing item {id}")
info("server started on port {port}")
warn("cache miss rate is high: {rate}%")
error("failed to connect to database: {e}")

// Structured logging with key-value pairs
log(Level::Info, "request", [
    ("method", "GET"),
    ("path", "/users/42"),
    ("status", "200"),
    ("duration_ms", "12"),
])

// Configure a logger
let logger = Logger::new()
    .level(Level::Info)
    .filter("std::net", Level::Warn)  // suppress noisy module
    .file("app.log")                  // also write to file
```

### `std::math` — Mathematical Functions

```tml
use std::math

// Trigonometry
let s: F64 = math::sin(math::PI / 6.0)     // 0.5
let c: F64 = math::cos(0.0)                // 1.0
let t: F64 = math::tan(math::PI / 4.0)     // 1.0
let a: F64 = math::atan2(1.0, 1.0)         // PI/4

// Exponential and logarithm
let e: F64 = math::exp(1.0)                // 2.718...
let l: F64 = math::ln(math::E)             // 1.0
let l2: F64 = math::log2(1024.0)           // 10.0
let l10: F64 = math::log10(1000.0)         // 3.0
let p: F64 = math::pow(2.0, 10.0)          // 1024.0

// Rounding
let fl: F64 = math::floor(3.7)             // 3.0
let ce: F64 = math::ceil(3.2)              // 4.0
let ro: F64 = math::round(3.5)             // 4.0

// Utilities
let sq: F64 = math::sqrt(2.0)              // 1.4142...
let ab: F64 = math::abs(-5.0)              // 5.0
let mx: I32 = math::max(10, 20)            // 20
let mn: I32 = math::min(10, 20)            // 10

// Constants
println(math::PI)     // 3.14159265358979...
println(math::E)      // 2.71828182845904...
println(math::TAU)    // 6.28318530717958...
```

### `std::random` — Pseudo-Random Numbers

```tml
use std::random::{Xoshiro256, ThreadRng}

// Seeded generator (deterministic)
var rng = Xoshiro256::seed(12345)
let n: I64 = rng.next_i64()
let f: F64 = rng.next_f64()              // [0.0, 1.0)
let r: I64 = rng.next_range(1, 100)     // [1, 100]

// Thread-local auto-seeded generator
let t: I64 = ThreadRng::next_i64()
let tf: F64 = ThreadRng::next_f64()

// Shuffle a list in place
ThreadRng::shuffle(mut ref items)
```

### `std::text` — Growable Strings

`Text` is a dynamic, mutable string with Small String Optimization (SSO): strings
up to 23 bytes are stored inline on the stack with no heap allocation.

```tml
use std::text::Text

let mut t: Text = Text::from("Hello")
t.push_str(", World!")
t.push_char('!')

println(t.len())                // 14
println(t.to_upper_case())      // "HELLO, WORLD!!"
println(t.to_lower_case())      // "hello, world!!"
println(t.trim())               // trims whitespace
println(t.contains("World"))    // true
println(t.replace("World", "TML"))

// Template literals produce Text automatically
let name = "Alice"
let greeting: Text = `Hello, {name}!`

// Convert to/from Str
let s: Str = t.as_str()
let t2: Text = Text::from(s)
```

### `std::semver` — Semantic Versioning

```tml
use std::semver::Version

let v1: Version = Version::parse("1.2.3")!
let v2: Version = Version::parse("2.0.0-alpha.1")!

println(v1.major)   // 1
println(v1.minor)   // 2
println(v1.patch)   // 3

println(v1 < v2)    // true
println(v2.is_prerelease())  // true

// Check compatibility
let req: Version = Version::parse("^1.0.0")!
println(req.matches(v1))  // true (1.2.3 satisfies ^1.0.0)
```

### `std::url` — URL Parsing

```tml
use std::url::Url

let u: Url = Url::parse("https://user:pass@example.com:8080/path?q=1&x=2#fragment")!

println(u.scheme())    // "https"
println(u.host())      // "example.com"
println(u.port())      // 8080
println(u.path())      // "/path"
println(u.query())     // "q=1&x=2"
println(u.fragment())  // "fragment"

let q: HashMap[Str, Str] = u.query_params()
println(q.get("q"))  // "1"

// Build a URL
let built = Url::builder()
    .scheme("https")
    .host("api.example.com")
    .path("/v1/users")
    .query_param("limit", "20")
    .build()
```

### `std::mime` — MIME Types

```tml
use std::mime

let ct: Str = mime::from_extension("png")      // "image/png"
let ext: Str = mime::to_extension("text/html") // "html"
let is_text: Bool = mime::is_text("text/plain")  // true
```

### `std::glob` — File Pattern Matching

```tml
use std::glob::glob

let files: List[Str] = glob("src/**/*.tml")
let tests: List[Str] = glob("lib/*/tests/**/*.test.tml")

// Glob with options
use std::glob::{GlobOptions, glob_with}
let opts = GlobOptions { case_insensitive: true, dot_files: false }
let results = glob_with("docs/*.md", opts)
```

### `std::os` — Operating System Operations

```tml
use std::os::{Command, Pid, Signal}

// Run a subprocess
let output = Command::new("git")
    .arg("log")
    .arg("--oneline")
    .arg("-10")
    .output()!

println(output.stdout)
println(output.exit_code)

// Run and stream output
let mut child = Command::new("tml")
    .arg("test")
    .spawn()!
child.wait()!
```

### `std::args` — CLI Argument Parsing

```tml
use std::args::{ArgParser, Arg}

let mut parser = ArgParser::new("myapp", "A useful tool")
parser.add(Arg::flag("verbose", "v", "enable verbose output"))
parser.add(Arg::option("output", "o", "output file", "FILE"))
parser.add(Arg::positional("input", "input file"))

let matches = parser.parse()!
let verbose: Bool = matches.flag("verbose")
let output: Str = matches.option("output").unwrap_or("out.txt")
let input: Str = matches.positional("input")!
```

### `std::env` — Environment Variables

```tml
use std::env

let home: Str = env::var("HOME")!
let path: Str = env::var_or("PATH", "/usr/bin")
let all: HashMap[Str, Str] = env::vars()

env::set_var("MY_KEY", "my_value")
env::remove_var("MY_KEY")
```

### `std::buffer` — Binary Buffer Operations

```tml
use std::buffer::Buffer

let mut buf: Buffer = Buffer::new(64)
buf.write_u8(0xFF as U8)
buf.write_i32_be(42)
buf.write_str("hello")

let n: I32 = buf.read_i32_be()
let s: Str = buf.read_str(5)

// From/to byte slices
let raw: [U8; 4] = [0x01, 0x02, 0x03, 0x04]
let b = Buffer::from(ref raw)
```

### `std::hash` — Non-Cryptographic Hashing

Fast hash functions for hash tables and checksums:

```tml
use std::hash::{fnv1a, fnv1a_64, murmur2, siphash}

let h1: U32 = fnv1a("hello")
let h2: U64 = fnv1a_64("hello world")
let h3: U32 = murmur2("data", 0)       // with seed

// SipHash (keyed, DoS-resistant, used in HashMap)
let key0: U64 = 0x0102030405060708
let key1: U64 = 0x090A0B0C0D0E0F00
let h4: U64 = siphash("input", key0, key1)
```

### `std::events` — Event Emitter

```tml
use std::events::{EventEmitter, EventHandler}

var emitter: EventEmitter = EventEmitter::new()

emitter.on("data", do(payload: Str) {
    println("received: {payload}")
})
emitter.on("error", do(msg: Str) {
    println("error: {msg}")
})

emitter.emit("data", "hello")  // prints "received: hello"
emitter.off("data")            // remove all handlers for "data"
emitter.once("close", do(s: Str) { println("closed: {s}") })
```

### `std::stream` — Streaming I/O

```tml
use std::stream::{Readable, Writable, Transform, pipeline}

// Transform stream: uppercase filter
let upper_case = Transform::new(do(chunk: Str) -> Str {
    chunk.to_upper_case()
})

// Pipeline: chain transforms (like Unix pipes)
pipeline(source, upper_case, destination)!
```

### `std::sqlite` — SQLite Database

```tml
use std::sqlite::{Database, Statement, Row}

let db = Database::open("app.db")!

db.execute("CREATE TABLE IF NOT EXISTS users (id INTEGER, name TEXT)")!

let stmt: Statement = db.prepare("INSERT INTO users VALUES (?, ?)")!
stmt.bind(1, 42)!
stmt.bind(2, "Alice")!
stmt.execute()!

let rows: Statement = db.prepare("SELECT * FROM users WHERE id = ?")!
rows.bind(1, 42)!
loop row in rows.query()! {
    let id: I64 = row.column_i64(0)
    let name: Str = row.column_str(1)
    println("{id}: {name}")
}

// Transactions
db.transaction(do() {
    db.execute("INSERT INTO users VALUES (1, 'Bob')")!
    db.execute("INSERT INTO users VALUES (2, 'Carol')")!
})!
```

### `std::profiler` — Runtime Profiling

```tml
use std::profiler::Profiler

// Outputs a .cpuprofile file readable by Chrome DevTools
let p = Profiler::new()
p.start()
do_expensive_work()
p.stop()
p.write("profile.cpuprofile")!
```

### `std::exception` — Object-Oriented Exception Hierarchy

For the OOP programming style (Chapter 15), TML provides a `throw`/`catch`
exception hierarchy:

```tml
use std::exception::{Exception, IOException, ValueError, RuntimeException}

func risky_operation() {
    throw IOException::new("file not found: data.csv")
}

try {
    risky_operation()
} catch (e: IOException) {
    println("I/O error: {e.message()}")
} catch (e: Exception) {
    println("error: {e.message()}")
}
```

---

## Choosing Between Core and Std

Use **core** when:
- Writing a `no_std` library (embedded targets, kernels)
- Implementing a new standard library module
- You need only types and algorithms, no I/O

Use **std** when:
- Writing application code
- You need file I/O, networking, threads, or OS interaction

The `std` prelude re-exports everything from `core`, so you never need to
explicitly import core types in application code.

## Best Practices

1. **Use `@derive`** for `PartialEq`, `Eq`, `Hash`, `Debug`, `Default`, and
   `Duplicate` — it generates correct implementations with no boilerplate.

2. **Prefer `Maybe[T]` over sentinel values** — `Just(x)` and `Nothing` communicate
   intent at the type level.

3. **Use `Outcome[T, E]` with the `!` operator** — error propagation is automatic
   and the success path reads cleanly.

4. **Use `Instant` and `Duration` for timing** — never use `DateTime` for measuring
   elapsed time, as `Instant` is monotonic.

5. **Use `Text` for mutable strings** — it has SSO so short strings are stack-
   allocated; use `Str` for immutable string references.

6. **Use `Arc[Mutex[T]]` for shared mutable state across threads** — the standard
   pattern that the type system enforces correctly.

## See Also

- [Chapter 21 — Working with JSON](ch21-00-json.md)
- [Chapter 22 — Cryptography](ch22-00-crypto.md)
- [Chapter 23 — Compression](ch23-00-compression.md)
- [Chapter 24 — Networking and HTTP](ch24-00-networking.md)
- [Appendix C — Builtin Functions](appendix-03-builtins.md)

---

*Previous: [Chapter 19 — Bitwise Operations](ch19-00-bitwise-operations.md)*
*Next: [Chapter 21 — Working with JSON](ch21-00-json.md)*
