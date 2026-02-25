# TML Standard Packages

## Overview

This directory contains specifications for TML's standard library packages. These packages provide essential functionality for building applications.

## Package Index

### Core Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std::collections](./10-COLLECTIONS.md) | Data structures (Vec, HashMap, etc.) | None |
| [std::iter](./11-ITER.md) | Iterators and adapters | None |
| [std::alloc](./12-ALLOC.md) | Memory allocators | None |
| [std::fmt](./20-FMT.md) | String formatting | None |

### I/O Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std::fs](./01-FS.md) | File system operations | `io::file` |
| [std::net](./02-NET.md) | Low-level networking (TCP/UDP) | `io::network` |
| [std::buffer](./03-BUFFER.md) | Binary buffers and streams | None |
| [std::tls](./06-TLS.md) | TLS/SSL connections | `io::network.tls` |
| [std::http](./07-HTTP.md) | HTTP client and server | `io::network.http` |
| [std::stream](./23-STREAM.md) | Streaming byte I/O (Readable, Writable, BufferedReader, BufferedWriter, ByteStream, pipe) | None |

### Concurrency Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std::sync](./13-SYNC.md) | Channels and synchronization | None |
| [std::async](./14-ASYNC.md) | Async runtime and futures | `io::time` (for timers) |
| [std::events](./25-EVENTS.md) | Publish/subscribe event emitter (Node.js-style) | None |
| [std::thread](./31-THREAD.md) | Native threads (spawn, join, sleep, park) | `io::process` |
| [std::runtime](./39-RUNTIME.md) | Async runtime (executor, timers, channels) | `io::time` |

### Database Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std::sqlite](./24-SQLITE.md) | SQLite embedded database (Database, Statement, Row, Value) | `io::file` |

### Data Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std::encoding](./04-ENCODING.md) | Text and binary encodings | None |
| [std::json](./09-JSON.md) | JSON parsing and serialization | None |
| [std::zlib](./08-COMPRESS.md) | Compression & checksums (deflate, gzip, brotli, zstd, CRC32) | None |
| [std::regex](./15-REGEX.md) | Regular expressions | None |

### Security Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std::crypto](./05-CRYPTO.md) | Cryptographic primitives (hash, HMAC, cipher, sign, KDF, CSPRNG) | None |
| [std::uuid](./17-UUID.md) | UUID generation | `io::random` |

### Math and Science Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std::math](./26-MATH.md) | Mathematical functions (sin, cos, sqrt, pow, etc.) | None |
| [std::random](./27-RANDOM.md) | Random number generation (Xoshiro256, ThreadRng) | None |
| [std::search](./29-SEARCH.md) | Search algorithms (BM25, HNSW, cosine distance) | None |
| [std::hash](./28-HASH.md) | Non-cryptographic hashing (FNV-1a, MurmurHash2) | None |

### Utility Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std::datetime](./16-DATETIME.md) | Date and time handling (SystemTime, DateTime) | `io::time` |
| [std::log](./18-LOG.md) | Logging framework | `io::file` (optional) |
| [std::args](./19-ARGS.md) | Command-line argument parsing | `io::process.env` |
| [std::env](./21-ENV.md) | Environment variables | `io::process.env` |
| [std::os](./30-OS.md) | OS-level operations (subprocess, signals, pipes) | `io::process` |
| [std::glob](./33-GLOB.md) | File glob pattern matching | `io::file` |
| [std::url](./34-URL.md) | URL parsing and manipulation | None |
| [std::mime](./35-MIME.md) | MIME type detection | None |
| [std::semver](./37-SEMVER.md) | Semantic versioning | None |
| [std::text](./36-TEXT.md) | Extended text operations (Text type) | None |
| [std::profiler](./38-PROFILER.md) | Runtime profiling (.cpuprofile output) | `io::time` |
| [std::exception](./32-EXCEPTION.md) | C#-style exception hierarchy | None |
| [std::oop](./40-OOP.md) | OOP interfaces and Object base class | None |
| core::encoding | Binary encoding (big/little endian, base64, hex, utf8) | None |

## Dependency Graph

