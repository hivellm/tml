# Standard Library

The TML standard library (`std`) provides essential types and functions for building TML programs. It includes common data structures, I/O operations, error handling, and utilities. All collection and algorithm modules are implemented in **pure TML** using memory intrinsics.

## Overview

The standard library is organized into modules:

```
core/                          - Core library (no OS dependencies)
├── ptr.tml                    - Pointer utilities (Ptr alias)
├── mem.tml                    - Memory operations
├── iter/                      - Iterator traits and adapters
├── slice.tml                  - Slice operations
├── ops.tml                    - Operator overloading behaviors
├── cmp.tml                    - Comparison behaviors (PartialEq, Ord)
├── hash.tml                   - Hashing behavior
├── fmt/                       - Formatting (Display, Debug)
├── error.tml                  - Error handling behaviors
├── encoding/                  - Binary encoding (big/little endian)
├── simd.tml                   - SIMD vector types
└── alloc.tml                  - Allocator interface

std/
├── option.tml                 - Maybe[T] type (Just / Nothing)
├── result.tml                 - Outcome[T, E] type (Ok / Err)
├── string.tml                 - String manipulation (Str methods)
├── text.tml                   - Text type (dynamic strings with SSO)
├── prelude.tml                - Auto-imported common types
├── collections/               - Vec, List, HashMap, HashSet, BTreeMap, BTreeSet, Deque, Buffer
├── io/                        - Input/output operations
├── fs/                        - File system operations (File, Path, Dir)
├── time/                      - Instant, Duration
├── datetime/                  - SystemTime, DateTime, formatting
├── fmt/                       - Formatting utilities
├── error/                     - Error handling utilities
├── sync/                      - Mutex, RwLock, Arc, channels
├── net/                       - Networking (TCP/UDP)
├── crypto/                    - Cryptographic primitives (hash, cipher, sign, KDF, CSPRNG)
├── compress/                  - Compression (deflate, gzip, brotli, zstd, lz4)
├── json/                      - JSON parsing and serialization
├── regex/                     - Regular expressions (Thompson's NFA)
├── math/                      - Math functions (sin, cos, sqrt, etc.)
├── os/                        - OS operations
├── random/                    - Random number generation (Xoshiro256, ThreadRng)
├── glob/                      - File glob pattern matching
├── search/                    - Search algorithms (BM25, HNSW, cosine distance)
├── hash/                      - Non-cryptographic hashing (FNV, SipHash)
├── url/                       - URL parsing
├── uuid/                      - UUID generation (v4, v7)
├── semver/                    - Semantic versioning
├── process/                   - Process management
├── path/                      - Path manipulation
├── env/                       - Environment variables and args
├── text/                      - Extended text operations
├── exception/                 - Exception handling
├── profiler/                  - Runtime profiling
└── log/                       - Logging framework
```

## Core Types

### Maybe[T] - Optional Values

The `Maybe[T]` type represents an optional value that might be present (`Just`) or absent (`Nothing`):

```tml
func find_user(id: I64) -> Maybe[Str] {
    if id == 42 {
        return Just("Alice")
    } else {
        return Nothing
    }
}

// Using Maybe
let result = find_user(42)
when result {
    Just(name) => println("Found: {name}"),
    Nothing => println("User not found"),
}
```

### Outcome[T, E] - Error Handling

The `Outcome[T, E]` type represents either a success (`Ok`) or an error (`Err`):

```tml
func parse_number(s: Str) -> Outcome[I64, Str] {
    // ... parsing logic ...
    if valid {
        return Ok(value)
    } else {
        return Err("Invalid number format")
    }
}

let result = parse_number("123")
when result {
    Ok(num) => println("Parsed: {num}"),
    Err(e) => println("Error: {e}"),
}
```

## Collections

See [Chapter 8 — Collections](ch08-00-collections.md) for detailed coverage. Quick summary:

```tml
use std::collections::List
use std::collections::HashMap
use std::collections::Vec
use std::collections::BTreeMap
use std::collections::BTreeSet
use std::collections::Deque
use std::collections::Buffer
```

| Type | Description |
|------|-------------|
| `List[T]` / `Vec[T]` | Dynamic array (growable) |
| `HashMap[K, V]` | Hash table with O(1) lookup |
| `BTreeMap` | Sorted map with O(log n) operations |
| `BTreeSet` | Sorted set |
| `Deque[T]` | Double-ended queue (ring buffer) |
| `Buffer` | Byte buffer for binary data |

## Time and Duration

```tml
use std::time::{Instant, Duration}

let start = Instant::now()
expensive_computation()
let elapsed = Instant::elapsed(start)
let ms = Duration::as_millis_f64(elapsed)
println("Time: {ms} ms")
```

## DateTime

```tml
use std::datetime::{SystemTime, DateTime}

let now = SystemTime::now()
let dt = DateTime::from_timestamp(now)
println("Year: {dt.year()}, Month: {dt.month()}, Day: {dt.day()}")
```

## Math

