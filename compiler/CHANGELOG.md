# Changelog — TML Compiler

All notable changes to the TML compiler will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] — 2026-03-19

### Added

- **Architecture Knowledge System** (2026-03-19) — Persistent architectural context for cross-subsystem awareness
  - Architecture map, cross-subsystem checklist, compiler pipeline skill, stdlib architecture skill
  - 9 agents updated with skill pre-loading

- **Language Reference** (2026-03-19) — Complete `docs/readme.md` indexing all language features and library modules

- **Claude Code Skills — 19 Slash Commands** (2026-02-18) — Project-level skills in `.claude/skills/`
  - Workflow: `/commit`, `/test`, `/build`, `/coverage`, `/slow-tests`, `/review-pr`
  - MCP wrappers: `/compile`, `/run`, `/check`, `/emit-ir`, `/emit-mir`, `/format`, `/lint`, `/docs`, `/explain`, `/structure`, `/affected-tests`, `/artifacts`, `/cache-invalidate`

- **MCP Tools Enhancement — 20 Tools** (2026-02-11) — Complete project-level MCP tool suite
  - `project/build`, `project/coverage`, `project/structure`, `project/affected-tests`, `project/artifacts`
  - `explain`, `test` enhancements (structured output), ANSI stripping, doc search enhancements

- **MCP Hybrid Documentation Search** (2026-02-11) — BM25 + HNSW vector retrieval with TF-IDF embeddings
  - Reciprocal Rank Fusion (RRF), query expansion with TML synonyms, MMR diversification
  - Index persistence to `build/debug/.doc-index/` (~15MB), 3-9ms query latency
  - SIMD-accelerated distance functions (AVX2/SSE4.1)

- **THIR Layer + Advanced Trait Solver** (2026-02-10) — Typed High-level IR between HIR and MIR
  - Numeric coercion insertion, method resolution via TraitSolver, operator desugaring
  - Pattern exhaustiveness checking, associated type normalization
  - THIR→MIR builder enabled by default; `--no-thir` falls back to legacy

