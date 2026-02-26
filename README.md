# TML - To Machine Language

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

**TML is a batteries-included programming language built for the AI era.** It ships a native MCP server inside the compiler, integrated documentation, built-in test/coverage/bench/fuzz tooling, an embedded LLVM+LLD backend, and self-documenting syntax designed for deterministic LLM code generation.

One binary. Zero external tools. Everything you need from code to production.

```tml
use std::json::{Json}
use std::hash::{fnv1a64}

func main() -> I32 {
    let data = Json::parse("{\"name\": \"TML\", \"year\": 2026}")
    let name = data.get_string("name")
    let hash = fnv1a64(name)

    println("Language: " + name)
    println("Hash: " + hash.to_hex())
    return 0
}
```

---

## What Makes TML Different

### 1. Native MCP Server in the Compiler

TML is the first language with a **Model Context Protocol server built directly into the compiler**. Any AI assistant (Claude, GPT, local models) can programmatically compile, test, lint, format, and inspect TML code through a standardized JSON-RPC 2.0 interface.

```bash
# Start the MCP server (stdio transport)
tml mcp
```

The server exposes 14 tools that map 1:1 to compiler capabilities:

| Tool | What it does |
|------|-------------|
| `compile` | Compile a source file to executable or library |
| `run` | Build and execute, returning program output |
| `build` | Full build with crate-type, optimization, output options |
| `check` | Type-check without compiling (fast feedback) |
| `emit-ir` | Emit LLVM IR with optional function filtering and chunked output |
| `emit-mir` | Emit Mid-level IR for debugging |
| `test` | Run tests with filtering, coverage, profiling, and suite-level targeting |
| `format` | Format source files (check or write mode) |
| `lint` | Lint for style and semantic issues (with auto-fix) |
| `docs/search` | Hybrid BM25 + HNSW semantic search over all documentation |
| `docs/get` | Get full documentation for a specific item by path |
| `docs/list` | List all items in a module, grouped by kind |
| `docs/resolve` | Resolve a documentation item by its qualified path |
| `cache/invalidate` | Invalidate compilation cache for specific files |

This is not a wrapper or plugin. The MCP server links against the same compiler internals used by `tml build`. When an AI calls `check`, it runs the real type checker. When it calls `emit-ir`, it generates real LLVM IR. The AI gets the same diagnostics, errors, and output a human developer would see.

The `docs/search` tool uses **hybrid retrieval** combining BM25 lexical scoring with HNSW semantic vector search, merged via Reciprocal Rank Fusion. It includes query expansion (65+ TML-specific synonyms), MMR diversification, and multi-signal ranking. Indices are cached to disk for sub-10ms query latency across 6000+ documentation items.

**Why this matters:** Traditional languages require AI assistants to shell out to `gcc`, `rustc`, or `go build` and parse text output. TML gives AI assistants structured, programmatic access to every compilation stage — including semantic documentation search that understands TML's vocabulary.

---

### 2. Everything Built In - Zero External Tools

Most languages require a constellation of separate tools. TML ships everything in a single binary:

| Capability | Traditional | TML |
|-----------|-------------|-----|
| **Compilation** | `gcc`/`clang` + `ld`/`lld` | Embedded LLVM + LLD (in-process) |
| **Testing** | External frameworks (gtest, pytest, jest) | `@test` decorator + DLL-based runner |
| **Coverage** | Separate tools (gcov, tarpaulin, istanbul) | `--coverage` flag |
| **Benchmarking** | External (criterion, hyperfine) | `@bench` decorator + baseline comparison |
| **Fuzzing** | External (AFL, libFuzzer) | `@fuzz` decorator + corpus management |
| **Formatting** | External (rustfmt, gofmt, prettier) | `tml fmt` |
| **Linting** | External (clippy, golint, eslint) | `tml lint` (style + semantic + complexity) |
| **Documentation** | External (rustdoc, godoc, jsdoc) | `tml doc` (JSON, HTML, Markdown) |
| **Profiling** | External (perf, valgrind, Instruments) | `--profile` (Chrome DevTools format) |
| **Package management** | External (cargo, npm, pip) | `tml deps` / `tml add` |
| **AI integration** | None / LSP workarounds | Native MCP server |

