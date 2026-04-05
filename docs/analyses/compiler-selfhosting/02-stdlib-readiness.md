# TML Standard Library Readiness for Compiler Self-Hosting

**Date**: 2026-04-05  
**Analyst**: Implementation agent  
**Scope**: Full audit of what a compiler needs from its stdlib vs what TML currently provides  
**Source data**: `lib/core/src/` (196 .tml files), `lib/std/src/` (220 .tml files, excluding runtime/), 796 core tests, 842 std tests

---

## Section 1: What a Compiler Needs from Its Standard Library

A production compiler is not a toy program. It processes hundreds of thousands of tokens per second, builds complex graph-shaped intermediate representations, traverses and transforms them repeatedly, emits multi-megabyte text output, and manages thousands of heap objects with precise ownership semantics. The stdlib categories it depends on are enumerated here in order of criticality.

### 1.1 String Handling

A compiler's most fundamental operation is reading source text and transforming it. The stdlib needs:

- **Immutable string slicing**: fast `O(1)` substring views without copying, used in lexer tokens that borrow from the source buffer
- **String building**: mutable, growing buffer for generating IR text output, error messages, mangled names, and diagnostics — produces multi-megabyte strings in a single compilation
- **String interning**: a global table that deduplicates identifier strings so the compiler can compare identifiers by pointer rather than content — critical for type checker performance where the same identifier appears in thousands of expressions
- **UTF-8 parsing**: validate and iterate codepoints for string literals and character literals
- **Parsing helpers**: `parse_i64`, `parse_f64`, `starts_with`, `contains`, `split`, `find` for lexer keyword detection and escape sequence handling
- **Formatting**: structured pretty-printing for diagnostic messages, LLVM IR text emission, and AST/IR dump output

### 1.2 Collections

A compiler's internal data structures are dominated by these patterns:

- **Dynamic arrays** (`List[T]`): token streams, AST node lists, basic block instruction sequences, phi-node operand lists, worklists for passes, type parameter lists
- **Hash maps** (`HashMap[K,V]`): symbol tables, type environments, function lookup tables, monomorphization caches, query result caches, string-to-type maps
- **Ordered maps** (`BTreeMap[K,V]`): sorted output for deterministic codegen, scope chains, module namespace lookup
- **Bit sets** (`BitSet`): liveness analysis, def-use sets, block reachability, live variable tracking in dataflow analysis
- **Priority queues** (`BinaryHeap[T]` / `MinHeap[T]`): worklist algorithms in dataflow, optimal register allocation ordering, Dijkstra for CFG analysis
- **Double-ended queues** (`Deque[T]`): BFS over CFG, recursive descent parser token lookahead buffer management
- **Tries** (`Trie[V]`): keyword recognition in lexer (alternative to perfect hash), prefix-based identifier completion in IDE mode
- **Arena allocator** (`Arena`): per-compilation-unit bump allocator that frees all AST/HIR nodes in one shot when compilation of a module completes — eliminates reference counting overhead for AST nodes
- **Object pool** (`Pool[T]`): recycling of frequently-allocated short-lived objects (tokens, spans, type constraints)
- **Graph** (not in stdlib): control-flow graph traversal, dominator computation, SSA construction — these are the heart of the MIR layer
- **Interval tree** (`IntervalTree`): source span lookup for diagnostics (find all spans containing an error position), incremental compilation fingerprinting
- **Ring buffer** (`RingBuffer[T]`): token lookahead in parser, streaming lexer output
- **String interner** (not in stdlib): deduplicated `Str`-keyed `HashMap` with `InternedStr` handle type — different from plain HashMap because the returned handle is a `Copy` integer

### 1.3 File I/O

A compiler reads source files, writes object files, reads precompiled module files, and writes cache fingerprints:

- **Read entire file** to string: source file loading
- **Read/write binary** bytes: `.rlib` precompiled module format, incremental cache
- **Path manipulation**: `join`, `parent`, `filename`, `extension`, `with_extension` for module resolution
- **Directory iteration**: finding all `.tml` source files in a package
- **File existence/metadata**: checking if a cached artifact is newer than the source
- **Atomic file write**: write to temp file then rename, to avoid leaving corrupt output if the compiler crashes mid-write
- **Memory-mapped files**: optional but important for large source files in the language server scenario

