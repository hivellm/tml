# Tasks: TML Language Completeness Roadmap

**Status**: In Progress (35%) — M5 ~75% done (VSCode v0.18.0, LSP 2100+ LOC, MCP done), M3/M4 ~25% (net sync done, TLS done, async pending), M2 ~30% (reflection 48%, docs pending), M1 ~20% (stdlib partial), M6 0%

---

## Milestone 1: Foundation (Q1/2026)

**Goal**: Linguagem usável para CLI tools e programas standalone

### 1.1 Test Coverage

Target: ≥70% global coverage. (Task archived — not current priority)

### 1.2 Standard Library Essentials

See [stdlib-essentials](../stdlib-essentials/tasks.md). HashSet, BTreeMap, env, process, Path, DateTime, Random.

### 1.3 Buffered I/O

See [stdlib-essentials](../stdlib-essentials/tasks.md) Phase 3. BufReader, BufWriter, LineWriter, Read/Write/Seek behaviors.

### 1.4 Error Context Chains (NEW - não tem task)

- [ ] 1.4.1 `Context` behavior - adicionar contexto a erros (`err.context("msg")`)
- [ ] 1.4.2 `Error` source chain - `.source()` retorna erro anterior (encadeamento)
- [ ] 1.4.3 `anyhow`-style `AnyError` - tipo para errors genéricos com context
- [ ] 1.4.4 `bail!` / `ensure!` macros equivalentes
- [ ] 1.4.5 Display formatado com backtrace e chain completo
- [ ] 1.4.6 Testes para error chains

### 1.5 Regex Engine (NEW - não tem task)

- [ ] 1.5.1 `Regex` type - compilação de pattern, syntax básica (., *, +, ?, [], |, ^, $)
- [ ] 1.5.2 `Regex::is_match()` - teste booleano
- [ ] 1.5.3 `Regex::find()` / `find_all()` - busca de matches com posições
- [ ] 1.5.4 `Regex::captures()` - grupos de captura nomeados e posicionais
- [ ] 1.5.5 `Regex::replace()` / `replace_all()` - substituição com backreferences
- [ ] 1.5.6 `Regex::split()` - split por pattern
- [ ] 1.5.7 Character classes (\\d, \\w, \\s, Unicode categories)
- [ ] 1.5.8 Quantifiers lazy vs greedy
- [ ] 1.5.9 Performance: NFA/DFA hybrid engine (sem backtracking exponencial)
- [ ] 1.5.10 Testes para regex

### 1.6 Compiler Bug Fixes

- [ ] 1.6.1 Fix generic cache O(n²) em test suites (`codegen/core/generic.cpp:303`)
- [ ] 1.6.2 Fix PartialEq para multi-element tuples (`derive/partial_eq.cpp:356`)
- [ ] 1.6.3 Fix PartialEq para struct variants (`derive/partial_eq.cpp:367`)
- [ ] 1.6.4 Fix Deserialize para nested structs (`derive/deserialize.cpp:241`)
- [ ] 1.6.5 Fix Reflect size/align computation (`derive/reflect.cpp:111`)
- [ ] 1.6.6 Fix partial field drops (`core/drop.cpp:227`)

**Gate M1**: Coverage ≥70%, `HashSet`/`BTreeMap` working, `env`/`path`/`datetime` usáveis, regex básico

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

### 2.3 Logging Framework (NEW - não tem task)

- [ ] 2.3.1 `Log` behavior com levels: Trace, Debug, Info, Warn, Error
- [ ] 2.3.2 Macros: `trace!()`, `debug!()`, `info!()`, `warn!()`, `error!()`
- [ ] 2.3.3 `Logger` type configurável com formatters e sinks
- [ ] 2.3.4 Formatters: text plain, JSON structured, colored terminal
- [ ] 2.3.5 Sinks: stdout, stderr, file, rotating file
- [ ] 2.3.6 Filtering por módulo e level (e.g., `myapp::db=debug`)
- [ ] 2.3.7 Thread-safe logging com buffer
- [ ] 2.3.8 Global logger singleton (`set_logger()`, `logger()`)
- [ ] 2.3.9 Testes para logging

### 2.4 Serialization Framework (NEW - não tem task)

- [ ] 2.4.1 `Serialize` / `Deserialize` behaviors genéricos (serde-style)
- [ ] 2.4.2 `@derive(Serialize, Deserialize)` - geração automática
- [ ] 2.4.3 TOML parser/writer (para tml.toml configs)
- [ ] 2.4.4 YAML parser/writer
- [ ] 2.4.5 MessagePack binary serialization
- [ ] 2.4.6 CSV reader/writer
- [ ] 2.4.7 Fix: nested struct deserialization (TODO existente)
- [ ] 2.4.8 Testes para cada formato

**Gate M2**: `tml doc` gera HTML navegável, `@derive(Reflect)` funciona, logging estruturado disponível

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

### 3.3 Thread Safety Completion (task: thread-safe-native) — PARTIAL

Sync primitives implemented: Mutex, RwLock, CondVar, Barrier, Arc, Atomic, MPSC, Once — `lib/std/src/sync/`

- [x] 3.3.1 Core sync primitives (mutex, rwlock, condvar, barrier, atomic, mpsc, once)
- [ ] 3.3.2 Thread-safe iterators
- [ ] 3.3.3 Stress tests com ThreadSanitizer
- [ ] 3.3.4 Documentação final
- [ ] 3.3.5 Fix: closure Send/Sync analysis

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

**Gate M5**: VSCode extension publicada, autocomplete funciona, `tml add` instala pacotes

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
- [ ] 6.1.10 Conditional compilation: `#if WINDOWS`, `#if ARM64`, etc. (já existe base)
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

## Tracking: Overall Completeness

| Milestone | Items | Done | Progress | Notes |
|-----------|-------|------|----------|-------|
| M1: Foundation | 52 | 10 | 19% | Collections done, env/process pending, regex pending |
| M2: Docs & Reflection | 35 | 10 | 29% | Reflection P1-2,4 done, doc gen pending |
| M3: Async & Networking | 32 | 6 | 19% | Net sync done, async runtime pending |
| M4: Web & HTTP | 30 | 7 | 23% | TLS done, HTTP pending |
| M5: Tooling | 35 | 27 | 77% | VSCode v0.18.0, LSP done, pkg partial |
| M6: Advanced | 33 | 0 | 0% | Cross-compile, auto-parallel, DB all pending |
| **TOTAL** | **217** | **60** | **28%** |

*Last updated: 2026-02-21*
