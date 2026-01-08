# RFC-0013: High-level Intermediate Representation (HIR)

## Status

**Active** - Implemented in v0.7.0

## Summary

This RFC specifies the High-level Intermediate Representation (HIR), a type-resolved, desugared representation that sits between the type-checked AST and MIR. HIR preserves semantic information while lowering syntactic sugar and performing monomorphization.

## Motivation

### Problem

The current compilation pipeline has a large semantic gap:

```
AST → Type Check → MIR → LLVM IR
```

This creates issues:
1. **Complex MIR generation** - MIR builder handles both desugaring and SSA conversion
2. **No monomorphization boundary** - Generic instantiation happens during codegen
3. **Lost semantic information** - AST types are replaced too early with MIR types
4. **Difficult debugging** - Hard to trace type-level issues after lowering

### Solution

Insert HIR between type checking and MIR:

```
AST → Type Check → HIR → MIR → LLVM IR → Machine Code
```

HIR provides:
1. **Type-resolved AST** - All types fully resolved, using semantic `TypePtr`
2. **Desugaring layer** - `var` → `let mut`, method desugaring, etc.
3. **Monomorphization boundary** - Generic instantiation happens at HIR level
4. **Closure capture analysis** - Identifies captured variables
5. **Semantic preservation** - Maintains high-level structure for optimization

## Specification

### 1. HIR Position in Pipeline

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
│ Type Check  │  → TAST (typed AST)
└─────────────┘
    │
    ▼
┌─────────────┐
│ HIR Builder │  → HIR (type-resolved, desugared)  ◄── NEW
└─────────────┘
    │
    ▼
┌─────────────┐
│ MIR Builder │  → MIR (SSA form)
└─────────────┘
    │
    ▼
┌─────────────┐
│   LLVM IR   │  → LLVM module
└─────────────┘
```

### 2. HIR Types

HIR uses semantic types from the type system (`types::TypePtr`), not its own type representation:

```cpp
using HirType = types::TypePtr;
```

This preserves full type information including:
- Generic type arguments
- Behavior implementations
- Lifetime information (future)
- Associated types

### 3. HIR Identifiers

Every HIR node has a unique identifier:

```cpp
struct HirId {
    uint64_t id;
};

class HirIdGenerator {
    auto fresh() -> HirId;
};
```

### 4. HIR Patterns

HIR patterns represent destructuring and matching:

```cpp
HirPatternKind ::=
    | HirWildcardPattern          ; `_`
    | HirBindingPattern           ; `x`, `mut x`
    | HirLiteralPattern           ; `42`, `"hello"`, `true`
    | HirTuplePattern             ; `(a, b, c)`
    | HirStructPattern            ; `Point { x, y, .. }`
    | HirEnumPattern              ; `Just(v)`, `Err(e)`
    | HirOrPattern                ; `a | b | c`
    | HirRangePattern             ; `0 to 10`, `'a' through 'z'`
    | HirArrayPattern             ; `[a, b, ..rest]`
```

### 5. HIR Expressions

HIR expressions are type-annotated and desugared:

```cpp
HirExprKind ::=
    // Literals
    | HirLiteralExpr              ; `42`, `3.14`, `"hello"`, `true`
    | HirVarExpr                  ; Variable reference

    // Operations
    | HirBinaryExpr               ; `a + b`, `x == y`
    | HirUnaryExpr                ; `-x`, `not b`, `ref v`
    | HirCastExpr                 ; `x as I64`

    // Function calls
    | HirCallExpr                 ; `foo(a, b)`
    | HirMethodCallExpr           ; `obj.method(args)`

    // Field and index access
    | HirFieldExpr                ; `point.x`
    | HirIndexExpr                ; `arr[i]`

    // Constructors
    | HirTupleExpr                ; `(a, b, c)`
    | HirArrayExpr                ; `[1, 2, 3]`
    | HirStructExpr               ; `Point { x: 1, y: 2 }`
    | HirEnumExpr                 ; `Just(42)`, `Nothing`

    // Control flow
    | HirBlockExpr                ; `{ stmts; expr }`
    | HirIfExpr                   ; `if cond { } else { }`
    | HirWhenExpr                 ; `when x { pat => expr }`
    | HirLoopExpr                 ; `loop { }`
    | HirWhileExpr                ; `loop while cond { }`
    | HirForExpr                  ; `for x in iter { }`

    // Control transfer
    | HirReturnExpr               ; `return value`
    | HirBreakExpr                ; `break 'label value`
    | HirContinueExpr             ; `continue 'label`

    // Advanced
    | HirClosureExpr              ; `do(x) x * 2`
    | HirTryExpr                  ; `expr!`
    | HirAwaitExpr                ; `expr.await`
    | HirAssignExpr               ; `x = value`
    | HirCompoundAssignExpr       ; `x += 1`
    | HirLowlevelExpr             ; `lowlevel { ... }`
