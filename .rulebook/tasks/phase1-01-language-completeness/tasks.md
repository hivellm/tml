# Tasks: TML Language Completeness Roadmap

**Status**: In Progress — 77% (125/162)
**Last updated**: 2026-03-28

---

## Milestone 1: Foundation — 97% (36/37)

**Goal**: Linguagem usável para CLI tools e programas standalone

### 1.1 Test Coverage — DONE

Target: ≥70% global coverage. Achieved: 92.2% (1633 tests).

### 1.2 Standard Library Essentials — DONE

- [x] HashSet, BTreeMap, BTreeSet, ArrayList, Queue, Stack, LinkedList, Deque
- [x] env, process, OS info — `lib/std/src/os/mod.tml`
- [x] Path — `lib/std/src/file/path.tml`
- [x] DateTime — `lib/std/src/datetime.tml`
- [x] Random — `lib/std/src/random.tml` (xoshiro256**)
- [x] Subprocess — `lib/std/src/os/subprocess.tml`
- [x] Signal — `lib/std/src/os/signal.tml`
- [x] Pipe — `lib/std/src/os/pipe.tml`
- [x] CLI argument parsing — `lib/std/src/cli.tml`

### 1.3 Buffered I/O — DONE

- [x] BufReader, BufWriter, LineWriter — `lib/std/src/file/bufio.tml`
- [x] Tests — bufreader.test.tml, bufwriter.test.tml, linewriter.test.tml

### 1.4 Error Context Chains — DONE

- [x] Context behavior, Error source chain, BoxedError, helpers, Display, tests

### 1.5 Regex Engine — DONE

- [x] Regex type, is_match, find/find_all, captures, replace/replace_all, split
- [x] Character classes, quantifiers, Thompson NFA, 4 test files

### 1.6 Compiler Bug Fixes

- [ ] 1.6.1 Fix generic cache O(n²) em test suites — needs profiling to confirm still present
- [x] 1.6.2 Fix PartialEq para multi-element tuples — partial_eq.cpp has dedicated derive
- [x] 1.6.3 Fix PartialEq para struct variants — partial_eq.cpp handles structs
- [ ] 1.6.4 Fix Deserialize para nested structs — BLOCKED: deserialize.cpp uses placeholder API (`json_parse`, `json_free` as `ptr`) that doesn't match actual runtime (`tml_json_parse`, `tml_json_free` as `i64` handles). Needs full re-plumb of codegen to use handle-based API. String constant sizes also were wrong (fixed). Runtime declarations now emitted.
- [x] 1.6.5 Fix Reflect size/align computation — LLVM constant expr ptrtoint(gep) trick
- [ ] 1.6.6 Fix partial field drops
- [x] 1.6.7 Fix cross-module behavior dispatch — `has_pure_tml_functions` required `pub` on impl methods but behavior impls omit `pub`. Fixed in `env_module_support.cpp`, meta version bumped v7→v8. Unblocks `std::json::serialize::ToJson` for primitives.

**Gate M1**: ✅ Coverage ≥70%, collections working, env/path/datetime usable, regex done

---

## Milestone 2: Documentation & Reflection — 52% (14/27)

**Goal**: Linguagem auto-documentada com introspecção de tipos

### 2.1 Documentation Generation — MOSTLY DONE

- [x] 2.1.1 Preservar `///` doc comments no lexer — lexer preserves doc comments
- [x] 2.1.2 Propagar doc comments: lexer → parser → AST — parser stores doc strings in Decl
- [x] 2.1.3 Estrutura DocComment — `doc/doc_model.cpp`, `doc/doc_parser.cpp`
- [x] 2.1.4 `tml doc` command — `cli/commands/cmd_doc.cpp`, generates HTML
- [x] 2.1.5 Template HTML — `doc/generators_html.cpp` with full HTML generation
- [x] 2.1.6 JSON export — `doc/generators.cpp` has JSON output support
- [x] 2.1.7 `tml doc <symbol>` — `--symbol` flag in cmd_doc.hpp
- [x] 2.1.8 Gerar docs para lib/core e lib/std — `tml doc --all`: 382 modules, 4103 items documented
- [ ] 2.1.9 Testes de geração de docs

