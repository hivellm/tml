# Tasks: Refactor Codebase — Replace Hardcoded lowlevel with Existing APIs

**Status**: IN PROGRESS
**Priority**: High
**Updated**: 2026-03-19
**Scope**: 44 files, ~3000+ lines lowlevel → ~600 lines using existing APIs

---

## Phase 1: HTTP Core — Headers, Response, Dispatch, Parse (CRITICAL)

- [ ] 1.1 Replace `Headers` type (headers.tml:32-364) with `HashMap[Str, Str]` wrapper
- [x] 1.2 Replace `headers_set_raw/get_raw/has_raw/serialize_raw` (server_response.tml) — reconstruct Headers from ptr, delegate to API
- [x] 1.3 Replace `get_header_from_ptr/has_header_from_ptr` (incoming.tml) — reconstruct Headers from ptr, delegate to API
- [ ] 1.4 Replace `body_chunks: I64` pointer array (server_response.tml:145-175) with `List[Str]`
- [ ] 1.5 Rewrite `serialize()` (server_response.tml:383-488) using `Buffer`
- [x] 1.6 Remove `fast_i64_to_str()` (server_response.tml) + `h2_i64_to_str()` (h2/server.tml) — use `core::fmt::helpers::i64_to_str`
- [ ] 1.7 Rewrite `app_build_response/app_build_response_into` (dispatch.tml:587-701) using `Buffer`
- [ ] 1.8 Rewrite `app_http_date()` (dispatch.tml:619-661) using `Text`
- [x] 1.9 Rewrite `app_error_response()` (dispatch.tml) using template literal
- [ ] 1.10 Replace `app_extract_method/path/body` (parse.tml:160-215) with `str::substring` — SKIPPED: these work on raw I64 buffers, not Str
- [ ] 1.11 Replace header flat array (parse.tml:361-424) with `List[(Str, Str)]`
- [ ] 1.12 Replace byte-by-byte case compare (parse.tml:429-464) — SKIPPED: works on raw I64 buffer, not Str
- [x] 1.13 Replace `app_extract_query/path_from_url` (parse.tml) with `str::substring_from`/`str::substring_to`
- [x] 1.14 Replace `app_pattern_match` param building (parse.tml) with `str::substring`
- [ ] 1.15 Run HTTP test suite — all tests pass

## Phase 2: HTTP Infrastructure — Worker, IOCP, App, Router (HIGH)

- [ ] 2.1 Define `type SharedState { router: I64, hooks_before: List[I64], ... }` — replace flat memory block
- [ ] 2.2 Refactor `app_listen()` (worker.tml:462-541) to use SharedState struct
- [ ] 2.3 Refactor `app_listen_evloop()` (worker.tml:763-808) — deduplicate with 2.2
- [ ] 2.4 Refactor `app_listen_iocp()` (iocp_worker.tml:611-638) — deduplicate with 2.2
- [ ] 2.5 Define `type ConnectionSlot { fd: I64, buf_ptr: I64, ... }` — replace offset constants in iocp_worker.tml:138-184
- [ ] 2.6 Replace `RateLimiter` offset-based state (rate_limit.tml:56-165) with `HashMap[Str, RateLimitEntry]`
- [ ] 2.7 Replace `Agent` pool (agent.tml:126-233) with `List[PoolEntry]`
- [ ] 2.8 Replace route/hook tables in app.tml (130-393) with `List[I64]`
- [ ] 2.9 Replace `queue_push/pop` (worker.tml:119-174) with `Mutex[RingBuffer]` or equivalent
- [ ] 2.10 Run HTTP test suite — all tests pass

## Phase 3: HTTP Growing Buffers + String Building (HIGH)

- [ ] 3.1 Replace manual growing buffer in `client.tml:157-207` with `Buffer` — BLOCKED: needs `Buffer::write_from_ptr()`
- [ ] 3.2 Replace manual growing buffer in `server.tml:176-218` with `Buffer` — BLOCKED: needs `Buffer::write_from_ptr()`
- [ ] 3.3 Remove duplicate `has_header_end()` in server.tml:220-235 — import from parse.tml
- [ ] 3.4 Replace manual growing buffer in `chunked.tml:145-285` with `Buffer` — BLOCKED: needs `Buffer::write_from_ptr()`
- [x] 3.5 Replace `url_decode()` (body_parser.tml) with `Buffer`-based decode
- [x] 3.6 Replace `h2_build_response()` byte loop (h2/server.tml) with `str::char_at`
- [x] 3.7 Replace `h2_validate_preface()` ptr_read loop (h2/server.tml) with `str::char_at`
- [ ] 3.8 Run HTTP test suite — all tests pass

