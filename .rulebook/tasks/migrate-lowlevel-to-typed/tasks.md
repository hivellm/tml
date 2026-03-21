# Tasks: Migrate lowlevel to Typed APIs

**Status**: Not Started
**Scope**: ~702 lowlevel blocks across 25 files (474 CRITICAL + 228 SHOULD MIGRATE)
**Legitimate**: ~1,137 blocks in core primitives, FFI, encodings — NO ACTION needed

## Phase 1: HTTP Shared State → Typed Structs (CRITICAL, highest impact)

- [ ] 1.1 Define `type SharedWorkerState` with all 15+ fields (shutdown, queue, table, count, router, timeouts, hooks, etc.)
- [ ] 1.2 Replace 100 ptr_read/ptr_write in worker.tml with struct field access
- [ ] 1.3 Replace 67 ptr_read/ptr_write in dispatch.tml with struct field access
- [ ] 1.4 Replace 46 ptr_read/ptr_write in iocp_worker.tml with struct field access
- [ ] 1.5 Verify all HTTP tests pass after shared state migration

## Phase 2: HTTP Router → Typed RadixNode (CRITICAL)

- [ ] 2.1 Define `type RadixNode { prefix: Str, kind: I64, children: List[RadixNode], handler: I64, param_name: Str, ... }`
- [ ] 2.2 Replace 60 ptr_read/ptr_write in router.tml with struct field access
- [ ] 2.3 Remove NODE_SIZE, OFF_PREFIX, OFF_KIND offset constants
- [ ] 2.4 Verify routing tests pass

## Phase 3: HTTP App → List-based Tables (CRITICAL)

- [ ] 3.1 Replace 8 raw mem_alloc tables in App::new() with List[I64] for hooks
- [ ] 3.2 Replace hook registration ptr_write with List.push()
- [ ] 3.3 Replace route table ptr_write with List or HashMap
- [ ] 3.4 Verify app tests pass

## Phase 4: HTTP Supporting Types (CRITICAL)

- [ ] 4.1 conn_pool.tml: Replace flat ENTRY_SIZE array with List[ConnEntry]
- [ ] 4.2 rate_limit.tml: Replace flat ENTRY_STRIDE array with HashMap[Str, RateLimitEntry]
- [ ] 4.3 h2/connection.tml: Replace stream table with HashMap[I64, H2Stream]
- [ ] 4.4 work_stealing.tml: Replace raw serialized LocalQueue with typed struct
- [ ] 4.5 agent.tml: Replace raw offset block with typed struct
- [ ] 4.6 bytes.tml: Replace manual refcount with Shared[Buffer]

## Phase 5: Stream Layer → Buffer Wrapper (HIGH)

- [ ] 5.1 buffered.tml: Replace 45 manual header reads with Buffer wrapper
- [ ] 5.2 pipe.tml: Replace 26 internal reads with public Buffer/BufferedReader API
- [ ] 5.3 byte_stream.tml: Replace 11 manual layout with Buffer wrapper
- [ ] 5.4 readable_stream.tml: Replace 10 manual alloc with Buffer.append_str()
- [ ] 5.5 writable_stream.tml: Replace 12 manual alloc with Buffer.append_str()
- [ ] 5.6 async_buffered.tml: Remove 4 redundant alloc/free wrappers
- [ ] 5.7 Verify all stream tests pass

## Phase 6: HTTP Parse → Typed Byte Access (HIGH)

- [ ] 6.1 parse.tml: Replace 79 ptr_read[U8] with Buffer.get(i) or Slice[U8]
- [ ] 6.2 Maintain performance (hot path — benchmark before/after)
- [ ] 6.3 Verify all HTTP tests pass

## Phase 7: Runtime → Typed Structs (CRITICAL)

- [ ] 7.1 multi_executor.tml: Define typed TaskQueue, WorkerCtx, Task structs (40 blocks)
- [ ] 7.2 aio/timer_wheel.tml: Define typed TimerEntry struct (42 blocks)
- [ ] 7.3 Verify async/executor tests pass

## Phase 8: Misc Remaining

- [ ] 8.1 net/buffer_view.tml: Replace 11 ptr_read[U8] with Buffer API
- [ ] 8.2 server_response.tml: Replace 4 copy_nonoverlapping with Buffer chain
- [ ] 8.3 vectored_io.tml + sendfile.tml: Replace 5 manual concat with Buffer.append()
- [ ] 8.4 events.tml: Replace 3 HashMap ctrl byte reads with iterator
- [ ] 8.5 crypto/hash.tml: Replace 1 fake Buffer header with Buffer.with_capacity()

## Validation

- [ ] V.1 Run full test suite after each phase
- [ ] V.2 Benchmark HTTP performance before/after (must not regress >5%)
- [ ] V.3 Count remaining lowlevel blocks — target: <100 non-legitimate