```bash
# All of this is one binary
tml build app.tml                    # Compile
tml run app.tml                      # Run
tml test --coverage --profile        # Test + coverage + profiling
tml fmt src/ --write                 # Format in-place
tml lint src/ --fix                  # Lint with auto-fix
tml doc src/ --format=html           # Generate documentation
tml mcp                              # Start MCP server for AI
```

---

### 3. Embedded LLVM + LLD (In-Process Compilation)

TML doesn't shell out to `clang` or call an external linker. The compiler **embeds ~55 LLVM static libraries and the LLD linker directly**. Compilation from source to executable happens entirely in-process:

```
Source (.tml) -> Lex -> Parse -> Typecheck -> Borrow Check
             -> HIR -> THIR -> MIR -> LLVM IR -> Object File -> Executable
                        ^                         ^                ^
                   Coercions, dispatch        Embedded LLVM     Embedded LLD
                   exhaustiveness             (in-process)      (in-process)
```

No subprocesses. No temporary `.o` files piped between tools. The compiler IS the backend.

This also means cross-compilation is built in:

```bash
tml build app.tml --target=x86_64-unknown-linux-gnu    # Linux
tml build app.tml --target=x86_64-apple-darwin          # macOS
tml build app.tml --target=x86_64-pc-windows-msvc       # Windows
```

---

### 4. Demand-Driven Query System with Red-Green Incremental Compilation

TML uses the same incremental compilation architecture as `rustc`: a **demand-driven query system** where each compilation stage is a memoized function with dependency tracking.

```
QueryContext
  ├── ReadSource(path)     -> SourceFile       [fingerprinted]
  ├── Tokenize(source)     -> TokenStream      [fingerprinted]
  ├── Parse(tokens)        -> AST              [fingerprinted]
  ├── Typecheck(ast)       -> TypedAST         [fingerprinted]
  ├── Borrowcheck(typed)   -> Verified         [fingerprinted]
  ├── HirLower(verified)   -> HIR              [fingerprinted]
  ├── ThirLower(hir)       -> THIR             [fingerprinted]
  ├── MirBuild(thir)       -> MIR              [fingerprinted]
  └── CodegenUnit(mir)     -> LLVM IR          [fingerprinted + cached to disk]
```

On rebuild, the system uses a **red-green algorithm**:
- **GREEN**: Input fingerprint unchanged -> skip entire pipeline, load cached LLVM IR from disk
- **RED**: Input changed -> recompute, then check if downstream fingerprints actually changed

Change one function in a large project? Only that module's queries are recomputed. If the change doesn't affect the public API, downstream modules stay GREEN.

---

### 5. THIR — Typed High-level IR with Exhaustiveness Checking

Between HIR and MIR, TML has a **THIR (Typed High-level IR)** pass that makes every implicit operation explicit before MIR generation. This is inspired by rustc's THIR but integrated as a first-class query stage:

| What THIR does | Before (HIR) | After (THIR) |
|---|---|---|
| **Materializes coercions** | `I8 + I32` (implicit widening) | `CoercionExpr(I8->I32, lhs) + rhs` |
| **Resolves method dispatch** | `x.to_string()` (unresolved) | `Display::to_string(x)` (resolved, monomorphized) |
| **Desugars operators** | `a + b` (might be overloaded) | `Add::add(a, b)` (explicit method call) |
| **Checks pattern exhaustiveness** | `when color { Red => ... }` | Warning: missing `Green`, `Blue` |

The exhaustiveness checker uses the **Maranget 2007 usefulness algorithm**, supporting enum variants, ranges, literals, wildcards, tuples, and structs.

THIR is enabled by default. The `--no-thir` flag falls back to direct HIR-to-MIR lowering.

