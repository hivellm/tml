# Proposal: Refactor Codebase — Replace Hardcoded lowlevel with Existing APIs

## Status: PROPOSED

## Why

A full codebase audit (2026-03-19) found **~3000+ lines** of raw `lowlevel` blocks across `lib/std/` where existing core/std APIs already provide the same functionality. This violates the CLAUDE.md mandate: lowlevel is only acceptable for FFI, performance-critical inner loops, and core primitives.

The `lib/core/` library is **clean** — lowlevel there is justified (implementing the primitives themselves).

### Impact

- **Security**: Raw `buf as Str` casts can produce invalid strings; manual pointer arithmetic is error-prone
- **Maintainability**: 366-line `Headers` reimplements `HashMap[Str, Str]`; 259-line `BufferView` duplicates `Slice[U8]`; `ByteStream` duplicates `Buffer`'s layout
- **Code duplication**: `fast_i64_to_str()` copied 3× across HTTP; `el_la_*` dynamic array reimplemented 2×; buffer growth pattern copied 6×
- **Consistency**: Observable subjects encode structs as `List[I64]` with magic slot indices; HTTP shared state is a flat memory block with 20+ offset constants

### Scope

**44 files** across 7 modules. **~3000+ lines** of lowlevel → **~600 lines** using existing APIs.

| Module | Files | Severity |
|--------|-------|----------|
| `http/` | 20 files | 9 Critical, 19 High, 22 Medium |
| `stream/` | 6 files | 4 Critical, 4 High |
| `aio/` | 2 files | 4 Critical, 7 High |
| `runtime/` | 1 file | 1 Critical |
| `observable/` | 1 file | 1 Critical, 2 High |
| `net/` | 2 files | 1 Critical, 1 High |
| Other (`search/`, `crypto/`, `uuid/`, `events/`) | 5 files | 1 Critical, 4 High |

---

## Area 1: HTTP Module (20 files, highest impact)

### 1.1 headers.tml — Replace with HashMap[Str, Str] (CRITICAL)

The entire 366-line `Headers` type is a manual reimplementation of `HashMap[Str, Str]` using parallel arrays, `mem_alloc`, manual growth, linear scan with `ptr_read`. The comment claims "HashMap's pointer-equality limitation" but HashMap does content comparison.

Consumers that bypass Headers API and read its internal memory directly (`server_response.tml:52-117`, `incoming.tml:284-320`) must also be fixed.

### 1.2 server_response.tml — Buffer + Text + existing APIs (CRITICAL)

| Lines | Pattern | Replacement |
|-------|---------|-------------|
| 52-117 | 4 functions bypassing Headers internal memory via `ptr_read/ptr_write` | Call `Headers::set/get/has/serialize()` directly |
| 145-175 | `body_chunks: I64` manual pointer array | `List[Str]` |
| 383-488 | `serialize()` via `mem_alloc + copy_nonoverlapping` chains | `Buffer` with `push_bytes` |
| 533-558 | `fast_i64_to_str()` reimplementation | `core::fmt::helpers::i64_to_str` |

### 1.3 dispatch.tml — Buffer + Text + template literals (CRITICAL)

| Lines | Pattern | Replacement |
|-------|---------|-------------|
| 517-558 | Second copy of `fast_i64_to_str()` | `core::fmt::helpers::i64_to_str` |
| 587-701 | `app_build_response()` via `copy_nonoverlapping + ptr_write[U8]` chains | `Buffer` |
| 619-661 | `app_http_date()` — 29+ individual `ptr_write[U8]` calls | `Text::push_str()` |
| 744-767 | `app_error_response()` manual JSON building | Template literal |

### 1.4 parse.tml — str::substring + List (HIGH)

| Lines | Pattern | Replacement |
|-------|---------|-------------|
| 160-215 | `app_extract_method/path/body` via `mem_alloc + copy_nonoverlapping` | `str::substring(buf, start, end)` |
| 361-424 | Header name/value flat `ptr_write[I64]` offset array | `List[(Str, Str)]` |
| 429-464 | Case-insensitive compare via byte-by-byte `ptr_read[U8]` | `str::to_lowercase` + `==` |
| 485-517 | `app_extract_query/path_from_url` manual substring | `str::substring` |
| 566-593 | `app_pattern_match` manual param string building | `str::substring` |

### 1.5 worker.tml + iocp_worker.tml — Typed shared state struct (HIGH)

