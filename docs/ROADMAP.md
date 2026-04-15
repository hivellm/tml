# TML Roadmap

**Last updated**: 2026-04-15
**Current state**: C++ compiler 100% functional (beta) — 11,000+ tests, 99% library coverage. Self-hosted TML compiler in development on `feat/self-hosting-compiler`. Persistent compilation daemon (22ms cached builds, 4.5x faster than `cargo check`). Embedded LLD removed in favor of native OS linker (−26% DLL size).

---

## Overview

```
Phase 1  [DONE]         Codegen bug fixes (closures, generics, iterators)
Phase 2  [DONE]         Test coverage 58% → 93%
Phase 3  [DONE]         Stdlib essentials (Math, DateTime, Regex, BufIO, Process, etc.)
Phase 4  [DONE]         C runtime migration → pure TML (0 migration candidates)
Phase 5  [DONE]         Networking + async I/O (HTTP 183K req/s, SQLite, TLS, WebSocket)
Phase 6  [IN PROGRESS]  Self-hosting compiler (C++ → TML) — parser ported, type checker WIP
Phase 7  [DONE]         Rust parity — 16 tasks, 215 items (function-level completeness)
```

### Current Metrics

| Metric | Value |
|--------|-------|
| TML tests passing | 11,000+ across 1,400+ test files |
| Library coverage | 99% (docs/API coverage) |
| Rust core parity | 95%+ (Phase 7 sprints closed major gaps) |
| Rust std parity | 80%+ module-level + 17 exclusive TML modules |
| C++ compiler size | ~240,000 lines |
| TML standard library | ~150,000+ lines |
| Compilation (daemon cached) | 22ms (4.5x faster than `cargo check`) |
| Core modules | 22 directories (traits, types, runtime, data, async, alloc, cell, fmt, iter, ops, ptr, slice, etc.) |
| Std modules | 20 directories (collections, net, http, crypto, sync, file, stream, sqlite, zlib, events, etc.) |
| HTTP performance | 183K req/s — beats Node.js cluster by 32% |

---

## Phase 1: Codegen Bug Fixes — DONE

Fixed ~40 critical compiler bugs blocking stdlib development:
- Closures with variable capture (fat pointer architecture)
- Generic enum method instantiation (Maybe, Outcome, Poll)
- Iterator associated types and adapter chains
- Generic function monomorphization
- Behavior dispatch on generic structs
- LLVM type mismatches for Maybe[ref T], when-expr void

---

## Phase 2: Test Coverage — DONE

Grew test coverage from 58% to 93.2%:
- 1,650+ tests across 200+ test files
- Subprocess-based test architecture (Go model)
- NDJSON protocol for result streaming
- Function-level coverage tracking
- Incremental compilation cache

---

## Phase 3: Stdlib Essentials — DONE

Implemented all foundational library modules:
- Collections: List, HashMap, HashSet, BTreeMap, BTreeSet, Deque, BinaryHeap
- Math, DateTime, Random, Regex (NFA), BufReader/BufWriter
- Error chains, FFI types, Process management
- Sync: Mutex, RwLock, Arc, MPSC channels, Barrier, Semaphore
- Net: TCP, UDP, DNS, TLS (OpenSSL/BCrypt)

---

## Phase 4: C Runtime Migration — DONE

Eliminated all non-essential C runtime code:
- 0 migration candidates remaining (was ~5,210 lines)
- 15 essential FFI files kept (I/O, crypto, net — OS interface)
- O0 optimization pipeline complete (SROA, Mem2Reg, EarlyCSE, Inlining)
- 702 lowlevel blocks migrated to typed accessors

---

## Phase 5: Networking + Async I/O — IN PROGRESS

### Done
- HTTP/1.1 server + client (52 files, 183K req/s)
- Stream module (Readable, Writable, Transform, Duplex, Pipeline)
- Async I/O (Poller, TimerWheel, EventLoop)
- SQLite bindings (7 files)
- WebSocket protocol
- HTTP/2 + HPACK compression
- Tracy profiler integration (70+ zones)
- Reflection system complete (TypeInfo, vtable dispatch, @derive(Reflect))
- Function contracts (pre:/post: with runtime assertions)
- BigInt, Trie, IntervalTree stdlib additions

### Remaining
- [ ] High-level async TCP/UDP APIs
- [ ] HTTP/2 full implementation
- [ ] Connection pooling
- [ ] HTTP performance regression investigation (183K → 8K)

---

## Phase 6: Self-Hosting Compiler — PLANNED

Rewrite the C++ compiler in TML. Requires Phase 7 (Rust parity) to be substantially complete.

### 6.1 Cross-compilation
- [ ] Linux x86_64 target from Windows
- [ ] ARM64 target (macOS Apple Silicon, Linux aarch64)
- [ ] Zig CC integration for cross-compilation

