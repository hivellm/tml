# Tasks: Refactor Codebase — Replace Hardcoded lowlevel with Existing APIs

**Status**: IN PROGRESS (safe refactors done, structural changes blocked)
**Priority**: High
**Updated**: 2026-03-19
**Scope**: 44 files identified, 23 files refactored, remaining blocked by codegen bugs or need new Buffer APIs

---

## Phase 1: HTTP Core — Headers, Response, Dispatch, Parse (CRITICAL)

- [ ] 1.1 Replace `Headers` type (headers.tml:32-364) with `HashMap[Str, Str]` wrapper
- [x] 1.2 Replace `headers_set_raw/get_raw/has_raw/serialize_raw` (server_response.tml) — reconstruct Headers from ptr, delegate to API
- [x] 1.3 Replace `get_header_from_ptr/has_header_from_ptr` (incoming.tml) — reconstruct Headers from ptr, delegate to API
- [x] 1.4 Replace `body_chunks: I64` pointer array with `List[Str]` — removed body_chunk_count/body_chunk_cap fields, write() uses List.push(), serialize() uses List.get(), destroy() uses List.destroy()
- [x] 1.5 Rewrite `serialize()` — SKIP: single-alloc hot path with pre-computed total size is optimal, body chunk iteration already refactored via List.get()
- [x] 1.6 Remove `fast_i64_to_str()` (server_response.tml) + `h2_i64_to_str()` (h2/server.tml) — use `core::fmt::helpers::i64_to_str`
- [x] 1.7 Rewrite `app_build_response/app_build_response_into` — SKIP: zero-alloc hot path with pre-allocated buffer, justified lowlevel for per-request performance
- [x] 1.8 Rewrite `app_http_date()` using `Text::with_capacity(30)` + `push_str` + `str::substring` lookups — replaced 29 ptr_write[U8] calls with readable Text builder
- [x] 1.9 Rewrite `app_error_response()` (dispatch.tml) using template literal
- [x] 1.10 Replace `app_extract_method/path/body` — SKIP: zero-copy parser operates on raw recv buffer, not Str
- [x] 1.11 Replace header flat array — SKIP: nginx-style zero-copy parser by design, stores ptrs into recv buffer, List[(Str,Str)] would violate zero-copy intent
- [x] 1.12 Replace byte-by-byte case compare — SKIP: works on raw I64 buffer, not Str
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

- [x] 5.1 Replace byte-by-byte copies in readable_stream.tml rbuf_append_str/rbuf_read with `copy_nonoverlapping`
- [ ] 5.2 Replace 10 offset constants in readable_stream.tml (206-228) with typed struct — BLOCKED: struct GEP codegen bug
- [x] 5.3 Replace byte-by-byte copies in writable_stream.tml wbuf_append_str/wbuf_append_bytes/wbuf_drain with `copy_nonoverlapping`
- [ ] 5.4 Replace writable_stream.tml offset constants with typed struct — BLOCKED: struct GEP codegen bug
- [ ] 5.5 Refactor `ByteStream` to wrap `Buffer` — BLOCKED: needs Buffer prepend/drain ops
- [ ] 5.6-5.7 ByteStream public API + pipe.tml — BLOCKED: depends on 5.5
- [ ] 5.8-5.10 buffered.tml fixes — BLOCKED: depends on ByteStream refactor
- [ ] 5.11-5.12 async_buffered.tml Buffer/Text — BLOCKED: needs Buffer prepend/drain ops
- [x] 5.13 Run stream test suite — 33/33 pass

## Phase 6: Async/IO — Event Loop + Timer Wheel (BLOCKED)

All items BLOCKED: `el_la_*` and IoSource patterns use I64 handles passed cross-thread via FFI.
Replacing with `List[I64]` or typed structs requires changing the handle-based API throughout
the async stack, and typed structs with List fields hit GEP codegen bugs.

- [ ] 6.1-6.10 — BLOCKED: needs codegen fix for struct-with-List fields

## Phase 7: Runtime — Multi-Threaded Executor (BLOCKED)

All items BLOCKED: SharedState/TaskQueue/WorkerContext use raw I64 handles passed to threads
via `raw_thread_spawn(fn, shared_ptr)`. Typed structs with mutex inline would need codegen
support for struct-to-pointer casts in FFI calls.

- [ ] 7.1-7.9 — BLOCKED: needs codegen fix for struct-with-mutex cross-thread

## Phase 8: Events + Observable (BLOCKED)

- [ ] 8.1-8.2 events.tml — BLOCKED: la_* uses same handle-based API as el_la_*
- [ ] 8.3-8.5 observable — BLOCKED: comment documents codegen bugs (List[func(T)] stride bug, struct GEP bug)
- [ ] 8.6 iterator adapters — BLOCKED: cross-module closures don't emit LLVM symbols

## Phase 9: Search, Crypto, UUID — Cleanup (HIGH)

- [x] 9.1 Remove unnecessary `lowlevel { }` wrappers around `@extern` calls in bm25.tml (10 functions)
- [x] 9.2 Remove unnecessary `lowlevel { }` wrappers around `@extern` calls in hnsw.tml (15 functions)
- [ ] 9.3 Replace `make_evp_digest()` manual Buffer header writes (crypto/hash.tml:73-86) with Buffer API — needs Buffer internal API
- [x] 9.4 Replace `Digest::to_hex()` (crypto/hash.tml) — use Buffer public helpers (buf_get_data, buf_read_byte_at, hex_digit_to_char)
- [x] 9.5 Replace `Uuid::to_string()` (uuid.tml) with `Text` builder
- [x] 9.6 Run search + crypto + uuid test suites — 25/25 crypto, 14/14 uuid pass

## Phase 10: Net — BufferView + AsyncUDP (CRITICAL/HIGH)

- [x] 10.1 Grep all `BufferView` consumers — RESULT: zero consumers outside buffer_view.tml and its test file
- [x] 10.2 Replace `BufferView` imports — N/A: no consumers to replace
- [x] 10.3 Delete `buffer_view.tml` — DEFER: type is unused but harmless, no breaking changes needed
- [ ] 10.4 Replace `mem_alloc(16)` in async_udp.tml (309-317) with `Heap[UdpHandleState]`
- [ ] 10.5 Run net + HTTP test suite — all tests pass

## Phase 11: H2 Connection (HIGH)

- [ ] 11.1 Replace `H2StreamTable` (h2/connection.tml:115-190) with `List[H2StreamEntry]`
- [ ] 11.2 Replace 6× `ptr_read[H2Stream]` + mutate + `ptr_write[H2Stream]` with `Heap[H2Stream]`
- [x] 11.3 Replace `h2_conn_append_buf()` byte loop with `Buffer::copy_to` — uses existing Buffer API instead of byte-by-byte loop
- [x] 11.4 Replace hpack.tml string encode with `str::char_at`, decode with `Buffer::to_string()` — h2/server.tml also fixed
- [ ] 11.5 Run HTTP/2 test suite — all tests pass

## Final Validation

- [ ] 12.1 Run full test suite (1599+ tests) — all pass
- [ ] 12.2 Run coverage — no regression
- [ ] 12.3 Grep `lowlevel` in all refactored files — only FFI/primitives allowed
- [ ] 12.4 Grep `mem_alloc` in lib/std/ — only in collection primitives and FFI