Shared state is a flat `mem_alloc(SHARED_SIZE)` block with 20+ fields accessed via `ptr_write[I64]` offset constants. This pattern appears **3 times** (worker.tml:462-541, worker.tml:763-808, iocp_worker.tml:611-638).

Replace with `type SharedState { router: I64, hooks_before: List[I64], ... }`.

Connection slots in iocp_worker.tml (138-184) use same offset pattern — replace with `type ConnectionSlot { fd: I64, buf_ptr: I64, ... }`.

### 1.6 client.tml + server.tml + chunked.tml — Buffer for growing buffers (HIGH)

Manual growing buffer pattern (`mem_alloc + copy_nonoverlapping + mem_free` on each growth) duplicated in:
- `client.tml:157-207` — `read_all()`
- `server.tml:176-218` — `read_request()`
- `chunked.tml:145-285` — `decode_chunked()`

All should use `Buffer` which auto-grows.

### 1.7 router.tml — Typed RadixNode struct (HIGH, deferred)

Radix tree nodes (77-406) are 80-byte manual heap allocations with offset constants. Comment says this is a codegen bug workaround (register clobbering). Fix when codegen bug is resolved.

### 1.8 rate_limit.tml — HashMap[Str, RateLimitEntry] (HIGH)

`RateLimiter` (56-165) stores all state in manual heap block with offset constants. Replace with `HashMap[Str, RateLimitEntry]`.

### 1.9 agent.tml — List[PoolEntry] (HIGH)

Connection pool (126-233) is `mem_alloc(cap * 16)` array of `{name, fd}` pairs. Replace with `List[PoolEntry]`.

### 1.10 app.tml — List[I64] for route/hook tables (MEDIUM)

Route and hook registration (130-393) uses `mem_alloc + ptr_write[I64]` for handler tables. Replace with `List[I64]`.

### 1.11 O(n²) string concatenation in loops (MEDIUM, 10+ files)

Pattern: `result = "{result}{...}"` inside loops creates O(n²) allocations. Found in:
- `headers.tml:227-245` (serialize)
- `request.tml:296-333` (serialize)
- `chunked.tml:23-35, 66-101` (i64_to_hex, encode)
- `stream.tml:41-95` (SSE serialize)
- `cache_control.tml:107-136`
- `cookie.tml:113-135`
- `security.tml:100-160`
- `static_server.tml:119-196`
- `etag.tml:12-39`
- `rate_limit.tml:40-50`

All should use `Text::push_str()` in the loop, then `.as_str()`.

### 1.12 Other HTTP findings (MEDIUM/LOW)

- `body_parser.tml:137-167` — `url_decode()` via `mem_alloc + ptr_write[U8]` → use `Buffer`
- `static_server.tml:119-167` — `mime_for_extension()` duplicates `std::mime::Mime` → use `Mime::from_extension`
- `etag.tml:42-50` — `pow2(n)` reimplements `1 << n` → use shift operator
- `h2/server.tml:180-191` — `h2_i64_to_str()` hardcoded for 9 status codes, silently wrong for others → `i64_to_str`
- `h2/server.tml:163-176` — byte loop to copy string to Buffer → `Buffer::from_string` or `str::char_at`

---

## Area 2: Stream Module (6 files)

### 2.1 readable_stream.tml — Buffer + typed struct (CRITICAL)

Internal buffer (`rbuf_*`, lines 84-200) reimplements `Buffer`'s API entirely (append, prepend, read, ensure capacity). State is an 80-byte heap struct with 10 offset constants. Replace with `Buffer` + typed TML struct.

### 2.2 writable_stream.tml — Buffer + typed struct (CRITICAL)

Identical to readable_stream: `wbuf_*` functions (49-120) duplicate `Buffer`'s layout `[0]=data_ptr, [8]=len, [16]=cap`. Replace with `Buffer`.

### 2.3 byte_stream.tml — Wrap Buffer (CRITICAL)

Entire file (1-464) duplicates `Buffer`'s 32-byte header layout with private accessor functions. `ByteStream` should wrap `Buffer` internally. `to_string()` and `from_string()` use byte-by-byte loops.

### 2.4 pipe.tml — Use ByteStream public API (HIGH)

Bypasses `ByteStream` API entirely (38-120), reading internal header fields at hardcoded offsets. Add `remaining()`, `advance()`, `read_ptr()`, `write_ptr()` to ByteStream, rewrite pipe without lowlevel.