### 1.4 Error Handling and Diagnostics

Compiler errors are not simple strings. They carry:

- **Spans**: a `(file_id, byte_offset, length)` triple pointing to source
- **Labels**: multiple annotations per error (primary label + secondary context labels)
- **Suggestions**: optional fix-it hints with `replace_span(span, text)` data
- **Error chains**: "error caused by" relationships for cascading type failures
- **Warning suppression**: attribute-based (`#[allow(unused)]`) silence mechanism
- **Multi-error collection**: a compiler must emit *all* errors in a module, not stop at the first

The stdlib must support building and rendering these structured diagnostics efficiently.

### 1.5 Formatting and Pretty-Printing

- **Indented printer**: for AST/IR dump, HTML doc generation, LLVM IR emission — needs push/pop indent, configurable width, overflow handling
- **Colored terminal output**: ANSI escape codes for diagnostic rendering (error = red, note = cyan, etc.)
- **Number formatting**: radix-specific integer formatting (hex addresses in IR, decimal in diagnostics)
- **Floating-point formatting**: accurate `F64` → decimal string with correct rounding (Grisu/Ryu algorithm)

### 1.6 Process and Environment Management

Self-hosted compiler needs:

- **Spawn child process**: invoke `ld` / `lld` for linking, `clang` for C interop compilation, build scripts
- **Capture stdout/stderr**: read linker output to extract error messages and check for success
- **Environment variables**: `TML_DEBUG`, `LLVM_VERSION`, `CC`, `CXX`, `PATH`, `HOME` for config
- **Command-line argument parsing**: the compiler's own CLI parsing for flags like `--optimize`, `--emit-ir`, `--target`
- **Exit code propagation**: return non-zero exit code on compile failure
- **Working directory**: resolve relative source paths against cwd

### 1.7 Memory Management

- **Heap-boxed recursive types** (`Heap[T]`): AST nodes like `Expr` have variants containing sub-`Expr` — requires heap boxing to break the recursive layout cycle
- **Reference counting** (`Shared[T]` / `Sync[T]`): shared AST nodes, type descriptors referenced from multiple places in the type environment
- **Arena allocation** (`Arena`): phase-scoped allocation that frees everything at once — ideal for per-file compilation passes
- **Slab/pool allocation** (`Pool[T]`): frequently-allocated node types

### 1.8 Serialization and Caching

- **Binary serialization**: reading/writing `.rlib` precompiled module metadata (type signatures, exported symbols)
- **JSON**: optional but useful for LSP protocol (language server) and config file parsing
- **Fingerprinting / hashing**: compute `FNV`/`xxHash` of source content and type signatures for incremental compilation
- **Checksum**: verify integrity of cached artifacts

### 1.9 Concurrency (Parallel Compilation)

- **Thread spawning**: per-file parallel compilation (each translation unit can be compiled independently)
- **Work queue** / **MPSC channel**: distributing files to worker threads
- **Mutex/RwLock**: protecting the global type environment during parallel type checking
- **Atomic counters**: error count, warning count, progress tracking

### 1.10 Hashing

- **`Hash` behavior**: all key types in symbol tables must be hashable
- **Non-cryptographic hash functions**: fast `fnv1a`, `xxhash`, `siphash` for symbol table performance
- **Cryptographic hash**: SHA-256 for reproducible build digests and content-addressed caching (optional but useful)

---

## Section 2: What TML Already Has

This section maps each requirement to actual files and module paths that exist in the current TML stdlib as of 2026-04-05.

### 2.1 String Handling

