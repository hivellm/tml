# Changelog — TML Compiler

All notable changes to the TML compiler will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.23] — 2026-04-15

### Added

- **Persistent compilation daemon** — `tml daemon start/stop/status` with named-pipe IPC (Windows) and Unix domain socket; in-process result cache returns 22ms on cache hit (4.5x faster than `cargo check`); DLL staleness detection auto-restarts daemon on compiler rebuild
- **`match` keyword diagnostic** — parser detects `match` (Rust/Swift/C#) and emits `error[S001]: use 'when' instead` with correct span; skips braced body to suppress cascading errors

### Fixed

- **K001 struct forward-reference** — `llvm_type_name` and `llvm_type_from_semantic` now emit struct definitions on-demand from the module registry when an unknown struct type is referenced; fixes `EventEmitter`/`ReadableStream` undefined type that blocked all `std::file` tests
- **i128 binary ops** — `int_type` determination in `binary_ops.cpp` was missing `i128` check; all U128/I128 comparisons and arithmetic (`==`, `%`, `/`, `<`, `>`, `+`, `-`, `*`) emitted `i32` instructions instead of `i128`
- **Embedded LLD removed** — linking now uses native OS linker via subprocess (`link.exe` on Windows, `ld` on Unix); `tml_codegen_x86.dll` reduced from 78 MB to 58 MB (−26%)
- **PID file location** — daemon PID file moved from project root to system temp directory (`<temp>/tml-daemon-<crc32>.pid`)

## [0.3.1] — 2026-04-12

### Changed
- **for-in loop migration** — replaced 80 manual index loops with `for i in 0 to N` in compiler-tml across serial/ast.tml, serial/typeenv.tml, types/checker, types/infer, types/builtins, serial/mod, serial/reader, source, parser/parse_expr

## [0.3.0] — 2026-04-11

### Added

- **For-in range loops** — `for i in 0 to 10` / `for i in 1 through 5` with MIR codegen (SSA phi nodes, counter alloca + header/body/increment/backedge)
- **Struct update syntax** — `Point { x: 5, ..p1 }` copies unspecified fields from base expression (MIR `extractvalue` + `insertvalue` chain, legacy GEP copy)
- **Struct destructuring in let** — `let Pair { first: a, second: b } = p` with alloca + GEP + load per field
- **Operator overloading** — `a + b` on user-defined structs dispatches to `Add::add(a, b)` behavior method (THIR lowering + MIR CallInst + AST legacy path)
- **@repr(U8/U16/I32/I64) directive** — sequential enum discriminants with specified integer type
- **@auto(duplicate, equal, debug, ...) directive** — alias for `@derive` with lowercase name mapping; all 11 derive codegen files updated
- **@packed directive** — structs emitted with LLVM packed layout `<{ ... }>` (no inter-field padding)
- **Behavior aliases** — `behavior Numeric = Add + Sub + Mul + Div` parsed as `BehaviorAliasDecl`, expanded by type checker
- **Closure type inference** — unannotated closure params inferred from expected function signature via bidirectional unification; implicit return for expression bodies

### Fixed

- **Operator overloading K001 GEP bug** — AST legacy codegen treated all binary ops as primitive arithmetic; added struct-aware operator dispatch in `binary_ops.cpp`
- **Bool struct field layout** — `i1` promoted to `i8` in struct contexts with `trunc`/`zext` on load/store (5 codegen paths fixed)
- **Pattern guards** — verified and tested end-to-end (already implemented but undocumented)
- **Or-patterns** — verified across parser, HIR, THIR, MIR, and exhaustiveness checker

### Tests

- 6 new test files organized into subdirectories:
  - `directives/auto.test.tml`, `directives/repr.test.tml`, `directives/packed.test.tml`
  - `behaviors/operator_overload.test.tml`, `behaviors/behavior_alias.test.tml`
  - `closures/closure_type_inference.test.tml`

## [0.2.5] — 2026-04-06

### Added

- **MIR consolidation (phase12a)** — Single THIR→MIR compilation path, removed legacy HIR→MIR builder for simplified, consistent code generation
- **Type checker invariants documentation** — Formal specification of 176 invariants across 5 sections (~95 pages)
- **Derive mangling unification (RC5)** — Unified `@derive` codegen with TypeInfo registration, stable mangle names across codegen units

### Fixed

- **RC1 (119 failures)** — MODULE_NOT_FOUND http: recursive private_imports tracking in env_module_load.cpp
- **RC2-RC3 (42 failures)** — Method resolution + return type substitution for nested generics
- **RC4 (13 failures)** — LINK failures: std::os glob exports + missing function declarations
- **RC5 (8 failures)** — Derive mangling: lowercase module path (std::sync::arc not Arc), typevar suffix replacement
- **RC6 (10 failures)** — Context/Waker unreservation: allow as type names in user code
- **RC7.1-7.3 (11 failures)** — Enum type param substitution in pattern matching (ThirMirBuilder)
- **RC7.4 (1 failure)** — Maybe::ok_or method added
- **RC9 (1 failure)** — Unit return codegen: `call {}` instead of `call void`
- **Nested generic type param collision** (method_impl.cpp) — Shared[PromiseState[I32]]::get
- **Typevar naming collision** — __T/__U/__K → __I64 workaround for cross-module generics

### Status

- **Compile failures: 214 → ~29 (86% reduction)**
- **Remaining:** phase0h (closure types) and phase0i (behavior FQN keying) track final ~29 failures

## [0.2.4] — 2026-03-28

### Added
- **Pre-condition codegen** — `pre: expr` emits `if (!cond) panic("contract violation: ...")` at function entry
- **Contract type checking** — validates pre/post expressions return Bool in `check_func_body()` (error T090)
- **`impl_count[T]()` / `impl_name[T](index)`** intrinsics for querying behavior implementations
- **Compile-time field index validation** — R001 error for out-of-bounds `field_name/type_id/offset`
- **MCP emit-ir in-process** — `LLVMIRGen::generate()` directly instead of subprocess

### Changed
- Type checker allowlist updated for `impl_count` (I64 return) and `impl_name` (Str return)
- `call_primitive.cpp` handles `impl_count`/`impl_name` via `env_.get_behavior_impls()`

## [0.2.3] — 2026-03-25

### Added

- **`black_box` intrinsic** — inline asm with memory clobber, prevents optimizer from eliminating values (for benchmarks)
- **`spin_loop_hint` intrinsic** — emits x86 PAUSE instruction for busy-wait loops
- **Panic hook support** — `tml_set_panic_hook`, `tml_get_panic_hook` in essential.c; `panic()` now calls user hook before longjmp/exit
- **`tml_catch_unwind_fn`** — general-purpose setjmp/longjmp panic catching with nested jmp_buf save/restore

### Fixed

- **`@derive(Reflect)` size/align** — TypeInfo `size` and `align` fields now computed correctly via LLVM constant expressions (`ptrtoint(getelementptr(..., null, 1))`). Previously hardcoded to 0.

## [0.2.2] — 2026-03-22

### Added

- **Tracy Profiler Integration** — 70+ instrumented zones across the full compiler pipeline
  - Lexer, parser, type checker, HIR/MIR lowering, LLVM backend, MIR pass manager, query cache
  - `--profile` build flag for Tracy-enabled builds
  - Profiler intrinsics (`profiler::begin`/`profiler::end`) in both AST and MIR codegen paths
  - Zero-cost when profiling is disabled — no runtime overhead in production builds

### Fixed

- **Build system** — Tracy `--profile` build improvements, correct library linking

## [0.2.1] — 2026-03-21

### Fixed

- **Pin-through trait method dispatch** — 4 interconnected bugs in type checker, generic inference, method_impl, static dispatch
- **Cross-module generic struct field resolution** — `lookup_struct` follows re-export chains
- **Range struct type declaration** — MIR codegen emits struct types for library structs
- **ptr_read/ptr_write multi-field structs** — 4 fixes across type_params, HIR, MIR, codegen
- **Struct field mutation** — mutable struct alloca dead code in thir_mir_builder
- **Integer literal coercion in fnptr calls** — sext from i32 to i64
- **Iterator::fold[B] monomorphization** — method-level generic dispatch
- **memcpy/memmove/memset MIR handlers** — codegen + LLVM intrinsic declarations
- **copy_nonoverlapping/copy/write_bytes** — type checker registration

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