### 2.5 buffered.tml — Buffer + Text + public APIs (HIGH)

- `fill_buf_raw()` byte-by-byte compaction → `copy_nonoverlapping`
- `read_line()` manual Str construction → `Text`
- `flush()` accesses ByteStream internals at offsets → use public API

### 2.6 async_buffered.tml — Buffer (already documented in original analysis)

Replace `abuf_*` helpers with `Buffer`.

---

## Area 3: Async/IO Module (2 files)

### 3.1 event_loop.tml — List[I64] + typed IoSource (CRITICAL)

`el_la_*` system (51 lines) reimplements `List[I64]`. IoSource is 48-byte raw struct with offset constants. ~120 lines affected. (Detailed in original analysis.)

### 3.2 timer_wheel.tml — List[TimerEntry] (CRITICAL)

Raw linked lists in heap memory with `ptr_read/ptr_write` traversal. ~200 lines affected. (Detailed in original analysis.)

---

## Area 4: Runtime Module (1 file)

### 4.1 multi_executor.tml — Typed structs + List + Mutex (CRITICAL)

The entire multi-threaded executor (82-570) is built on raw heap memory with 15+ layout constants (`TASK_SIZE`, `TASK_OFF_FN`, `SS_OFF_QUEUE`, `WC_OFF_STATE`, etc.). `TaskQueue`, `SharedState`, `WorkerContext` are all flat memory blocks.

Replace with typed TML structs. Use `List[Task]` for task queue. Use `Mutex[List[Task]]` for thread-safe queue. Use `AtomicI64` for shutdown flag.

---

## Area 5: Events, Search, Crypto, UUID (5 files)

### 5.1 events.tml — List[I64] + HashMap API (CRITICAL)

`la_*` listener array (81-143) reimplements `List[I64]`. HashMap internal iteration (391-408, 517-533) bypasses public API.

### 5.2 search/bm25.tml + hnsw.tml — Remove unnecessary lowlevel wrappers (HIGH)

Every `@extern("c")` FFI call is unnecessarily wrapped in `lowlevel { }`. Since these are `@extern` declarations, they can be called directly.

### 5.3 crypto/hash.tml — Buffer API + to_hex (HIGH)

`make_evp_digest()` manually writes Buffer internal header fields at offsets. `Digest::to_hex()` is manual hex encoding that `Buffer::to_hex()` already does.

### 5.4 uuid.tml — Text builder (HIGH)

`to_string()` (452-517) is 65 lines of manual hex nibble writing into `mem_alloc(37)`. Replace with `Text`.

---

## Area 6: Observable + Net (3 files, already documented)

- `observable/mod.tml` — typed structs + `RingBuffer[I32]`
- `net/buffer_view.tml` — replace entire type with `Slice[U8]`
- `net/async_udp.tml` — `Heap[UdpHandleState]`

---

## What Does NOT Change

- **lib/core/** — All lowlevel is justified (implementing primitives)
- **lib/test/** — Clean, no lowlevel found
- **poller.tml** — FFI buffer for IOCP events (C struct layout)
- **net/eventloop.tml** — `@extern("c")` wrappers
- **async_tcp.tml** — OS socket operations
- **router.tml** — Codegen bug workaround (deferred until bug fixed)
- **Collections primitives** (list.tml, hashmap.tml, buffer.tml) — They ARE the primitives
- **Sync primitives** (atomic.tml, Arc.tml, mpsc.tml) — Lock-free algorithms need lowlevel
- **crypto/sha256_impl.tml** — Pure TML crypto implementation, justified

## Risks

1. **Headers replacement is breaking**: All HTTP consumers use Headers directly; switching to HashMap changes the API surface
2. **BufferView removal is breaking**: All consumers across HTTP, H2, and net must be updated
3. **ByteStream refactoring is breaking**: pipe.tml, buffered.tml depend on its internal layout
4. **Generic codegen limitations**: Some patterns may hit codegen bugs — test incrementally
5. **Performance regression**: Timer wheel and HTTP response building are hot paths — benchmark

## Validation

- All 1599+ tests must pass after each phase
- No new lowlevel blocks introduced (except where explicitly justified)
- Grep for `lowlevel` in refactored files — only FFI allowed
- Run coverage to verify no regression
