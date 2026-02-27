# Tasks: TML Language Completeness Roadmap

**Status**: In Progress (48%) — M1 ~65% (stdlib done, error chains done, regex done, buffered I/O done, compiler bugs pending), M2 ~35% (logging done, reflection partial, doc gen pending, serialization pending), M3 ~25% (net sync done, sync 100%, async pending), M4 ~23% (TLS done, HTTP pending), M5 ~77% (VSCode done, MCP done, pkg partial), M6 0%

---

## Milestone 1: Foundation (Q1/2026)

**Goal**: Linguagem usável para CLI tools e programas standalone

### 1.1 Test Coverage

Target: ≥70% global coverage. (Task archived — not current priority)

### 1.2 Standard Library Essentials — DONE

See [stdlib-essentials](../stdlib-essentials/tasks.md).

- [x] HashSet, BTreeMap, BTreeSet, ArrayList, Queue, Stack, LinkedList, Deque — `lib/std/src/collections/`
- [x] env, process, OS info — `lib/std/src/os/mod.tml`
- [x] Path — `lib/std/src/file/path.tml`
- [x] DateTime — `lib/std/src/datetime.tml` (644 lines, ISO8601/RFC2822)
- [x] Random — `lib/std/src/random.tml` (xoshiro256**, ThreadRng)
- [x] Subprocess — `lib/std/src/os/subprocess.tml` (Command builder, Child, Output)
- [x] Signal — `lib/std/src/os/signal.tml` (SIGINT, SIGTERM, etc.)
- [x] Pipe — `lib/std/src/os/pipe.tml`
- [x] CLI argument parsing — `lib/std/src/cli.tml` (App/Arg builder, 24 tests)

### 1.3 Buffered I/O — DONE

- [x] BufReader — `lib/std/src/file/bufio.tml` (read_line, read_all, is_eof)
- [x] BufWriter — `lib/std/src/file/bufio.tml` (write, write_line, flush, with_capacity)
- [x] LineWriter — `lib/std/src/file/bufio.tml`
- [x] Tests — bufreader.test.tml, bufwriter.test.tml, linewriter.test.tml

### 1.4 Error Context Chains — DONE

Implemented in `lib/core/src/error.tml` (30KB, 20 test files).

- [x] 1.4.1 `Context` behavior — `.context("msg")` and `.with_context()` on Outcome
- [x] 1.4.2 `Error` source chain — `.source()` retorna erro anterior (ChainedError)
- [x] 1.4.3 `BoxedError` — tipo para errors genéricos com type erasure
- [x] 1.4.4 Helper functions — `error_chain()` iterator, `SimpleError`, `IoError`, `ParseError`
- [x] 1.4.5 Display formatado — Error, ChainedError, BoxedError com Debug/Display
- [x] 1.4.6 Testes — 20 test files (error_context, chained_error_source, boxed_error_new, error_with_position, etc.)

### 1.5 Regex Engine — DONE