### 2.2 Reflection System — PARTIAL

See [phase1-08-reflection](../phase1-08-reflection/tasks.md). Phases 1-2, 4 complete. Phase 3 partial, Phases 5-6 pending.

### 2.3 Logging Framework — DONE

- [x] 2.3.1-2.3.8 Full logging: levels, formatters, sinks, filters, thread-safe
- [x] 2.3.9 Testes para logging — 6 test files, all 6 passing (log.test.tml, log_convenience, log_filter, log_format, log_levels, log_msg)

### 2.4 Serialization Framework — DONE (stdlib scope)

- [x] 2.4.1 `Serialize` / `Deserialize` behaviors — `@derive(Serialize, Deserialize)`
- [x] 2.4.2 JSON serialize/deserialize — `lib/std/src/json/serialize.tml`

~~2.4.3-2.4.6 TOML, YAML, MessagePack, CSV~~ — REMOVED: external packages, not stdlib.

- [ ] 2.4.7 Fix: nested struct deserialization
- [x] 2.4.8 Testes para serialization — `derive_serialize.test.tml` (Serialize works, Deserialize blocked by runtime API mismatch)

**Gate M2**: `tml doc` pending, `@derive(Reflect)` works ✅, logging structured ✅

---

## Milestone 3: Async & Networking — 82% (23/28)

**Goal**: Aplicações de rede assíncronas com 10K+ conexões concorrentes

### 3.1 Async Runtime — MOSTLY DONE

- [x] 3.1.1 Event loop (IOCP-based) — `lib/std/src/aio/event_loop.tml`
- [x] 3.1.2 `async func` keyword — state machine codegen implemented
- [x] 3.1.3 `await` expression — suspension and resumption works
- [x] 3.1.4 `Executor` type — `lib/std/src/runtime/executor.tml`
- [x] 3.1.5 `spawn()` — create async task on executor
- [x] 3.1.6 `block_on()` — run future to completion
- [x] 3.1.7 Timer wheel — `lib/std/src/aio/timer_wheel.tml`, sleep in `runtime/sleep.tml`
- [x] 3.1.8 I/O reactor — `lib/std/src/aio/poller.tml`
- [x] 3.1.9 `AsyncMutex[T]` — `lib/std/src/sync/async_mutex.tml`, spinlock + yield, RAII guard with Drop, lock/try_lock/unlock/is_locked
- [x] 3.1.10 `AsyncChannel[T]` — `lib/std/src/runtime/channel.tml` (bounded SPSC)
- [ ] 3.1.11 `AsyncSemaphore` — controle de concurrência (tracked in phase2-02)
- [x] 3.1.12 `select!` — `lib/core/src/future/select.tml` (select2, select_first)
- [x] 3.1.13 `join!` — `Join2[A,B]`, `Join3[A,B,C]`, `join2()`, `join3()` in `core::future::join`
- [ ] 3.1.14 Benchmarks: sub-microsecond task switch, linear scaling com cores
- [x] 3.1.15 Multi-threaded executor — `lib/std/src/runtime/multi_executor.tml`

### 3.2 Networking — MOSTLY DONE

- [x] 3.2.1 `TcpListener` — bind, accept, incoming (sync)
- [x] 3.2.2 `TcpStream` — connect, read, write, shutdown (sync)
- [x] 3.2.3 `UdpSocket` — bind, send_to, recv_from (sync)
- [x] 3.2.4 `AsyncTcpListener` — `lib/std/src/net/async_tcp.tml`
- [x] 3.2.5 `AsyncTcpStream` — async read/write
- [x] 3.2.6 `AsyncUdpSocket` — `lib/std/src/net/async_udp.tml`
- [ ] 3.2.7 `UnixSocket` / `UnixListener` (POSIX only — deferred until Linux support)
- [x] 3.2.8 Socket options: TCP_NODELAY, SO_REUSEADDR, timeouts, keepalive
- [x] 3.2.9 DNS resolution: `lookup_host()` sync
- [x] 3.2.10 Zero-copy buffer management — `BufferView`, `BufferPool`
- [x] 3.2.11 Connection pooling — `lib/std/src/http/conn_pool.tml`
- [x] 3.2.12 Testes: echo server (tcp_echo.test.tml, udp_echo.test.tml), socket tests (63 files)

