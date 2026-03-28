# TML Roadmap

**Last updated**: 2026-03-28
**Current state**: Compiler functional, 1,650+ tests passing, 87% Rust core parity, 68% Rust std parity, HTTP server at 183K req/s, async network stack complete, reflection system complete, function contracts implemented

---

## Overview

```
Phase 1  [DONE]         Codegen bug fixes (closures, generics, iterators)
Phase 2  [DONE]         Test coverage 58% → 93%
Phase 3  [DONE]         Stdlib essentials (Math, DateTime, Regex, BufIO, Process, etc.)
Phase 4  [DONE]         C runtime migration → pure TML (0 migration candidates)
Phase 5  [IN PROGRESS]  Networking + async I/O (HTTP 183K req/s, SQLite, TLS, WebSocket)
Phase 6  [PLANNED]      Self-hosting compiler (C++ → TML)
Phase 7  [PLANNED]      Rust parity — 16 tasks, 215 items (function-level completeness)
```

### Current Metrics

| Metric | Value |
|--------|-------|
| TML tests passing | 1,650+ across 200+ test files |
| Library coverage | 93.2% (5,918/6,352 functions) |
| Rust core parity | 87% module-level, detailed gaps in Phase 7 |
| Rust std parity | 68% module-level + 17 exclusive TML modules |
| C++ compiler size | ~240,000 lines |
| TML standard library | ~150,000+ lines |
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

## Phase 7: Rust Parity — PLANNED

Close all function-level gaps between TML and Rust's core + std libraries.
Full gap analysis: [docs/compare-rust-tml.md](compare-rust-tml.md)

**16 tasks, 215 items total.**

### Critical Priority (blocks common patterns)

| Task | ID | Items | Description |
|------|----|-------|-------------|
| List[T] completeness | `phase7-01` | 25 | sort, insert, remove, contains, reverse, iter, binary_search, extend |
| Iterator.collect() | `phase7-02` | 6 | FromIterator trait + collect for List, HashMap, Str |
| HashMap Entry API | `phase7-03` | 14 | Entry, or_insert, keys, values, retain, is_empty |

### High Priority (frequently needed)

| Task | ID | Items | Description |
|------|----|-------|-------------|
| env module | `phase7-04` | 10 | getenv, setenv, getcwd, args, temp_dir |
| Str completeness | `phase7-05` | 15 | split_once, strip_prefix/suffix, splitn, is_ascii, parse[T] |
| File I/O completeness | `phase7-06` | 13 | Binary read/write, read_dir, remove_dir_all, metadata |
| Bounded channels | `phase7-07` | 6 | sync_channel, SyncSender, Receiver as Iterator |

### Medium Priority

| Task | ID | Items | Description |
|------|----|-------|-------------|
| Maybe extras | `phase7-08` | 6 | is_just_and, get_or_insert, replace, unzip |
| Iterator extras | `phase7-09` | 8 | max, min, partition, unzip, is_sorted |
| Prelude + Bool + Unit | `phase7-10` | 10 | Bool::then/then_some, Unit impls, auto-import prelude |
| Thread completeness | `phase7-11` | 13 | park/unpark real impl, Builder::spawn, TLS, panicking() |
| Alloc + Cell + Pin extras | `phase7-14` | 21 | Weak[T], downgrade, get_or_init, Pin safe ops |
| Collections generics | `phase7-15` | 21 | BTreeMap/Set generics [K,V], Deque insert/remove/iter |

### Low Priority

| Task | ID | Items | Description |
|------|----|-------|-------------|
| Net + Sync extras | `phase7-12` | 9 | TcpListener::incoming, write_all, Arc::make_mut |
| Core I/O + ASCII + Random | `phase7-13` | 17 | Read/Write traits in core, AsciiChar, Random trait |
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