Implemented in `lib/std/src/regex.tml` (1068 lines, Thompson's NFA). Task archived: `implement-regex-module`.

- [x] 1.5.1 `Regex` type — compilação de pattern, syntax completa
- [x] 1.5.2 `Regex::is_match()` — teste booleano
- [x] 1.5.3 `Regex::find()` / `find_all()` — busca de matches com posições
- [x] 1.5.4 `Regex::captures()` — grupos de captura
- [x] 1.5.5 `Regex::replace()` / `replace_all()` — substituição
- [x] 1.5.6 `Regex::split()` — split por pattern
- [x] 1.5.7 Character classes (`\d`, `\w`, `\s`, `[a-z]`, `[^abc]`)
- [x] 1.5.8 Quantifiers `*`, `+`, `?` (greedy)
- [x] 1.5.9 Thompson NFA engine (sem backtracking exponencial)
- [x] 1.5.10 Testes — 4 test files (regex_basic, regex_advanced, regex_captures, regex_invalid)

### 1.6 Compiler Bug Fixes

- [ ] 1.6.1 Fix generic cache O(n²) em test suites
- [ ] 1.6.2 Fix PartialEq para multi-element tuples
- [ ] 1.6.3 Fix PartialEq para struct variants
- [ ] 1.6.4 Fix Deserialize para nested structs
- [ ] 1.6.5 Fix Reflect size/align computation
- [ ] 1.6.6 Fix partial field drops

**Gate M1**: Coverage ≥70%, `HashSet`/`BTreeMap` working ✅, `env`/`path`/`datetime` usáveis ✅, regex ✅

---

## Milestone 2: Documentation & Reflection (Q2/2026)

**Goal**: Linguagem auto-documentada com introspecção de tipos

### 2.1 Documentation Generation (task: implement-tml-doc)

- [ ] 2.1.1 Preservar `///` doc comments no lexer (hoje são descartados)
- [ ] 2.1.2 Propagar doc comments: lexer → parser → AST → HIR
- [ ] 2.1.3 Estrutura DocComment: summary, description, params, returns, examples
- [ ] 2.1.4 `tml doc` command - gerar HTML estilo Rust docs
- [ ] 2.1.5 Template HTML com search, navigation, source links
- [ ] 2.1.6 JSON export para integração MCP/LLM
- [ ] 2.1.7 `tml doc <symbol>` - lookup no terminal
- [ ] 2.1.8 Gerar docs para lib/core e lib/std completos
- [ ] 2.1.9 Testes de geração de docs

### 2.2 Reflection System — PARTIAL

See [implement-reflection](../implement-reflection/tasks.md). Phases 1-2, 4 complete (intrinsics, TypeInfo, Any type). Phase 3 partial, Phases 5-6 pending.

### 2.3 Logging Framework — DONE

Implemented in `lib/std/src/log.tml` (12KB).

- [x] 2.3.1 Log levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
- [x] 2.3.2 Functions: `trace()`, `debug()`, `info()`, `warn()`, `error()`, `fatal()`
- [x] 2.3.3 Configurável: `set_level()`, `set_format()`, `init_from_env()`
- [x] 2.3.4 Formatters: FORMAT_TEXT, FORMAT_JSON, FORMAT_COMPACT
- [x] 2.3.5 Sinks: stderr (default), file via `open_file()` / `close_file()`
- [x] 2.3.6 Filtering por módulo e level: `set_filter()`, `module_enabled()`
- [x] 2.3.7 Thread-safe via runtime backend
- [x] 2.3.8 Global logger: `set_level()` / `get_level()`, structured key-value fields
- [ ] 2.3.9 Testes para logging (nenhum test file ainda)

### 2.4 Serialization Framework

- [x] 2.4.1 `Serialize` / `Deserialize` behaviors — `@derive(Serialize, Deserialize)` funciona
- [x] 2.4.2 JSON serialize/deserialize — `lib/std/src/json/serialize.tml` (ToJson/FromJson)
- [ ] 2.4.3 TOML parser/writer
- [ ] 2.4.4 YAML parser/writer
- [ ] 2.4.5 MessagePack binary serialization
- [ ] 2.4.6 CSV reader/writer
- [ ] 2.4.7 Fix: nested struct deserialization (TODO existente)
- [ ] 2.4.8 Testes para cada formato

**Gate M2**: `tml doc` gera HTML navegável, `@derive(Reflect)` funciona, logging estruturado ✅

---

## Milestone 3: Async & Networking (Q2/2026)

**Goal**: Aplicações de rede assíncronas com 10K+ conexões concorrentes

### 3.1 Async Runtime (task: multi-threaded-runtime)

- [ ] 3.1.1 Event loop single-threaded como base (epoll/IOCP/kqueue)
- [ ] 3.1.2 `async func` keyword - state machine codegen real (não stub)
- [ ] 3.1.3 `await` expression - suspensão e retomada de coroutines
- [ ] 3.1.4 `Executor` type com work-stealing scheduler (N worker threads)
- [ ] 3.1.5 `spawn()` - criar async task no executor
- [ ] 3.1.6 `block_on()` - rodar future até completar (entry point)
- [ ] 3.1.7 Timer wheel - `sleep()`, `timeout()`, `interval()` async
- [ ] 3.1.8 I/O reactor - async read/write em file descriptors
- [ ] 3.1.9 `AsyncMutex[T]` - mutex que não bloqueia thread do executor
- [ ] 3.1.10 `AsyncChannel[T]` - channel async (mpsc, oneshot, broadcast)
- [ ] 3.1.11 `AsyncSemaphore` - controle de concorrência
- [ ] 3.1.12 `select!` - aguardar primeiro de N futures
- [ ] 3.1.13 `join!` - aguardar todos os N futures
- [ ] 3.1.14 Benchmarks: sub-microsecond task switch, linear scaling com cores
- [ ] 3.1.15 Testes para async runtime

### 3.2 Networking (task: add-network-stdlib) — PARTIAL

- [x] 3.2.1 `TcpListener` - bind, accept, incoming (sync) — `lib/std/src/net/tcp.tml`
- [x] 3.2.2 `TcpStream` - connect, read, write, shutdown (sync)
- [x] 3.2.3 `UdpSocket` - bind, send_to, recv_from (sync) — `lib/std/src/net/udp.tml`
- [ ] 3.2.4 `AsyncTcpListener` - async accept
- [ ] 3.2.5 `AsyncTcpStream` - async read/write com reactor
- [ ] 3.2.6 `AsyncUdpSocket` - async send/recv
- [ ] 3.2.7 `UnixSocket` / `UnixListener` (POSIX only)
- [x] 3.2.8 Socket options: TCP_NODELAY, SO_REUSEADDR, timeouts, keepalive — `lib/std/src/net/socket.tml`
- [x] 3.2.9 DNS resolution: `lookup_host()` sync — `lib/std/src/net/dns.tml`
- [ ] 3.2.10 Zero-copy buffer management para high-throughput
- [ ] 3.2.11 Connection pooling
- [ ] 3.2.12 Testes: echo server, concurrent clients, benchmarks

### 3.3 Thread Safety — DONE

Sync primitives implemented: Mutex, RwLock, CondVar, Barrier, Arc, Atomic, MPSC, Once, lock-free Queue/Stack — `lib/std/src/sync/`

- [x] 3.3.1 Core sync primitives (mutex, rwlock, condvar, barrier, atomic, mpsc, once)
- [x] 3.3.2 Lock-free data structures (Michael-Scott queue, Treiber stack)
- [x] 3.3.3 Atomic types (Bool, I32, I64, U32, U64, Isize, Usize, Ptr) — 1432 lines
- [x] 3.3.4 Thread scopes, thread-local storage — `lib/std/src/thread/`
- [x] 3.3.5 57 sync tests + 7 thread tests passing
- [ ] 3.3.6 Thread-safe iterators
- [ ] 3.3.7 Stress tests com ThreadSanitizer
- [ ] 3.3.8 Fix: closure Send/Sync analysis

**Gate M3**: TCP echo server funciona, async/await compila e executa, 10K conexões simultâneas

---

## Milestone 4: Web & HTTP (Q3/2026)

**Goal**: Framework HTTP completo para web apps e APIs

### 4.1 HTTP Runtime (task: async-http-runtime)

- [ ] 4.1.1 HTTP/1.1 parser (request/response, headers, body, chunked transfer)
- [ ] 4.1.2 HTTP/1.1 server com keep-alive e pipelining
- [ ] 4.1.3 HTTP/1.1 client com connection pooling
- [ ] 4.1.4 HTTP/2 multiplexing (HPACK header compression, streams)
- [ ] 4.1.5 Router: path matching, params, wildcards, method routing
- [ ] 4.1.6 Middleware pipeline: logging, auth, CORS, compression, rate limiting
- [ ] 4.1.7 Decorator-based routing: `@Controller`, `@Get`, `@Post`, `@Param`, `@Body`
- [ ] 4.1.8 Request/Response types com headers, status codes, body streaming
- [ ] 4.1.9 JSON body parsing/serialization automático
- [ ] 4.1.10 Static file serving
- [ ] 4.1.11 WebSocket support
- [ ] 4.1.12 Benchmarks: 500K req/s HTTP/1.1, <10ms p99 latency

### 4.2 TLS Integration — DONE

- [x] 4.2.1 `TlsContext` - carregar certificados (PEM), client/server contexts, CA config
- [x] 4.2.2 `TlsStream` - wrapping de TcpStream com TLS handshake (connect/accept)
- [x] 4.2.3 `TlsContext::server()` / `TlsContext::client()` - server e client TLS
- [x] 4.2.4 ALPN negotiation (get_alpn, e.g. "h2", "http/1.1")
- [x] 4.2.5 SNI (Server Name Indication) support (set_hostname)
- [x] 4.2.6 Certificate verification e chain validation (TlsVerifyMode, peer_verified, verify_result)
- [ ] 4.2.7 HTTPS server e client (HTTP layer not yet implemented)
- [x] 4.2.8 Testes em `lib/std/tests/net/tls.test.tml`

### 4.3 Promises & Reactivity (task: promises-reactivity)

- [ ] 4.3.1 `Promise[T]` - then, catch, finally, map, flat_map
- [ ] 4.3.2 `Promise::all()`, `Promise::race()`, `Promise::any()`
- [ ] 4.3.3 `Observable[T]` - subscribe, map, filter, merge, zip, combine_latest
- [ ] 4.3.4 `Subject[T]` - multicast observable
- [ ] 4.3.5 `BehaviorSubject[T]` - com valor atual
- [ ] 4.3.6 `ReplaySubject[T]` - com buffer de valores
- [ ] 4.3.7 Operators: debounce, throttle, distinct, buffer, window
- [ ] 4.3.8 Pipe operator `|>` para composição fluente
- [ ] 4.3.9 Backpressure handling (buffer, drop, latest strategies)
- [ ] 4.3.10 Testes para promises e observables

**Gate M4**: HTTP server com rotas decorator-based funciona, HTTPS disponível, WebSocket funciona

---

## Milestone 5: Tooling & Ecosystem (Q3/2026)

**Goal**: Developer experience profissional com IDE support e package management

### 5.1 VSCode Extension — DONE

See [developer-tooling](../developer-tooling/tasks.md) Phase 3. Published v0.18.0 with syntax highlighting, snippets, build integration, updated grammar and LSP.

### 5.2 Language Server Protocol — PARTIAL

See [developer-tooling](../developer-tooling/tasks.md) Phase 4. Completion, hover, diagnostics, formatting working. Missing: go-to-definition, references, rename.

### 5.3 Compiler MCP — DONE

20 tools registered, docs search, project tools. (Task archived)

### 5.4 Package Manager (task: package-manager)

- [x] 5.4.1 `tml.toml` manifest format (name, version, dependencies, scripts) — implemented
- [ ] 5.4.2 Git-based dependencies (`tml add github.com/user/pkg`)
- [ ] 5.4.3 Version resolution (semver compatibility, conflict detection)
- [x] 5.4.4 Lock file (`tml.lock`) para builds reproduzíveis — Lockfile class implemented
- [ ] 5.4.5 Package registry server (REST API)
- [ ] 5.4.6 `tml publish` - publicar pacote no registry
- [ ] 5.4.7 `tml search` - buscar pacotes
- [x] 5.4.8 `tml deps` / `tml remove <pkg>` - gerenciar dependências — implemented
- [ ] 5.4.9 Workspace support (monorepo com múltiplos packages)
- [ ] 5.4.10 Private registries para organizações

**Gate M5**: VSCode extension publicada ✅, autocomplete funciona ✅, `tml add` instala pacotes

---

## Milestone 6: Advanced Features (Q4/2026)

**Goal**: Diferencial competitivo e suporte enterprise

### 6.1 Cross-Compilation (task: cross-compilation)

- [ ] 6.1.1 Target triple parsing: `<arch>-<vendor>-<os>-<env>`
- [ ] 6.1.2 LLVM target configuration por triple
- [ ] 6.1.3 Sysroot discovery e configuração
- [ ] 6.1.4 Tier 1: x86_64-windows-msvc, x86_64-linux-gnu, x86_64-linux-musl
- [ ] 6.1.5 Tier 1: aarch64-apple-darwin (macOS ARM), x86_64-apple-darwin (macOS Intel)
- [ ] 6.1.6 Tier 2: wasm32-unknown-unknown (WebAssembly)
- [ ] 6.1.7 Tier 2: aarch64-linux-gnu (Linux ARM64), x86_64-freebsd
- [ ] 6.1.8 Tier 3: aarch64-linux-android, riscv64-linux-gnu
- [ ] 6.1.9 `tml build --target <triple>` CLI integration
- [x] 6.1.10 Conditional compilation: `#if WINDOWS`, `#if ARM64`, etc. — preprocessor implemented
- [ ] 6.1.11 CI/CD: cross-compile matrix em GitHub Actions
- [ ] 6.1.12 Testes cross-compilation

### 6.2 Auto-Parallelization (task: auto-parallel)

- [ ] 6.2.1 Purity analyzer: detectar funções sem side effects
- [ ] 6.2.2 Dependency analyzer: RAW, WAR, WAW detection em loops
- [ ] 6.2.3 Loop parallelizer: detectar loops paralelizáveis
- [ ] 6.2.4 Work partitioner: dividir work entre threads
- [ ] 6.2.5 `@parallel` annotation para forçar paralelização
- [ ] 6.2.6 `@sequential` annotation para forçar serialização
- [ ] 6.2.7 Thread pool com work-stealing para tarefas paralelas
- [ ] 6.2.8 `parallel_for`, `parallel_map`, `parallel_reduce` builtins
- [ ] 6.2.9 Benchmark: 3-4x speedup em 4 cores para workloads adequados
- [ ] 6.2.10 Testes e validação de correctness

### 6.3 Database Drivers (NEW - não tem task)

- [ ] 6.3.1 `Database` behavior genérico: connect, query, execute, prepare
- [ ] 6.3.2 Connection pooling com min/max connections, idle timeout
- [ ] 6.3.3 SQLite driver (embedded, sem dependências externas)
- [ ] 6.3.4 PostgreSQL driver (wire protocol)
- [ ] 6.3.5 MySQL driver (wire protocol)
- [ ] 6.3.6 Query builder fluent API: `db.select("users").where("age > ?", 18).limit(10)`
- [ ] 6.3.7 Prepared statements com parameter binding (anti SQL injection)
- [ ] 6.3.8 Transaction support: begin, commit, rollback
- [ ] 6.3.9 Async query execution
- [ ] 6.3.10 Migrations: create_table, add_column, etc.
- [ ] 6.3.11 Testes com SQLite in-memory

**Gate M6**: Cross-compile Windows→Linux funciona, SQL queries executam, auto-parallel detecta loops

---

## Bonus: Implemented but not in original roadmap

These modules were implemented via other tasks and contribute to overall completeness:

- **Crypto** — SHA/MD5/BLAKE, HMAC, AES/ChaCha20, RSA/ECDSA/Ed25519, PBKDF2/scrypt/Argon2, X.509, DH/ECDH (15 files in `lib/std/src/crypto/`)
- **Compression** — DEFLATE, GZIP, Brotli, Zstd, streaming, CRC32 (`lib/std/src/zlib/`)
- **Search** — BM25 full-text, HNSW vector search, distance functions (`lib/std/src/search/`)
- **Encoding** — Base32/36/45/58/62/64/85/91, Hex, Percent, ASCII85 (`lib/core/src/encoding/`)
- **SIMD** — I32x4, F32x4, I64x2, F64x2, I8x16, U8x16, Mask types (`lib/core/src/simd/`)
- **URL** — RFC 3986 parser with UrlBuilder (`lib/std/src/url.tml`)
- **UUID** — v1-v8 per RFC 9562 (`lib/std/src/uuid.tml`)
- **SemVer** — 2.0.0 spec with VersionReq (`lib/std/src/semver.tml`)
- **MIME** — Type parsing, 50+ extension mappings (`lib/std/src/mime.tml`)
- **Glob** — Pattern matching with `**`, `{a,b}`, `[!x]` (`lib/std/src/glob.tml`)
- **JSON** — Native parser, builder, serialize/deserialize (`lib/std/src/json/`)
- **Mock Framework** — MockContext with call recording and verification (`lib/test/src/mock.tml`)
- **Bitset** — Heap-backed BitSet with set operations and iterator (`lib/core/src/bitset.tml`)
- **RingBuf** — Circular buffer with front/back ops (`lib/core/src/ringbuf.tml`)
- **Arena/Pool** — Arena allocator, object pool (`lib/core/src/arena.tml`, `pool.tml`)
- **SOO** — SmallVec, SmallString, SmallBox (`lib/core/src/soo.tml`)
- **OOP** — Object base, interfaces (`lib/std/src/oop/`)
- **Exception** — Exception class hierarchy (`lib/std/src/exception.tml`)
- **Profiler** — Performance profiling utilities (`lib/std/src/profiler.tml`)

---

## Tracking: Overall Completeness

| Milestone | Items | Done | Progress | Notes |
|-----------|-------|------|----------|-------|
| M1: Foundation | 37 | 31 | 84% | Stdlib ✅, errors ✅, regex ✅, bufio ✅, compiler bugs pending |
| M2: Docs & Reflection | 27 | 11 | 41% | Logging ✅, reflection partial, doc gen pending, serialization pending |
| M3: Async & Networking | 28 | 10 | 36% | Net sync ✅, thread safety ✅, async runtime pending |
| M4: Web & HTTP | 30 | 7 | 23% | TLS ✅, HTTP pending |
| M5: Tooling | 17 | 10 | 59% | VSCode ✅, MCP ✅, LSP partial, pkg partial |
| M6: Advanced | 33 | 1 | 3% | Conditional compilation done, rest pending |
| **TOTAL** | **172** | **70** | **41%** |

*Last updated: 2026-02-22*