```tml
use std::math

let x = math::sqrt(2.0)
let y = math::sin(3.14159)
let z = math::pow(2.0, 10.0)
let a = math::abs(-42)
let m = math::max(10, 20)
```

## Regular Expressions

TML includes a Thompson's NFA-based regex engine implemented in pure TML:

```tml
use std::regex::Regex

let re = Regex::new("hello (\\w+)")
let matched = re.is_match("hello world")    // true
let captures = re.captures("hello world")   // capture groups
```

## Cryptography

```tml
use std::crypto::hash::{sha256, sha512, md5}
use std::crypto::hmac::hmac_sha256
use std::crypto::cipher::{aes_encrypt, aes_decrypt}

let digest = sha256("hello world")
let mac = hmac_sha256("key", "message")
```

See [Chapter 16 — Cryptography](ch16-00-crypto.md) for details.

## Compression

```tml
use std::compress::{deflate, inflate, gzip, gunzip}

let compressed = deflate(data)
let original = inflate(compressed)
```

See [Chapter 17 — Compression](ch17-00-compression.md) for details.

## JSON

```tml
use std::json::{parse, stringify, JsonValue}

let val = parse("{\"name\": \"Alice\", \"age\": 30}")
let name = val.get("name")
let json_str = stringify(val)
```

See [Chapter 14 — Working with JSON](ch14-00-json.md) for details.

## Random Numbers

```tml
use std::random::{Xoshiro256, ThreadRng}

var rng = Xoshiro256::seed(42)
let n = rng.next_i64()
let f = rng.next_f64()

// Thread-local RNG (auto-seeded)
let r = ThreadRng::next_i64()
```

## UUID

```tml
use std::uuid::Uuid

let id = Uuid::v4()     // Random UUID
let id7 = Uuid::v7()    // Time-ordered UUID
println("{id}")
```

## Semantic Versioning

```tml
use std::semver::Version

let v = Version::parse("1.2.3")
let v2 = Version::parse("2.0.0")
let is_newer = v2.gt(v)  // true
```

## Search

```tml
use std::search::bm25::BM25
use std::search::hnsw::HNSW

// BM25 text search
var index = BM25::new()
index.add_document(0, "hello world")
let results = index.search("hello", 10)

// HNSW vector similarity search
var hnsw = HNSW::new(128)  // 128-dimensional vectors
```

## Glob

```tml
use std::glob::glob

let files = glob("src/**/*.tml")
```

## URL Parsing

```tml
use std::url::Url

let u = Url::parse("https://example.com:8080/path?q=1")
let host = u.host()     // "example.com"
let port = u.port()     // 8080
```

## Hash Functions

```tml
use std::hash::{fnv1a, siphash}

let h = fnv1a("hello")
let h2 = siphash("hello", key0, key1)
```

## Smart Pointers

```tml
use std::alloc::Heap
use std::sync::{Shared, Sync}

// Heap-allocated value (like Rust's Box)
let boxed = Heap::new(42)

// Reference-counted (like Rust's Rc)
let shared = Shared::new(42)
let copy = shared.duplicate()

// Atomic reference-counted (like Rust's Arc)
let arc = Sync::new(42)
```

## File Operations

```tml
use std::fs::{File, read_to_string}

let content = read_to_string("data.txt")
```

## Text Type

The `Text` type is a dynamic, growable string with Small String Optimization (SSO):

```tml
use std::text::Text

let t = Text::from("Hello")
let upper = t.to_upper_case()     // "HELLO"
let lower = t.to_lower_case()     // "hello"
let trimmed = t.trim()

// Template literals produce Text
let name = "World"
let greeting = `Hello, {name}!`
```

## Environment and Process

```tml
use std::env
use std::os

let home = env::var("HOME")
let args = env::args()
```

## Encoding

```tml
use core::encoding::{big_endian, little_endian}

let bytes = big_endian::encode_i32(42)
let val = little_endian::decode_i64(bytes)
```

## Prelude

The prelude module contains commonly used types that are automatically imported:

```tml
// These are always available without explicit import:
Maybe, Just, Nothing
Outcome, Ok, Err
print, println
```

## Best Practices

1. **Use `Maybe` instead of null** — `Just(x)` or `Nothing` instead of sentinel values
2. **Use `Outcome` for error handling** — `Ok(x)` or `Err(e)` instead of panics
3. **Use `List[T]` / `Vec[T]` for dynamic arrays** — pure TML, automatically growable
4. **Use `Instant` for timing** — high-resolution microsecond timestamps
5. **Use `@derive(...)` for common behaviors** — auto-generate PartialEq, Hash, Debug, etc.

## See Also

- [Appendix C - Builtin Functions](appendix-03-builtins.md)
- [Chapter 8 - Collections](ch08-00-collections.md)
- [Chapter 11 - Testing](ch11-00-testing.md)
- [Chapter 14 - Working with JSON](ch14-00-json.md)
- [Chapter 16 - Cryptography](ch16-00-crypto.md)
- [Chapter 17 - Compression](ch17-00-compression.md)
