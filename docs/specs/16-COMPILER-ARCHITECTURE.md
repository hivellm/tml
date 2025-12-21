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
// - Multiple &T OR single &mut T
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
    Ref,      // &T
    RefMut,   // &mut T

    // Smart pointers (library types)
    Box, Rc, Arc,

    // Generic
    TypeVar,      // T (unresolved)
    TypeParam,    // T (bound)
    Applied,      // List[I32]

    // Function
    Func,         // func(A, B) -> C

    // Special
    Option,       // T?
    Result,       // Result[T, E]

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
- Method calls: resolve trait, check receiver
- Lambdas: may need CHECK mode for param types
```

### 3.3 Constraint Solving

```
For generic functions:

1. Collect constraints from:
   - Explicit bounds: T: Ord
   - Usage sites: T + T requires Add
   - Return type requirements

2. Solve constraints:
   - Unify type variables
   - Check trait implementations
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
func check_borrow(place: Place, kind: BorrowKind) -> Result[(), BorrowError] {
    let existing = state.get_borrows(place);

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
// &T + &T = OK
// &T + &mut T = ERROR
// &mut T + &T = ERROR
// &mut T + &mut T = ERROR
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
func infer_effects(body: &FuncBody) -> Effect {
    let effects = Effect::pure();

    for stmt in body.statements {
        effects = effects.union(infer_stmt_effects(stmt));
    }

    effects
}

func infer_stmt_effects(stmt: &Stmt) -> Effect {
    match stmt {
        Call(func, args) => {
            let func_effects = lookup_function_effects(func);
            let arg_effects = args.map(infer_expr_effects).union_all();
            func_effects.union(arg_effects)
        },
        // ... other statement kinds
    }
}
```

### 5.3 Capability Checking

```cpp
func check_capabilities(module: &Module) -> Result[(), CapError] {
    let declared_caps = module.caps;
    let required_caps = infer_required_caps(module);

    if !declared_caps.covers(required_caps) {
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
&T -> T*
&mut T -> T*
Box[T] -> T*
Option[T] -> { i1, T }  // tag, value (if fits)
Result[T,E] -> { i8, max(T,E) }  // tag, union

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

// Reference counting (for Rc/Arc)
extern "C" void tml_rc_inc(void* ptr);
extern "C" void tml_rc_dec(void* ptr);
extern "C" void tml_arc_inc(void* ptr);
extern "C" void tml_arc_dec(void* ptr);
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
// - Drop trait implementations called
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
    let cached = cache.get(module);

    if cached.is_none() {
        return true;
    }

    let info = cached.unwrap();

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
    Trait,    // trait start
    Extend,   // extend start
    RBrace,   // end of block
    Semicolon // end of statement (if used)
];

func recover(parser: &mut Parser) {
    while !parser.at_end() {
        if SYNC_TOKENS.contains(parser.current().kind) {
            return;
        }
        parser.advance();
    }
}
```

### 10.2 Type Error Recovery

```cpp
// On type error, use Error type to continue checking
// This allows reporting multiple errors

func check_expr(expr: &Expr, expected: Type) -> Type {
    let actual = infer(expr);

    if !unify(actual, expected) {
        report_error(TypeMismatch { expected, actual, span: expr.span });
        return Type::Error;  // Continue with error type
    }

    actual
}
```

---

*Previous: [15-ERROR-HANDLING.md](./15-ERROR-HANDLING.md)*
*Next: [17-CODEGEN.md](./17-CODEGEN.md) — Code Generation*