---

### 6. Syntax Designed for LLM Code Generation

Every syntax decision in TML eliminates ambiguity that confuses language models. The grammar is strictly **LL(1)** — deterministic with one-token lookahead — so both LLMs and parsers can process TML without backtracking.

#### Keywords & Operators

| Concept | Rust | Go | C++ | TML | Why |
|---------|------|-----|-----|-----|-----|
| Function | `fn` | `func` | `void f()` | `func` | Self-documenting |
| Logical AND | `&&` | `&&` | `&&` | `and` | Natural language |
| Logical OR | `\|\|` | `\|\|` | `\|\|` | `or` | Natural language |
| Logical NOT | `!` | `!` | `!` | `not` | Natural language |
| Pattern match | `match` | `switch` | `switch` | `when` | Intent-revealing |
| Unsafe block | `unsafe {}` | — | — | `lowlevel {}` | Accurate, not scary |
| Loop constructs | `for`/`while`/`loop` | `for` | `for`/`while`/`do` | `loop` (unified) | One keyword |
| Exclusive range | `0..10` | — | — | `0 to 10` | English-readable |
| Inclusive range | `0..=10` | — | — | `0 through 10` | English-readable |
| Error propagation | `expr?` | `if err != nil` | `throw` | `expr!` | Visible marker |
| Ternary | `if c { a } else { b }` | — | `c ? a : b` | `cond ? a : b` | C-style, readable |

#### Generics & References

| Concept | Rust | Go | C++ | TML | Why |
|---------|------|-----|-----|-----|-----|
| Generic syntax | `Vec<T>` | `[]T` | `vector<T>` | `Vec[T]` | `[` has no dual meaning with `<` |
| Nested generics | `Vec<Vec<T>>` | — | `vector<vector<T>>` | `Vec[Vec[T]]` | No `>>` ambiguity |
| Immutable ref | `&T` | `*T` | `const T&` | `ref T` | Words over symbols |
| Mutable ref | `&mut T` | implicit | `T&` | `mut ref T` | Explicit intent |
| Closures | `\|x\| x * 2` | `func(x) {}` | `[](x) {}` | `do(x) x * 2` | `\|` has no dual meaning |
| Lifetimes | `'a`, `'static` | — | — | Always inferred | Zero syntax noise |
| Directives | `#[test]` | `// +build` | `[[nodiscard]]` | `@test` | Clean, unambiguous |
| Auto-derive | `#[derive(Debug)]` | — | — | `@auto(debug)` | Self-documenting |

#### Type System

| Concept | Rust | Go | C++ | TypeScript | TML | Why |
|---------|------|-----|-----|------------|-----|-----|
| Optional | `Option<T>` | `*T` / nil | `optional<T>` | `T \| null` | `Maybe[T]` | Intent-revealing |
| Some / None | `Some(x)` / `None` | — | — | — | `Just(x)` / `Nothing` | Self-documenting |
| Result type | `Result<T,E>` | `(T, error)` | — | — | `Outcome[T,E]` | Describes what it is |
| Traits | `trait` | `interface` | abstract class | `interface` | `behavior` | Self-documenting |
| Heap pointer | `Box<T>` | implicit | `unique_ptr<T>` | — | `Heap[T]` | Describes storage |
| Ref counted | `Rc<T>` | — | `shared_ptr<T>` | — | `Shared[T]` | Describes purpose |
| Atomic RC | `Arc<T>` | — | atomic shared_ptr | — | `Sync[T]` | Describes purpose |
| Clone | `.clone()` | — | `.clone()` | — | `.duplicate()` | No git confusion |
| Unit type | `()` | — | `void` | `void` | `Unit` | Named, explicit |
| Never type | `!` | — | `[[noreturn]]` | `never` | `Never` | Named, explicit |
| Type decl | `struct` / `enum` | `type struct` | `struct` / `enum` | `type` / `enum` | `type` (unified) | One keyword |