```
                              ┌─────────────────────────────────────────┐
                              │            Core Packages                │
                              │  std::collections ← std::iter ← std::alloc │
                              │           ↑                             │
                              │       std::fmt                           │
                              └─────────────────────────────────────────┘
                                              │
        ┌─────────────────────────────────────┼─────────────────────────────────────┐
        │                                     │                                     │
        ▼                                     ▼                                     ▼
┌───────────────────┐               ┌───────────────────┐               ┌───────────────────┐
│   I/O Packages    │               │ Concurrency       │               │  Data Packages    │
│                   │               │                   │               │                   │
│ std::http          │               │ std::async         │               │ std::json          │
│   ├── std::tls     │               │   └── std::sync    │               │   └── std::encoding│
│   │     └── crypto│               │                   │               │                   │
│   └── std::net     │               │                   │               │ std::compress      │
│         └── buffer│               │                   │               │   └── std::buffer  │
│               │   │               │                   │               │                   │
│           std::fs  │               │                   │               │ std::regex         │
└───────────────────┘               └───────────────────┘               └───────────────────┘
        │                                     │
        └─────────────────┬───────────────────┘
                          │
                          ▼
                ┌───────────────────┐
                │ Utility Packages  │
                │                   │
                │ std::log ──────────┼──> std::datetime
                │   └── std::fs      │         │
                │                   │         ▼
                │ std::args          │    std::uuid ──> std::crypto
                │   └── std::env     │
                └───────────────────┘
```

## Capability Hierarchy

```
io
├── file
│   ├── read
│   └── write
├── network
│   ├── tcp
│   ├── udp
│   ├── tls
│   └── http
├── process
│   ├── env
│   ├── spawn
│   └── signal
├── time
│   ├── read
│   └── sleep
└── random
```

## Package Versioning

All standard packages follow TML's version:
- TML 1.0.x → std packages 1.0.x
- Breaking changes only in minor versions
- Patches are backwards compatible

## Usage

```tml
// Import entire package
use std::fs

// Import specific items
use std::fs::{File, read_to_string}

// Import with alias
use std::http as http

// Import multiple packages
use std::collections::{Vec, HashMap}
use std::iter::Iterator
use std::fmt::{format, Display}
```

## Quick Reference

### Most Used Types

| Type | Package | Description |
|------|---------|-------------|
| `Vec[T]` | std::collections | Dynamic array |
| `HashMap[K, V]` | std::collections | Hash table |
| `String` | builtin | UTF-8 string |
| `Maybe[T]` | builtin | Optional value |
| `Outcome[T, E]` | builtin | Success or error |
| `Heap[T]` | std::alloc | Heap allocation (owned) |
| `Sync[T]` | std::sync | Atomic reference counted |
| `Shared[T]` | std::sync | Reference counted (non-atomic) |
| `Mutex[T]` | std::sync | Mutual exclusion lock |
| `Digest` | std::crypto::hash | Cryptographic hash result |
| `Cipher` | std::crypto::cipher | Symmetric encryption context |
| `HttpClient` | std::http | HTTP client for making requests |
| `Database` | std::sqlite | SQLite database connection |
| `ByteStream` | std::stream | In-memory byte stream |
| `Router` | std::http | HTTP request router |
| `EventEmitter` | std::events | Publish/subscribe event emitter |

### Most Used Functions

| Function | Package | Description |
|----------|---------|-------------|
| `println!()` | std::fmt | Print with newline |
| `format!()` | std::fmt | Format string |
| `spawn()` | std::async | Spawn async task |
| `channel()` | std::sync | Create channel |
| `File.open()` | std::fs | Open file |
| `Regex.new()` | std::regex | Compile regex |
| `DateTime.now()` | std::datetime | Current time |
| `Uuid.v4()` | std::uuid | Random UUID |
| `sha256()` | std::crypto::hash | SHA-256 hash |
| `crc32()` | std::zlib::crc32 | CRC32 checksum |
| `gzip()` | std::zlib::gzip | Gzip compression |
| `random_bytes()` | std::crypto::random | Secure random bytes |
| `HttpClient::get()` | std::http | Send HTTP GET request |
| `Database::open_in_memory()` | std::sqlite | Open in-memory SQLite database |
| `pipe()` | std::stream | Create connected read/write stream pair |
