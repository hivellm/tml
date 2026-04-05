# 5. Compiler Architecture

TML's compiler is implemented in C++ and targets native code generation through embedded LLVM. Its design draws from lessons learned in modern systems language compilers — particularly rustc — while making distinct choices suited to TML's goals of LLM-friendliness and incremental self-hosting.

---

## 5.1 Query-Based Demand-Driven Compilation

### 5.1.1 Motivation

Traditional compilers organize work as batch passes: lex all source, parse all tokens, type-check all declarations, and so on. This model is simple but forces the compiler to re-execute every pass for the entire compilation unit when any single function changes.

TML addresses this with a **demand-driven query system** inspired by rustc's `TyCtxt` infrastructure. Rather than executing phases in a fixed sequence, the compiler exposes each phase as a named, memoized query. A query is only executed when its result is demanded. Results are cached in memory and persisted to disk across sessions.

### 5.1.2 Query System Design

The `QueryContext` is the central orchestrator:

```
QueryContext::force(QueryKey) -> cached result OR compute
```

Available queries form a DAG (Directed Acyclic Graph):

```
ReadSource -> Tokenize -> ParseModule -> Typecheck
           -> Borrowcheck -> HirLower -> ThirLower
           -> MirBuild -> MirOptimize -> CodegenUnit
```

Each query:
1. Checks if a cached result exists with a valid fingerprint.
2. If valid, returns the cached result (GREEN).
3. If the input fingerprint changed, re-executes and compares output (YELLOW if output unchanged, RED if changed).
4. Downstream queries are only re-executed if their input query produced a RED result.

### 5.1.3 Incremental Compilation

Fingerprint-based caching extends across compilation sessions via `.incr-cache/incr.bin`. When a source file changes:

1. `ReadSource` query detects the content change (RED).
2. `Tokenize` and `ParseModule` re-execute (may be RED or YELLOW depending on whether the change affects parsed structure).
3. `Typecheck` re-executes only if the parse tree changed.
4. Functions whose types didn't change get YELLOW — their downstream codegen is skipped.

This typically reduces incremental rebuild time from ~100s (full build) to 5-15s (changed function only).

### 5.1.4 Comparison with Other Approaches

| Compiler | Compilation Model | Incremental? | Cache Persistence |
|----------|------------------|-------------|-------------------|
| TML | Query-based, demand-driven | Yes (fingerprints) | `.incr-cache/incr.bin` |
| Rust (rustc) | Query-based, demand-driven | Yes (fingerprints) | Per-crate incremental cache |
| Clang | Batch passes | No (relies on build system) | None (precompiled headers partial) |
| GCC | Batch passes | No (relies on build system) | None |
| Go | Single-pass per package | Partial (package-level) | Build cache |
| Zig | Incremental, self-hosted | Yes (fine-grained) | In-memory + disk |

TML and Rust share the same architectural approach. The key advantage over batch compilers (Clang, GCC) is that a change to one function does not require re-type-checking unrelated functions. The advantage over Go is finer granularity — Go caches at the package level, while TML caches at the function/query level.

---

## 5.2 Compilation Pipeline

The full compilation pipeline consists of nine phases:

