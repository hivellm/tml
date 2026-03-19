# TML Standard Library (`std`)

The TML standard library provides common data structures, I/O operations, networking, cryptography, compression, and more.

**Status**: 1550+ tests passing | [Changelog](CHANGELOG.md)

## Modules

### `std::collections` — Data Structures

- **`List[T]`** — Generic dynamic array (Vec equivalent)
- **`HashMap[K, V]`** — Generic key-value store
- **`HashSet[T]`** — Unique value set
- **`Buffer`** — Byte buffer for binary data
- **`Deque[T]`** — Double-ended queue (ring buffer)
- **`BinaryHeap[T]`** — Priority queue
- **`BTreeMap[K, V]`** — Ordered map (sorted-array, binary search)
- **`BTreeSet[T]`** — Ordered set
- **`ArrayList[T]`** — Alternative dynamic array

All collections are implemented in **pure TML** — no C runtime dependency.

```tml
use std::collections::{List, HashMap}

var numbers: List[I32] = List[I32]::new()
numbers.push(10)
numbers.push(20)

var scores: HashMap[Str, I32] = HashMap[Str, I32]::new()
scores.insert("Alice", 100)
```

### `std::http` — HTTP Server and Client

Full HTTP/1.1 implementation with router, middleware, TLS support.

- **Router** — Radix tree routing with path params and wildcards
- **Request / Response** — HTTP message types with headers, body, status
- **Headers** — Case-insensitive header storage
- **Cookies** — Cookie parsing and serialization
- **Encoding** — URL encoding/decoding, multipart, form data
- **HttpClient** — `get()`, `post()`, `put()`, `delete()`, `send()`
- **Connection** — DNS + TCP + optional TLS

```tml
use std::http::client::HttpClient

let client: HttpClient = HttpClient::new()
let response = client.get("https://example.com")!
```

### `std::crypto` — Cryptography

Comprehensive crypto suite via OpenSSL/BCrypt FFI.

- **Hash**: SHA-256, SHA-512, MD5, BLAKE3
- **HMAC**: Message authentication codes
- **Cipher**: AES-GCM, ChaCha20-Poly1305
- **KDF**: PBKDF2, HKDF, scrypt
- **Signatures**: ECDSA, Ed25519
- **Key Management**: Key generation and exchange
- **DH / ECDH**: Diffie-Hellman key exchange
- **RSA**: RSA encryption and signing
- **X.509**: Certificate handling and verification
- **Random**: `random_bytes()`, `random_int()`, `random_uuid()`, `SecureRandom`

### `std::zlib` — Compression

- **Deflate / Inflate** — Configurable compression levels
- **Gzip** — `gzip_compress()`, `gzip_decompress()`
- **Brotli** — `brotli_compress()`, `brotli_decompress()`
- **Zstd** — `zstd_compress()`, `zstd_decompress()`
- **CRC32** — `crc32()`, `crc32_combine()`
- **Streaming API** — `DeflateStream`, `InflateStream`

### `std::sqlite` — SQLite Database

- **`Database`** — Open file or in-memory DB, `exec()`, `prepare()`, transactions
- **`Statement`** — Prepared statements with typed binds and column accessors
- **`Row`** / **`Value`** — Result row and dynamic value types

### `std::json` — JSON

- **`JsonValue`** — JSON value type (object, array, string, number, bool, null)
- `parse()` — Parse JSON string
- `stringify()` — Convert to JSON string

```tml
use std::json::{JsonValue, parse}

let json: JsonValue = parse("{\"name\": \"Alice\", \"age\": 30}")
```

### `std::regex` — Regular Expressions

Thompson's NFA engine with O(n*m) worst case — no exponential backtracking.

- **`Regex`** — `is_match()`, `find()`, `find_all()`, `replace()`, `replace_all()`, `split()`
- Syntax: `.`, `*`, `+`, `?`, `|`, `()`, `[a-z]`, `[^0-9]`, `\d`, `\w`, `\s`, `^`, `$`

### `std::search` — Search and Indexing

- **`BM25Index`** — Full-text search with TF-IDF scoring
- **`HnswIndex`** — HNSW approximate nearest neighbor search
- **`TfIdfVectorizer`** — Document embedding
- **Distance Functions** — SIMD-accelerated dot product, L2, cosine similarity

### `std::net` — Networking

- **`IpAddr`** / **`Ipv4Addr`** / **`Ipv6Addr`** — IP addresses
- **`SocketAddr`** — Socket address (IP + port)
- **TCP** — `TcpStream`, `TcpListener` with connect/bind/accept
- **UDP** — `UdpSocket` with sendto/recvfrom
- **DNS** — DNS resolution
- **TLS** — `TlsConfig`, `TlsStream` (OpenSSL/BCrypt)

### `std::sync` — Synchronization

- **`Mutex[T]`** / **`MutexGuard[T]`** — Mutual exclusion lock
- **`RwLock[T]`** — Reader-writer lock
- **`Condvar`** — Condition variable
- **`Barrier`** — Thread barrier
- **`Once`** — One-time initialization

```tml
use std::sync::{Mutex, MutexGuard}

let mutex: Mutex[I32] = Mutex::new(42)
{
    var guard: MutexGuard[I32] = mutex.lock()
    *guard.get_mut() = 100
}
```