| Requirement | Module Path | Files |
|------------|------------|-------|
| Immutable string slicing (`Str`) | `core::str` | `lib/core/src/str/basic.tml`, `split.tml`, `search.tml`, `replace.tml`, `transform.tml` |
| String parsing (`parse_i32`, `parse_i64`, `parse_f64`) | `core::str` | `lib/core/src/str/parse.tml` |
| String building (mutable, growing) | `std::text` | `lib/std/src/text.tml` |
| SIMD-accelerated string search | `core::str::simd` | `lib/core/src/str/simd.tml` |
| UTF-8 / Unicode classification | `core::unicode` | `lib/core/src/unicode/char.tml`, `unicode_data.tml` |
| Character utilities | `core::char` | `lib/core/src/char/methods.tml`, `convert.tml`, `decode.tml` |
| ASCII classification | `core::ascii` | `lib/core/src/ascii/char.tml`, `functions.tml` |
| Structured formatting | `core::fmt` | `lib/core/src/fmt/formatter.tml`, `builders.tml`, `float.tml`, `num.tml`, `traits.tml` |
| Byte string (non-UTF-8) | `core::encoding::bstr` | `lib/core/src/encoding/bstr.tml` |
| C-compatible strings | `core::ffi::cstr`, `std::ffi::cstring` | `lib/core/src/ffi/cstr.tml`, `lib/std/src/ffi/cstring.tml` |

**Verdict**: Comprehensive. `Str` provides 58 functions. `Text` provides a mutable builder. SIMD search exists. Unicode 15.1.0 data is embedded.

**Gap**: No dedicated string interning type. `HashMap[Str, I32]` works as a workaround but requires heap-allocating each key string independently. A proper interner would store strings in a single bump-allocated pool and return integer handles. Estimated: ~200 lines of TML to build.

### 2.2 Core Collections

| Requirement | Module Path | Files |
|------------|------------|-------|
| Dynamic array | `std::collections::list` | `lib/std/src/collections/list.tml` |
| Hash map | `std::collections::hashmap` | `lib/std/src/collections/hashmap.tml` |
| Ordered/sorted map | `std::collections::btreemap` | `lib/std/src/collections/btreemap.tml` |
| Ordered/sorted set | `std::collections::btreeset` | `lib/std/src/collections/btreeset.tml` |
| Double-ended queue | `std::collections::deque` | `lib/std/src/collections/deque.tml` |
| Priority queue (max-heap) | `std::collections::binary_heap` | `lib/std/src/collections/binary_heap.tml` |
| Priority queue (min-heap) | `std::collections::binary_heap` | `lib/std/src/collections/binary_heap.tml` (MinHeap) |
| Prefix tree | `std::collections::trie` | `lib/std/src/collections/trie.tml` |
| Range query tree | `std::collections::interval_tree` | `lib/std/src/collections/interval_tree.tml` |
| Byte buffer | `std::collections::buffer` | `lib/std/src/collections/buffer.tml` |
| Bit set | `core::data::bitset` | `lib/core/src/data/bitset.tml` |
| Ring buffer | `core::data::ringbuf` | `lib/core/src/data/ringbuf.tml` |
| Iterator framework (30+ adapters) | `core::iter` | `lib/core/src/iter/` (whole directory, 25 files) |
| Fixed-size arrays | `core::array` | `lib/core/src/array/mod.tml` |
| Zero-copy slices | `core::slice` | `lib/core/src/slice/mod.tml`, `iter.tml`, `sort.tml`, `cmp.tml` |

**Verdict**: Extremely comprehensive. All major collection types exist with real implementations. The `BitSet` uses hardware `popcount` for O(n/64) population count. The `Trie` supports prefix queries. The `IntervalTree` supports `O(log n + k)` overlap queries. `BinaryHeap` and `MinHeap` both exist in the same file.

**Gap**: No dedicated graph/CFG type. A `HashMap[I64, List[I64]]` adjacency list can represent CFGs but lacks built-in dominator tree computation, post-order traversal, or strongly-connected-components algorithms. These are moderate-complexity algorithms that would need to be written when porting the MIR passes layer.

### 2.3 Memory Management

