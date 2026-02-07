# Tasks: TML Language Completeness Roadmap

**Status**: Proposed (0%)

---

## Milestone 1: Foundation (Q1/2026)

**Goal**: Linguagem usável para CLI tools e programas standalone

### 1.1 Test Coverage (task: increase-library-coverage)

- [ ] 1.1.1 Phase 1: convert, fmt/impls, fmt/float (0% → 80%)
- [ ] 1.1.2 Phase 2: ops/arith, ops/bit (9% → 80%)
- [ ] 1.1.3 Phase 3: cmp (17% → 80%)
- [ ] 1.1.4 Phase 4: str (13% → 75%)
- [ ] 1.1.5 Phase 5: array, array/iter, iter/range, iter/accumulators (0-25% → 70%)
- [ ] 1.1.6 Phase 6: hash (25% → 75%)
- [ ] 1.1.7 Phase 7: collections/class_collections (0% → 60%)
- [ ] 1.1.8 Phase 8: alloc/heap, alloc/shared, alloc/sync (38-56% → 80%)
- [ ] 1.1.9 Phase 9: sync/atomic, task (partial → 60%)
- [ ] 1.1.10 Phase 10: json, pool, cache, arena, intrinsics
- [ ] 1.1.11 Phase 11: Validar coverage >= 70% global

### 1.2 Standard Library Essentials (task: stdlib-essentials)

- [ ] 1.2.1 `HashSet[T]` - Set baseado em hash com insert, remove, contains, iter
- [ ] 1.2.2 `BTreeMap[K,V]` - Map ordenado com insert, get, remove, range queries
- [ ] 1.2.3 `BTreeSet[T]` - Set ordenado
- [ ] 1.2.4 `Deque[T]` - Double-ended queue com push_front/back, pop_front/back
- [ ] 1.2.5 `std::env` - read env vars (`var()`, `vars()`), current dir, home dir
- [ ] 1.2.6 `std::args` - Command-line argument parsing (positional, flags, options)
- [ ] 1.2.7 `std::process` - Spawn subprocesses, pipe stdin/stdout/stderr, wait, kill
- [ ] 1.2.8 `Path` / `PathBuf` - Cross-platform path manipulation (join, parent, extension, exists)
- [ ] 1.2.9 Directory operations - list_dir, create_dir, remove_dir, walk_dir
- [ ] 1.2.10 `Instant` / `SystemTime` - Wall clock, monotonic clock
- [ ] 1.2.11 `DateTime` - Calendar date/time with formatting, parsing, arithmetic
- [ ] 1.2.12 Timezone support - UTC, local, named timezones
- [ ] 1.2.13 `Rng` trait, `ThreadRng` - Random number generation with distributions
- [ ] 1.2.14 Testes para todos os novos módulos

### 1.3 Buffered I/O (NEW - não tem task)

- [ ] 1.3.1 `BufReader[R]` - Buffered reader com configurable buffer size
- [ ] 1.3.2 `BufWriter[W]` - Buffered writer com flush automático
- [ ] 1.3.3 `LineWriter[W]` - Writer que faz flush a cada newline
- [ ] 1.3.4 `Read` behavior - read, read_to_end, read_to_string, read_exact
- [ ] 1.3.5 `Write` behavior - write, write_all, flush
- [ ] 1.3.6 `Seek` behavior - seek, rewind, stream_position
- [ ] 1.3.7 `stdin()`, `stdout()`, `stderr()` - Standard streams com locking
- [ ] 1.3.8 Testes para buffered I/O

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

### 2.2 Reflection System (task: implement-reflection)

- [ ] 2.2.1 Compile-time intrinsics: `field_count[T]()`, `variant_count[T]()`
- [ ] 2.2.2 `TypeInfo` struct: name, size, align, kind, fields[], methods[]
- [ ] 2.2.3 `FieldInfo` struct: name, type_id, offset, is_public
- [ ] 2.2.4 `Reflect` behavior: `type_info()`, `field_value()`, `set_field()`
- [ ] 2.2.5 `Any` type: type erasure com `downcast[T]()`, `is_type[T]()`
- [ ] 2.2.6 `@derive(Reflect)` - geração automática de metadata
- [ ] 2.2.7 Codegen: emitir TypeInfo como constantes globais no LLVM IR
- [ ] 2.2.8 Fix: compute actual size/align em reflect.cpp (TODO existente)
- [ ] 2.2.9 Testes para reflection

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

### 3.2 Networking (task: add-network-stdlib)

- [ ] 3.2.1 `TcpListener` - bind, accept, incoming (sync)
- [ ] 3.2.2 `TcpStream` - connect, read, write, shutdown (sync)
- [ ] 3.2.3 `UdpSocket` - bind, send_to, recv_from (sync)
- [ ] 3.2.4 `AsyncTcpListener` - async accept
- [ ] 3.2.5 `AsyncTcpStream` - async read/write com reactor
- [ ] 3.2.6 `AsyncUdpSocket` - async send/recv
- [ ] 3.2.7 `UnixSocket` / `UnixListener` (POSIX only)
- [ ] 3.2.8 Socket options: TCP_NODELAY, SO_REUSEADDR, timeouts, keepalive
- [ ] 3.2.9 DNS resolution: `lookup_host()` sync e async
- [ ] 3.2.10 Zero-copy buffer management para high-throughput
- [ ] 3.2.11 Connection pooling
- [ ] 3.2.12 Testes: echo server, concurrent clients, benchmarks