### `std::sync::mpsc` — Channels

- **`Sender[T]`** / **`Receiver[T]`** — Multi-producer, single-consumer
- `channel[T]()` — Create unbounded channel

### `std::thread` — Threading

- **`Thread`** / **`JoinHandle[T]`** — Thread handle and join
- `spawn()`, `sleep()`, `current()`, `yield_now()`

### `std::aio` — Async I/O

Event-driven I/O with Node.js/libuv-style architecture.

- **`Poller`** — Platform I/O polling (epoll on Linux, WSAPoll on Windows)
- **`TimerWheel`** — Hashed 2-level timer wheel (O(1) schedule/cancel/fire)
- **`EventLoop`** — Single-threaded event loop with I/O + timer + callback dispatch

### `std::stream` — Streams

Composable byte streams with backpressure.

- **`Readable`** / **`Writable`** — Stream behaviors
- **`BufferedReader`** / **`BufferedWriter`** — Buffered I/O
- **`ByteStream`** — In-memory read/write stream
- **`DuplexStream`** — Combined read+write
- **`TransformStream`** — Stateful transformations
- **`PipelineStream`** — Chain multiple streams
- `pipe()` — Fluent reader → transform → writer

### `std::file` — File I/O

- **`File`** — File handle with read/write methods
- **`Path`** — Path utilities
- **`BufReader`** / **`BufWriter`** / **`LineWriter`** — Buffered I/O

```tml
use std::file::File

File::write_all("hello.txt", "Hello, World!")
let content: Str = File::read_all("hello.txt")
```

### `std::math` — Mathematics

- **Trigonometric**: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`
- **Hyperbolic**: `sinh`, `cosh`, `tanh`
- **Exponential**: `exp`, `ln`, `log2`, `log10`, `pow`
- **Rounding**: `floor`, `ceil`, `round`, `trunc`
- **Utility**: `abs`, `sqrt`, `cbrt`, `min`, `max`, `clamp`
- **Constants**: `PI`, `E`, `TAU`, `SQRT_2`, `LN_2`, `LN_10`

### `std::datetime` — Date and Time

- **`DateTime`** — `now()`, `from_timestamp()`, `from_parts()`
- Components: `year()`, `month()`, `day()`, `hour()`, `minute()`, `second()`
- Calendar: `weekday()`, `day_of_year()`, `is_leap_year()`
- Formatting: `to_iso8601()`, `to_rfc2822()`
- Parsing: `parse_iso8601()`, `parse_date()`, `parse()`

### `std::os` — Operating System

- Environment: `env_get()`, `env_set()`, `env_unset()`
- Process: `process_exit()`, `process_id()`, `exec()`, `exec_status()`
- System: `cpu_count()`, `total_memory()`, `system_name()`
- Directory: `get_cwd()`, `set_cwd()`

### `std::time` — Time

- **`Instant`** — Monotonic clock (`now()`, `elapsed()`, `duration_since()`)
- **`SystemTime`** — Wall clock (`now()`, `as_secs()`, `duration_since_epoch()`)
- `sleep(millis)` — Thread sleep

### `std::glob` — Glob Patterns

- Pattern matching: `*`, `?`, `**`, `[a-z]`, `{a,b}`
- Directory walking: `find()`, `find_all()`, `count()`
- Cross-platform (Windows + POSIX)

### `std::random` — Random Numbers

- `random_i64()`, `random_f64()`, `random_bool()`, `random_range()`
- `shuffle_i64()`, `shuffle_i32()` — Fisher-Yates shuffle
- **`Rng`** — Seeded random number generator

### `std::hash` — Fast Hashing

Non-cryptographic hash functions (NOT for security).

- **FNV-1a**: `fnv1a32()`, `fnv1a64()`
- **MurmurHash2**: `murmur2_32()`, `murmur2_64()`
- ETag helpers: `etag_weak()`, `etag_strong()`

### `std::text` — Text Builder

- **`Text`** — Heap-allocated, growable string with SSO (≤23 bytes on stack)
- 40+ methods: `len`, `push`, `concat`, `substring`, `trim`, `replace`, etc.
- Template literals: `` `Hello, {name}!` `` produces `Text`

### Additional Modules

| Module | Description |
|--------|-------------|
| `std::cli` | Command-line argument parsing |
| `std::events` | Event emitter pattern |
| `std::exception` | Exception types and stack traces |
| `std::io` | I/O utilities |
| `std::iter` | Extended iterator adapters |
| `std::log` | Logging framework |
| `std::mime` | MIME type detection |
| `std::profiler` | Code profiling |
| `std::semver` | Semantic versioning |
| `std::traits` | Extended behavior implementations |
| `std::types` | Extended type utilities |
| `std::url` | URL parsing |
| `std::uuid` | UUID generation |

## Runtime

The std library uses C runtime via `@extern` FFI for OS-level operations:

- `sync.c` — Synchronization primitives
- `thread.c` — Threading support
- `file.c` — File I/O
- `net.c` — Networking
- `crypto.c` — Cryptography (OpenSSL/BCrypt)
- `zlib/` — Compression (zlib, brotli, zstd)

Collections (`List`, `HashMap`, `Buffer`) are implemented in **pure TML** with no C runtime dependency.