```
Source (.tml)
    |
    v
[1] LEXER (lexer.cpp)
    Token stream: identifiers, keywords, literals, operators
    Approach: Hand-written, single-pass
    |
    v
[2] PARSER (parser.cpp, parser_expr.cpp, parser_decl.cpp)
    AST: Module with declarations, expressions, patterns
    Approach: Recursive descent for declarations, Pratt parser for expressions
    |
    v
[3] TYPE CHECKER (checker.cpp, checker_*.cpp)
    TypeEnv: Symbol table with resolved types
    Approach: Hindley-Milner inference, 4 phases
    Phase 1: Register all type/function signatures
    Phase 2: Resolve imports and foreign symbols
    Phase 3: Bind behavior implementations
    Phase 4: Check function bodies with full inference
    |
    v
[4] BORROW CHECKER (borrow/checker.cpp)
    Validation: Ownership and lifetime correctness
    Approach: NLL (Non-Lexical Lifetimes), place-based tracking
    |
    v
[5] HIR LOWERING (hir/hir_builder.cpp)
    HirModule: Typed, desugared, monomorphized
    Transforms: Type resolution, sugar expansion, generic instantiation,
                closure capture analysis, field/variant index resolution
    |
    v
[6] THIR LOWERING (thir/thir_lower.cpp)
    ThirModule: Coercions inserted, methods resolved
    Transforms: Implicit coercion insertion, operator desugaring,
                method resolution via trait solver, exhaustiveness checking
    |
    v
[7] MIR BUILDING (mir/hir_mir_builder.cpp OR mir/thir_mir_builder.cpp)
    mir::Module: SSA form with basic blocks
    Two parallel paths: HIR->MIR (legacy) and THIR->MIR (new)
    |
    v
[8] MIR OPTIMIZATION (mir/mir_pass.cpp, mir/passes/*.cpp)
    mir::Module (optimized): 52 passes applied
    Critical: mem2reg, dead code elimination, inlining, constant folding
    |
    v
[9] CODEGEN (codegen/mir_codegen.cpp)
    LLVM IR text: Generated from optimized MIR
    |
    v
[10] LLVM BACKEND (backend/llvm_backend.cpp)
     .obj file: Native object code
     |
     v
[11] LLD LINKER (backend/lld_linker.cpp)
     .exe: Final executable linked with C runtime
```

### 5.2.1 Pratt Parser for Expressions

TML uses a Pratt parser (top-down operator precedence) for expression parsing. This approach elegantly handles:

- Operator precedence without explicit precedence tables in the grammar.
- Prefix, infix, and postfix operators.
- Right-associative operators (exponentiation `**`).
- Call expressions, index expressions, and field access as postfix operators.

The Pratt parser is combined with recursive descent for declarations (`func`, `type`, `enum`, `behavior`, `impl`), where the regular structure of declaration syntax makes recursive descent more natural.

### 5.2.2 Four-Phase Type Checking

The type checker's four-phase approach is necessary because TML supports forward references and mutual recursion:

1. **Register**: All type names and function signatures enter the environment. No bodies are checked.
2. **Imports**: Foreign symbols are linked. At this point, all names in scope are known.
3. **Implementations**: `impl Behavior for Type` blocks are registered. Coherence (orphan rules) is checked.
4. **Bodies**: Function bodies are type-checked with full inference, using the complete type environment.

This phased approach is similar to Rust's resolution strategy and differs from Go's single-pass approach (which requires declaration before use within a file, though not across files in a package).

---

## 5.3 Embedded LLVM and LLD

TML embeds LLVM 19+ and LLD (the LLVM linker) directly into the compiler binary. This is an architectural choice with significant implications:

### 5.3.1 Advantages

1. **No external toolchain**: The TML compiler is a single binary. No separate installation of Clang, LLVM, or a system linker is required.
2. **In-memory IR processing**: LLVM IR is generated as text, parsed in-memory, and compiled to object code without writing temporary files.
3. **In-process linking**: LLD links object files in-process, eliminating the overhead of spawning a linker subprocess.
4. **Deterministic output**: The LLVM version is pinned, ensuring identical codegen across environments.
5. **Faster compilation**: Eliminating process creation and file I/O saves 100-500ms per compilation unit.

### 5.3.2 Disadvantages

1. **Large binary**: The compiler binary is ~100MB (debug) because it includes the full LLVM library.
2. **LLVM version coupling**: Upgrading LLVM requires rebuilding the compiler.
3. **Memory usage**: LLVM's in-process memory usage adds to the compiler's footprint.

### 5.3.3 Comparison

