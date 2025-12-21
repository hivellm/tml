# TML Standard Packages

## Overview

This directory contains specifications for TML's standard library packages. These packages provide essential functionality for building applications.

## Package Index

### Core Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std.collections](./10-COLLECTIONS.md) | Data structures (Vec, HashMap, etc.) | None |
| [std.iter](./11-ITER.md) | Iterators and adapters | None |
| [std.alloc](./12-ALLOC.md) | Memory allocators | None |
| [std.fmt](./20-FMT.md) | String formatting | None |

### I/O Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std.fs](./01-FS.md) | File system operations | `io.file` |
| [std.net](./02-NET.md) | Low-level networking (TCP/UDP) | `io.network` |
| [std.buffer](./03-BUFFER.md) | Binary buffers and streams | None |
| [std.tls](./06-TLS.md) | TLS/SSL connections | `io.network.tls` |
| [std.http](./07-HTTP.md) | HTTP client and server | `io.network.http` |

### Concurrency Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std.sync](./13-SYNC.md) | Channels and synchronization | None |
| [std.async](./14-ASYNC.md) | Async runtime and futures | `io.time` (for timers) |

### Data Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std.encoding](./04-ENCODING.md) | Text and binary encodings | None |
| [std.json](./09-JSON.md) | JSON parsing and serialization | None |
| [std.compress](./08-COMPRESS.md) | Compression algorithms | None |
| [std.regex](./15-REGEX.md) | Regular expressions | None |

### Security Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std.crypto](./05-CRYPTO.md) | Cryptographic primitives | None |
| [std.uuid](./17-UUID.md) | UUID generation | `io.random` |

### Utility Packages

| Package | Description | Capabilities Required |
|---------|-------------|----------------------|
| [std.datetime](./16-DATETIME.md) | Date and time handling | `io.time` |
| [std.log](./18-LOG.md) | Logging framework | `io.file` (optional) |
| [std.args](./19-ARGS.md) | Command-line argument parsing | `io.process.env` |
| [std.env](./21-ENV.md) | Environment variables | `io.process.env` |

## Dependency Graph

```
                              ┌─────────────────────────────────────────┐
                              │            Core Packages                │
                              │  std.collections ← std.iter ← std.alloc │
                              │           ↑                             │
                              │       std.fmt                           │
                              └─────────────────────────────────────────┘
                                              │
        ┌─────────────────────────────────────┼─────────────────────────────────────┐
        │                                     │                                     │
        ▼                                     ▼                                     ▼
┌───────────────────┐               ┌───────────────────┐               ┌───────────────────┐
│   I/O Packages    │               │ Concurrency       │               │  Data Packages    │
│                   │               │                   │               │                   │
│ std.http          │               │ std.async         │               │ std.json          │
│   ├── std.tls     │               │   └── std.sync    │               │   └── std.encoding│
│   │     └── crypto│               │                   │               │                   │
│   └── std.net     │               │                   │               │ std.compress      │
│         └── buffer│               │                   │               │   └── std.buffer  │
│               │   │               │                   │               │                   │
│           std.fs  │               │                   │               │ std.regex         │
└───────────────────┘               └───────────────────┘               └───────────────────┘
        │                                     │
        └─────────────────┬───────────────────┘
                          │
                          ▼
                ┌───────────────────┐
                │ Utility Packages  │
                │                   │
                │ std.log ──────────┼──> std.datetime
                │   └── std.fs      │         │
                │                   │         ▼
                │ std.args          │    std.uuid ──> std.crypto
                │   └── std.env     │
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
import std.fs

// Import specific items
import std.fs.{File, read_to_string}

// Import with alias
import std.http as http

// Import multiple packages
import std.collections.{Vec, HashMap}
import std.iter.Iterator
import std.fmt.{format, Display}
```

## Quick Reference

### Most Used Types

| Type | Package | Description |
|------|---------|-------------|
| `Vec[T]` | std.collections | Dynamic array |
| `HashMap[K, V]` | std.collections | Hash table |
| `String` | builtin | UTF-8 string |
| `Option[T]` | builtin | Optional value |
| `Result[T, E]` | builtin | Success or error |
| `Box[T]` | std.alloc | Heap allocation |
| `Arc[T]` | std.sync | Atomic reference counted |
| `Mutex[T]` | std.sync | Mutual exclusion lock |

### Most Used Functions

| Function | Package | Description |
|----------|---------|-------------|
| `println!()` | std.fmt | Print with newline |
| `format!()` | std.fmt | Format string |
| `spawn()` | std.async | Spawn async task |
| `channel()` | std.sync | Create channel |
| `File.open()` | std.fs | Open file |
| `Regex.new()` | std.regex | Compile regex |
| `DateTime.now()` | std.datetime | Current time |
| `Uuid.v4()` | std.uuid | Random UUID |
