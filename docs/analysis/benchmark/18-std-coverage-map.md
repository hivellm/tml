# 18 — Std Library Coverage Map

Mapping every `std` module to benchmark status.

## std::collections — 12 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `collections::list` (List[T]) | Yes | 2-5x vs Rust | list_bench: push, pop, access |
| `collections::hashmap` (HashMap[K,V]) | Yes | 1.3-1.6x vs Rust | hashmap_bench: insert, lookup, remove |
| `collections::buffer` (Buffer) | No | — | |
| `collections::deque` (Deque) | No | — | |
| `collections::btreemap` (BTreeMap) | No | — | |
| `collections::btreeset` (BTreeSet) | No | — | |
| `collections::binary_heap` (BinaryHeap) | No | — | |
| `class_collections` (ArrayList, HashSet, Queue, Stack, LinkedList) | No | — | |

**Coverage**: 2/12 modules (17%)

### Missing Collection Benchmarks (High Impact)

| Collection | Rust Equivalent | Why It Matters |
|-----------|----------------|----------------|
| BTreeMap | `std::collections::BTreeMap` | Sorted map — critical for range queries |
| Deque | `std::collections::VecDeque` | Used in BFS, sliding windows |
| BinaryHeap | `std::collections::BinaryHeap` | Priority queues, Dijkstra |
| HashSet | `std::collections::HashSet` | Membership tests |
| LinkedList | `std::collections::LinkedList` | Rare but sometimes needed |

## std::text — 1 file

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `text::Text` (StringBuilder) | BLOCKED | K001 | str::len undefined |

**Coverage**: 0/1 (0%) — BLOCKED

## std::json — 4 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `json` (parser) | BLOCKED | K001 | Bool i32/i1 mismatch |
| `json::builder` | BLOCKED | K001 | |
| `json::serialize` | BLOCKED | K001 | |
| `json::types` | BLOCKED | K001 | |

**Coverage**: 0/4 (0%) — BLOCKED

## std::crypto — 16 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `crypto::hash` (SHA, MD5, BLAKE) | BLOCKED | N002 | Missing .obj files |
| `crypto::hmac` | BLOCKED | N002 | |
| `crypto::cipher` (AES, ChaCha20) | BLOCKED | N002 | |
| `crypto::random` | BLOCKED | N002 | |
| `crypto::sign` (RSA, ECDSA, Ed25519) | BLOCKED | N002 | |
| `crypto::kdf` (PBKDF2, Argon2) | BLOCKED | N002 | |
| `crypto::rsa` | BLOCKED | N002 | |
| `crypto::x509` | BLOCKED | N002 | |

**Coverage**: 0/16 (0%) — BLOCKED

## std::net (Networking) — 16 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `net::tcp` | BLOCKED | N002 | Depends on crypto/tls |
| `net::async_tcp` | BLOCKED | N002 | |
| `net::udp` | BLOCKED | N002 | |
| `net::dns` | BLOCKED | N002 | |
| `net::url` | BLOCKED | K001 | Uses str::len |
| `net::mime` | BLOCKED | K001 | Uses str::len |
| `net::sys` | BLOCKED | N002 | |

**Coverage**: 0/16 (0%) — BLOCKED

## std::http — 68 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| HTTP/1.1 (request, response, headers) | No | — | Depends on net (blocked) |
| HTTP/2 (frames, HPACK, streams) | No | — | Depends on net (blocked) |

**Coverage**: 0/68 (0%) — BLOCKED (depends on net)

## std::zlib (Compression) — 10 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `zlib::deflate` | No | — | Needs dedicated bench |
| `zlib::gzip` | No | — | |
| `zlib::brotli` | No | — | |
| `zlib::zstd` | No | — | |
| `zlib::crc32` | No | — | |

**Coverage**: 0/10 (0%)

## std::db (Database) — 55+ files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `db::sqlite::database` | No | — | SQLite bench exists but not profiled |
| `db::sqlite::statement` | No | — | |
| `db::sqlite::row` | No | — | |

**Coverage**: 0/55 (0%)

## std::streams — 15 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `streams::ByteStream` | No | — | |
| `streams::BufferedReader` | No | — | |
| `streams::TransformStream` | No | — | |

**Coverage**: 0/15 (0%)

## std::file (I/O) — 6 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `file::File` (read, write) | No | — | |
| `file::Dir` (create, list) | No | — | |
| `file::Path` | No | — | |
| `file::BufReader/Writer` | No | — | |

**Coverage**: 0/6 (0%)

## std::time — 2 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `time::Instant` | Used everywhere | <1 ns overhead | Used as timer in all benchmarks |
| `time::DateTime` | No | — | |

**Coverage**: 1/2 (50%)

## std::sync (Synchronization) — 25 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `sync::mutex` | No | — | |
| `sync::condvar` | No | — | |
| `sync::semaphore` | No | — | |
| `sync::wait_group` | No | — | |
| Atomic types | No | — | |

**Coverage**: 0/25 (0%)

## std::runtime (Async) — 6 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `runtime::executor` | No | — | |
| `runtime::channel` | No | — | |
| `runtime::sleep` | No | — | |

**Coverage**: 0/6 (0%)

## std::search (Information Retrieval) — 4 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `search::bm25` | No | — | |
| `search::hnsw` | No | — | |
| `search::distance` | No | — | |

**Coverage**: 0/4 (0%)

## std::alloc (Allocators) — 5 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `alloc::arena` | No | — | |
| `alloc::cache` | No | — | |
| `alloc::pool` | No | — | |
| `alloc::soo` | No | — | |

**Coverage**: 0/5 (0%)

## Summary

| std Category | Files | Benchmarked | Coverage | Status |
|-------------|-------|-------------|----------|--------|
| collections | 12 | 2 | 17% | Partial |
| text | 1 | 0 | 0% | BLOCKED (K001) |
| json | 4 | 0 | 0% | BLOCKED (K001) |
| crypto | 16 | 0 | 0% | BLOCKED (N002) |
| net | 16 | 0 | 0% | BLOCKED (N002) |
| http | 68 | 0 | 0% | BLOCKED (net) |
| zlib | 10 | 0 | 0% | Not run |
| db | 55 | 0 | 0% | Not run |
| streams | 15 | 0 | 0% | Not run |
| file | 6 | 0 | 0% | Not run |
| time | 2 | 1 | 50% | OK |
| sync | 25 | 0 | 0% | Not run |
| runtime | 6 | 0 | 0% | Not run |
| search | 4 | 0 | 0% | Not run |
| alloc | 5 | 0 | 0% | Not run |
| **TOTAL** | **~245** | **~3** | **~1%** | |

**Only 1% of std is covered by benchmarks.** 104 modules (42%) are blocked by K001/N002. The remaining 138 modules (56%) simply lack benchmarks.

## Priority Unblocking

Fixing 3 bugs unblocks 42% of std:

| Fix | Unblocks | Modules |
|-----|----------|---------|
| K001 str::len | text, json, url, mime, fmt | ~80 |
| K001 bool i32/i1 | json, complex conditionals | ~10 |
| N002 crypto .obj | crypto, tls, net, http | ~100+ |
