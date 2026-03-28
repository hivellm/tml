# TML Standard Library

Full-featured standard library with collections, networking, crypto, HTTP, JSON, database, compression, and more. Depends on `core`.

[Changelog](CHANGELOG.md)

## Module Index

### Collections (`collections/`)

| Module | Path | Description |
|--------|------|-------------|
| List[T] | `std::collections::List` | Dynamic array (Rust's `Vec`) — push, pop, get, set, retain, drain |
| HashMap[K,V] | `std::collections::HashMap` | Hash map — Swiss-table style, open addressing |
| BTreeMap[K,V] | `std::collections::BTreeMap` | Ordered map — sorted array, binary search |
| BTreeSet[T] | `std::collections::BTreeSet` | Ordered set |
| HashSet[T] | `std::collections::HashSet` | Hash-based unique set |
| Deque[T] | `std::collections::Deque` | Double-ended queue (ring buffer) |
| BinaryHeap[T] | `std::collections::BinaryHeap` | Priority queue |
| Buffer | `std::collections::Buffer` | Byte buffer for binary data |
| Trie[V] | `std::collections::Trie` | String-keyed prefix tree — autocomplete, prefix search |
| IntervalTree[V] | `std::collections::IntervalTree` | Augmented BST — range/point overlap queries |
| ArrayList[T] | `std::collections::ArrayList` | Alternative dynamic array (class-based) |
| LinkedList[T] | `std::collections::LinkedList` | Doubly-linked list (class-based) |

### Networking (`net/`)

| Module | Path | Description |
|--------|------|-------------|
| ip | `std::net::ip` | `IpAddr`, `Ipv4Addr`, `Ipv6Addr` — address types and classification |
| tcp | `std::net::tcp` | `TcpStream`, `TcpListener` — connect, bind, accept, read, write |
| udp | `std::net::udp` | `UdpSocket` — sendto, recvfrom, multicast |
| tls | `std::net::tls` | `TlsConfig`, `TlsStream` — OpenSSL/BCrypt TLS |
| dns | `std::net::dns` | DNS resolution |
| socket | `std::net::socket` | Low-level socket operations |
| async_tcp | `std::net::async_tcp` | Async TCP with IOCP/epoll |
| async_udp | `std::net::async_udp` | Async UDP |
| url | `std::net::url` | URL parsing, building, query params |
| mime | `std::net::mime` | MIME type detection and parsing |

### HTTP (`http/`)

| Module | Path | Description |
|--------|------|-------------|
| server | `std::http::server` | HTTP/1.1 server with connection handling |
| client | `std::http::client` | `HttpClient` — get, post, put, delete, send |
| router | `std::http::router` | Radix tree routing with path params and wildcards |
| request | `std::http::request` | `Request` — method, headers, body, URL |
| response | `std::http::response` | `Response` — status, headers, body |
| headers | `std::http::headers` | Case-insensitive header storage |
| cookies | `std::http::cookies` | Cookie parsing and serialization |
| middleware | `std::http::middleware` | Middleware chain — cors, logger, compress |
| static_server | `std::http::static_server` | Static file serving with MIME detection |

### Cryptography (`crypto/`)

| Module | Path | Description |
|--------|------|-------------|
| hash | `std::crypto::hash` | SHA-256, SHA-512, MD5, BLAKE3 |
| hmac | `std::crypto::hmac` | HMAC message authentication |
| cipher | `std::crypto::cipher` | AES-GCM, ChaCha20-Poly1305 |
| kdf | `std::crypto::kdf` | PBKDF2, HKDF, scrypt |
| dh | `std::crypto::dh` | Diffie-Hellman key exchange |
| ecdh | `std::crypto::ecdh` | Elliptic curve Diffie-Hellman |
| key | `std::crypto::key` | Key generation and management |
| random | `std::crypto::random` | `SecureRandom` — cryptographic RNG |

### Synchronization (`sync/`)

| Module | Path | Description |
|--------|------|-------------|
| mutex | `std::sync::Mutex` | `Mutex[T]`, `MutexGuard[T]` — mutual exclusion |
| rwlock | `std::sync::RwLock` | `RwLock[T]` — reader-writer lock |
| arc | `std::sync::Arc` | `Arc[T]`, `Weak[T]` — atomic reference counting |
| mpsc | `std::sync::mpsc` | `Sender[T]`, `Receiver[T]` — channels |
| barrier | `std::sync::Barrier` | Thread barrier |
| condvar | `std::sync::Condvar` | Condition variable |
| semaphore | `std::sync::Semaphore` | Counting semaphore |
| once | `std::sync::Once` | One-time initialization |
| wait_group | `std::sync::WaitGroup` | Wait for group of tasks |
| atomic | `std::sync::atomic` | Atomic types and operations |
| queue | `std::sync::queue` | Lock-free concurrent queue |
| stack | `std::sync::stack` | Lock-free concurrent stack |

### File I/O (`file/`)

| Module | Path | Description |
|--------|------|-------------|
| file | `std::file::File` | File handle — open, read, write, seek, close |
| path | `std::file::path` | Path utilities — exists, join, parent, extension, create_dir |
| dir | `std::file::dir` | Directory creation and removal |
| bufio | `std::file::bufio` | `BufReader` — buffered line/chunk reading |
| glob | `std::file::glob` | Glob pattern matching and directory walking |

### Streams (`stream/`)

| Module | Path | Description |
|--------|------|-------------|
| readable | `std::stream::Readable` | Readable behavior for byte sources |
| writable | `std::stream::Writable` | Writable behavior for byte sinks |
| buffered | `std::stream::buffered` | Buffered reader/writer |
| duplex | `std::stream::duplex` | Combined read+write stream |
| transform | `std::stream::transform` | Stateful stream transformations |
| pipeline | `std::stream::pipeline` | Chain multiple streams |
| pipe | `std::stream::pipe` | Readable → Writable pipe |
| seek | `std::stream::seek` | `Seek`, `SeekFrom`, `Cursor` — random access |
| passthrough | `std::stream::passthrough` | Pass-through stream |
| byte_stream | `std::stream::byte_stream` | In-memory byte stream |

### Database (`sqlite/`)

| Module | Path | Description |
|--------|------|-------------|
| database | `std::sqlite::Database` | Open, exec, prepare, transactions |
| statement | `std::sqlite::Statement` | Prepared statements with typed binds |
| row | `std::sqlite::Row` | Result row with column accessors |
| value | `std::sqlite::Value` | Dynamic value type (text, int, float, blob, null) |

### Compression (`zlib/`)

| Module | Path | Description |
|--------|------|-------------|
| deflate | `std::zlib` | Deflate/inflate with configurable levels |
| gzip | `std::zlib` | Gzip compress/decompress |
| brotli | `std::zlib` | Brotli compress/decompress |
| zstd | `std::zlib` | Zstandard compress/decompress |
| crc32 | `std::zlib` | CRC32 checksum |
| streaming | `std::zlib` | `DeflateStream`, `InflateStream` |

### Events & Async (`events/`)

| Module | Path | Description |
|--------|------|-------------|
| events | `std::events` | `EventEmitter` — pub/sub event pattern |
| observable | `std::events::observable` | RxJS-style observables, subjects, operators |
| promise | `std::events::promise` | `Promise[T]` — async value resolution |

### Time (`time/`)

| Module | Path | Description |
|--------|------|-------------|
| time | `std::time` | `Instant`, `SystemTime`, `sleep`, `time_ns` |
| datetime | `std::time::datetime` | `DateTime` — now, parse, format, components |

### Math

| Module | Path | Description |
|--------|------|-------------|
| math | `std::math` | Trig, exp, log, rounding, abs, sqrt + Complex numbers |
| bigint | `std::bigint` | `BigInt` — arbitrary precision integers, mod_pow, Miller-Rabin |

### Search (`search/`)

| Module | Path | Description |
|--------|------|-------------|
| bm25 | `std::search` | BM25 full-text search with TF-IDF scoring |
| hnsw | `std::search` | HNSW approximate nearest neighbor |
| distance | `std::search` | SIMD-accelerated dot product, L2, cosine |

### Standalone Modules

| Module | Path | Description |
|--------|------|-------------|
| json | `std::json` | `JsonValue` — parse, stringify |
| regex | `std::regex` | `Regex` — NFA engine, no backtracking |
| random | `std::random` | `Rng` (xoshiro256**), `Random` trait, `random_range` |
| text | `std::text` | `Text` — growable string builder with SSO |
| uuid | `std::uuid` | UUID v4/v7 generation and parsing |
| semver | `std::semver` | Semantic version parsing, comparison, ranges |
| cli | `std::cli` | Command-line argument parsing |
| hash | `std::hash` | FNV-1a, MurmurHash2 — non-crypto hashing |
| log | `std::log` | Logging framework with levels and filters |
| debug | `std::debug` | `debug_print[T: Reflect]`, `to_json[T: Reflect]` |
| profiler | `std::profiler` | Code profiling utilities |
| io | `std::io` | `IoError`, `IoErrorKind` |
| exception | `std::exception` | Exception types — `Exception`, `ArgumentException` |
| iter | `std::iter` | Extended iterator adapters |
| traits | `std::traits` | Extended behavior implementations |
| types | `std::types` | Extended type utilities |
| interfaces | `std::interfaces` | Common interface definitions |
| object | `std::object` | Re-export of `std::oop::object` |

### OS & Platform (`os/`)

| Module | Path | Description |
|--------|------|-------------|
| subprocess | `std::os::subprocess` | `Command` builder, child process, stdio redirect |
| signal | `std::os::signal` | Signal registration and polling |
| pipe | `std::os::pipe` | Anonymous pipes for IPC |

### OOP (`oop/`)

| Module | Path | Description |
|--------|------|-------------|
| object | `std::oop::object` | `Object` base class with `to_string`, `get_type`, `get_hash_code` |
| interfaces | `std::oop::interfaces` | OOP interface definitions |

### Threading (`thread/`)

| Module | Path | Description |
|--------|------|-------------|
| thread | `std::thread` | `spawn`, `sleep`, `yield_now`, `current`, `JoinHandle` |
| builder | `std::thread` | `Builder` — named threads with stack size config |
| scope | `std::thread` | Scoped thread spawning |

### Async I/O (`aio/`)

| Module | Path | Description |
|--------|------|-------------|
| poller | `std::aio` | Platform I/O polling (epoll/WSAPoll) |
| timer_wheel | `std::aio` | Hashed timer wheel — O(1) schedule/cancel |
| event_loop | `std::aio` | Single-threaded event loop |

### FFI (`ffi/`)

| Module | Path | Description |
|--------|------|-------------|
| types | `std::ffi` | FFI type definitions |
| helpers | `std::ffi` | FFI helper utilities |

### Allocation (`alloc/`)

| Module | Path | Description |
|--------|------|-------------|
| global | `std::alloc` | Global allocator interface |
| tracking | `std::alloc` | Allocation tracking and leak detection |
