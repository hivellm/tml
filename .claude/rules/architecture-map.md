# Architecture Map — Subsystem Boundaries & Data Flow

This rule provides Claude with a persistent architectural map of ALL major subsystems in the TML project. It is auto-loaded in every conversation and every agent. Its purpose is to prevent "context fragmentation" — the loss of inter-subsystem connections when reading multiple files.

## How to Use This Map

**BEFORE editing any file**, find it in this map and check:
1. What subsystem does it belong to?
2. What data flows INTO this subsystem (upstream)?
3. What data flows OUT of this subsystem (downstream)?
4. If I change the output shape, what breaks downstream?

## 1. Compiler Pipeline (Source → Executable)

```
Source (.tml file)
  │
  ▼
LEXER ─────────────────────────────────────────────────────────
  Files: compiler/src/lexer/lexer.cpp
  Types: Source → std::vector<Token>
  Key:   Lexer::next_token(), Lexer::tokenize()
  │
  ▼
PARSER ────────────────────────────────────────────────────────
  Files: compiler/src/parser/parser.cpp, parser_expr.cpp, parser_decl.cpp
  Types: std::vector<Token> → Module (AST)
  Key:   Parser::parse_module()
  Notes: Pratt parser for expressions, recursive descent for declarations
  │
  ▼
TYPE CHECKER ──────────────────────────────────────────────────
  Files: compiler/src/types/checker.cpp, checker_*.cpp (split by concern)
  Types: Module → TypeEnv (symbol table + resolved types)
  Key:   TypeChecker::check_module()
  Notes: Hindley-Milner inference, 4 phases (register → imports → impls → bodies)
  │
  ▼
BORROW CHECKER ────────────────────────────────────────────────
  Files: compiler/src/borrow/checker.cpp
  Types: Module + TypeEnv → validation result
  Key:   BorrowChecker::check_module()
  Notes: NLL (Non-Lexical Lifetimes), place-based tracking
  │
  ▼
HIR LOWERING ──────────────────────────────────────────────────
  Files: compiler/src/hir/hir_builder.cpp, hir_builder_expr.cpp
  Types: Module + TypeEnv → HirModule
  Key:   HirBuilder::lower_module()
  Transforms:
    - Type resolution (all exprs get concrete types)
    - Desugaring (var→let mut, for→iterator, if let→when)
    - Monomorphization (generics → concrete instantiations)
    - Field/variant index resolution
    - Closure capture analysis
  │
  ▼
THIR LOWERING ─────────────────────────────────────────────────
  Files: compiler/src/thir/thir_lower.cpp
  Types: HirModule + TypeEnv + TraitSolver → ThirModule
  Key:   ThirLower::lower_module()
  Transforms:
    - Implicit coercion insertion (CoercionExpr nodes)
    - Method resolution via trait solver
    - Operator desugaring to method calls (a + b → a.add(b))
    - Pattern exhaustiveness checking
    - Associated type normalization
  │
  ▼
MIR BUILDING ──────────────────────────────────────────────────
  TWO PATHS (both active):

  Path A (HIR→MIR, legacy):
    Files: compiler/src/mir/hir_mir_builder.cpp, builder/hir_expr.cpp,
           builder/hir_expr_control.cpp, builder/builder_types.cpp
    Types: HirModule + TypeEnv → mir::Module
    Key:   HirMirBuilder::build()

  Path B (THIR→MIR, new):
    Files: compiler/src/mir/thir_mir_builder.cpp, thir_mir_builder_expr.cpp
    Types: ThirModule → mir::Module
    Key:   ThirMirBuilder::build()

  Output: mir::Module (SSA form with basic blocks, instructions, terminators)
  │
  ▼
MIR PASSES (optimization) ────────────────────────────────────
  Files: compiler/src/mir/mir_pass.cpp, compiler/src/mir/passes/*.cpp
  Types: mir::Module → mir::Module (optimized)
  Key passes: mem2reg (CRITICAL), dead_function_elimination, block_merge
  Full list: 30+ passes in compiler/include/mir/passes/
  │
  ▼
MIR CODEGEN (MIR → LLVM IR text) ─────────────────────────────
  Files: compiler/src/codegen/mir_codegen.cpp,
         compiler/src/codegen/mir/instructions.cpp (emit_call_inst, etc.),
         compiler/src/codegen/mir/instructions_method.cpp,
         compiler/src/codegen/mir/instructions_misc.cpp,
         compiler/src/codegen/mir/mir_types.cpp
  Types: mir::Module → std::string (LLVM IR text)
  Key:   MirCodegen::generate(), MirCodegen::generate_cgu()
  ⚠️ CRITICAL: Method calls in MIR path go through emit_call_inst (NOT MethodCallInst)
  │
  ▼
LLVM BACKEND ──────────────────────────────────────────────────
  Files: compiler/src/backend/llvm_backend.cpp
  Types: LLVM IR text → .obj file
  Key:   LLVMBackend::compile_ir_to_object()
  │
  ▼
LLD LINKER ────────────────────────────────────────────────────
  Files: compiler/src/backend/lld_linker.cpp
  Types: .obj files + runtime → .exe
  Links: runtime C files + system libs → final executable
```