```

### 6. HIR Statements

HIR has two statement kinds:

```cpp
HirStmtKind ::=
    | HirLetStmt                  ; `let x: T = value`
    | HirExprStmt                 ; Expression as statement
```

Note: `var` is desugared to `let mut` during HIR lowering.

### 7. HIR Declarations

```cpp
// Function declaration
struct HirFunction {
    std::string name;
    std::string mangled_name;
    std::vector<std::string> attributes;
    bool is_public;
    bool is_async;
    bool is_extern;
    std::optional<std::string> extern_abi;
    std::vector<HirParam> params;
    HirType return_type;
    std::optional<std::unique_ptr<HirExpr>> body;
};

// Struct declaration
struct HirStruct {
    std::string name;
    std::string mangled_name;
    bool is_public;
    std::vector<HirField> fields;
};

// Enum declaration
struct HirEnum {
    std::string name;
    std::string mangled_name;
    bool is_public;
    std::vector<HirVariant> variants;
};
```

### 8. HIR Module

A module contains all declarations:

```cpp
struct HirModule {
    std::string name;
    std::string source_path;

    std::vector<HirStruct> structs;
    std::vector<HirEnum> enums;
    std::vector<HirFunction> functions;
    std::vector<HirImpl> impls;
    std::vector<HirBehavior> behaviors;
    std::vector<HirConst> constants;

    // Lookup methods
    auto find_struct(const std::string& name) const -> const HirStruct*;
    auto find_enum(const std::string& name) const -> const HirEnum*;
    auto find_function(const std::string& name) const -> const HirFunction*;
    auto find_const(const std::string& name) const -> const HirConst*;
};
```

### 9. HIR Builder

The HIR builder lowers type-checked AST to HIR:

```cpp
class HirBuilder {
    explicit HirBuilder(types::TypeEnv& type_env);

    auto lower_module(const parser::Module& ast_module) -> HirModule;
    auto lower_function(const parser::FuncDecl& func) -> HirFunction;
    auto lower_struct(const parser::StructDecl& struct_decl) -> HirStruct;
    auto lower_enum(const parser::EnumDecl& enum_decl) -> HirEnum;

private:
    auto lower_expr(const parser::Expr& expr) -> HirExprPtr;
    auto lower_stmt(const parser::Stmt& stmt) -> HirStmtPtr;
    auto lower_pattern(const parser::Pattern& pattern, HirType expected) -> HirPatternPtr;
};
```

### 10. Monomorphization

HIR tracks generic instantiations via a cache:

```cpp
struct MonomorphizationCache {
    std::unordered_map<std::string, std::string> type_instances;
    std::unordered_map<std::string, std::string> func_instances;

    auto get_or_create_type(const std::string& base_name,
                           const std::vector<HirType>& type_args) -> std::string;
    auto get_or_create_func(const std::string& base_name,
                           const std::vector<HirType>& type_args) -> std::string;
};
```

When a generic function is called with concrete types:
1. Check if monomorphized instance exists
2. If not, create new instance with mangled name
3. Add to module's function list

### 11. Closure Capture Analysis

HIR builder identifies captured variables:

```cpp
struct HirCapture {
    std::string name;
    HirType type;
    bool is_mut;        // Captured by mutable reference
    bool is_move;       // Captured by move (ownership transfer)
};