| Requirement | Module Path | Files |
|------------|------------|-------|
| Heap boxing for recursive types | `core::alloc::heap` | `lib/core/src/alloc/heap.tml` |
| Reference counting (single-thread) | `core::alloc::shared` | `lib/core/src/alloc/shared.tml` |
| Reference counting (thread-safe) | `core::alloc::sync` | `lib/core/src/alloc/sync.tml` |
| Arena allocator | `core::data::arena` (also `std::alloc::arena`) | `lib/core/src/data/arena.tml` |
| Object pool | `core::data::pool` (also `std::alloc::pool`) | `lib/core/src/data/pool.tml` |
| Small object optimization | `core::data::soo` | `lib/core/src/data/soo.tml` |
| Cache-aligned layouts | `core::data::cache` | `lib/core/src/data/cache.tml` |
| Raw pointers | `core::ptr` | `lib/core/src/ptr/mod.tml`, `non_null.tml`, `const_ptr.tml`, `mut_ptr.tml` |
| Memory intrinsics | `core::intrinsics` | `lib/core/src/intrinsics.tml` |
| Memory operations | `core::runtime::mem` | `lib/core/src/runtime/mem.tml` |
| Interior mutability | `core::cell` | `lib/core/src/cell/cell.tml`, `ref_cell.tml`, `once.tml`, `lazy.tml`, `unsafe_cell.tml` |

**Verdict**: Complete. `Heap[T]` is the equivalent of Rust's `Box<T>` and is the correct tool for building recursive AST types (e.g., `enum Expr { Literal(I64), Add(Heap[Expr], Heap[Expr]) }`). The arena allocator is production-quality with statistics tracking.

### 2.4 File I/O

| Requirement | Module Path | Files |
|------------|------------|-------|
| File read/write (text and binary) | `std::file` | `lib/std/src/file/file.tml` |
| Buffered reader/writer | `std::file` | `lib/std/src/file/bufio.tml` |
| Path manipulation | `std::file::path` | `lib/std/src/file/path.tml` |
| Directory listing | `std::file::dir` | `lib/std/src/file/dir.tml` |
| Glob pattern matching | `std::file::glob` | `lib/std/src/file/glob.tml` |
| Environment variables | `std::env` | `lib/std/src/env.tml` |

**Verdict**: Comprehensive for compiler use. `File::read_all(path)` reads a file to `Str`. `File::write_all(path, content)` writes a `Str`. Binary read/write via `file_read_bytes`/`file_write_bytes` FFI. Path manipulation for `join`, `parent`, `filename`, `extension`, `exists`, `is_file`, `is_dir`, `create_dir_all`, `rename`. Directory iteration exists in `dir.tml`. Glob exists.

**Gap**: No atomic file write (write-to-temp + rename). Easy to implement in ~30 lines using `path_rename`. No memory-mapped file support. For a compiler this means large source files must be `read_all`'d into heap strings rather than memory-mapped — acceptable for correctness, potentially a performance concern for very large files.

### 2.5 Process and Environment

| Requirement | Module Path | Files |
|------------|------------|-------|
| Spawn child process | `std::os::subprocess` | `lib/std/src/os/subprocess.tml` |
| Environment variables | `std::env` | `lib/std/src/env.tml` |
| OS-level operations | `std::os` | `lib/std/src/os/mod.tml` |
| Signals | `std::os::signal` | `lib/std/src/os/signal.tml` |
| Pipes | `std::os::pipe` | `lib/std/src/os/pipe.tml` |
| CLI arg parsing | `std::cli` | `lib/std/src/cli.tml` |

**Verdict**: Complete for compiler use. `Command::new("lld").arg(...).stdout(Stdio::Piped).output()` works. Environment variable get/set/remove works. `std::cli` provides flag parsing for `--flag`, `--flag=value`, positional args.

### 2.6 Concurrency

