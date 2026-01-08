# TML v1.0 — HIR (High-Level Intermediate Representation)

> This document describes the High-Level Intermediate Representation (HIR) used by
> the TML compiler. The HIR is the primary IR used in most of `tml` after type checking.

## 1. Overview

The HIR is a compiler-internal representation of TML source code that is produced
after parsing and type checking. It is fundamentally a more explicitly-typed,
slightly desugared version of the AST, with most source-level constructs
preserved but enriched with type information.

**Key Characteristics:**
- Every expression and pattern carries its resolved semantic type
- Syntactic sugar is expanded (e.g., `var` → `let mut`)
- Generic types and functions are prepared for monomorphization
- Closure captures are identified
- All identifiers are linked to their definitions via resolved types

### 1.1 HIR vs AST

While the AST represents the exact structure of what the user wrote, the HIR
represents what it *means*. Some differences include:

| AST | HIR |
|-----|-----|
| `var x = 5` | `let mut x: I64 = 5` |
| `Just(x)` | `Maybe[I32]::Just(x)` with variant_index=0 |
| `point.x` | `point.x` with field_index=0, type=I32 |
| `do(x) x + n` | closure with explicit captures `[n: I32]` |

### 1.2 Pipeline Position

```
Source Code (.tml)
       │
       ▼
  ┌─────────┐
  │  Lexer  │ ── Token stream
  └─────────┘
       │
       ▼
  ┌─────────┐
  │ Parser  │ ── AST (untyped)
  └─────────┘
       │
       ▼
  ┌─────────────┐
  │ Type Check  │ ── TypeEnv (type information)
  └─────────────┘
       │
       ▼
  ┌─────────────┐
  │ HIR Builder │ ── HIR (typed, desugared)   ◄─── YOU ARE HERE
  └─────────────┘
       │
       ▼
  ┌─────────────┐
  │ MIR Builder │ ── MIR (SSA form)
  └─────────────┘
       │
       ▼
  ┌───────────┐
  │  Codegen  │ ── LLVM IR
  └───────────┘
```

## 2. Out-of-Band Storage and the `HirModule` Type

The top-level data structure in the HIR is `HirModule`. This contains the HIR
for a single compilation unit (typically corresponding to a single source file
or a module).

```cpp
struct HirModule {
    std::string name;           // Module name
    std::string source_path;    // Path to source file

    // Type definitions
    std::vector<HirStruct> structs;
    std::vector<HirEnum> enums;

    // Behaviors (traits)
    std::vector<HirBehavior> behaviors;

    // Implementation blocks
    std::vector<HirImpl> impls;

    // Functions
    std::vector<HirFunction> functions;

    // Module-level constants
    std::vector<HirConst> constants;

    // Imported modules
    std::vector<std::string> imports;
};
```

Items are stored in separate vectors by kind, enabling efficient iteration
over specific categories without filtering.

### 2.1 Lookup Methods

The module provides lookup methods for finding declarations by name:

```cpp
const HirStruct* find_struct(const std::string& name) const;
const HirEnum* find_enum(const std::string& name) const;
const HirFunction* find_function(const std::string& name) const;
const HirConst* find_const(const std::string& name) const;
```

## 3. Identifiers in the HIR

Every HIR node has a unique identifier. The HIR uses a simple incrementing
integer scheme for node IDs.

### 3.1 HirId

```cpp
using HirId = uint64_t;
constexpr HirId INVALID_HIR_ID = 0;
```

Each `HirId`:
- Is unique within a compilation session
- Starts from 1 (0 is reserved as invalid)
- Enables efficient node lookup in maps
- Supports stable references for error reporting

### 3.2 HirIdGenerator

```cpp
class HirIdGenerator {
    auto next() -> HirId;      // Generate next ID
    auto count() const -> HirId; // Number of IDs generated
    void reset();              // Reset to initial state
};
```

## 4. HIR Types

HIR does **not** define its own type system. Instead, it reuses the semantic
type system from the type checker:

```cpp
using HirType = types::TypePtr;
```

This means HIR nodes carry fully-resolved types that include:
- Concrete types (primitives, structs, enums)
- Generic instantiations with resolved type arguments
- Reference and pointer types with mutability
- Function and closure types
- Associated types

## 5. HIR Data Structures

### 5.1 Expressions (`HirExpr`)

Expressions produce values. Every `HirExpr` has:
- `id`: Unique HirId
- `type()`: Resolved semantic type
- `span()`: Source location

Expression kinds:

