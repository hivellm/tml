# TML Compiler (C++ Bootstrap)

The TML bootstrap compiler, written in C++20. This is the first-stage compiler that compiles TML source code — including the self-hosting `compiler-tml` — to native binaries via LLVM.

## Building

### Requirements

- CMake 3.20+
- Zig CC (preferred, ADR-007) or C++20 compiler (GCC 12+, Clang 15+, MSVC 19.30+)
- LLVM 17+ (linked as ~55 static libs, in-process)

### Build Steps

```bash
# From project root — NEVER run cmake directly
scripts/build.bat              # Windows - Debug build
scripts/build.bat release      # Windows - Release build
scripts/build.bat --no-tests   # Skip tests
```

### Build Outputs

| Artifact | Size | Description |
|----------|------|-------------|
| `tml.exe` | — | Main compiler binary |
| `tml_compiler.dll` | ~104 MB | Compiler core: lexer → parser → types → HIR → MIR → LLVM IR |
| `tml_codegen_x86.dll` | ~78 MB | Backend: LLVM IR → object code + LLD linker |

## Usage

```bash
# Core commands
tml check file.tml             # Type-check without compiling
tml build file.tml             # Compile to executable
tml run file.tml               # Compile and run
tml test                       # Run test suite
tml test --suite=core          # Run specific test suite

# Stage override (TML parser is default since v0.2.15)
tml build file.tml --stage=parser:cpp   # Use C++ parser fallback

# Debug / diagnostics
tml debug lex file.tml         # Show tokens
tml debug parse file.tml       # Show AST
tml debug check file.tml       # Show type info
tml emit-ir file.tml           # Emit LLVM IR
tml emit-mir file.tml          # Emit MIR

# Code coverage
tml cv                         # Project-scoped test coverage

# Daemon (22ms cached builds)
tml daemon start
tml daemon stop
tml daemon status
```

## Architecture

```
compiler/
├── include/                # C++ headers
│   ├── lexer/              # Tokenizer
│   ├── parser/             # Parser (AST, OOP constructs)
│   ├── types/              # Type system
│   ├── borrow/             # Borrow checker
│   ├── hir/                # High-level IR
│   ├── mir/                # Mid-level IR (SSA)
│   ├── codegen/            # LLVM codegen
│   └── query/              # Query system (demand-driven, incremental)
├── src/
│   ├── lexer/              # Tokenizer implementation
│   ├── parser/             # AST generation (LL(1), ADR-008)
│   ├── preprocessor/       # Conditional compilation
│   ├── types/              # Type checker with module system
│   ├── borrow/             # Borrow checker
│   ├── hir/                # HIR generation
│   ├── thir/               # Typed HIR (THIR→MIR is the single path, T5)
│   ├── mir/                # MIR passes (devirt, escape analysis, DCE, etc.)
│   ├── codegen/
│   │   ├── llvm/           # AST→LLVM IR (legacy path)
│   │   └── mir/            # MIR→LLVM IR (primary path)
│   ├── query/              # Demand-driven query system (8 stages)
│   ├── backend/            # LLVM backend + embedded LLD linker
│   ├── cli/
│   │   ├── commands/       # CLI commands (build, test, run, check, cv, etc.)
│   │   ├── builder/        # Build system
│   │   └── tester/         # Test runner (parallel, NDJSON protocol)
│   ├── pipeline/           # Compilation pipeline orchestration
│   ├── serial/             # Binary serialization (.tml.meta)
│   └── main.cpp            # Entry point
├── tests/                  # TML integration tests (~234 test files)
└── runtime/                # C runtime library (essential.c)
```

## Compilation Pipeline

```
Source → Lexer → Parser → AST → Type Checker → HIR → THIR → MIR → LLVM IR → Object → Executable
                  ↑                                     ↑
            TML parser (default)              Single path (T5)
            C++ parser (--stage=parser:cpp)
```

Key architectural decisions:

- **THIR→MIR single path** (ADR-005, T5): legacy HIR→MIR removed; all MIR fixes go in `thir_mir_builder.cpp`
- **LL(1) grammar** (ADR-008): single-token lookahead, no backtracking
- **Demand-driven queries** (ADR-002): like rustc's `TyCtxt`, 8 memoized stages with red-green incremental
- **In-process LLVM** (ADR-001): ~55 static libs linked, no clang/lld subprocess
- **Zig CC toolchain** (ADR-007): preferred over MSVC for C/C++ compilation

## Compiler Features

### Language Support
- Types: I8–I128, U8–U128, F32, F64, Bool, Char, Str, RawPtr
- Generics with monomorphization and behavior bounds
- Enums (simple, data variants, tagged unions)
- Pattern matching (`when` expressions with guards)
- Behaviors (traits) with `dyn Behavior` vtable dispatch
- Closures with environment capture
- Async/await
- For-in loops (`to`/`through`), let-else, optional chaining (`?.`)
- `@auto`/`@derive`, `@repr`, `@packed`, `@inline` directives
- Module system with `use` declarations and glob re-exports
- C#-style OOP: classes, interfaces, inheritance, virtual dispatch, `@value`/`@pool`

### Optimization Passes (MIR)
- Devirtualization (final methods, sealed classes)
- Dead code elimination (DCE)
- Dead function elimination (DFE)
- Constant folding and propagation
- Copy propagation
- mem2reg (alloca → SSA)
- SROA (scalar replacement of aggregates)
- Escape analysis (stack promotion)
- Loop-invariant code motion (LICM)
- Block merging and CFG simplification
- Unreachable code elimination (UCE)
- Tail call optimization
- Return value optimization (RVO)
- Instruction simplification
- Inlining
- Peephole optimizations

### Codegen Optimizations
- LLVM `select` for scalar if-else (CMOV, phase0h)
- LLVM `switch` for dense integer `when` (phase0d)
- Short-circuit `and`/`or` with 2-block phi layout (phase0f)
- `@inline` → LLVM `alwaysinline` propagation (phase0e)
- Bool `i1→i8` promotion for struct fields

### Infrastructure
- Parallel test execution (NDJSON subprocess protocol, ADR-004)
- Code coverage instrumentation (`tml cv`)
- Debug info (DWARF)
- Incremental compilation with cross-session persistence (red-green)
- Compilation daemon (22ms cached builds)

## Test Status

```bash
# TML integration tests
tml test                                   # Full suite
tml test --suite=compiler                  # Compiler-specific tests
tml test --suite=core                      # Core library tests
tml test --suite=std                       # Standard library tests

# Specific file
tml test compiler/tests/compiler/select_if_else.test.tml
```

## License

Apache-2.0