| Compiler | LLVM Integration | Linker | Binary Size |
|----------|-----------------|--------|-------------|
| TML | Embedded (in-process) | Embedded LLD | ~100MB |
| Rust | Embedded (in-process) | System or LLD | ~50MB (rustc alone) |
| Clang | IS LLVM | System or LLD | ~100MB |
| Go | Custom backend | Custom linker | ~20MB |
| Zig | Embedded LLVM + custom | Embedded LLD | ~150MB |

---

## 5.4 Dual MIR Building Paths

TML maintains two parallel paths for converting high-level IR to MIR:

**Path A — HIR to MIR (legacy):**
- Files: `hir_mir_builder.cpp`, `builder/hir_expr.cpp`, `builder/hir_expr_control.cpp`
- Input: HirModule (no THIR step)
- Status: Mature, production-ready, handles all language features
- Used with: `--legacy` flag

**Path B — THIR to MIR (new):**
- Files: `thir_mir_builder.cpp`, `thir_mir_builder_expr.cpp`
- Input: ThirModule (after THIR lowering)
- Status: Under development, growing feature coverage
- Used by: Default (when supported)

The dual-path architecture exists for migration safety: Path B can be developed and tested incrementally while Path A continues to serve production compilation. Tests can be run against both paths to verify equivalence.

This mirrors Rust's own history: rustc maintained both AST-based and MIR-based codegen paths during the migration to MIR, with the old path serving as a reference implementation.

---

## 5.5 Build System

TML uses CMake for build configuration with custom build scripts (`scripts/build.bat`) that handle environment setup:

| Mode | Command | Output |
|------|---------|--------|
| Debug (monolithic) | `scripts/build.bat` | `build/debug/bin/tml.exe` (~100MB) |
| Release | `scripts/build.bat release` | `build/release/bin/tml.exe` |
| Clean | `scripts/build.bat --clean` | Fresh build |
| Modular | `scripts/build.bat --modular` | Launcher + `tml_compiler.dll` + `tml_codegen_x86.dll` |

The modular build produces a thin launcher executable that loads compiler functionality from DLLs. This enables:
- Faster incremental rebuilds (only relink changed DLL).
- Plugin architecture for future extension.
- Smaller updates when only one component changes.

The compiler uses Zig CC as the C/C++ compiler (replacing MSVC), providing cross-platform compilation with bundled libc.

---

## 5.6 Comparison with Major Compiler Architectures

### 5.6.1 TML vs rustc

The closest architectural relative. Both use query-based demand-driven compilation, five IR layers (AST, HIR, THIR, MIR, LLVM IR), embedded LLVM, and incremental compilation with fingerprinting. TML's implementation is younger and smaller (~100K C++ lines vs rustc's ~500K Rust lines), but architecturally similar.

### 5.6.2 TML vs GCC

GCC uses a traditional batch compilation model with three IR layers (GENERIC, GIMPLE, RTL). It has no query system and relies on the build system (Make) for incremental rebuilds. GCC's strength is its mature optimization pipeline (~300 passes) and broad target support. TML's advantage is finer-grained incrementality and simpler architecture.

### 5.6.3 TML vs Clang

Clang has a shallower pipeline (Clang AST -> LLVM IR, two layers) because C++ has simpler semantic constructs than TML (no ownership, no algebraic types, no behavior system). Clang relies on precompiled headers for compilation speed rather than query-based caching. Both embed LLVM.

### 5.6.4 TML vs Go Compiler

The Go compiler is remarkably simple: a single-pass compiler with a custom SSA backend. It compiles an order of magnitude faster than TML or Rust but produces less optimized code. Go's design prioritizes compilation speed over runtime performance — a valid trade-off for server-side software where developer productivity matters more than CPU efficiency.

### 5.6.5 TML vs Zig

Zig is self-hosted with a custom backend that can optionally target LLVM. Its incremental compilation is more fine-grained than TML's (instruction-level vs query-level). Zig's unique contribution is comptime (compile-time evaluation) which eliminates the need for a macro system — a philosophy TML shares (TML uses decorators instead of macros).
