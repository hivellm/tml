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

```
Source (.tml)
    │
    ▼
┌─────────────┐
│   Lexer     │  → Token stream
└─────────────┘
    │
    ▼
┌─────────────┐
│   Parser    │  → AST (untyped)
└─────────────┘
    │
    ▼
┌─────────────┐
│  Resolver   │  → AST (names resolved)
└─────────────┘
    │
    ▼
┌─────────────┐
│ Type Check  │  → TAST (typed AST)
└─────────────┘
    │
    ▼
┌─────────────┐
│Borrow Check │  → TAST (ownership verified)
└─────────────┘
    │
    ▼
┌─────────────┐
│Effect Check │  → TAST (effects verified)
└─────────────┘
    │
    ▼
┌─────────────┐
│  Lowering   │  → TML-IR (canonical IR)
└─────────────┘
    │
    ▼
┌─────────────┐
│    MIR      │  → Mid-level IR (control flow)
└─────────────┘
    │
    ▼
┌─────────────┐
│   LLVM IR   │  → LLVM module
└─────────────┘
    │
    ▼
┌─────────────┐
│   Codegen   │  → Object files
└─────────────┘
    │
    ▼
┌─────────────┐
│   Linker    │  → Executable / Library
└─────────────┘
```

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

#### Lowering to IR
```cpp
// Input: TAST
// Output: TML-IR (S-expression or JSON)

// Transformations:
// 1. Desugar syntax (method calls, operators)
// 2. Normalize (field ordering, expression forms)
// 3. Assign stable IDs
// 4. Emit canonical representation
```

#### MIR Generation
```cpp
// Input: TML-IR
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

The TML compiler uses a modern build system with content-based caching and parallel compilation.

#### Build Stages

```
Source (.tml) → LLVM IR (.ll) → Object File (.obj/.o) → Executable/Library
     │                │                  │                      │
     │                │                  │                      │
     ▼                ▼                  ▼                      ▼
  Compiler        Compiler           Clang                  Linker
  Frontend        Backend                              (clang/lld/link)
```

#### Object File Generation

```cpp
// Compile LLVM IR to object file
ObjectCompileResult compile_ll_to_object(
    const fs::path& ll_file,          // Input: LLVM IR file
    const std::optional<fs::path>& output_file,  // Output: .obj/.o
    const std::string& clang_path,
    const ObjectCompileOptions& options
);

struct ObjectCompileOptions {
    int optimization_level = 3;        // 0-3 (-O0 to -O3)
    bool debug_info = false;           // Include debug symbols
    bool position_independent = false; // -fPIC for shared libs
    bool verbose = false;              // Print commands
};
```

**Platform-Specific Extensions:**
- Windows: `.obj` (COFF format)
- Unix/Linux: `.o` (ELF format)
- macOS: `.o` (Mach-O format)

#### Build Cache System

The compiler implements a two-level cache for fast incremental builds:

**Level 1: Object File Cache**
```
build/debug/.run-cache/<content-hash>.obj
```
- Content-based hashing of LLVM IR
- Instant reuse if IR unchanged
- Shared across all builds

**Level 2: Executable Cache**
```
build/debug/.run-cache/<combined-hash>.exe
```
- Combined hash of all object files
- Skips linking if nothing changed
- Includes runtime libraries in hash

**Cache Hit Performance:**
- Full rebuild: ~3 seconds (compile + link)
- Cache hit (object files): ~0.5 seconds (link only)
- Cache hit (executable): ~0.075 seconds (no work)
- **91% speedup** for `tml run` on unchanged code
- **52% speedup** for `tml test` with parallel compilation

**File Organization:**
```
build/
├── debug/
│   ├── .run-cache/           # Cached artifacts
│   │   ├── a1b2c3d4.obj      # Object files (content hash)
│   │   ├── e5f6g7h8.obj
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
BatchCompileResult compile_ll_batch(
    const std::vector<fs::path>& ll_files,
    const std::string& clang_path,
    const ObjectCompileOptions& options,
    int num_threads = 0  // 0 = auto-detect (hardware_concurrency)
);
```

**Implementation Details:**
- Uses `std::thread` for parallelism
- Auto-detects CPU cores with `std::thread::hardware_concurrency()`
- Thread-safe compilation with mutex-protected shared state
- Each thread compiles independent `.ll` files
- Achieves 52% speedup for test suite compilation

#### Linking Strategies

The linker supports three output types:

```cpp
enum class OutputType {
    Executable,     // .exe (Windows) / no extension (Unix)
    StaticLib,      // .lib (Windows) / .a (Unix)
    DynamicLib      // .dll (Windows) / .so (Unix)
};

LinkResult link_objects(
    const std::vector<fs::path>& object_files,
    const fs::path& output_file,
    const std::string& clang_path,
    const LinkOptions& options
);
```

**Static Libraries:**
```bash
# Windows
lib.exe /OUT:mylib.lib file1.obj file2.obj

# Unix
ar rcs libmylib.a file1.o file2.o
```

**Dynamic Libraries:**
```bash
# Windows
clang -shared -o mylib.dll file1.obj file2.obj

# Unix
clang -shared -fPIC -o libmylib.so file1.o file2.o
```

**Executables:**
```bash
clang -o myapp.exe file1.obj file2.obj essential.obj -lruntime
```

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
# Compile TML to library
tml build mylib.tml --crate-type dylib --emit-header --out-dir .

# Compile C code
clang use_mylib.c mylib.dll -o use_mylib.exe

# Run
./use_mylib.exe
```

#### Build Optimization Techniques

**Hard Link Optimization:**
- Cache uses hard links instead of file copies
- Zero data duplication for cached artifacts
- Instant "copy" operation (metadata only)
- Supported on Windows NTFS, Linux ext4, macOS APFS

**Incremental Linking:**
- Detects which object files changed
- Only recompiles modified modules
- Reuses cached objects for unchanged code
- Combined hash prevents unnecessary relinking

**Compiler Flags:**
```bash
# Debug build (default)
tml build main.tml              # -O0, keep .ll files

# Release build
tml build main.tml --release    # -O3, delete intermediates

# Custom optimization
tml build main.tml --opt-level 2  # -O2

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

### 9.1 Dependency Graph

```cpp
// Track dependencies at module level
struct ModuleDeps {
    ModuleId module;
    Vec<ModuleId> imports;
    Hash source_hash;
    Hash interface_hash;
};

// Recompile if:
// 1. Source changed (source_hash differs)
// 2. Any import's interface changed
```

### 9.2 Caching

```
target/
├── cache/
│   ├── <module_hash>.ast    # Parsed AST
│   ├── <module_hash>.tast   # Typed AST
│   └── <module_hash>.o      # Object file
├── deps/
│   └── <dep_name>/
│       └── <version>/
└── <target>/
    └── debug|release/
```

### 9.3 Invalidation

```cpp
func needs_recompile(module: ModuleId) -> bool {
    let cached: Maybe[ModuleInfo] = cache.get(module);

    if cached.is_none() {
        return true;
    }

    let info: ModuleInfo = cached.unwrap();

    // Source changed?
    if hash(source(module)) != info.source_hash {
        return true;
    }

    // Any dependency interface changed?
    for dep in info.imports {
        if interface_hash(dep) != info.dep_hashes[dep] {
            return true;
        }
    }

    false
}
```

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