| Requirement | Module Path | Files |
|------------|------------|-------|
| Thread spawning | `std::thread` | `lib/std/src/thread/mod.tml`, `scope.tml`, `local.tml` |
| Mutex | `std::sync::mutex` | `lib/std/src/sync/mutex.tml` |
| RwLock | `std::sync::rwlock` | `lib/std/src/sync/rwlock.tml` |
| Arc | `std::sync::Arc` | `lib/std/src/sync/Arc.tml` |
| MPSC channels | `std::sync::mpsc` | `lib/std/src/sync/mpsc.tml` |
| Atomics | `std::sync::atomic` | `lib/std/src/sync/atomic/` (11 files) |
| Barrier | `std::sync::barrier` | `lib/std/src/sync/barrier.tml` |
| Condvar | `std::sync::condvar` | `lib/std/src/sync/condvar.tml` |
| Once/OnceLock | `std::sync::once` | `lib/std/src/sync/once.tml` |
| Wait group | `std::sync::wait_group` | `lib/std/src/sync/wait_group.tml` |
| Lock-free queue | `std::sync::queue` | `lib/std/src/sync/queue.tml` |
| Lock-free stack | `std::sync::stack` | `lib/std/src/sync/stack.tml` |
| Semaphore | `std::sync::semaphore` | `lib/std/src/sync/semaphore.tml` |

**Verdict**: Complete. Every concurrency primitive needed for parallel compilation exists. `Mutex[T]`, `Arc[T]`, MPSC channels, and atomics are the critical ones. Scoped threads exist (`lib/std/src/thread/scope.tml`) which simplifies borrowing across thread boundaries.

### 2.7 Hashing

| Requirement | Module Path | Files |
|------------|------------|-------|
| `Hash` behavior for keys | `core::traits::hash` | `lib/core/src/traits/hash.tml` |
| General hash utilities | `std::hash` | `lib/std/src/hash.tml` |
| Cryptographic hashing (SHA-256) | `std::crypto::hash` | `lib/std/src/crypto/hash.tml` |
| SHA-256 pure TML impl | `std::crypto::sha256_impl` | `lib/std/src/crypto/sha256_impl.tml` |

**Verdict**: Adequate. `Hash` behavior is implemented for all primitive types. `std::hash` provides non-cryptographic hashing utilities. SHA-256 exists in pure TML for content-addressed caching.

### 2.8 Serialization

| Requirement | Module Path | Files |
|------------|------------|-------|
| JSON parse + emit | `std::json` | `lib/std/src/json/mod.tml`, `types.tml`, `builder.tml`, `serialize.tml` |
| `ToJson`/`FromJson` behaviors | `std::json` | `lib/std/src/json/serialize.tml` |

**Verdict**: JSON is present. No general binary serialization beyond `Buffer` byte operations. For `.rlib` format, the compiler would need to write its own binary encoder/decoder using `Buffer` primitives or extend the existing format with TML-specific tooling.

### 2.9 Diagnostics and Formatting

| Requirement | Module Path | Files |
|------------|------------|-------|
| Structured formatting | `core::fmt` | `lib/core/src/fmt/` (9 files including `builders.tml` for `DebugStruct`/`DebugList`) |
| `Display` behavior | `core::fmt::traits` | `lib/core/src/fmt/traits.tml` |
| `Debug` behavior | `core::fmt::traits` | `lib/core/src/fmt/traits.tml` |
| Float formatting (Ryu/Grisu) | `core::fmt::float` | `lib/core/src/fmt/float.tml` |
| Number formatting | `core::fmt::num` | `lib/core/src/fmt/num.tml` |
| Logging | `std::log` | `lib/std/src/log.tml` |

**Verdict**: Adequate. `Display` and `Debug` behaviors exist. Float formatting exists. The formatting module is substantial. However, there is no built-in *terminal diagnostic renderer* (the colorized `error[E001]: message → file:line:col → ^^^^^` format used by tools like `rustc`/`miette`). This would need to be built — roughly 300–500 lines of TML for a basic implementation.

### 2.10 Regex and Pattern Matching

| Requirement | Module Path | Files |
|------------|------------|-------|
| Regex engine | `std::regex` | `lib/std/src/regex.tml` |

The regex engine uses Thompson's NFA construction and supports: `.`, `*`, `+`, `?`, `|`, `()`, `[abc]`, `[a-z]`, `[^abc]`, `\d`, `\w`, `\s`, `^`, `$`. Not needed for the compiler's core but useful for source transformation tools built on top.

---

## Section 3: Gap Analysis Table