| Kind | Example | Description |
|------|---------|-------------|
| `HirLiteralExpr` | `42`, `"hello"` | Literal values |
| `HirVarExpr` | `x` | Variable reference |
| `HirBinaryExpr` | `a + b` | Binary operation |
| `HirUnaryExpr` | `-x`, `not b` | Unary operation |
| `HirCallExpr` | `foo(a, b)` | Function call |
| `HirMethodCallExpr` | `obj.method()` | Method call |
| `HirFieldExpr` | `point.x` | Field access (with resolved index) |
| `HirIndexExpr` | `arr[i]` | Index access |
| `HirTupleExpr` | `(a, b)` | Tuple construction |
| `HirArrayExpr` | `[1, 2, 3]` | Array literal |
| `HirArrayRepeatExpr` | `[0; 10]` | Array repeat |
| `HirStructExpr` | `Point { x: 1, y: 2 }` | Struct construction |
| `HirEnumExpr` | `Just(x)` | Enum variant construction |
| `HirBlockExpr` | `{ stmts; expr }` | Block expression |
| `HirIfExpr` | `if c { a } else { b }` | Conditional |
| `HirWhenExpr` | `when x { ... }` | Pattern match |
| `HirLoopExpr` | `loop { ... }` | Infinite loop |
| `HirWhileExpr` | `while c { ... }` | While loop |
| `HirForExpr` | `for x in iter` | For loop |
| `HirReturnExpr` | `return x` | Return |
| `HirBreakExpr` | `break 'label x` | Break with optional value |
| `HirContinueExpr` | `continue` | Continue |
| `HirClosureExpr` | `do(x) x + 1` | Closure (with captures) |
| `HirCastExpr` | `x as I64` | Type cast |
| `HirTryExpr` | `expr!` | Try operator |
| `HirAwaitExpr` | `expr.await` | Await |
| `HirAssignExpr` | `x = y` | Assignment |
| `HirCompoundAssignExpr` | `x += 1` | Compound assignment |
| `HirLowlevelExpr` | `lowlevel { }` | Unsafe block |

### 5.2 Patterns (`HirPattern`)

Patterns are used in `let` bindings, `when` arms, and function parameters:

| Kind | Example | Description |
|------|---------|-------------|
| `HirWildcardPattern` | `_` | Matches anything, discards |
| `HirBindingPattern` | `x`, `mut x` | Binds to variable |
| `HirLiteralPattern` | `42`, `true` | Matches exact value |
| `HirTuplePattern` | `(a, b)` | Destructures tuple |
| `HirStructPattern` | `Point { x, y }` | Destructures struct |
| `HirEnumPattern` | `Just(v)` | Matches enum variant |
| `HirOrPattern` | `a \| b` | Alternative patterns |
| `HirRangePattern` | `0 to 10` | Range match |
| `HirArrayPattern` | `[a, ..rest]` | Array destructure |

### 5.3 Statements (`HirStmt`)

HIR has minimal statement types:

| Kind | Example | Description |
|------|---------|-------------|
| `HirLetStmt` | `let x: I32 = 5` | Variable binding |
| `HirExprStmt` | `foo();` | Expression as statement |

Note: `var x = 5` is desugared to `let mut x = 5` during HIR lowering.

### 5.4 Declarations

Top-level items are stored as declarations:

```cpp
struct HirFunction {
    HirId id;
    std::string name;
    std::string mangled_name;     // For monomorphized functions
    std::vector<HirParam> params;
    HirType return_type;
    std::optional<HirExprPtr> body; // None for extern functions
    bool is_public;
    bool is_async;
    bool is_extern;
    std::optional<std::string> extern_abi;
    std::vector<std::string> attributes;
    SourceSpan span;
};

struct HirStruct {
    HirId id;
    std::string name;
    std::string mangled_name;
    std::vector<HirField> fields;
    bool is_public;
    SourceSpan span;
};

struct HirEnum {
    HirId id;
    std::string name;
    std::string mangled_name;
    std::vector<HirVariant> variants;
    bool is_public;
    SourceSpan span;
};
```

## 6. Lowering AST to HIR

The `HirBuilder` class converts type-checked AST to HIR.

### 6.1 Usage

```cpp
#include "hir/hir_builder.hpp"

// After type checking
types::TypeEnv type_env = /* from type checker */;

// Create builder
HirBuilder builder(type_env);

// Lower module
HirModule hir_module = builder.lower_module(ast_module);
```

### 6.2 What the Lowering Process Does

1. **Type Resolution**: Every expression gets its fully-resolved type from
   the type environment

2. **Desugaring**:
   - `var x = e` → `let mut x = e`
   - `x += 1` → compound assignment with resolved operator
   - `for` loops → iterator protocol calls
   - `if let` → `when` with two arms