#### Integers, Floats & Strings

| Concept | Rust | Go | C++ | TML |
|---------|------|-----|-----|-----|
| Signed integers | `i8`..`i128` | `int8`..`int64` | `int8_t`..`int64_t` | `I8`..`I128` |
| Unsigned integers | `u8`..`u128` | `uint8`..`uint64` | `uint8_t`..`uint64_t` | `U8`..`U128` |
| Floats | `f32`, `f64` | `float32`, `float64` | `float`, `double` | `F32`, `F64` |
| String (owned) | `String` | `string` | `std::string` | `Str` |
| Mutable string | — | `string` | `std::string` | `Text` (SSO) |
| Interpolation | `format!("{x}")` | `fmt.Sprintf` | `std::format` | `"Hello {name}"` |
| Raw string | `r"text"` | `` `text` `` | `R"(text)"` | `r"text"` |
| Multiline | — | `` `multi` `` | — | `"""multi"""` |

#### Memory & Ownership

| Concept | Rust | Go | C++ | TML |
|---------|------|-----|-----|-----|
| Ownership | Move semantics | GC | Copy semantics | Move semantics |
| Borrow checker | Yes | No | No | Yes |
| Interior mutability | `Cell<T>`, `RefCell<T>` | implicit | — | `Cell[T]`, `RefCell[T]` |
| RAII / cleanup | `Drop` trait | `defer` | Destructors | `Disposable` behavior |
| Memory safety | Compile-time | Runtime (GC) | Manual | Compile-time |
| Null safety | No null | `nil` | `nullptr` | No null (use `Maybe[T]`) |

The result: LLMs generate TML code with significantly fewer syntax errors because there are no ambiguous tokens, no overloaded symbols, and every construct has exactly one unambiguous parse.

---

### 7. DLL-Based Test Runner — 5,000+ Tests in 8 Seconds

TML's test runner compiles tests to DLLs and loads them in-process — no process spawning per test file. The result is a test system fast enough to be used as a **real-time feedback loop for AI-driven development**:

| Metric | Value |
|--------|-------|
| **Total tests** | 9,000+ across 780+ files |
| **Full suite (no cache)** | ~43 seconds |
| **Full suite (cached)** | ~8 seconds |
| **Single file (filtered)** | Milliseconds |

```bash
# Full suite — 8 seconds with cache
tml test

# Filter to one file — millisecond feedback
tml test --filter json_parse

# Run a specific module's tests only
tml test --suite=core/str --no-cache

# Full rebuild + coverage + profiling
tml test --no-cache --coverage --profile
```

This performance is not accidental — it's designed for **LLM-assisted debugging workflows**. An AI assistant can run a targeted test in milliseconds, read the result, fix the code, and re-run — all within a single conversation turn. The full suite at 8 seconds means the AI can validate that nothing else broke before committing.

```tml
use test

@test
func test_json_parsing() -> I32 {
    let data = Json::parse("{\"key\": 42}")
    assert(data.is_object(), "should parse as object")
    assert_eq(data.get_i64("key"), 42 as I64, "key should be 42")
    return 0
}

@bench
func bench_hash_fnv(b: Bencher) {
    b.iter(do() {
        fnv1a64("benchmark input string")
    })
}

@fuzz
func fuzz_parser(input: Slice[U8]) -> I32 {
    let s = Str::from_utf8(input).unwrap_or("")
    let _ = Json::parse(s)  // Should never crash
    return 0
}
```

Under the hood:
- **Hash-based caching**: Source file fingerprints determine what needs recompilation — unchanged tests are loaded from cached DLLs instantly
- **Suite batching**: Tests in the same directory are grouped into a single DLL, reducing link overhead. `--suite=core/str` targets a specific module
- **LLVM coverage**: Line, function, and branch coverage with HTML reports
- **Benchmark baselines**: Save and compare performance across runs
- **Crash capture**: Backtraces on test failures
- **Filtered execution**: `--filter` narrows to specific files or test names for instant feedback