| Category | Needed | Have | Gap | Severity | Est. Work |
|----------|--------|------|-----|----------|-----------|
| Immutable string (Str) | Complete API | `core::str` — 58 functions | None | — | — |
| Mutable string builder | Growable buffer | `std::text::Text` | None | — | — |
| String interning | `InternedStr` handle + pool | Not present | **Must build** | P0 | ~200 lines |
| Dynamic array | `List[T]` | `std::collections::list` | None | — | — |
| Hash map | `HashMap[K,V]` | `std::collections::hashmap` | None | — | — |
| Ordered map | `BTreeMap[K,V]` | `std::collections::btreemap` | None | — | — |
| Bit set | `BitSet` | `core::data::bitset` | None | — | — |
| Priority queue | `BinaryHeap` + `MinHeap` | `std::collections::binary_heap` | None | — | — |
| Deque | `Deque[T]` | `std::collections::deque` | None | — | — |
| Trie | `Trie[V]` | `std::collections::trie` | None | — | — |
| Interval tree | `IntervalTree[V]` | `std::collections::interval_tree` | None | — | — |
| Graph/CFG | Adjacency list + algorithms | Not a dedicated type | **No stdlib support** | P1 | ~800 lines |
| Ring buffer | `RingBuffer[T]` | `core::data::ringbuf` | None | — | — |
| Arena allocator | `Arena` | `core::data::arena` | None | — | — |
| Object pool | `Pool[T]` | `core::data::pool` | None | — | — |
| Heap boxing (recursive types) | `Heap[T]` | `core::alloc::heap` | None | — | — |
| Reference counting | `Shared[T]`, `Arc[T]` | `core::alloc::shared`, `std::sync::Arc` | None | — | — |
| Interior mutability | `Cell[T]`, `RefCell[T]` | `core::cell` | None | — | — |
| File read/write | `File` | `std::file` | None | — | — |
| Path manipulation | `Path` | `std::file::path` | None | — | — |
| Directory iteration | `Dir::list()` | `std::file::dir` | None | — | — |
| Atomic file write | write+rename | Not in stdlib | **Minor gap** | P2 | ~30 lines |
| Memory-mapped files | `MmapFile` | Not present | Missing | P2 | ~150 lines + C FFI |
| Child process | `Command` | `std::os::subprocess` | None | — | — |
| Environment variables | `env::get_var` | `std::env` | None | — | — |
| CLI arg parsing | flags + positionals | `std::cli` | None | — | — |
| Threads | `thread::spawn` | `std::thread` | None | — | — |
| Mutex/RwLock | `Mutex[T]`, `RwLock[T]` | `std::sync` | None | — | — |
| MPSC channels | `Sender/Receiver` | `std::sync::mpsc` | None | — | — |
| Atomics | `AtomicI64`, etc. | `std::sync::atomic` | None | — | — |
| Hashing (`Hash` behavior) | `Hash` for all keys | `core::traits::hash` | None | — | — |
| Non-crypto hashing | `FNV`, `xxHash` | `std::hash` (partial) | Partial | P1 | ~100 lines |
| JSON | parse/emit | `std::json` | None | — | — |
| Binary serialization | byte-level encode/decode | `std::collections::buffer` (primitives only) | No schema system | P1 | ~400 lines |
| `Display`/`Debug` formatting | behavior impls | `core::fmt` | None | — | — |
| Structured diagnostics | spans, labels, suggestions | Not present | **Must build** | P0 | ~500 lines |
| Terminal color output | ANSI codes | Not in stdlib | Minor gap | P2 | ~80 lines |
| Semver parsing | version comparison | `std::semver` | None | — | — |
| Regex | pattern matching | `std::regex` | None | — | — |

**P0** = blocker for self-hosting  
**P1** = significant pain without it, workaround exists  
**P2** = minor convenience gap

---

## Section 4: What Must Be Built

### 4.1 String Interner (P0 — ~200 lines)

**What it is**: A `StringInterner` type that stores unique strings in an arena and returns `InternedStr` handles (essentially a `U32` index into the pool). Two interned strings with the same content have equal handles, so equality is O(1) pointer comparison rather than O(n) byte comparison.