3. **Field/Variant Resolution**: Field accesses and enum variants get their
   numeric indices resolved:
   ```cpp
   // point.x becomes:
   HirFieldExpr {
       field_name: "x",
       field_index: 0,  // Resolved!
       type: I32
   }
   ```

4. **Closure Capture Analysis**: For closures, the builder identifies which
   variables from enclosing scopes are used:
   ```cpp
   // do(x) x + y  (where y is from outer scope)
   HirClosureExpr {
       params: [(x, I32)],
       captures: [HirCapture { name: "y", type: I32, is_mut: false }],
       body: (x + y)
   }
   ```

### 6.3 Monomorphization Support

HIR tracks generic instantiations through `MonomorphizationCache`:

```cpp
struct MonomorphizationCache {
    // (generic_name, [type_args]) -> mangled_name
    std::unordered_map<std::string, std::string> type_instances;
    std::unordered_map<std::string, std::string> func_instances;

    auto get_or_create_type(const std::string& base_name,
                           const std::vector<HirType>& type_args) -> std::string;
    auto get_or_create_func(const std::string& base_name,
                           const std::vector<HirType>& type_args) -> std::string;
};
```

When `Vec[I32]` is used, the cache generates a mangled name like `Vec__I32`
and tracks that this instantiation exists.

## 7. Debugging the HIR

### 7.1 HirPrinter

The `HirPrinter` class produces human-readable output:

```cpp
#include "hir/hir_printer.hpp"

HirPrinter printer(true);  // with colors
std::string output = printer.print_module(hir_module);
std::cout << output;
```

Or use the convenience function:

```cpp
std::cout << print_hir_module(hir_module, /* colors= */ true);
```

### 7.2 Example Output

For this TML code:
```tml
type Point { x: I32, y: I32 }

func add_points(a: Point, b: Point) -> Point {
    return Point { x: a.x + b.x, y: a.y + b.y }
}
```

The HIR printer produces:
```
module test

struct Point {
    x: I32
    y: I32
}

func add_points(a: Point, b: Point) -> Point {
    return Point {
        x: (a.x + b.x),
        y: (a.y + b.y)
    }
}
```

### 7.3 Compiler Debug Flags

Use `--emit-hir` flag to dump HIR during compilation:

```bash
tml build file.tml --emit-hir
```

## 8. HIR Invariants

When working with HIR, these invariants should hold:

1. **Every expression has a type**: `expr->type()` is never null for
   well-formed HIR

2. **All types are resolved**: No unresolved type variables or inference
   placeholders remain

3. **Field/variant indices are valid**: All `field_index` and `variant_index`
   values correspond to actual fields/variants

4. **Closure captures are complete**: All free variables in a closure body
   are listed in `captures`

5. **IDs are unique**: No two nodes share the same `HirId` within a module

## 9. API Reference

### 9.1 Header Files

| File | Description |
|------|-------------|
| `hir/hir.hpp` | Main header, includes all components |
| `hir/hir_id.hpp` | HirId type and generator |
| `hir/hir_pattern.hpp` | Pattern types and factories |
| `hir/hir_expr.hpp` | Expression types and factories |
| `hir/hir_stmt.hpp` | Statement types and factories |
| `hir/hir_decl.hpp` | Declaration types (functions, structs, etc.) |
| `hir/hir_module.hpp` | Module container type |
| `hir/hir_builder.hpp` | AST to HIR lowering |
| `hir/hir_printer.hpp` | Pretty printing |

### 9.2 Source Files

| File | Description |
|------|-------------|
| `hir/hir_pattern.cpp` | Pattern factory implementations |
| `hir/hir_expr.cpp` | Expression factory implementations |
| `hir/hir_stmt.cpp` | Statement factory implementations |
| `hir/hir_module.cpp` | Module lookup implementations |
| `hir/hir_printer.cpp` | Printer implementation |
| `hir/hir_builder.cpp` | Core builder, module lowering |
| `hir/hir_builder_expr.cpp` | Expression lowering |
| `hir/hir_builder_stmt.cpp` | Statement lowering |
| `hir/hir_builder_pattern.cpp` | Pattern lowering |

## 10. Future Work

- **HIR Passes**: Optimization passes operating at HIR level
- **Incremental Compilation**: HIR caching for unchanged modules
- **HIR Verification**: Validation pass ensuring invariants
- **Lifetime Tracking**: Explicit lifetime annotations in HIR

## 11. See Also

- [RFC-0013-HIR.md](../rfcs/RFC-0013-HIR.md) - Original HIR RFC
- [16-COMPILER-ARCHITECTURE.md](16-COMPILER-ARCHITECTURE.md) - Overall compiler architecture
- [MIR Documentation](MIR.md) - Mid-level IR that follows HIR