## Phase 4: HTTP O(n²) String Concatenation (MEDIUM, 10+ files)

- [x] 4.1 Fix `headers.tml` serialize() — use `Text`
- [x] 4.2 Fix `request.tml` serialize() — use `Text`
- [x] 4.3 Fix `chunked.tml` i64_to_hex() — use `Text`
- [x] 4.4 Fix `chunked.tml` encode_body_multi_chunk() — use `Text`
- [x] 4.5 Fix `stream.tml` SSE serialize — use `Text`
- [ ] 4.6 Fix `cache_control.tml` — BLOCKED: pre-existing crash, skip for now
- [x] 4.7 Fix `cookie.tml` to_set_cookie() — use `Text`
- [x] 4.8 Fix `security.tml` — use `Text`
- [x] 4.9 Fix `static_server.tml` static_file_headers() — use `Text`
- [x] 4.10 Fix `etag.tml` fnv1a_hex() — use `Text`
- [x] 4.11 Fix `rate_limit.tml` headers() — use `Text`
- [ ] 4.12 Replace `mime_for_extension()` — SKIPPED: existing version includes charset info that Mime doesn't
- [x] 4.13 Replace `pow2(n)` (etag.tml) with `1 << n`
- [x] 4.14 Replace `app_status_line()` fallback (dispatch.tml) with template literal
- [ ] 4.15 Run HTTP test suite — all tests pass

## Phase 5: Stream Module — Buffer + typed structs (CRITICAL)

- [ ] 5.1 Replace `rbuf_*` in readable_stream.tml (84-200) with `Buffer`
- [ ] 5.2 Replace 10 offset constants in readable_stream.tml (206-228) with typed struct
- [ ] 5.3 Replace `wbuf_*` in writable_stream.tml (49-120) with `Buffer`
- [ ] 5.4 Replace writable_stream.tml offset constants with typed struct
- [ ] 5.5 Refactor `ByteStream` (byte_stream.tml:1-464) to wrap `Buffer` internally
- [ ] 5.6 Add `remaining()`, `advance()`, `read_ptr()`, `write_ptr()` to ByteStream public API
- [ ] 5.7 Rewrite `pipe.tml` (38-120) using ByteStream public API instead of internal offsets
- [ ] 5.8 Fix `buffered.tml` compaction (153-159) — use `copy_nonoverlapping`
- [ ] 5.9 Fix `buffered.tml` read_line() (246-254) — use `Text`
- [ ] 5.10 Fix `buffered.tml` flush() (491-524) — use ByteStream public API
- [ ] 5.11 Replace `abuf_*` helpers in async_buffered.tml with `Buffer`
- [ ] 5.12 Rewrite async_buffered.tml extract_line/extract_remaining with `Text`
- [ ] 5.13 Run stream test suite — all tests pass

## Phase 6: Async/IO — Event Loop + Timer Wheel (CRITICAL)

- [ ] 6.1 Define `type IoSource { socket: I64, token: U32, interests: U32, state: I32, callback: I64, user_data: I64 }`
- [ ] 6.2 Replace `el_la_*` system (event_loop.tml:38-88) with `List[I64]`
- [ ] 6.3 Replace `sources: I64` with `List[IoSource]` + typed field access
- [ ] 6.4 Replace `pending_queue`/`next_tick_queue` with `List[I64]`
- [ ] 6.5 Remove `grow_sources()` — List auto-grows
- [ ] 6.6 Define `type TimerEntry { deadline: I64, callback: I64, user_data: I64, next: I64 }`
- [ ] 6.7 Replace timer_wheel.tml raw pointers (level0/level1/entries) with `List[TimerEntry]` + `List[I64]`
- [ ] 6.8 Rewrite timer schedule/cancel/alloc/free/insert/fire with typed operations
- [ ] 6.9 Remove `grow_entries()` — List auto-grows
- [ ] 6.10 Run async test suite — all tests pass