**Why it matters**: The type checker creates thousands of string comparisons per second. In the C++ compiler, all identifiers are interned using `llvm::StringRef` + `llvm::StringMap`. Without interning, a TML type checker using `Str` comparisons would be significantly slower due to heap allocation of each identifier copy.

**How to build it in TML**:
```tml
type StringInterner {
    // Backing store: one large Arena that grows as needed
    arena: Arena,
    // Index: map from string content to intern ID
    table: HashMap[Str, I32],
    // Reverse map: ID to string slice into arena
    strings: List[Str],
}

type InternedStr {
    id: I32,
}

impl StringInterner {
    pub func new() -> StringInterner { ... }
    pub func intern(mut this, s: Str) -> InternedStr { ... }
    pub func resolve(this, key: InternedStr) -> Str { ... }
}
```

The `intern` method allocates `s` into the arena on first encounter, stores the arena-allocated `Str` in `strings`, records the mapping in `table`, and returns the index. Subsequent calls for the same string find it in `table` and return the existing index without allocating.

**Estimated size**: 180–220 lines of TML. No C dependencies needed.

### 4.2 CFG / Graph Library (P1 — ~800 lines)

**What it is**: Control-flow graph representation for MIR (mid-level intermediate representation). The MIR layer needs:

- `BasicBlock` nodes with `List[Instruction]` and a `Terminator`
- `CfgGraph` with adjacency (predecessor/successor lists)
- Post-order and reverse post-order traversal iterators
- Dominator tree computation (Lengauer-Tarjan algorithm)
- Strongly-connected-component detection (Tarjan's / Kosaraju's)
- Loop detection and natural loop identification

**Why it matters**: The MIR passes (mem2reg, dead code elimination, loop invariant motion) all require CFG algorithms. The C++ compiler has 31,719 lines in `compiler/src/mir/` partly because it includes a hand-rolled CFG library. When porting MIR to TML, these algorithms need TML implementations.

**Workaround**: Use `HashMap[I64, List[I64]]` for adjacency and write traversals inline in each pass. This works but duplicates traversal logic across all passes. A library approach is cleaner.

**Estimated size**: 600–900 lines of TML using `HashMap` + `List` internally.

### 4.3 Structured Diagnostics Library (P0 — ~500 lines)

**What it is**: A `Diagnostic` type with:
- `Span { file_id: I64, start: I64, end: I64 }` — points into source
- `Label { span: Span, message: Str, style: LabelStyle }` — annotates a span
- `Diagnostic { level: Level, message: Str, labels: List[Label], notes: List[Str] }`
- A `DiagnosticSink` that collects all diagnostics for a compilation unit
- A `DiagnosticRenderer` that outputs them in the `rustc`-style terminal format

**Why it matters**: Quality diagnostics are non-negotiable for a production compiler. The C++ compiler's diagnostic layer (`compiler/src/cli/diagnostic.cpp`, 1,040 lines) is one of the most user-visible subsystems.

**Estimated size**: 400–600 lines of TML. Rendering uses `Text` for building the output string and `core::fmt` for number formatting.

### 4.4 Binary Serialization Module (P1 — ~400 lines)

**What it is**: A schema-based binary encode/decode layer for `.rlib` (precompiled module) format:
- `BinaryEncoder` / `BinaryDecoder` operating on `Buffer`
- Variable-length integer encoding (LEB128)
- String table encoding (length-prefixed or null-terminated)
- Type descriptor serialization

**Workaround**: Use JSON for `.rlib` format (slower and larger but correct). The existing `std::json` module handles this. Switch to binary later.

**Estimated size**: 300–500 lines of TML using `Buffer` primitives. No C dependencies needed.

### 4.5 Non-Cryptographic Hash Functions (P1 — ~100 lines)

**What it is**: Implementations of `FNV-1a` and/or `xxHash64` as standalone functions, exposed as `Hasher` implementations so they can be plugged into `HashMap` via the `Hash` behavior.