- **Query-Based Build Pipeline** (2026-02-09) — Demand-driven compilation (like rustc's `TyCtxt`)
  - 8 core queries: read_source → tokenize → parse → typecheck → borrowcheck → hir → mir → codegen
  - Thread-safe QueryCache, DependencyTracker with cycle detection, 128-bit CRC32C fingerprinting

- **Red-Green Incremental Compilation** (2026-02-09) — Cross-session persistence for near-instant rebuilds
  - Binary cache format in `.incr-cache/incr.bin`, IR cached per compilation unit
  - GREEN path skips entire pipeline; RED path recomputes only affected queries

- **Polonius Borrow Checker** (2026-02-09) — Alternative Datalog-style constraint solver (`--polonius` flag)
  - Strictly more permissive than NLL, accepts conditional borrows across branches
  - Location-insensitive pre-check for O(n) fast path

- **Cranelift Backend (Experimental)** (2026-02-09) — Alternative codegen backend (`--backend=cranelift`)

- **Embedded LLD Linker** (2026-02-09) — In-process linking via `lld::lldMain()`, zero subprocess spawning

- **Embedded LLVM Backend** (2026-02-09) — IR-to-object compilation via LLVM C API, zero disk I/O
  - Full test suite dropped from ~15 minutes to ~17 seconds (50x+ improvement)

- **Comprehensive @derive Macro System** (2026-02-05) — Automatic trait implementation
  - `@derive(PartialEq, Duplicate, Hash, Default, PartialOrd, Ord, Debug)` for structs and enums

- **Reflection: Enum Methods** (2026-02-04) — `variant_name()` and `variant_tag()` for `@derive(Reflect)` enums

- **Build Script Enforcement** (2026-02-04) — CMake direct builds blocked via TML_BUILD_TOKEN verification

- **Struct Update Syntax** (2026-02-01) — `Point { x: 5, ..base_struct }` copies fields from base

- **Loop Variable Declaration** (2026-01-21) — `loop (var i: I32 < N) { ... }` inline variable declaration

- **Array Bounds Check Elimination** (2026-01-20) — Zero-cost safety for constant indices and loop induction variables

- **Compile-Time String Literal Concatenation** (2026-01-20) — `"Hello" + " " + "World"` → static constant

- **String Concat Chain Fusion** (2026-01-20) — Chains like `a + b + c + d` fused into single allocation

- **Inline String Concat Codegen** (2026-01-20) — Inline LLVM IR with `llvm.memcpy` for 2-4 string concat

- **MIR Codegen Modularization** (2026-01-20) — Split `mir_codegen.cpp` into helpers, types, terminators, instructions

- **Text Type with Template Literals** (2026-01-15) — `` `Hello, {name}!` `` produces `Text` type

- **Implicit Numeric Literal Coercion** (2026-01-15) — `var a: U8 = 128` works without `as U8` cast

- **Self-Contained Compiler** (2026-01-19) — No external tool dependencies (LLVM + LLD embedded)

- **`__FILE__`, `__DIRNAME__`, `__LINE__` Constants** (2026-02-17) — Compile-time expansion for script-relative paths

- **Native SSE2 Intrinsics** (2026-02-24) — `sse2_cmpeq_epi8`, `sse2_movemask_epi8`, `simd_splat`, `simd_load_ptr`, `cttz`

### Changed

- **Zig CC as Primary Compiler** (2026-03-12) — Default C/C++ compiler for building TML
  - Auto-detected when `zig` and `ninja` are in PATH; falls back to MSVC or Clang
  - CMake toolchain file, wrapper scripts for unsupported linker args

- **Phase 30: Dead C File Cleanup** (2026-02-19) — Deleted 6 dead C runtime files (2,661 lines)

- **Phase 29.1: Dead FuncSig Removal** (2026-02-19) — Removed 29 dead string FuncSig entries

- **Phase 28: On-Demand Runtime Declarations** (2026-02-19) — Conditional declare emission based on imports
  - ~25 fewer declares for typical programs (287→122→97 for simple programs)

- **Phase 27: Float Math → LLVM IR** (2026-02-19) — `isnan`/`isinf`/`isfinite` → native `fcmp` instructions

- **Phase 26: Dead C File Removal** (2026-02-19) — Removed `text.c`, `thread.c`, `async.c` from build

- **Phase 25: Time Builtins → @extern FFI** (2026-02-19) — `sleep`, `monotonic_now`, `system_time` migrated

- **Phase 24: Sync/Threading → @extern FFI** (2026-02-19) — Removed 23 hardcoded declares, -575 lines C++

- **Phase 24b: String.c Dead Code** (2026-02-19) — string.c reduced from 1,202 to ~490 lines (59% reduction)

- **Phase 23: Float Math → LLVM Intrinsics** (2026-02-19) — 16 float math C calls → `@llvm.*` intrinsics

- **Phase 18.2: Char-to-String Migration** (2026-02-19) — 4 char C calls → pure TML

- **Runtime Modularization** (2026-01-20) — Split `essential.c` into `sync.c`, `pool.c`, `collections.c`

### Fixed

- **dyn Behavior Trait Object Dispatch** (2026-03-19) — Invalid IR (undefined `%v1`) in dyn dispatch

- **sret Convention for Indirect Calls** (2026-03-19) — Struct return via indirect calls now uses sret

- **Bool/i1 in Structs Through fn Pointers** (2026-03-19) — SEGFAULT from i1 struct field layout

- **async func + .await Chain** (2026-03-19) — IR type mismatch (i64 vs i32)

- **Object Cache for LLVM Backend** (2026-03-14) — SHA-256 based `.obj` cache skips backend on repeated runs

- **Incremental IR Cache for Test Suites** (2026-03-14) — `test_entry_index` in CodegenUnitKey prevents stale IR

- **MIR Closure/Lambda Codegen** (2026-03-14) — Implemented MIR codegen path for closures and lambdas

- **Coverage Runs to Completion** (2026-03-15) — Fixed 6 codegen/build bugs blocking full coverage run
  - `Ptr[U8]` mangling, incremental cache + `--no-cache`, runtime library discovery
  - Maybe/Outcome double-load, generic struct type_subs remap, phantom generic parameters

- **Test Runtime Archive** (2026-03-13) — Pre-build runtime .obj files into `tml_test_runtime.lib`

- **Pure Hash Runtime Split** (2026-03-13) — Separated FNV-1a/MurmurHash2 from OpenSSL-dependent crypto.c

- **Generic Struct Type Conflicts** (2026-03-14) — Fixed type redefinition in suite merging

- **UBSan Checks Disabled** (2026-03-14) — Disabled false-positive UBSan in Zig CC toolchain

- **Parser Position Restore** (2026-03-04) — `parse_if_expr` consumed newlines without restoring position

- **type_id[T] Hash Collision** (2026-03-06) — Fixed same hash for different types

- **Unsized %struct.T Emission** (2026-03-05) — Generic methods emitting unsized type fixed

- **Closure Param Semantic Type** (2026-03-05) — Inline closure params losing semantic type info

- **Method-Level Generic Instantiation** (2026-03-04) — GlobalModuleCache fallback added

- **DoubleEndedIterator PIMs** (2026-03-03) — Protocol implementation methods and nth_back return type

- **Str::eq/ne Inline Codegen** (2026-03-03) — Inline codegen for string comparison dispatch

- **Atomic Builtins last_expr_type_** (2026-03-03) — Fixed type inference in pool operations

- **Private Enum Constructor** (2026-03-03) — `internal_enums` support for private constructors

- **Generic Default::default() Dispatch** (2026-03-02) — Resolved trait name dispatch in generic impls

- **Maybe::default() and Maybe::eq()** (2026-03-02) — Enabled generic builtin enum methods

- **Array Mutable Method Dispatch** (2026-03-02) — `mut this` dispatch for Array methods

- **When Arm String Codegen** (2026-03-02) — Incorrect string literals in `when` arms

- **5 Compiler Bugs + Coverage Runtime Refactor** (2026-03-01) — Enum I64 payload, pathbuf, BufWriter fixes

- **Sealed Class Codegen** (2026-03-01) — Type codegen, phi predecessors, ABI mismatches

- **Entry-Block Alloca Hoisting** (2026-02-24) — Loop-body allocas hoisted to function entry block
  - Removed `stacksave`/`stackrestore`, enables LLVM `mem2reg` promotion

- **Recursive Enum Support** (2026-02-24) — Enums with `Heap[Self]` for tree/AST data structures
  - Cycle detection, infinite-size detection (T085), enum drop-glue generation

- **Nullable Maybe Pointer Optimization** (2026-02-24) — `Maybe[Heap[T]]` uses null pointer as `Nothing`
  - Pointer-sized (8 bytes) instead of `{ i8, ptr }` (16 bytes)

- **String Concat Memory Leak** (2026-02-24) — Free heap-allocated Str temporaries after concatenation

- **SIMD Field Access on SSA Values** (2026-02-24) — `extractelement` for SSA instead of invalid load

- **Lazy Library Defs in Coverage Mode** (2026-02-24) — Fixed ~2,700 silent test failures

- **`when` Codegen: Arm Bindings** (2026-02-23) — Bindings not consumed when used as result

- **`return` Codegen: Args Not Consumed** (2026-02-23) — `return Ok(x)` didn't mark x as consumed

- **Channel/WaitGroup Type Mapping** (2026-02-23) — Opaque ptr → proper struct types

- **Double-Free in Tail Expressions** (2026-02-23) — Tail expressions now mark returned vars consumed

- **`@tml_Str_len` Undefined in Suite Mode** (2026-02-19) — Premature `generated_functions_` insertion

- **Shared[T] Memory Leak** (2026-02-17) — Broken increment/decrement codegen for library generics

- **Pointer-to-I32 Truncation Warning** (2026-02-16) — Compiler warns on `ptr as I32` truncation

- **VEH Crash Handler longjmp** (2026-02-16) — Changed to `EXCEPTION_CONTINUE_SEARCH`

- **Bool ABI and Ternary Alloca** (2026-02-10) — Correct i1/i8 representation across boundaries

- **Float Comparison in Assert Builtins** (2026-02-10) — `icmp` → `fcmp` for float types

- **Integer Literal Type Inference** (2026-02-10) — Unsuffixed literals infer type from context

- **Dynamic Impl Resolution for Primitives** (2026-02-10) — Method calls on primitive types

- **BoolLiteral Dangling Lexeme** (2026-02-05) — Corrupted boolean constants from imported modules

- **Ref Parameter Passing** (2026-02-04) — Stack slot address vs pointer value for ref params

- **MIR Codegen Class Type Resolution** (2026-01-15) — Class instance method calls with -O3

- **Memory Pointer Type Consistency** (2026-01-14) — Unified pointer types (`*Unit`)

- **ptr_offset for Opaque Pointers** (2026-01-14) — Fixed `getelementptr void` for `*Unit`

### Performance

- **O0 Optimization Pipeline** (2026-02-24) — Complete overhaul of `-O0` LLVM pass pipeline
  - SROA, Mem2Reg, EarlyCSE, Inlining, DestinationProp, UnreachableProp
  - Core string operations replaced with `copy_nonoverlapping` and `memchr`/`memcmp`

- **SSA Codegen Improvements** (2026-02-24) — `insertvalue`/`extractvalue` for struct construction/access
  - Parameter allocas eliminated, `inbounds` flag on all GEP instructions

- **Int-to-String Conversion** (2026-01-20) — 9.6x faster via lookup table (134ns → 10ns)

- **Text Builder push_i64** (2026-01-20) — 1.6x faster log building via inline integer append

## [0.1.0] — 2025-12-22

Initial release of the TML compiler.

### Added
- Lexer, parser, type checker, borrow checker
- HIR and MIR intermediate representations
- LLVM codegen backend
- Basic test runner
- CLI with `build`, `run`, `test` commands