### 3.3 Thread Safety Completion (task: thread-safe-native)

- [ ] 3.3.1 Fechar Phase 12: thread-safe collections (se pendente)
- [ ] 3.3.2 Phase 13: thread-safe iterators
- [ ] 3.3.3 Phase 14: stress tests com ThreadSanitizer
- [ ] 3.3.4 Phase 15: documentação final
- [ ] 3.3.5 Fix: closure Send/Sync analysis (`env_lookups.cpp:497`)

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

### 4.2 TLS Integration (NEW - não tem task)

- [ ] 4.2.1 `TlsConfig` - carregar certificados (PEM, DER, PKCS12)
- [ ] 4.2.2 `TlsStream` - wrapping de TcpStream com TLS handshake
- [ ] 4.2.3 `TlsAcceptor` / `TlsConnector` - server e client TLS
- [ ] 4.2.4 ALPN negotiation (para HTTP/2)
- [ ] 4.2.5 SNI (Server Name Indication) support
- [ ] 4.2.6 Certificate verification e chain validation (usar crypto/x509)
- [ ] 4.2.7 HTTPS server e client
- [ ] 4.2.8 Testes com self-signed certs

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

### 5.1 VSCode Extension (task: create-vscode-extension)

- [ ] 5.1.1 TextMate grammar para syntax highlighting (.tml files)
- [ ] 5.1.2 Snippets: func, struct, enum, behavior, test, when, loop
- [ ] 5.1.3 Bracket matching e auto-indentation
- [ ] 5.1.4 Code folding para blocos
- [ ] 5.1.5 Theme contributions (cores semânticas para TML keywords)
- [ ] 5.1.6 Task provider (`tml build`, `tml test` integrados no VSCode)
- [ ] 5.1.7 Problem matcher para diagnostics do compilador
- [ ] 5.1.8 Publicar na VSCode Marketplace

### 5.2 Language Server Protocol (task: developer-tooling)

- [ ] 5.2.1 LSP server base com JSON-RPC sobre stdio
- [ ] 5.2.2 `textDocument/completion` - autocomplete para types, functions, methods
- [ ] 5.2.3 `textDocument/hover` - mostrar type info e doc comments
- [ ] 5.2.4 `textDocument/definition` - go-to-definition (jump to source)
- [ ] 5.2.5 `textDocument/references` - find all references
- [ ] 5.2.6 `textDocument/rename` - rename symbol across files
- [ ] 5.2.7 `textDocument/diagnostic` - erros em tempo real (type check, borrow check)
- [ ] 5.2.8 `textDocument/formatting` - integrar tml format
- [ ] 5.2.9 `textDocument/codeAction` - quick fixes e refactorings
- [ ] 5.2.10 Incremental parsing para responsividade (<100ms response time)
- [ ] 5.2.11 Integrar com VSCode extension

### 5.3 Compiler MCP (task: compiler-mcp-integration)

- [ ] 5.3.1 Separar compiler core em biblioteca reutilizável
- [ ] 5.3.2 Compilation daemon com persistent workspace state
- [ ] 5.3.3 MCP server JSON-RPC: analyze, lint, format, build, test
- [ ] 5.3.4 File overlays (edição in-memory sem salvar)
- [ ] 5.3.5 Auto-fix capabilities (sugerir e aplicar correções)
- [ ] 5.3.6 Testes de integração MCP

### 5.4 Package Manager (task: package-manager)

- [ ] 5.4.1 `tml.toml` manifest format (name, version, dependencies, scripts)
- [ ] 5.4.2 Git-based dependencies (`tml add github.com/user/pkg`)
- [ ] 5.4.3 Version resolution (semver compatibility, conflict detection)
- [ ] 5.4.4 Lock file (`tml.lock`) para builds reproduzíveis
- [ ] 5.4.5 Package registry server (REST API)
- [ ] 5.4.6 `tml publish` - publicar pacote no registry
- [ ] 5.4.7 `tml search` - buscar pacotes
- [ ] 5.4.8 `tml add <pkg>` / `tml remove <pkg>` - gerenciar dependências
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

| Milestone | Items | Done | Progress |
|-----------|-------|------|----------|
| M1: Foundation | 52 | 0 | 0% |
| M2: Docs & Reflection | 35 | 0 | 0% |
| M3: Async & Networking | 32 | 0 | 0% |
| M4: Web & HTTP | 30 | 0 | 0% |
| M5: Tooling | 35 | 0 | 0% |
| M6: Advanced | 33 | 0 | 0% |
| **TOTAL** | **217** | **0** | **0%** |