**Why it matters**: The default `HashMap` uses whatever hash algorithm is hardcoded in its implementation. For maximum symbol-table performance, FNV-1a (fastest for short strings like identifiers) or xxHash64 (fastest for longer data) should be selectable.

**Estimated size**: 80–120 lines of TML. No C dependencies needed.

### 4.6 Atomic File Write Helper (P2 — ~30 lines)

**What it is**: A single function:
```tml
func write_file_atomic(path: Str, content: Str) -> Outcome[Unit, Str] {
    let tmp = path + ".tmp"
    File::write_all(tmp, content)!
    path_rename(tmp, path)!
    return Ok(())
}
```

This ensures the compiler never writes a partially-written output file if it crashes mid-write. All the pieces exist in `std::file` already — this is just a missing convenience wrapper.

---

## Section 5: Readiness Score

### 5.1 By Category

| Category | Weight | Score | Weighted |
|----------|--------|-------|----------|
| String handling (Str, Text, Unicode) | 15% | 97% | 14.6% |
| Collections (all data structures) | 20% | 94% | 18.8% |
| Memory management | 10% | 100% | 10.0% |
| File I/O | 10% | 88% | 8.8% |
| Process/Environment | 8% | 95% | 7.6% |
| Concurrency | 10% | 100% | 10.0% |
| Hashing | 5% | 80% | 4.0% |
| Error handling (Outcome, Maybe, !) | 5% | 100% | 5.0% |
| Formatting/Display/Debug | 7% | 85% | 6.0% |
| Serialization | 5% | 55% | 2.8% |
| Diagnostics infrastructure | 5% | 20% | 1.0% |

**Total weighted readiness: 88.6%**

### 5.2 Interpretation

The TML stdlib is approximately **89% ready** for a compiler self-hosting effort. This is remarkably high for a language of this age. The three P0 gaps are:

1. **String interning** — the most important missing piece for type-checker performance. Without it, the self-hosted type checker would use `HashMap[Str, T]` which heap-allocates every key comparison. Workable but slower. ~200 lines to fix.

2. **Structured diagnostics** — the most important missing piece for user-facing quality. The compiler would emit plain `print("error: ...")` messages without spans or labels. Functional but poor UX. ~500 lines to fix.

The remaining P1 gaps (graph/CFG library, binary serialization, non-crypto hash) are real but have workable fallbacks using existing stdlib primitives. They represent approximately 1,300 additional lines of TML that should be built during the MIR porting phase.

### 5.3 What This Means in Practice

To begin self-hosting work **today**, a developer could:

1. Use `HashMap[Str, I32]` instead of a proper interner — accept some performance overhead during the bootstrap phase
2. Use `print(...)` for diagnostics — accept reduced error quality during bootstrap
3. Use `HashMap[I64, List[I64]]` for CFG representation — write traversal algorithms inline in each pass
4. Use JSON instead of binary for `.rlib` format — accept slower module loading

These workarounds would produce a **correct but slower and less polished** self-hosted compiler that could nonetheless compile real TML programs, including eventually itself. After bootstrap is achieved, the polish work (interning, diagnostics, binary rlib) can be added with the self-hosted compiler.

This matches the strategy used by Go (bootstrap with a slow interpreter, then optimize), Rust (bootstrap with OCaml, then optimize), and Zig (bootstrap with C, then self-hosted).

### 5.4 Files Summary

| Layer | Source files | Estimated .tml equivalent |
|-------|-------------|--------------------------|
| `lib/core/src/` | 196 .tml files | Already TML |
| `lib/std/src/` | 220 .tml files (excl. runtime) | Already TML |
| Missing: StringInterner | 0 | ~200 lines |
| Missing: CFG/Graph library | 0 | ~800 lines |
| Missing: Diagnostics renderer | 0 | ~500 lines |
| Missing: Binary serialization | 0 | ~400 lines |
| **Total to build** | **4 modules** | **~1,900 lines** |

These 1,900 lines of new TML code are the delta between "stdlib as it exists today" and "stdlib sufficient for a production-quality self-hosted compiler." The current stdlib already contains over 416 source files across core and std, so this represents a ~0.5% increase in stdlib size to unlock self-hosting.