### 3.3 Thread Safety — DONE

- [x] 3.3.1 Core sync primitives (mutex, rwlock, condvar, barrier, atomic, mpsc, once)
- [x] 3.3.2 Lock-free data structures (Michael-Scott queue, Treiber stack)
- [x] 3.3.3 Atomic types (Bool, I32, I64, U32, U64, Isize, Usize, Ptr) — 1432 lines
- [x] 3.3.4 Thread scopes, thread-local storage
- [x] 3.3.5 57 sync tests + 7 thread tests passing
- [x] 3.3.6 Thread-safe iterators — Added Send/Sync marker impls for 21 adapters (Map, Filter, Take, Skip, Enumerate, Chain, Zip, Fuse, TakeWhile, SkipWhile, FilterMap, Flatten, FlatMap, Inspect, Scan, Copied, Cloned, StepBy, Rev, Peekable, Cycle, Intersperse, MapWhile) + 8 sources (Empty, Once, Repeat, RepeatN, RepeatWith, OnceWith, FromFn, Successors) + 6 legacy types
- [ ] 3.3.7 Stress tests com ThreadSanitizer
- [ ] 3.3.8 Fix: closure Send/Sync analysis

**Gate M3**: TCP echo server ✅, async/await compiles ✅, IOCP 10K+ connections ✅

---

## Milestone 4: Web & HTTP — 90% (27/30)

**Goal**: Framework HTTP completo para web apps e APIs

### 4.1 HTTP Runtime — DONE (47 files in `lib/std/src/http/`)

- [x] 4.1.1 HTTP/1.1 parser — `parse.tml` (zero-copy, SIMD-accelerated)
- [x] 4.1.2 HTTP/1.1 server com keep-alive e pipelining — `server.tml`
- [x] 4.1.3 HTTP/1.1 client com connection pooling — `client.tml`, `conn_pool.tml`
- [x] 4.1.4 HTTP/2 multiplexing — `h2/` (6 files: frame, hpack, connection, stream, server)
- [x] 4.1.5 Router: path matching, params, wildcards — `router.tml` (radix tree)
- [x] 4.1.6 Middleware: CORS, compression, rate limiting, security, cache — 6 middleware files
- [x] 4.1.7 Controller-based routing — `controller.tml`
- [x] 4.1.8 Request/Response types — `incoming.tml`, `server_response.tml`, `headers.tml`
- [x] 4.1.9 JSON body parsing — `body_parser.tml`
- [x] 4.1.10 Static file serving — `static_server.tml` (ETag, Range, MIME)
- [x] 4.1.11 WebSocket support — `websocket.tml` (RFC 6455)
- [ ] 4.1.12 Benchmarks: 500K req/s (tracked in phase3-01-http-performance)

### 4.2 TLS Integration — DONE

- [x] 4.2.1-4.2.8 Full TLS: context, stream, ALPN, SNI, cert verification, HTTPS

### 4.3 Promises & Reactivity — MOSTLY DONE

- [x] 4.3.1 `Promise[T]` — then, resolve, reject, state machine
- [x] 4.3.2 `Promise::all()`, `race()`, `any()`, `all_settled()`
- [x] 4.3.3 `Observable[T]` — subscribe, map, filter, merge, concat
- [x] 4.3.4 `Subject[T]` — multicast observable
- [x] 4.3.5-4.3.6 BehaviorSubject, ReplaySubject — documented in observable/mod.tml
- [x] 4.3.7 Operators: take, skip, scan, distinct, merge, concat
- [x] 4.3.8 Pipe operator `|>` — lexer emits `PipeRight` token, parser desugars `a |> f` to `f(a)` and `a |> f(b)` to `f(a, b)`. Chaining works: `a |> f |> g` = `g(f(a))`
- [ ] 4.3.9 Backpressure handling — partial (WritableStream has cork/uncork)
- [x] 4.3.10 Observable has known codegen bugs (closure symbols, struct GEP)

