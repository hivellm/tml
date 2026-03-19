---
name: stdlib-architecture
description: Deep knowledge of TML standard library structure, module dependencies, key types, and C runtime mapping. Inject before working on library code.
user-invocable: false
---

# TML Standard Library Architecture — Deep Reference

This skill provides the knowledge needed to work on TML library code (lib/core/, lib/std/, lib/test/) without accidentally duplicating existing functionality or breaking module dependencies.

## Library Layers

```
lib/core/   ← Foundation primitives (zero external deps)
     ↓
lib/std/    ← Full standard library (depends on core + C runtime)
     ↓
lib/test/   ← Test framework (depends on core + std)
```

**Rule**: Core NEVER imports std. Std CAN import core. Test CAN import both.

## Core Library (lib/core/) — Key Modules

| Module | Key Types | Purpose |
|--------|-----------|---------|
| `alloc/` | `Heap[T]`, `Shared[T]`, `Sync[T]`, `Layout`, `Allocator` | Smart pointers, allocation |
| `fmt/` | `Display`, `Debug`, `Formatter`, `Binary`, `Hex` | Formatting behaviors |
| `iter/` | `Iterator`, `IntoIterator`, `FromIterator`, `DoubleEndedIterator` | Iteration protocol |
| `slice/` | `Slice[T]`, `MutSlice[T]` | View types (binary search, sort, chunks) |
| `ptr/` | `ConstPtr`, `MutPtr`, `NonNull` | Pointer types |
| `num/` | `Integer`, numeric traits | Number operations |
| `char/` | Character classification, UTF-8 | Character handling |
| `cell/` | `Cell`, `RefCell`, `Lazy` | Interior mutability |
| `ops/` | `Deref`, `Index`, `Try`, `Coroutine` | Operator traits |
| `cmp.tml` | `Eq`, `Ord`, `PartialOrd`, `PartialEq` | Comparison |
| `convert.tml` | `Into`, `From` | Type conversion |
| `result.tml` | `Outcome[T, E]` | Error handling (Rust's Result) |
| `str.tml` | `Str` | String slices |
| `range.tml` | Range types | Ranges |
| `clone.tml` | `Clone` | Cloning |
| `default.tml` | `Default` | Default values |
| `marker.tml` | `Send`, `Sync`, `Copy`, `Sized` | Marker traits |

## Standard Library (lib/std/) — Key Modules

### Collections
| Module | Key Types | Backend |
|--------|-----------|---------|
| `collections/list.tml` | `List[T]` | C runtime (`collections.c`) |
| `collections/hashmap.tml` | `HashMap[K,V]` | C runtime (`collections.c`) |
| `collections/buffer.tml` | `Buffer` | C runtime (`collections.c`) |
| `collections/class_collections.tml` | `ArrayList[T]`, `HashSet[T]`, `Queue[T]`, `Stack[T]`, `Vec[T]` | **Pure TML** |
| `collections/btreemap.tml` | `BTreeMap[K,V]` | **Pure TML** |
| `collections/btreeset.tml` | `BTreeSet[T]` | **Pure TML** |

### Strings & Text
| Module | Key Types | Purpose |
|--------|-----------|---------|
| `text.tml` | `Text` | Dynamic mutable strings (use for string building!) |

### Synchronization
| Module | Key Types | Backend |
|--------|-----------|---------|
| `sync/mutex.tml` | `Mutex[T]`, `MutexGuard[T]` | C runtime (`sync.c`) |
| `sync/rwlock.tml` | `RwLock[T]` | C runtime (`sync.c`) |
| `sync/arc.tml` | `Arc[T]`, `Weak[T]` | C runtime (atomics) |
| `sync/atomic.tml` | `AtomicI32`, `AtomicI64`, etc. | C runtime (`sync.c`) |
| `sync/mpsc.tml` | `Sender[T]`, `Receiver[T]` | C runtime (channels) |
| `sync/queue.tml` | `LockFreeQueue[T]` | C runtime (CAS) |

### Networking
| Module | Key Types | Backend |
|--------|-----------|---------|
| `net/tcp.tml` | `TcpListener`, `TcpStream` | C runtime (`net.c`) |
| `net/udp.tml` | `UdpSocket` | C runtime (`net.c`) |
| `net/tls.tml` | TLS support | C runtime (`tls.c`) |
| `net/ip.tml` | `IpAddr`, `Ipv4Addr`, `Ipv6Addr` | **Pure TML** |

### HTTP (36 files in `http/`)
| Area | Key Files | Purpose |
|------|-----------|---------|
| Core types | `method.tml`, `status.tml`, `headers.tml` | HTTP primitives |
| Server | `server.tml`, `incoming.tml`, `dispatch.tml` | Server framework |
| Client | `client.tml`, `agent.tml` | HTTP client |
| Router | `router.tml` | Radix tree routing |
| Middleware | `cors.tml`, `security.tml`, `etag.tml`, `rate_limit.tml` | Request processing |
| IOCP | `iocp_worker.tml` | Windows async I/O |

### Other Important Modules
| Module | Purpose | Backend |
|--------|---------|---------|
| `file/` | File I/O | C runtime (`file.c`) |
| `json/` | JSON parse/serialize | **Pure TML** |
| `crypto/` | SHA, AES, RSA, ECDH | C runtime (OpenSSL/BCrypt) |
| `zlib/` | Compression | C runtime (zlib, brotli, zstd) |
| `thread/` | Threading | C runtime (`sync.c`) |
| `regex.tml` | Regular expressions | **Pure TML** |
| `search/` | BM25, HNSW | **Pure TML** |
| `glob.tml` | Glob patterns | C runtime (`glob.c`) |
| `time.tml` | `Instant`, `sleep` | C runtime (`time.c`) |
| `math.tml` | Math functions | **Pure TML** |
| `random.tml` | PRNG | **Pure TML** |
| `hash.tml` | FNV-1a, Murmur2 | **Pure TML** |
| `sqlite/` | SQLite3 | C FFI |

## C Runtime Mapping

| C File | Location | TML Modules Using It | Migration Status |
|--------|----------|---------------------|-----------------|
| `essential.c` | `compiler/runtime/core/` | I/O (print), panic, test harness | KEEP |
| `mem.c` | `compiler/runtime/memory/` | All (malloc/free) | KEEP |
| `collections.c` | `compiler/runtime/collections/` | List, HashMap, Buffer | MIGRATE |
| `sync.c` | `compiler/runtime/concurrency/` | Mutex, atomics, channels | KEEP |
| `async.c` | `compiler/runtime/concurrency/` | Async runtime | KEEP |
| `net.c` | `compiler/runtime/net/` | TCP, UDP sockets | KEEP |
| `tls.c` | `compiler/runtime/net/` | TLS/SSL | KEEP |
| `iocp.c` | `compiler/runtime/net/` | Windows IOCP | KEEP |
| `dns.c` | `compiler/runtime/net/` | DNS resolution | KEEP |
| `crypto*.c` | `compiler/runtime/crypto/` | SHA, AES, RSA | KEEP (FFI) |
| `time.c` | `compiler/runtime/time/` | Instant, Duration | KEEP |
| `os.c` | `compiler/runtime/os/` | Subprocess, signals | KEEP |
| `file.c` | `lib/std/runtime/` | File I/O | KEEP |
| `glob.c` | `lib/std/runtime/` | Glob matching | KEEP |
| `zlib/*.c` | `lib/std/runtime/zlib/` | Compression | KEEP (FFI) |

## Test Structure

Tests mirror module structure:
```
lib/core/tests/     → 54 directories, ~1000 test files
lib/std/tests/      → 42 directories, ~350 test files
lib/test/tests/     → 10+ directories, ~100 test files
```

Run specific module tests: `mcp__tml__test` with `suite="core/str"`, `suite="std/json"`, etc.

## BEFORE Writing Any TML Code

1. **Check docs/readme.md** for existing types and functions
2. **Use `Text`** for string building (NOT manual `copy_nonoverlapping`)
3. **Use `Buffer`** for bytes (NOT raw `ptr_read[U8]`/`ptr_write[U8]`)
4. **Use `HashMap`/`List`** for collections (NOT manual memory layouts)
5. **Use `Outcome[T,E]`** for errors (NOT raw I64 codes)
6. **Use template literals** `` `Hello, {name}!` `` (returns `Text`)
7. **Use `Mutex[T]`/`Sync[T]`** for shared state
8. **`lowlevel` blocks** ONLY for FFI, performance-critical paths, or core primitives