---

### 8. Integrated Documentation Generation

Documentation is extracted from source comments (`///` for items, `//!` for modules) and generated in three formats without any external tools:

```tml
//! Hash functions for data integrity and lookup tables.
//!
//! This module provides non-cryptographic hash functions optimized
//! for speed: FNV-1a (32/64-bit) and MurmurHash2 (32/64-bit).

/// Computes a 64-bit FNV-1a hash of the input string.
///
/// FNV-1a is a fast, non-cryptographic hash function with good
/// distribution properties. Suitable for hash tables and checksums.
///
/// @param input The string to hash
/// @returns A Hash64 value containing the computed hash
///
/// @example
/// let h = fnv1a64("hello")
/// println(h.to_hex())  // prints hex representation
pub func fnv1a64(input: Str) -> Hash64 { ... }
```

```bash
tml doc lib/ --format=html      # Interactive HTML docs
tml doc lib/ --format=json      # Machine-readable JSON
tml doc lib/ --format=markdown  # Markdown for GitHub wikis
```

---

### 9. Built-In Linter with Semantic Analysis

The linter combines text-based style checking with AST-based semantic analysis:

```bash
tml lint src/
# W001: Unused variable 'temp' at line 42
# S003: Line exceeds 100 characters at line 87
# C001: Function 'process_data' has cyclomatic complexity 15 (max: 10)

tml lint src/ --fix   # Auto-fix where possible
```

Rule categories:
- **Style (S)**: Indentation, line length, naming conventions, trailing whitespace
- **Warnings (W)**: Unused variables, imports, functions, parameters
- **Complexity (C)**: Function length, cyclomatic complexity, nesting depth

---

### 10. Conditional Compilation with Platform Symbols

Built-in preprocessor with auto-detected platform symbols:

```tml
#if WINDOWS
func get_home() -> Str { return env::var("USERPROFILE") }
#elif MACOS
func get_home() -> Str { return env::var("HOME") }
#elif LINUX
func get_home() -> Str { return env::var("HOME") }
#endif

#ifdef DEBUG
func log(msg: Str) { print("[DEBUG] {msg}\n") }
#endif
```

Predefined symbols include: `WINDOWS`, `LINUX`, `MACOS`, `X86_64`, `ARM64`, `PTR_64`, `DEBUG`, `RELEASE`, `TEST`, and more.

---

### 11. SIMD-Optimized Native JSON Engine

TML's JSON parser is not a library written in TML — it's a **native C++ engine compiled into the runtime** with V8-inspired optimizations:

- **SSE2/AVX2 whitespace skipping**: Processes 16 bytes per cycle using `_mm_cmpeq_epi8` to find spaces, tabs, and newlines in parallel
- **SIMD string scanning**: Vectorized search for quotes, backslashes, and control characters — critical for large string values
- **SWAR hex parsing**: Parses 4 hex digits of `\uXXXX` escapes in a single register operation
- **O(1) character classification**: Pre-computed 256-entry lookup tables (`kCharFlags`) for instant character categorization
- **Zero-copy parsing**: Uses `std::string_view` to avoid allocations during parse

The JSON engine is exposed to TML code through a handle-based FFI:

```tml
use std::json::{Json}

let data = Json::parse("{\"users\": [{\"name\": \"Alice\"}]}")
let name = data.get_path_string("users.0.name")  // "Alice"
let json_str = data.to_string()                   // Round-trips cleanly
```

This design means JSON-heavy workloads (HTTP APIs, config parsing, data pipelines) run at near-native speed without needing a separate C library.

---

### 12. Go-Inspired Concurrency Primitives

TML's concurrency model draws directly from Go's design, implemented as native C runtime functions for zero-overhead performance:

**Channels** — Go-style bounded MPMC (Multi-Producer Multi-Consumer):

