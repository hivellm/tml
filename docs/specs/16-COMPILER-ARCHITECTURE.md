# TML v1.0 — Compiler Architecture

## 1. Bootstrap Strategy

### 1.1 Two-Phase Approach

```
Phase 1: Bootstrap Compiler (tmlc-bootstrap)
├── Written in: C++ (cross-platform, mature tooling)
├── Generates: Native executables via LLVM
├── Target: Self-host the native compiler
└── Lifecycle: Frozen after Phase 2 complete

Phase 2: Native Compiler (tmlc)
├── Written in: TML
├── Compiled by: tmlc-bootstrap
├── Generates: Native executables via LLVM (initially)
├── Target: Production use, ongoing development
└── Lifecycle: Actively maintained, extensible via libs
```

### 1.2 Bootstrap Requirements

The bootstrap compiler must implement:

| Feature | Required | Notes |
|---------|----------|-------|
| Lexer | ✓ | Full spec |
| Parser (LL(1)) | ✓ | Full spec |
| Type Checker | ✓ | Full inference |
| Borrow Checker | ✓ | Full rules |
| Effect Checker | ✓ | Full propagation |
| IR Generation | ✓ | S-expression format |
| LLVM Backend | ✓ | x86_64, aarch64, wasm32 |
| Standard Library | ✓ | Core subset only |
| Package Manager | ○ | Basic only |
| Incremental Build | ○ | Optional |

Legend: ✓ = Required, ○ = Optional

### 1.3 Feature Parity

After self-hosting, the native compiler MUST support:

```tml
// These features must work to compile tmlc itself
- Full type system with generics
- Ownership and borrowing
- Effects and capabilities
- Pattern matching
- Error handling with !
- Standard collections (List, Map, Set)
- File I/O
- Basic concurrency
```

## 2. Compiler Pipeline

### 2.1 Phases