auto collect_captures(const parser::ClosureExpr& closure) -> std::vector<HirCapture>;
```

## Examples

### Example 1: Simple Function

**Source:**
```tml
func add(a: I32, b: I32) -> I32 {
    return a + b
}
```

**HIR:**
```
func add /* _tml_add */ (a: I32, b: I32) -> I32 {
    return (a + b)
}
```

### Example 2: Pattern Matching

**Source:**
```tml
func describe(m: Maybe[I32]) -> Str {
    when m {
        Just(v) => "Value: " + v.to_string(),
        Nothing => "Empty"
    }
}
```

**HIR:**
```
func describe /* _tml_describe */ (m: Maybe__I32) -> Str {
    when m {
        Maybe__I32::Just(v) => ("Value: " + v.to_string())
        Maybe__I32::Nothing => "Empty"
    }
}
```

### Example 3: Closure with Captures

**Source:**
```tml
func make_adder(n: I32) -> func(I32) -> I32 {
    return do(x) x + n
}
```

**HIR:**
```
func make_adder /* _tml_make_adder */ (n: I32) -> func(I32) -> I32 {
    return closure {
        captures: [n: I32 (by_ref)]
        params: [x: I32]
        body: (x + n)
    }
}
```

## Implementation Files

### Headers (`compiler/include/hir/`)

| File | Description |
|------|-------------|
| `hir.hpp` | Main header, includes all components |
| `hir_id.hpp` | HIR ID types and generator |
| `hir_pattern.hpp` | Pattern definitions |
| `hir_expr.hpp` | Expression definitions |
| `hir_stmt.hpp` | Statement definitions |
| `hir_decl.hpp` | Declaration definitions |
| `hir_module.hpp` | Module container |
| `hir_printer.hpp` | Pretty printer |
| `hir_builder.hpp` | Builder class |

### Sources (`compiler/src/hir/`)

| File | Description |
|------|-------------|
| `hir_pattern.cpp` | Pattern factory functions |
| `hir_expr.cpp` | Expression factory functions |
| `hir_stmt.cpp` | Statement factory functions |
| `hir_module.cpp` | Module lookup methods |
| `hir_printer.cpp` | HIR pretty printing |
| `hir_builder.cpp` | Core builder, module lowering |
| `hir_builder_expr.cpp` | Expression lowering |
| `hir_builder_stmt.cpp` | Statement lowering |
| `hir_builder_pattern.cpp` | Pattern lowering |

## Compatibility

### With RFC-0007 (High-level IR)

HIR is the internal compiler representation, while RFC-0007 IR is for external interchange:
- **RFC-0007 IR**: JSON/Protobuf format for tool interchange
- **HIR**: In-memory representation for compilation

### With RFC-0012 (MIR)

HIR feeds into MIR:
- HIR preserves high-level structure with resolved types
- MIR converts to SSA form with explicit control flow
- HIR → MIR lowering handles monomorphization completion

## Alternatives Rejected

### Direct AST to MIR

- Pro: Simpler pipeline
- Con: MIR builder becomes overly complex, handles both desugaring and SSA
- **Rejected**: Separation of concerns improves maintainability

### Using RFC-0007 IR as HIR

- Pro: Single intermediate representation
- Con: RFC-0007 IR is for interchange, not optimization
- **Rejected**: Different purposes require different representations

### Late Monomorphization (in codegen)

- Pro: Potentially smaller IR
- Con: Codegen becomes complex, harder to cache
- **Rejected**: HIR-level monomorphization is cleaner

## References

1. Rust HIR documentation
2. Swift SIL (Swift Intermediate Language)
3. "Engineering a Compiler" - Cooper & Torczon
4. RFC-0007 (TML Interchange IR)
5. RFC-0012 (MIR)

## Future Work

1. **HIR Optimizations** - High-level optimizations before MIR lowering
2. **HIR Caching** - Serialize HIR for incremental compilation
3. **HIR Verification** - Type consistency checks
4. **Lifetime Tracking** - Explicit lifetime annotations in HIR