```tml
use std::sync::{channel, Sender, Receiver}

let (tx, rx) = channel[I32](10)  // Bounded channel, capacity 10

// Producer thread
thread::spawn(do() {
    tx.send(42)
    tx.send(99)
})

// Consumer
let value = rx.recv()  // Blocks until data available
```

The channel implementation uses platform-native primitives (`CRITICAL_SECTION` + `CONDITION_VARIABLE` on Windows, `pthread_mutex_t` + `pthread_cond_t` on POSIX) with a circular buffer for efficient storage. Non-blocking variants (`try_send`, `try_recv`) are also available.

**Full sync toolkit** — all backed by native C:

| Primitive | Implementation | Inspiration |
|-----------|---------------|-------------|
| `Mutex[T]` | `SRWLOCK` (Win) / `pthread_mutex_t` | Rust |
| `RwLock[T]` | `SRWLOCK` shared/exclusive | Rust |
| `Channel[T]` | Circular buffer + condition vars | Go |
| `Sender[T]`/`Receiver[T]` | MPSC on top of channels | Go + Rust |
| `Arc[T]` | Atomic reference counting | Rust |
| `AtomicI32/I64/Bool` | `InterlockedExchange` / `__sync_*` | Go + Rust |
| `Barrier` | Count-based synchronization | Go `sync.WaitGroup` |
| `ConcurrentQueue[T]` | Lock-free MPMC queue | Go channels |
| `ConcurrentStack[T]` | Lock-free push/pop | — |
| Async/Await | Task queue + cooperative scheduler | Rust Futures |

---

## Language at a Glance

```tml
// Types: I8-I128, U8-U128, F32, F64, Bool, Char, Str
let name: Str = "TML"
let mut counter: I32 = 0

// Maybe[T] (like Option) and Outcome[T, E] (like Result)
let value: Maybe[I32] = Just(42)
let result: Outcome[I32, Str] = Ok(100)

// Pattern matching with 'when'
when value {
    Just(n) => println(n.to_string()),
    Nothing => println("empty")
}

// Error propagation with !
func load() -> Outcome[Data, Str] {
    let content = read_file("data.json")!
    let parsed = parse(content)!
    return Ok(parsed)
}

// Closures with 'do'
let doubled = numbers.map(do(x) x * 2)

// Behaviors (traits)
pub behavior Hashable {
    pub func hash(this) -> I64
}

// Generics with [T] instead of <T>
func first[T](items: Slice[T]) -> Maybe[ref T] {
    return items.get(0)
}

// Enums with data
type Message {
    Text(Str),
    Number(I32),
    Quit
}
```

---

## Standard Library

TML ships with a comprehensive standard library covering:

| Module | Contents |
|--------|----------|
| `core::array` | Fixed-size arrays with map, zip, get, first, last |
| `core::iter` | Iterator adapters (map, filter, fold, chain, zip, enumerate, ...) |
| `core::option` | Maybe[T] with combinators |
| `core::result` | Outcome[T, E] with combinators |
| `core::str` | String manipulation (split, trim, contains, replace, ...) |
| `core::slice` | Slice[T] and MutSlice[T] fat pointers |
| `core::fmt` | Display, Debug, binary/hex formatting |
| `core::cmp` | Ordering, PartialEq, PartialOrd, min, max, clamp |
| `core::hash` | Hash behavior for hash-based collections |
| `core::cell` | Interior mutability (Cell, RefCell, OnceCell) |
| `core::mem` | Memory operations (size_of, align_of, swap) |
| `std::collections` | List, HashMap, HashSet, Queue, Stack, Buffer |
| `std::file` | File I/O, Path operations, directory traversal |
| `std::json` | JSON parsing, serialization, builder pattern |
| `std::net` | TCP/UDP sockets, DNS, TLS |
| `std::hash` | FNV-1a, MurmurHash2, ETag generation |
| `std::crypto` | X.509, HMAC, key management (via OpenSSL) |
| `std::zlib` | Deflate, gzip, brotli, zstd compression |
| `std::sync` | Mutex, RwLock, Barrier, channels, atomics |
| `std::thread` | Thread spawning, thread-local storage |
| `std::os` | Environment variables, system info |
| `std::log` | Structured logging with levels and sinks |
| `std::search` | BM25 text index, HNSW vector index, TF-IDF vectorizer, SIMD distance |
| `std::text` | Regex, Unicode utilities |