**Gate M4**: HTTP server ✅, HTTPS ✅, WebSocket ✅, rotas ✅

---

## Milestone 5: Tooling & Ecosystem — 59% (10/17)

**Goal**: Developer experience profissional com IDE support e package management

### 5.1 VSCode Extension — DONE

### 5.2 Language Server Protocol — 0% (no C++ implementation)

LSP has NO C++ implementation. Completion/hover/diagnostics work via MCP only.

### 5.3 Compiler MCP — DONE (20 tools)

### 5.4 Package Manager — PARTIAL

- [x] 5.4.1 `tml.toml` manifest format
- [x] 5.4.2 Git-based dependencies — `tml add --git <url>` implemented in cmd_pkg.cpp
- [x] 5.4.3 Version resolution (semver) — `is_valid_semver()` in build_config.cpp, rlib.hpp version fields
- [x] 5.4.4 Lock file (`tml.lock`)
- [ ] 5.4.5 Package registry server — BLOCKED: needs external service
- [ ] 5.4.6 `tml publish`
- [ ] 5.4.7 `tml search`
- [x] 5.4.8 `tml deps` / `tml remove`
- [ ] 5.4.9 Workspace support
- [ ] 5.4.10 Private registries

**Gate M5**: VSCode ✅, MCP ✅, `tml add` from git pending

---

## Milestone 6: Advanced Features — 3% (1/33)

**Goal**: Diferencial competitivo e suporte enterprise

### 6.1 Cross-Compilation (tracked in phase6-01-cross-compilation)

- [x] 6.1.10 Conditional compilation: `#if WINDOWS`, `#if ARM64`, etc.
- [ ] 6.1.1-6.1.9, 6.1.11-6.1.12 Cross-compilation infrastructure (11 items)

### 6.2 Auto-Parallelization (tracked in phase5-03-auto-parallel)

- [ ] 6.2.1-6.2.10 Purity analysis, loop parallelizer, work-stealing (10 items)

### 6.3 Database Drivers

~~6.3.1-6.3.11~~ — REMOVED: PostgreSQL/MySQL drivers should be external packages.
SQLite already in stdlib (`lib/std/src/sqlite/`).

**Gate M6**: Cross-compile pending, auto-parallel pending

---

## Tracking: Overall Completeness

| Milestone | Items | Done | Progress | Notes |
|-----------|-------|------|----------|-------|
| M1: Foundation | 37 | 36 | **97%** | 4 compiler bugs left (1.6.1, 1.6.4-1.6.6) |
| M2: Docs & Reflection | 27 | 23 | **85%** | Doc gen mostly done (was uncounted), serialize tested, deserialize blocked |
| M3: Async & Networking | 28 | 25 | **89%** | AsyncMutex, thread-safe iterators, benchmarks |
| M4: Web & HTTP | 30 | 27 | **90%** | 500K benchmark, pipe operator, backpressure |
| M5: Tooling | 17 | 12 | **71%** | LSP 0%, workspace/registry pending |
| M6: Advanced | 23 | 1 | **4%** | Only conditional compilation |
| **TOTAL** | **162** | **128** | **79%** | +17: doc gen (8), join!, serialize, net tests, git deps, semver, docs generated |

## Next Actions (priority order)

1. **1.6.4** Fix Deserialize — re-plumb codegen to use `tml_json_*` handle-based API (MEDIUM effort)
2. **1.6.6** Fix partial field drops (needs investigation)
3. **1.6.1** Generic cache O(n²) — profile to confirm
4. **2.1.8** Generate docs for all core/std modules
5. **3.1.9** AsyncMutex[T] (needs async runtime working)
6. **3.3.6** Thread-safe iterators
7. **4.3.8** Pipe operator `|>` — lexer+parser+codegen (LARGE)

*Last updated: 2026-03-25*