## 2. Query System (Demand-Driven)

All pipeline phases are wrapped in queries for caching/incremental compilation.

```
QueryContext::force(QueryKey) → cached result or compute
  │
  Queries:  ReadSource → Tokenize → ParseModule → Typecheck
            → Borrowcheck → HirLower → ThirLower → MirBuild → CodegenUnit
  │
  Cache:    In-memory (per-session) + .incr-cache/incr.bin (cross-session)
  │
  Fingerprints: input hash + output hash → RED/YELLOW/GREEN coloring
```

Files: `compiler/src/query/query_context.cpp`, `compiler/include/query/query_key.hpp`

## 3. Testing System

```
CLI (tml test) → TestConfig → testing_coordinator
  │
  ├─ discover_tests() → find *.test.tml files
  ├─ group_into_suites() → group by directory
  ├─ compile_suites_parallel() → QueryContext pipeline → .exe per suite
  ├─ Process::launch() → subprocess per suite
  ├─ parse_json_event() → NDJSON protocol (test results streamed)
  └─ print_coverage_report()
```

Files: `compiler/src/testing/testing_coordinator.cpp`, `compiler/include/testing/`
Protocol: NDJSON from subprocess → coordinator
Coverage: `TML_COVERAGE_FILE` env var

## 4. Standard Library Layers

```
lib/core/     ← Foundation (zero external deps)
  │  Types: Heap[T], Shared[T], Sync[T], Maybe[T], Outcome[T,E]
  │  Traits: Iterator, Display, Debug, Clone, Eq, Ord, Hash
  │  Modules: alloc, fmt, iter, slice, ptr, num, char, ops, cell
  │
  ▼
lib/std/      ← Full standard library (depends on core + C runtime)
  │  Collections: List[T], HashMap[K,V], Buffer (C-backed)
  │               ArrayList[T], BTreeMap, HashSet (pure TML)
  │  Sync: Mutex[T], Arc[T], RwLock[T], atomics, MPSC channels
  │  Net: TCP, UDP, TLS, DNS, IOCP (C-backed)
  │  HTTP: server, client, router, middleware (36 files)
  │  IO: File, Stream, buffered readers
  │  Other: JSON, crypto, zlib, regex, search, sqlite, datetime
  │
  ▼
lib/test/     ← Test framework (depends on core + std)
  │  assert_eq, assert_true, property-based, mocking, benchmarks
  │
  ▼
compiler/runtime/  ← C runtime (FFI layer to OS)
     core/essential.c — I/O, panic, test harness (KEEP)
     memory/mem.c — malloc/free wrappers (KEEP)
     collections/ — List, HashMap, Buffer (MIGRATE to TML)
     concurrency/ — sync, async (KEEP — OS interface)
     net/ — sockets, IOCP, DNS, TLS (KEEP — OS interface)
     crypto/ — OpenSSL/BCrypt wrappers (KEEP — FFI)
```

## 5. Build System

```
scripts/build.bat → CMake → tml.exe (monolithic, ~100MB)
                  or        → tml.exe + tml_compiler.dll + tml_codegen_x86.dll (modular)

⚠️ NEVER use cmake directly — build scripts pass required token
⚠️ tml_compiler.dll = ALL compiler code including TML→IR
⚠️ tml_codegen_x86.dll = only IR→obj + LLD linker
⚠️ When fixing codegen bugs, rebuild tml_compiler_plugin NOT tml_codegen_x86_plugin
```

## 6. Cross-Subsystem Impact Rules

**If you change...** → **You MUST also check...**

| Changed | Check | Why |
|---------|-------|-----|
| Type checker (`types/`) | HIR builder, borrow checker | TypeEnv shape affects downstream |
| HIR builder (`hir/`) | Both MIR builders (hir_mir + thir_mir) | HirExpr/HirModule consumed by both |
| THIR lowerer (`thir/`) | thir_mir_builder only | ThirModule consumed by THIR→MIR path |
| MIR types (`mir/mir.hpp`) | mir_pass.cpp, mir_codegen.cpp, mir_printer.cpp | MIR instruction changes affect all consumers |
| MIR codegen (`codegen/mir/`) | Run affected test suites | IR changes affect all compiled output |
| Runtime C files | Rebuild compiler, run full test suite | ABI changes affect all TML code |
| Core library (`lib/core/`) | Std library + all tests | Core types used everywhere |
| Std library (`lib/std/`) | Tests for that module | Module-scoped impact |
| Query system (`query/`) | Everything — full test suite | Query changes affect all compilation |
| Test system (`testing/`) | Run tests to verify test infra itself | Meta: testing the tester |