---

## Build and Run

### Prerequisites

- **C++20 compiler** (GCC 11+, Clang 15+, MSVC 19.30+)
- **CMake 3.20+**
- **LLVM 15+**

### Platform Support

| Platform | Status | Architecture |
|----------|--------|-------------|
| **Windows** | Full support | x86_64 |
| **macOS** | Full support | ARM64 (Apple Silicon), x86_64 |
| **Linux** | Planned | x86_64 |

### Optional Dependencies

| Module | Requires | Purpose |
|--------|----------|---------|
| `std::crypto` | OpenSSL 3.0+ | Cryptographic operations |
| `std::zlib` | zlib, brotli, zstd | Compression algorithms |

**Windows** — install via vcpkg:

```bash
vcpkg install --x-install-root=vcpkg_installed --triplet=x64-windows
```

**macOS** — install via Homebrew:

```bash
brew install llvm openssl@3 zstd brotli sqlite3
```

### Build

```bash
# Windows
scripts\build.bat              # Debug build
scripts\build.bat release      # Release build

# macOS/Linux
bash scripts/build.sh          # Debug build (auto-detects Homebrew LLVM)
bash scripts/build.sh release  # Release build
```

### Usage

```bash
tml build app.tml              # Compile to executable
tml run app.tml                # Compile and run
tml check app.tml              # Type-check only (fast)
tml test                       # Run test suite
tml test --suite=core/str      # Run one module's tests
tml test --coverage            # Tests + coverage report
tml fmt src/                   # Format code
tml lint src/                  # Lint code
tml mcp                        # Start MCP server
tml build app.tml --emit-ir    # Emit LLVM IR
tml build app.tml --release    # Optimized build
```

---

## Compiler Architecture

```
tml/
├── compiler/           # C++ compiler implementation
│   ├── src/
│   │   ├── lexer/      # Tokenizer
│   │   ├── parser/     # LL(1) parser
│   │   ├── types/      # Type checker
│   │   ├── borrow/     # Borrow checker (NLL + Polonius)
│   │   ├── hir/        # High-level IR
│   │   ├── thir/       # Typed HIR (coercions, dispatch, exhaustiveness)
│   │   ├── mir/        # Mid-level IR (SSA form)
│   │   ├── codegen/    # LLVM IR generation
│   │   ├── query/      # Demand-driven query system
│   │   ├── backend/    # Embedded LLVM + LLD
│   │   ├── mcp/        # MCP server (JSON-RPC 2.0)
│   │   ├── doc/        # Documentation generator
│   │   ├── format/     # Code formatter
│   │   └── cli/        # CLI, test runner, linter, builder
│   └── include/        # Headers
├── lib/
│   ├── core/           # Core library (array, iter, str, fmt, ...)
│   ├── std/            # Standard library (collections, file, net, ...)
│   └── test/           # Test framework
├── docs/               # Language specification (14 chapters)
└── scripts/            # Build scripts
```

## License

**Apache License 2.0** - see [LICENSE](LICENSE) file.

## Acknowledgments

- **Rust** - Ownership model, borrow checking, pattern matching, THIR/MIR architecture, query-based compilation
- **V8 (Google)** - JSON parser design: SIMD whitespace skipping, SWAR hex parsing, lookup-table character classification
- **Go** - Channel-based concurrency, bounded MPMC channels, goroutine-inspired async runtime, thread-safe primitives design
- **LLVM** - Code generation backend (embedded as static libraries)
- **LLD** - In-process linker (COFF/ELF/MachO)
- **MCP (Anthropic)** - Model Context Protocol specification for AI-compiler integration