The compiler uses a **demand-driven query system** (analogous to rustc's `TyCtxt`).
Each phase is a memoized query; results are cached in-memory and persisted to disk
for incremental compilation across sessions.

```
Source (.tml)
    │
    ▼
┌──────────────────┐
│  Preprocessor    │  → Conditional compilation, #if/#ifdef
│  + ReadSource    │     (Query: read_source)
└──────────────────┘
    │
    ▼
┌──────────────────┐
│     Lexer        │  → Token stream
│                  │     (Query: tokenize)
└──────────────────┘
    │
    ▼
┌──────────────────┐
│     Parser       │  → AST (untyped)
│                  │     (Query: parse_module)
└──────────────────┘
    │
    ▼
┌──────────────────┐
│   Type Check     │  → TAST (typed AST)
│   + Resolver     │     (Query: typecheck_module)
└──────────────────┘
    │
    ▼
┌──────────────────┐
│  Borrow Check    │  → TAST (ownership verified)
│                  │     (Query: borrowcheck_module)
└──────────────────┘
    │
    ▼
┌──────────────────┐
│   HIR Builder    │  → High-level IR (type-resolved, desugared)
│                  │     (Query: hir_lower)
└──────────────────┘
    │
    ▼
┌──────────────────┐
│   MIR Builder    │  → Mid-level IR (SSA form, control flow)
│   + LLVM Codegen │     (Query: mir_build + codegen_unit)
└──────────────────┘
    │
    ▼
┌──────────────────┐
│  Embedded LLVM   │  → Object files (in-process, no clang subprocess)
│  (IR → .obj)     │     via LLVMParseIRInContext + LLVMTargetMachineEmit
└──────────────────┘
    │
    ▼
┌──────────────────┐
│  Embedded LLD    │  → Executable / Library (in-process, no linker subprocess)
│  (COFF/ELF/MachO)│     via lld::lldMain()
└──────────────────┘
```

#### Query System

All 8 compilation stages are wrapped as queries in a `QueryContext`:

| Query | Input | Output |
|-------|-------|--------|
| `read_source` | file path | preprocessed source |
| `tokenize` | file path | token stream |
| `parse_module` | file path + module name | AST |
| `typecheck_module` | file path + module name | typed AST |
| `borrowcheck_module` | file path + module name | verified AST |
| `hir_lower` | file path + module name | HIR |
| `mir_build` | file path + module name | MIR |
| `codegen_unit` | file path + module name + options | LLVM IR string |

Each query is executed via `QueryContext::force<R>(key)`:
1. Check in-memory cache → return if hit
2. For `codegen_unit`: try incremental reuse from previous session (GREEN path)
3. Detect dependency cycles
4. Execute the query provider
5. Record dependencies, compute fingerprints, cache result
6. Persist fingerprints to disk for next session

### 2.2 Phase Details

#### Lexer
```cpp
// Input: source text
// Output: Vec<Token>
struct Token {
    TokenKind kind;
    Span span;          // file, line, column, length
    StableId id;        // @xxxxxxxx (optional)
    String lexeme;      // raw text
};
```

#### Parser
```cpp
// Input: Vec<Token>
// Output: AST

// LL(1) parser - single token lookahead
// No backtracking required (by design)
// Error recovery via synchronization points
```

#### Resolver
```cpp
// Input: AST
// Output: AST with resolved names

// Tasks:
// 1. Build symbol tables
// 2. Resolve imports
// 3. Resolve type references
// 4. Resolve variable references
// 5. Detect undefined/duplicate names
```

#### Type Checker
```cpp
// Input: Resolved AST
// Output: Typed AST (TAST)

// Algorithm: Bidirectional type inference
// 1. Checking mode: check(expr, expected_type)
// 2. Synthesis mode: infer(expr) -> type

// Constraint solving for generics
// Trait resolution for methods
```

**Implementation Structure** (modular design for maintainability):
```
src/types/
├── checker/                # Core type checking (split from checker.cpp)
│   ├── helpers.cpp         # Utilities, Levenshtein distance, type compatibility
│   ├── core.cpp            # check_module, register_*, check_func_decl
│   ├── expr.cpp            # check_expr, check_literal, check_call, check_interp_string
│   ├── stmt.cpp            # check_stmt, check_let, check_var, bind_pattern
│   ├── control.cpp         # check_if, check_when, check_loop, check_return
│   ├── types.cpp           # check_tuple, check_closure, check_path
│   └── resolve.cpp         # resolve_type, resolve_type_path
├── builtins/               # Builtin function registration
│   ├── register.cpp        # Main registration entry point
│   ├── io.cpp              # print, println
│   ├── string.cpp          # str_len, str_concat, str_contains, etc.
│   ├── math.cpp            # sqrt, pow, abs, floor, ceil, round
│   ├── mem.cpp             # alloc, dealloc, mem_copy, mem_set
│   ├── time.cpp            # time_ms, elapsed_ms, sleep_ms
│   ├── atomic.cpp          # atomic_load/store/add/cas, fence
│   ├── sync.cpp            # mutex_*, channel_*, waitgroup_*
│   └── collections.cpp     # list_*, hashmap_*, buffer_*
└── env_*.cpp               # Environment management
```

#### Borrow Checker
```cpp
// Input: TAST
// Output: TAST (verified)

// Algorithm based on:
// 1. Lifetime inference
// 2. Region tracking
// 3. Conflict detection

// Rules enforced:
// - Single owner
// - Multiple ref T OR single mut ref T
// - No use after move
// - No dangling references
```

#### Effect Checker
```cpp
// Input: TAST
// Output: TAST (effects verified)

// Algorithm:
// 1. Infer effects from function bodies
// 2. Check declared effects cover actual
// 3. Propagate through call graph
// 4. Verify capabilities match effects
```

#### HIR Generation
```cpp
// Input: TAST (typed AST)
// Output: HIR (High-level IR)

// Purpose:
// 1. Type-resolved representation using semantic types::TypePtr
// 2. Desugar syntax (var -> let mut, method desugaring)
// 3. Monomorphization of generic types/functions
// 4. Closure capture analysis

// Structure:
// - HirModule: Top-level container
// - HirFunction, HirStruct, HirEnum, HirImpl, HirBehavior
// - HirExpr: 30+ expression types (calls, closures, control flow)
// - HirStmt: let declarations, expression statements
// - HirPattern: wildcard, binding, literal, tuple, struct, enum, or, range, array
```

**Implementation Structure** (modular design for maintainability):
```
include/hir/                 # Headers
├── hir.hpp                  # Main header (includes all)
├── hir_id.hpp               # ID types and generator
├── hir_pattern.hpp          # Pattern definitions
├── hir_expr.hpp             # Expression definitions
├── hir_stmt.hpp             # Statement definitions
├── hir_decl.hpp             # Declaration definitions
├── hir_module.hpp           # Module container
├── hir_printer.hpp          # Pretty printer
└── hir_builder.hpp          # Builder class

src/hir/                     # Implementation
├── hir_pattern.cpp          # Pattern factory functions
├── hir_expr.cpp             # Expression factory functions
├── hir_stmt.cpp             # Statement factory functions
├── hir_module.cpp           # Module lookup methods
├── hir_printer.cpp          # Debug output
├── hir_builder.cpp          # Core builder
├── hir_builder_expr.cpp     # Expression lowering
├── hir_builder_stmt.cpp     # Statement lowering
└── hir_builder_pattern.cpp  # Pattern lowering
```

#### MIR Generation
```cpp
// Input: HIR (High-level IR)
// Output: MIR (control flow graph)

// Structure:
// - Basic blocks
// - SSA form
// - Explicit drops
// - Explicit moves/copies
```

#### LLVM IR Generation
```cpp
// Input: MIR
// Output: LLVM Module

// Transformations:
// 1. Monomorphize generics
// 2. Lower types to LLVM types
// 3. Generate function bodies
// 4. Insert runtime calls (alloc, drop)
```

### 2.3 Object File and Build Pipeline

The TML compiler uses a fully self-contained build pipeline with embedded LLVM and LLD.
No external tools (clang, system linker) are required at runtime.

#### Build Stages

```
Source (.tml) → [Query Pipeline] → LLVM IR (in-memory) → Object File (.obj/.o) → Executable/Library
     │                │                    │                       │
     │                │                    │                       │
     ▼                ▼                    ▼                       ▼
  QueryContext     8 memoized         Embedded LLVM           Embedded LLD
  (demand-driven)  query stages      (in-process)            (in-process)
```

#### Embedded LLVM Backend

The compiler links ~55 LLVM static libraries directly into the `tml` binary. LLVM IR
is compiled to object files **in-process** — no intermediate `.ll` files are written
to disk (unless `--emit-ir` is specified).

```cpp
// Compile LLVM IR string directly to object file (no disk I/O for IR)
ObjectCompileResult compile_ir_string_to_object(
    const std::string& llvm_ir,                    // In-memory IR string
    const std::optional<fs::path>& output_file,    // Output: .obj/.o
    const ObjectCompileOptions& options
);

struct ObjectCompileOptions {
    int optimization_level = 3;        // 0-3 (-O0 to -O3)
    bool debug_info = false;           // Include debug symbols
    bool position_independent = false; // -fPIC for shared libs
    bool verbose = false;              // Print commands
};
```

The pipeline: `LLVMParseIRInContext()` → `LLVMRunPasses()` → `LLVMTargetMachineEmitToFile()`

**Platform-Specific Extensions:**
- Windows: `.obj` (COFF format)
- Unix/Linux: `.o` (ELF format)
- macOS: `.o` (Mach-O format)

#### Embedded LLD Linker

The compiler embeds LLD (LLVM's linker) for in-process linking via `lld::lldMain()`.
Supports all major platforms:

| Platform | Driver | Format |
|----------|--------|--------|
| Windows | `lld::coff::link` | COFF/PE |
| Linux | `lld::elf::link` | ELF |
| macOS | `lld::macho::link` | Mach-O |

Falls back to system linker via subprocess when `TML_HAS_LLD_EMBEDDED` is not defined.

#### Build Cache System

The compiler implements a multi-level cache for fast incremental builds:

**Level 1: Incremental Query Cache (Red-Green)**
```
build/debug/.incr-cache/incr.bin      # Binary fingerprint/dependency cache
build/debug/.incr-cache/ir/<hash>.ll  # Cached LLVM IR per compilation unit
build/debug/.incr-cache/ir/<hash>.libs # Cached link libraries
```
- 128-bit CRC32C fingerprints for all 8 query stages
- Dependency edges persisted across sessions
- GREEN path: skip entire compilation pipeline if source unchanged
- No-op rebuild: < 100ms

**Level 2: Object File Cache**
```
build/debug/.run-cache/<content-hash>.obj
```
- Content-based hashing of LLVM IR
- Instant reuse if IR unchanged
- Shared across all builds

**Level 3: Executable Cache**
```
build/debug/.run-cache/<combined-hash>.exe
```
- Combined hash of all object files
- Skips linking if nothing changed
- Includes runtime libraries in hash

**Cache Hit Performance:**
- No-op rebuild (incremental GREEN): < 100ms
- Full rebuild: ~3 seconds (compile + link)
- Cache hit (object files): ~0.5 seconds (link only)
- Cache hit (executable): ~0.075 seconds (no work)

**File Organization:**
```
build/
├── debug/
│   ├── .incr-cache/          # Incremental compilation cache
│   │   ├── incr.bin          # Binary fingerprint/dep cache
│   │   └── ir/               # Cached LLVM IR strings
│   │       ├── a1b2c3d4.ll
│   │       └── a1b2c3d4.libs
│   ├── .run-cache/           # Cached build artifacts
│   │   ├── e5f6g7h8.obj      # Object files (content hash)
│   │   └── 9i0j1k2l.exe      # Executables (combined hash)
│   ├── tml.exe               # Main compiler
│   └── tml_tests.exe         # Test executable
└── cache/
    └── x86_64-pc-windows-msvc/
        └── debug/            # CMake build cache
```

#### Parallel Compilation

For test suites and multi-file projects, the compiler supports parallel object file generation:

```cpp
BatchCompileResult compile_ir_string_batch(
    const std::vector<std::string>& ir_strings,
    const ObjectCompileOptions& options,
    int num_threads = 0  // 0 = auto-detect (hardware_concurrency)
);
```

**Implementation Details:**
- Uses `std::thread` for parallelism
- Auto-detects CPU cores with `std::thread::hardware_concurrency()`
- Thread-safe compilation with mutex-protected shared state
- Each thread compiles independent IR strings via embedded LLVM
- Achieves 52% speedup for test suite compilation

#### Linking Strategies

The embedded LLD linker supports three output types:

```cpp
enum class OutputType {
    Executable,     // .exe (Windows) / no extension (Unix)
    StaticLib,      // .lib (Windows) / .a (Unix)
    DynamicLib      // .dll (Windows) / .so (Unix)
};

LinkResult link_objects(
    const std::vector<fs::path>& object_files,
    const fs::path& output_file,
    const LinkOptions& options
);
```

All linking is performed in-process via embedded LLD. No external linker is required.

#### FFI Integration

For C interoperability, the build system supports:

1. **Header Generation:**
   - `--emit-header` flag generates C header from TML code
   - Maps TML types to C types
   - Preserves function signatures with `@[export]`

2. **Symbol Export:**
   ```tml
   @[export]
   func add(a: I32, b: I32) -> I32 {
       return a + b
   }
   // Generates: int32_t tml_add(int32_t a, int32_t b);
   ```

3. **Calling Convention:**
   - Uses C calling convention by default
   - Compatible with MSVC (Windows) and GCC/Clang (Unix)
   - No name mangling for exported functions

4. **Build Modes:**
   ```bash
   # Static library + header
   tml build --crate-type staticlib --emit-header

   # Dynamic library + header
   tml build --crate-type dylib --emit-header
   ```

**Integration Example:**
```c
// C code using TML library
#include "mylib.h"

int main() {
    int32_t result = tml_add(5, 3);  // Calls TML function
    printf("Result: %d\n", result);
    return 0;
}
```

**Build Commands:**
```bash
# Compile TML to library (self-contained, no external tools needed)
tml build mylib.tml --crate-type dylib --emit-header --out-dir .

# Compile C code linking against TML library
clang use_mylib.c mylib.dll -o use_mylib.exe

# Run
./use_mylib.exe
```

#### Build Optimization Techniques

**Query-Based Incremental Compilation:**
- Demand-driven pipeline skips unchanged stages entirely
- 128-bit fingerprints detect changes at query granularity
- GREEN path: reuse cached LLVM IR from previous session
- No-op rebuild completes in < 100ms

**Hard Link Optimization:**
- Cache uses hard links instead of file copies
- Zero data duplication for cached artifacts
- Instant "copy" operation (metadata only)
- Supported on Windows NTFS, Linux ext4, macOS APFS

**Incremental Linking:**
- Detects which object files changed
- Only recompiles modified modules via embedded LLVM
- Reuses cached objects for unchanged code
- Combined hash prevents unnecessary relinking

**Compiler Flags:**
```bash
# Debug build (default) — query pipeline with incremental compilation
tml build main.tml              # -O0, incremental enabled

# Release build
tml build main.tml --release    # -O3, optimized

# Custom optimization
tml build main.tml --opt-level 2  # -O2

# Legacy pipeline (bypass query system)
tml build main.tml --legacy     # Use traditional sequential pipeline

# Force full recompilation (skip incremental cache)
tml build main.tml --no-cache

# Verbose mode
tml build main.tml --verbose    # Print all commands
```

## 3. Type System Implementation

### 3.1 Type Representation

```cpp
enum class TypeKind {
    // Primitives
    Bool, I8, I16, I32, I64, I128,
    U8, U16, U32, U64, U128,
    F32, F64, Char, Unit, Never,

    // Compound
    Struct, Enum, Tuple, Array,

    // References
    Ref,      // ref T
    RefMut,   // mut ref T

    // Smart pointers (library types)
    Heap, Shared, Sync,

    // Generic
    TypeVar,      // T (unresolved)
    TypeParam,    // T (bound)
    Applied,      // List[I32]

    // Function
    Func,         // func(A, B) -> C

    // Special
    Maybe,        // Maybe[T]
    Outcome,      // Outcome[T, E]

    // Error
    Error,        // type checking failed
};

struct Type {
    TypeKind kind;
    StableId id;
    Vec<Type> args;      // for generics
    Vec<Bound> bounds;   // for constraints
};
```

### 3.2 Type Inference Algorithm

```
Bidirectional Type Checking:

1. CHECK mode: check(expr, type) -> bool
   - Given expression and expected type
   - Return whether expression has type

2. INFER mode: infer(expr) -> type
   - Given expression only
   - Return synthesized type

Key rules:
- Literals: infer numeric type from context or default
- Variables: lookup in environment
- Function calls: instantiate generics, check args
- Method calls: resolve behavior, check receiver
- Lambdas: may need CHECK mode for param types
```

### 3.3 Constraint Solving

```
For generic functions:

1. Collect constraints from:
   - Explicit bounds: T: Ordered
   - Usage sites: T + T requires Addable
   - Return type requirements

2. Solve constraints:
   - Unify type variables
   - Check behavior implementations
   - Report unsatisfied bounds
```

## 4. Borrow Checker Implementation

### 4.1 Lifetime Inference

```cpp
// Each reference has a lifetime region
struct Lifetime {
    RegionId region;
    Span origin;      // where reference was created
};

// Regions form a tree based on lexical scope
struct Region {
    RegionId id;
    RegionId parent;  // enclosing scope
    Vec<RegionId> children;
};
```

### 4.2 Borrow Tracking

```cpp
struct BorrowState {
    Map<Place, BorrowInfo> active_borrows;
};

struct BorrowInfo {
    BorrowKind kind;     // Shared or Mutable
    Lifetime lifetime;
    Span borrow_site;
};

enum class Place {
    Local(VarId),
    Field(Box<Place>, FieldId),
    Index(Box<Place>, Box<Expr>),
    Deref(Box<Place>),
};
```

### 4.3 Conflict Detection

```cpp
// When creating a new borrow:
func check_borrow(place: Place, kind: BorrowKind) -> Outcome[(), BorrowError] {
    let existing: Borrows = state.get_borrows(place);

    for borrow in existing {
        if conflicts(borrow.kind, kind) {
            return Err(BorrowError::Conflict {
                existing: borrow,
                new_kind: kind,
            });
        }
    }

    Ok(())
}

// Conflict rules:
// ref T + ref T = OK
// ref T + mut ref T = ERROR
// mut ref T + ref T = ERROR
// mut ref T + mut ref T = ERROR
```

## 5. Effect System Implementation

### 5.1 Effect Representation

```cpp
struct Effect {
    Vec<EffectKind> effects;
};

enum class EffectKind {
    Pure,
    IoFileRead,
    IoFileWrite,
    IoNetworkConnect,
    IoNetworkSend,
    IoNetworkReceive,
    IoTimeRead,
    IoTimeSleep,
    IoProcessSpawn,
    IoProcessEnvRead,
    IoProcessEnvWrite,
    StateRead,
    StateWrite,
    Panic,
    Diverge,
    // ... extensible
};
```

### 5.2 Effect Inference

```cpp
func infer_effects(body: ref FuncBody) -> Effect {
    let effects: Effect = Effect::pure();

    for stmt in body.statements {
        effects = effects.union(infer_stmt_effects(stmt));
    }

    effects
}

func infer_stmt_effects(stmt: ref Stmt) -> Effect {
    when stmt {
        Call(func, args) => {
            let func_effects: Effect = lookup_function_effects(func);
            let arg_effects: Effect = args.map(infer_expr_effects).union_all();
            func_effects.union(arg_effects)
        },
        // ... other statement kinds
    }
}
```

### 5.3 Capability Checking

```cpp
func check_capabilities(module: ref Module) -> Outcome[(), CapError] {
    let declared_caps: Caps = module.caps;
    let required_caps: Caps = infer_required_caps(module);

    if not declared_caps.covers(required_caps) {
        return Err(CapError::Missing {
            required: required_caps.difference(declared_caps),
        });
    }

    Ok(())
}
```

## 6. Code Generation

### 6.1 Target Support Matrix

| Target | Bootstrap | Native | Notes |
|--------|-----------|--------|-------|
| x86_64-linux | ✓ | ✓ | Primary |
| x86_64-windows | ✓ | ✓ | Primary |
| x86_64-macos | ✓ | ✓ | Primary |
| aarch64-linux | ○ | ✓ | ARM servers |
| aarch64-macos | ○ | ✓ | Apple Silicon |
| wasm32 | ○ | ✓ | Web/WASI |

### 6.2 LLVM Type Mapping

```cpp
// TML Type -> LLVM Type
I8   -> i8
I16  -> i16
I32  -> i32
I64  -> i64
I128 -> i128
U8   -> i8
U16  -> i16
U32  -> i32
U64  -> i64
U128 -> i128
F32  -> float
F64  -> double
Bool -> i1
Char -> i32 (Unicode scalar)
Unit -> void
Never -> noreturn

// Compound types
String -> { i8*, i64 }  // ptr, len
ref T -> T*
mut ref T -> T*
Heap[T] -> T*
Maybe[T] -> { i1, T }  // tag, value (if fits)
Outcome[T,E] -> { i8, max(T,E) }  // tag, union

// Struct -> LLVM struct (field order preserved)
// Enum -> tagged union
```

### 6.3 Function ABI

```cpp
// Calling convention: C (platform default)
// Small values: passed in registers
// Large values: passed by pointer

// Return values:
// - Unit: void
// - Small: in registers
// - Large: sret parameter

// Special handling:
// - Generic functions: monomorphized
// - Closures: { function_ptr, env_ptr }
```

## 7. Runtime System

### 7.1 Minimal Runtime

```cpp
// Required runtime functions:

// Memory allocation
extern "C" void* tml_alloc(size_t size, size_t align);
extern "C" void tml_dealloc(void* ptr, size_t size, size_t align);

// Panic handling
extern "C" [[noreturn]] void tml_panic(const char* msg, size_t len);

// Reference counting (for Shared/Sync)
extern "C" void tml_shared_inc(void* ptr);
extern "C" void tml_shared_dec(void* ptr);
extern "C" void tml_sync_inc(void* ptr);
extern "C" void tml_sync_dec(void* ptr);
```

### 7.2 Memory Layout

```cpp
// Object header (for heap objects)
struct ObjectHeader {
    size_t size;      // allocation size
    TypeId type;      // for debugging
    uint32_t flags;   // GC bits (unused), etc.
};

// Reference counted header
struct RcHeader {
    ObjectHeader base;
    atomic<size_t> strong_count;
    atomic<size_t> weak_count;
};
```

### 7.3 Stack Unwinding

```cpp
// Panic uses platform unwinding:
// - Linux: libunwind
// - macOS: libunwind
// - Windows: SEH

// Cleanup during unwind:
// - Disposable behavior implementations called
// - RAII resources released
```

## 8. Library System

### 8.1 Library Types

| Type | Extension | Contains | Linkage |
|------|-----------|----------|---------|
| Static | `.tml.a` | Objects + metadata | Compile-time |
| Dynamic | `.tml.so/.dll` | Code + metadata | Runtime |
| Package | `.tml.pkg` | Source + metadata | Build-time |

### 8.2 Library Metadata

```toml
# .tml.meta (embedded or sidecar)
[library]
name = "mylib"
version = "1.0.0"
tml_version = "1.0"

[exports]
modules = ["mylib", "mylib.utils"]
types = 42
functions = 128

[dependencies]
std = "1.0"

[abi]
version = 1
platform = "x86_64-linux"
```

### 8.3 Symbol Resolution

```cpp
// Exported symbols use mangled names:
// _TML_<module>_<item>_<signature_hash>

// Example:
// mylib.utils.parse -> _TML_mylib_utils_parse_a1b2c3d4

// Lookup order:
// 1. Current package
// 2. Explicit dependencies
// 3. Standard library
```

## 9. Incremental Compilation

TML implements a **Red-Green incremental compilation** system, inspired by rustc's
query-based architecture. Fingerprints and dependency edges are persisted to disk
between compilation sessions, enabling near-instant rebuilds when source is unchanged.

### 9.1 Red-Green Model

Each query result is assigned a **color**:

- **GREEN**: All inputs unchanged from previous session → reuse cached result
- **RED**: One or more inputs changed → must recompute

```
Session N (first build):
  Execute all queries → save fingerprints + deps + IR to .incr-cache/

Session N+1 (rebuild):
  Load .incr-cache/incr.bin → for each CodegenUnit query:
    ReadSource input_fp == file hash on disk? → GREEN
    All deps GREEN? → this query is GREEN
    CodegenUnit GREEN? → load cached .ll → skip entire pipeline!
    Any dep RED? → recompute from that point
```

### 9.2 Fingerprinting

Every query has an **input fingerprint** (what goes in) and an **output fingerprint** (what comes out). Both are 128-bit values computed via CRC32C.

**Input fingerprints:**

| Query | Input Fingerprint |
|-------|-------------------|
| `read_source` | `fingerprint_source(file_path)` + defines |
| `tokenize` | output_fp of ReadSource dependency |
| `parse_module` | output_fp of Tokenize dependency |
| `typecheck_module` | combine(output_fp of ParseModule, lib_env_fingerprint) |
| `borrowcheck_module` | combine(output_fp of TypecheckModule, output_fp of ParseModule) |
| `hir_lower` | combine(output_fp of TypecheckModule, output_fp of ParseModule) |
| `mir_build` | combine(output_fp of HirLower, output_fp of TypecheckModule) |
| `codegen_unit` | combine(deps' output_fps, target_triple, opt_level, coverage) |

**Output fingerprints:**

| Query | Output Fingerprint |
|-------|-------------------|
| `read_source` | `fingerprint_string(preprocessed_source)` |
| `tokenize` – `mir_build` | Same as input_fp (deterministic stages) |
| `codegen_unit` | `fingerprint_string(llvm_ir)` |

### 9.3 Dependency Graph

Dependencies are tracked at query granularity (not module level). Each query records
which other queries it depends on during execution via a stack-based `DependencyTracker`.

```cpp
struct PrevSessionEntry {
    QueryKey key;
    Fingerprint input_fingerprint;    // 128-bit
    Fingerprint output_fingerprint;   // 128-bit
    std::vector<QueryKey> dependencies;
};
```

### 9.4 Binary Cache Format

The incremental cache is stored as a binary file at `build/{debug|release}/.incr-cache/incr.bin`:

```
Header (24 bytes):
  magic: u32          = 0x544D4943 ("TMIC")
  version_major: u16  = 1
  version_minor: u16  = 0
  entry_count: u32    = N
  session_timestamp: u64
  options_hash: u32   = hash(opt_level, debug_info, target, defines, coverage)

Per-entry:
  query_kind: u8
  key_len: u16
  key_data: [u8; key_len]
  input_fp: {u64 high, u64 low}   # 16 bytes
  output_fp: {u64 high, u64 low}  # 16 bytes
  dep_count: u16
  deps: [dep_count × {kind: u8, key_len: u16, key_data: bytes}]
```

LLVM IR cached separately: `build/{debug|release}/.incr-cache/ir/<hash>.ll`

### 9.5 Green Checking

On rebuild, `verify_all_inputs_green(key)` recursively checks the dependency tree:

```
verify_all_inputs_green(CodegenUnit):
  └─ verify_all_inputs_green(MirBuild):
       └─ verify_all_inputs_green(HirLower):
            └─ verify_all_inputs_green(TypecheckModule):
                 └─ verify_all_inputs_green(ParseModule):
                      └─ verify_all_inputs_green(Tokenize):
                           └─ verify_all_inputs_green(ReadSource):
                                → compare file hash to prev input_fp
                                → GREEN if unchanged
```

A color cache (`color_map_`) prevents redundant checking of shared dependencies.

### 9.6 Cache Invalidation

The cache is automatically invalidated when:
- Build options change (optimization level, debug info, target triple, defines, coverage)
- Source file content changes (detected by fingerprinting file on disk)
- Library environment changes (`.tml.meta` files modified)
- Cache format version mismatch

Force full rebuild: `tml build --no-cache`

### 9.7 Performance

| Scenario | Time |
|----------|------|
| No-op rebuild (all GREEN) | < 100ms |
| Single function change | < 500ms |
| Full rebuild (no cache) | ~3 seconds |
| Test suite (3,632 tests) | ~17 seconds |

## 10. Error Recovery

### 10.1 Parser Recovery

```cpp
// Synchronization tokens (recover to these):
const SYNC_TOKENS = [
    Func,     // function start
    Type,     // type start
    Behavior, // behavior start
    Extend,   // extend start
    RBrace,   // end of block
    Semicolon // end of statement (if used)
];

func recover(parser: mut ref Parser) {
    loop while not parser.at_end() {
        if SYNC_TOKENS.contains(parser.current().kind) {
            return
        }
        parser.advance()
    }
}
```

### 10.2 Type Error Recovery

```cpp
// On type error, use Error type to continue checking
// This allows reporting multiple errors

func check_expr(expr: ref Expr, expected: Type) -> Type {
    let actual: Type = infer(expr);

    if not unify(actual, expected) {
        report_error(TypeMismatch { expected, actual, span: expr.span });
        return Type::Error;  // Continue with error type
    }

    actual
}
```

---

*Previous: [15-ERROR-HANDLING.md](./15-ERROR-HANDLING.md)*
*Next: [17-CODEGEN.md](./17-CODEGEN.md) — Code Generation*