### 6.2 Bootstrap compiler in TML
- [ ] Lexer rewrite in TML
- [ ] Parser rewrite in TML
- [ ] Type checker rewrite in TML
- [ ] HIR/MIR/codegen in TML

### 6.3 Cranelift backend (alternative to LLVM)
- [ ] Cranelift IR generation
- [ ] Faster debug builds via Cranelift

---

## Phase 7: Rust Parity — IN PROGRESS (14/16 complete)

Close all function-level gaps between TML and Rust's core + std libraries.
Full gap analysis: [docs/analyses/compare-rust-tml.md](analyses/compare-rust-tml.md)

**16 tasks, 215 items total. 14 tasks done in 3 sprint sessions (2026-03-28).**

### Completed (Sprint 1 — phase7-01 to 7-05)

| Task | ID | Status | Description |
|------|----|--------|-------------|
| List[T] completeness | `phase7-01` | ✅ Done | sort_by, binary_search, index_of, reserve, truncate, dedup, split_at, resize, fill, sort, insert, remove, contains, reverse, swap |
| Iterator.collect() | `phase7-02` | ✅ Done | FromIterator behavior + collect(), ListIter::to_list(), HashMap::from_iter |
| HashMap extras | `phase7-03` | ✅ Done | is_empty, get_or_set, keys, values, retain, drain_keys, drain_values, extend_from |
| env module | `phase7-04` | ✅ Done | get_var, set_var, remove_var, current_dir, temp_dir, args |
| Str completeness | `phase7-05` | ✅ Done | split_once, rsplit_once, strip_prefix/suffix, splitn, is_ascii, eq_ignore_ascii_case, rsplit, replacen, trim_matches, matches, bytes |

### Completed (Sprint 2 — phase7-06 to 7-08)

| Task | ID | Status | Description |
|------|----|--------|-------------|
| File I/O completeness | `phase7-06` | ✅ Done | Binary read_bytes/write_bytes, DirEntry, read_dir, remove_all, FileMetadata, metadata() |
| Bounded channels | `phase7-07` | ✅ Done | sync_channel, SyncSender, TrySendError, Iterator for Receiver |
| Maybe extras | `phase7-08` | ✅ Done | is_just_and, get_or_insert, replace, unzip |

### Completed (Sprint 3 — phase7-09 to 7-15)

| Task | ID | Status | Description |
|------|----|--------|-------------|
| Iterator extras | `phase7-09` | ✅ Done | is_sorted, is_sorted_by |
| Prelude + Bool + Unit | `phase7-10` | ✅ Done | Bool then_some/then_with, core::prelude re-exports |
| Thread completeness | `phase7-11` | ✅ Done | Real park/unpark (Windows Events), spawn_fn/spawn_i64, panicking(), detach() |
| Net + Sync extras | `phase7-12` | ✅ Done | TcpListener::incoming, write_all, read_to_string |
| Core I/O + ASCII + Random | `phase7-13` | ✅ Done | Read/Write/BufRead in core::io, 10 ASCII funcs, core::random trait |
| Alloc + Cell + Pin extras | `phase7-14` | ✅ Done | SharedWeak/SyncWeak downgrade/upgrade/ptr_eq, Cell extras, Pin extras |
| Collections generics | `phase7-15` | ✅ Done | BTreeMap[K,V]/BTreeSet[T] fully generic, range iterators |

### Remaining

| Task | ID | Items | Description |
|------|----|-------|-------------|
| Slice + Num + Fmt extras | `phase7-16` | 27 | split_first/last, NonZero math, Formatter Write, Infallible |

---

## TML Extras Beyond Rust

TML provides 17 modules that Rust requires external crates for:

| TML Module | Rust Equivalent | Size |
|------------|-----------------|------|
| `std::crypto` | ring, rustls | 16 files |
| `std::http` | hyper, axum | 46 files |
| `std::json` | serde_json | 4 files |
| `std::sqlite` | rusqlite | 7 files |
| `std::search` | (no equivalent) | 4 files |
| `std::zlib` | flate2 | 10 files |
| `std::regex` | regex | 1,190 lines |
| `std::bigint` | num-bigint | 975 lines |
| `std::semver` | semver | 563 lines |
| `std::uuid` | uuid | 634 lines |
| `std::text` | (String is builtin) | 1,173 lines |
| `std::cli` | clap | 480 lines |
| `std::events` | (no equivalent) | 3 files |
| `std::oop` | (not in Rust) | 3 files |
| `std::stream` | tokio::io | 15 files |
| `core::reflect` | (not in Rust) | runtime reflection |
| `core::encoding` | base64, hex | 14 files |