## Phase 7: Runtime — Multi-Threaded Executor (CRITICAL)

- [ ] 7.1 Define `type Task { fn_ptr: I64, data: I64, id: I64 }`
- [ ] 7.2 Define `type TaskQueue { tasks: List[Task], mutex: I64 }`
- [ ] 7.3 Define `type SharedState { queue: TaskQueue, shutdown: AtomicI64, active: I64, ... }`
- [ ] 7.4 Define `type WorkerContext { state: I64, shared: I64, id: I64, ... }`
- [ ] 7.5 Replace 15 layout constants (multi_executor.tml:82-133) with typed structs
- [ ] 7.6 Replace `tq_init/push/pop/len/destroy` (135-207) with `List[Task]` operations
- [ ] 7.7 Replace `ss_init/destroy` (209-280) with struct initialization
- [ ] 7.8 Replace worker loop `ptr_read` offset access (282-360) with struct field access
- [ ] 7.9 Run runtime test suite — all tests pass

## Phase 8: Events + Observable (CRITICAL/HIGH)

- [ ] 8.1 Replace `la_*` listener array in events.tml (81-143) with `List[I64]`
- [ ] 8.2 Replace HashMap internal iteration (events.tml:391-408, 517-533) with public API
- [ ] 8.3 Define `type SubjectState { completed: Bool, has_error: Bool, next_id: I64 }`
- [ ] 8.4 Replace `List[I64]`-as-struct in Subject/BehaviorSubject with typed structs
- [ ] 8.5 Replace ReplaySubject ring buffer with `core::ringbuf::RingBuffer[I32]`
- [ ] 8.6 Replace operator collect loops with iterator adapters
- [ ] 8.7 Run observable test suite — all tests pass

## Phase 9: Search, Crypto, UUID — Cleanup (HIGH)

- [x] 9.1 Remove unnecessary `lowlevel { }` wrappers around `@extern` calls in bm25.tml (10 functions)
- [x] 9.2 Remove unnecessary `lowlevel { }` wrappers around `@extern` calls in hnsw.tml (15 functions)
- [ ] 9.3 Replace `make_evp_digest()` manual Buffer header writes (crypto/hash.tml:73-86) with Buffer API — needs Buffer internal API
- [x] 9.4 Replace `Digest::to_hex()` (crypto/hash.tml) with `this.data.to_hex()`
- [x] 9.5 Replace `Uuid::to_string()` (uuid.tml) with `Text` builder
- [ ] 9.6 Run search + crypto + uuid test suites — all pass

## Phase 10: Net — BufferView + AsyncUDP (CRITICAL/HIGH)

- [ ] 10.1 Grep all `BufferView` consumers across codebase
- [ ] 10.2 Replace `BufferView` imports with `Slice[U8]` in all consumers
- [ ] 10.3 Delete `buffer_view.tml` after migration
- [ ] 10.4 Replace `mem_alloc(16)` in async_udp.tml (309-317) with `Heap[UdpHandleState]`
- [ ] 10.5 Run net + HTTP test suite — all tests pass

## Phase 11: H2 Connection (HIGH)

- [ ] 11.1 Replace `H2StreamTable` (h2/connection.tml:115-190) with `List[H2StreamEntry]`
- [ ] 11.2 Replace 6× `ptr_read[H2Stream]` + mutate + `ptr_write[H2Stream]` with `Heap[H2Stream]`
- [ ] 11.3 Replace `h2_conn_append_buf()` byte loop with `Buffer::append`
- [x] 11.4 Replace hpack.tml string encode with `str::char_at`, decode with `Buffer::to_string()` — h2/server.tml also fixed
- [ ] 11.5 Run HTTP/2 test suite — all tests pass

## Final Validation

- [ ] 12.1 Run full test suite (1599+ tests) — all pass
- [ ] 12.2 Run coverage — no regression
- [ ] 12.3 Grep `lowlevel` in all refactored files — only FFI/primitives allowed
- [ ] 12.4 Grep `mem_alloc` in lib/std/ — only in collection primitives and FFI
