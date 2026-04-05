# 8. Standard Library Design

## 8.1 Overview

The TML standard library is organized into three distinct layers: a foundation layer (core::), a full-featured standard library (std::), and a test framework (test::). As of April 2026, the library comprises 2,251 total source files with 1,682 test files achieving 93.2% function coverage across 1,659 passing tests.

This architecture reflects a batteries included philosophy. Unlike Rust, which defers most functionality to crates.io, TML bundles HTTP server, database drivers, cryptographic primitives, full-text search, and vector search into the standard distribution.

---

## 8.2 Three-Layer Architecture

### 8.2.1 The Core Layer (core::)

The core:: layer contains 199 source files providing the language foundation with zero external dependencies. Every TML program links against core:: by default.

Types and Optionals. Maybe[T] and Outcome[T, E] are zero-cost abstractions for optional and result types. Constructors are Just(T)/Nothing and Ok(T)/Err(E). Naming prioritizes clarity: Maybe communicates optionality explicitly; Outcome communicates two-way computations.

String and Character Processing. core::str provides 58 functions for immutable UTF-8 string slices: len, char_at, substring, contains, find, split, trim, replace, join, and parse_* functions. Unicode 15.1.0 support in core::unicode.

Collections and Iteration. core::iter defines Iterator behavior with 30+ adapters: map, filter, fold, take, skip, zip, chain, flatten, enumerate. core::slice provides zero-copy views Slice[T] and MutSlice[T].

Behaviors (Traits). core::clone defines Duplicate and Copy. core::cmp defines PartialEq, Eq, PartialOrd, Ord. core::fmt defines Display and Debug. core::ops covers arithmetic, logical, indexing, and callable object operators.

Memory and Allocation. core::alloc provides Heap[T] (exclusive ownership), Shared[T] (reference-counted), Sync[T] (atomic reference-counted). Specialized allocators: Arena (bump-pointer), Pool (lock-free), SmallVec/SmallString (SSO), CacheAligned/Padded (cache-friendly).

Encoding. core::encoding provides base64, base32, base58, hex, and 9 other binary-to-text formats.

Async Primitives. core::task provides Poll[T], Context, Waker. core::future defines Future. core::async_iter defines AsyncIterator.

SIMD. core::simd provides I32x4, F32x4, I64x2, F64x2, U8x16 vector types with intrinsics: ptr_read, ptr_write, mem_alloc, mem_free, copy_nonoverlapping.

### 8.2.2 The Standard Library (std::)

The std:: layer contains 336 source files providing full application development surface.

Text. std::text provides Text - mutable, dynamically-growing string with push operations and Small String Optimization.

Collections. List[T] (dynamic array), HashMap[K,V], BTreeMap, BTreeSet, Deque, BinaryHeap, MinHeap, Buffer (byte operations).

Concurrency. Mutex[T], RwLock[T], Arc[T], atomics, channels (Sender/Receiver), Barrier, CondVar, Once, lock-free queues/stacks, Semaphore, WaitGroup.

Networking. TcpStream, TcpListener, socket address types, NetEventLoop for non-blocking I/O. Windows IOCP support.

HTTP. 11 subdirectories covering: App (Express-like routing), Router (radix tree), framework (middleware, guards, pipes), protocol (HTTP/2, WebSockets), utilities (chunked, CORS, compression, rate-limiting, static files, cookies, multipart, SSE, range requests, cache-control).

File I/O. File, Dir, Path, PathBuf, BufReader, BufWriter, LineWriter, Lines.

Database. Multi-driver architecture: SQLite with 3-4x Rust performance, PostgreSQL support, ORM, query building, schema management, migrations.

JSON. Json, JsonObject, JsonArray, parse/to_string, ToJson/FromJson behaviors, fluent Builder.

Cryptography. Hash (SHA variants, MD5, BLAKE3), HMAC, AES-GCM, ChaCha20-Poly1305, RSA, ECDSA, Ed25519, PBKDF2, Argon2, X.509, Diffie-Hellman.

Search. BM25 full-text search, HNSW approximate nearest neighbor, SIMD distance.

Other. math, random, regex, zlib, datetime, uuid, url, mime, semver, log, cli, glob, events, profiler, console (structured logging).

### 8.2.3 The Test Framework (test::)

Contains 14 files providing test infrastructure as library. Decorators: @test, @bench, @should_panic, @should_error, @before_all, @after_all, @before_each, @after_each, @fixture.

Assertions: assert, assert_eq, assert_ne, assert_lt, assert_le, assert_gt, assert_ge with custom messages.

Modules: property-based testing, mocking, end-to-end network testing, coverage tracking.

---

## 8.3 Design Philosophy: Batteries Included

Rust deliberately excludes almost everything beyond memory and I/O traits. Tradeoff: applications require substantial dependencies.

Go includes HTTP, JSON, database interface, crypto. Go dominates cloud infrastructure because programmers can write complete services in 50 lines.

Python bundles 80MB stdlib. Programmers benefit from integrated libraries.

TML follows Go. Inclusions reflect target use case: AI-adjacent services, data pipelines, developer tools. A language designed for LLM-assisted development benefits from LLMs writing complete services without understanding dependency conventions.

---

## 8.4 C Runtime Migration

The library is transitioning from C implementations to pure-TML using memory intrinsics. Migration rule: pure TML (preferred), C FFI (acceptable), new C code (last resort for OS-level I/O only).

Goal: self-hosting compiler rewritten in TML. Each migrated function serves double duty: works today, eliminates C dependency for future compiler.

---

## 8.5 Module Statistics

| Layer | Files | Subsystems |
|-------|-------|-----------|
| core:: | 199 | Types, strings, collections, behaviors, memory, encoding, async, SIMD |
| std:: | 336 | Text, collections, sync, net, HTTP (11), file, JSON, crypto, DB, search |
| test:: | 14 | Assertions, benchmarks, mocking, coverage, e2e |
| Total | 549 | -- |

Test files: 1,682
Total .tml files: 2,251
Coverage: 93.2% (1,659/1,775 functions)
Tests passing: 1,659
Tests crashing: 0
Target: 95% coverage
