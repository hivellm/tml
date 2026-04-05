# 6. Multi-Layer Intermediate Representation Pipeline

## 6.1 Overview

TML employs a five-layer intermediate representation pipeline — an unusually deep IR stack compared to most compilers. The pipeline is:

```
Source (.tml)
    |
    v
  AST          Faithful syntax tree
    |
    v
  HIR          Typed, desugared, monomorphized
    |
    v
  THIR         Coercions inserted, methods resolved, operators desugared
    |
    v
  MIR          SSA form, basic blocks, explicit control flow
    |
    v
  LLVM IR      Machine-level, target-specific
    |
    v
  Object Code  Native binary
```

Each layer serves a distinct purpose, and information is progressively lowered from high-level language semantics toward machine-level operations. This multi-layer approach provides several advantages over shallower pipelines: each layer's transformations are isolated, making them easier to test and debug; optimization passes can target the layer where they are most effective; and the compiler can emit diagnostic output at any layer for debugging.

### 6.1.1 Comparison with Other Compilers

| Compiler | IR Layers | Pipeline |
|----------|-----------|----------|
| TML | 5 (AST, HIR, THIR, MIR, LLVM IR) | Deep, progressive lowering |
| Rust (rustc) | 5 (AST, HIR, THIR, MIR, LLVM IR) | Very similar to TML |
| Clang | 2 (Clang AST, LLVM IR) | Shallow, direct lowering |
| GCC | 3 (GENERIC, GIMPLE, RTL) | Medium depth, two optimization layers |
| Go | 2 (AST, SSA) | Shallow, custom backend |
| Zig | 3 (ZIR, AIR, Machine Code or LLVM IR) | Medium, self-hosted |
| V | 1 (AST to C) | Transpiler model |

TML's pipeline is architecturally closest to Rust's — not coincidentally, since both languages have similar semantic complexity (ownership, generics, algebraic types, closures) that benefits from progressive lowering.

---

## 6.2 AST (Abstract Syntax Tree)

The AST is produced by the parser and represents the exact syntactic structure of the source code. It preserves all syntactic details including source locations, comments, and sugar.

**Key properties:**
- Untyped: expressions carry no type information.
- Sugar preserved: `var x = 5` is represented as-is (not desugared to `let mut`).
- Source-faithful: source positions enable accurate error messages and IDE features.

The parser uses a **Pratt parser** for expressions (precedence climbing) and **recursive descent** for declarations. This combination is common in modern compilers; it handles operator precedence cleanly while keeping declaration parsing straightforward.

---

## 6.3 HIR (High-level Intermediate Representation)

The HIR is the first semantically-enriched representation. It is produced by the HIR builder (`hir_builder.cpp`) from the type-checked AST.

### 6.3.1 Transformations

| AST | HIR | Transformation |
|-----|-----|---------------|
| `var x = 5` | `let mut x: I32 = 5` | Desugaring + type annotation |
| `Just(x)` | `Maybe[I32]::Just(x)` with variant_index=0 | Type resolution + index |
| `point.x` | `point.x` with field_index=0, type=F64 | Field resolution |
| `do(x) x + n` | Closure with captures `[n: I32]` | Capture analysis |
| `for item in list {}` | Iterator protocol calls | Desugar to `.into_iter()` + loop |
| `items.map(do(x) x * 2)` | Monomorphized `map[I32, I32]` | Generic instantiation |

### 6.3.2 Monomorphization

TML performs monomorphization at the HIR level: generic functions and types are instantiated with concrete type arguments. For example, `List[I32]` and `List[Str]` become separate, concrete types in the HIR, each with their own methods.

This is an architectural choice with trade-offs:

- **Advantage**: Downstream passes (MIR building, codegen) work entirely with concrete types, simplifying their implementation.
- **Advantage**: Enables per-instantiation optimization — `sort[I32]` can be optimized differently than `sort[Str]`.
- **Disadvantage**: Code size increase — N instantiations produce N copies of each function.
- **Comparison**: Rust also monomorphizes, at approximately the same stage. Go uses dictionary passing (gcshape stenciling) to avoid code duplication at the cost of runtime overhead.

### 6.3.3 Closure Capture Analysis

The HIR builder analyzes closures to determine which variables they capture and how (by value, by reference, or by mutable reference). This information is encoded in the HIR closure representation:

```
// Source
let factor = 2
let doubled = items.map(do(x) x * factor)

// HIR representation
Closure {
    params: [x: I32],
    captures: [factor: I32, by_value],
    body: Mul(x, factor),
    return_type: I32,
}
```

The capture analysis determines the closure's type classification: `Fn` (immutable captures), `FnMut` (mutable captures), or `FnOnce` (moved captures). This is identical to Rust's closure classification.

---

## 6.4 THIR (Typed HIR)

The THIR is a relatively new addition to TML's pipeline, inserted between HIR and MIR. It performs transformations that require full type information and trait resolution.

### 6.4.1 Transformations

**Implicit coercion insertion:**
```
// HIR: add(a: I32, b: I64) — type mismatch
// THIR: add(CoercionExpr(a, I32 -> I64), b) — coercion made explicit
```

**Operator desugaring to method calls:**
```
// HIR: a + b  (where a: Point, b: Point)
// THIR: a.add(b)  (resolved to impl Add for Point)
```

**Method resolution via trait solver:**
```
// HIR: items.sort()  (which sort? could be from multiple behaviors)
// THIR: <List[I32] as Sortable>::sort(items)  (resolved to specific impl)
```

**Pattern exhaustiveness checking:**
The THIR verifies that `when` expressions cover all possible variants of the matched type. Missing patterns produce a compile-time error.

### 6.4.2 Why THIR Exists

The THIR exists because certain transformations require both type information AND trait resolution results, which are not available during HIR building. Specifically:

1. **Coercion insertion** requires knowing the target type, which depends on how the expression is used — information that flows bidirectionally and is only fully resolved after type checking.
2. **Operator desugaring** requires knowing which `Add`/`Sub`/etc. implementation to call, which is determined by the trait solver.
3. **Method resolution** in the presence of multiple applicable behaviors requires the trait solver's coherence and specificity analysis.

Rust's THIR serves a similar purpose, though the specific transformations differ in detail.

---

## 6.5 MIR (Mid-level Intermediate Representation)

The MIR is the primary optimization and analysis target. It is an SSA-form representation with basic blocks, explicit terminators, and typed values.

### 6.5.1 Structure

A MIR function consists of:
- **Basic blocks**: Sequences of instructions followed by a terminator.
- **Instructions**: Operations that produce values (arithmetic, loads, stores, calls).
- **Terminators**: Control flow operations (branch, conditional branch, return, switch).
- **Value identifiers**: Every computed value has a unique `%N` identifier.

```
func @max_I32(%0: i32, %1: i32) -> i32 {
  bb0:
    %2 = cmp.gt %0, %1
    br.cond %2, bb1, bb2

  bb1:
    ret %0

  bb2:
    ret %1
}
```

### 6.5.2 Dual MIR Building Paths

TML uniquely maintains **two parallel paths** for MIR construction:

**Path A: HIR to MIR (legacy)**
- Files: `hir_mir_builder.cpp`, `builder/hir_expr.cpp`, `builder/hir_expr_control.cpp`
- Input: HirModule
- Status: Mature, handles all language features, used for production compilation

**Path B: THIR to MIR (new)**
- Files: `thir_mir_builder.cpp`, `thir_mir_builder_expr.cpp`
- Input: ThirModule
- Status: Under development, supports a growing subset of language features

The rationale for dual paths is migration: Path B will eventually replace Path A, but the transition is incremental. Path A bypasses THIR entirely (performing coercions and method resolution during MIR building), while Path B receives a fully-resolved THIR and performs a cleaner, more principled lowering.

This is analogous to Rust's own migration from AST-based codegen to MIR-based codegen, which took several years and was done incrementally.

### 6.5.3 Key MIR Instructions

| Category | Instructions | Description |
|----------|-------------|-------------|
| Constants | `const.i32`, `const.f64`, `const.str`, `const.bool` | Literal values |
| Arithmetic | `add`, `sub`, `mul`, `div`, `rem`, `neg` | Numeric operations |
| Comparison | `cmp.eq`, `cmp.ne`, `cmp.lt`, `cmp.gt`, `cmp.le`, `cmp.ge` | Comparisons |
| Logical | `and`, `or`, `not` | Boolean operations |
| Memory | `alloca`, `load`, `store`, `gep` | Stack allocation and access |
| Aggregates | `struct_create`, `struct_extract`, `enum_create`, `enum_discriminant` | Composite types |
| Calls | `call`, `call_indirect` | Function invocation |
| Casts | `cast`, `bitcast`, `trunc`, `zext`, `sext` | Type conversions |

### 6.5.4 Optimization

The MIR undergoes 52 optimization passes (detailed in Section 7), organized in a carefully ordered pipeline. The most critical pass is **mem2reg**, which promotes stack allocations to SSA registers — essential because the MIR builder conservatively allocates values on the stack, and mem2reg removes the unnecessary indirection.

---

## 6.6 LLVM IR Generation

The final lowering step converts optimized MIR into LLVM IR text, which is then parsed and compiled by the embedded LLVM library.

### 6.6.1 MirCodegen

The `MirCodegen` class (`mir_codegen.cpp`) traverses the MIR module and emits LLVM IR text. Key responsibilities:

- **Type lowering**: TML types to LLVM types (`I32` to `i32`, `Str` to `ptr`, structs to LLVM struct types).
- **Function emission**: MIR functions to LLVM function definitions with correct calling conventions.
- **Instruction lowering**: MIR instructions to LLVM instructions (most are 1:1 mappings).
- **ABI handling**: sret convention for large struct returns, byval for value parameters.
- **Runtime declarations**: C runtime function declarations for FFI calls.

### 6.6.2 Method Call Dispatch

An important implementation detail: in the MIR path, method calls are represented as regular `CallInst` instructions (not a separate `MethodCallInst`). The method is resolved to a concrete function during THIR lowering or HIR-to-MIR building, and by the time it reaches MIR, it is simply a function call with the receiver as the first argument. This simplifies MIR optimization (no special handling for method calls) and matches Rust's MIR representation.

### 6.6.3 Rust-as-Reference Methodology

TML employs a systematic approach to evaluating IR quality: the Rust-as-Reference methodology. Since TML and Rust have similar semantics and both target LLVM, equivalent TML and Rust code should produce comparable LLVM IR. When TML's output diverges significantly (more instructions, worse type layouts, unnecessary allocations), it indicates a codegen bug or optimization opportunity.

This methodology is described in detail in Section 7.

---

## 6.7 Debug Layers Feature

A unique aspect of TML's IR pipeline is its integration with the testing infrastructure through the **debug_layers** feature. When a test fails, the compiler can be instructed to emit HIR, MIR, and LLVM IR for the failing function, along with diagnosis hints that identify which layer contains the bug:

```
Test FAILED: test_sort_list

=== HIR ===
[HIR output showing type-resolved, desugared code]
Diagnosis: HIR types look correct

=== MIR ===
[MIR output showing SSA form with basic blocks]
Diagnosis: MIR control flow correct, but %7 has unexpected type

=== LLVM IR ===
[LLVM IR output]
Diagnosis: Type mismatch at call instruction — likely MIR codegen bug
```

This feature is designed specifically for LLM-assisted debugging: an AI agent can read all three IR layers, compare them, and localize the bug to a specific compilation phase. No other language provides this level of integrated IR debugging in its test framework.

---

## 6.8 Summary

TML's five-layer IR pipeline provides:

1. **Progressive lowering** — Each layer removes one level of abstraction, making transformations isolated and testable.
2. **Optimization at the right level** — Type-aware optimizations at MIR level, machine-level optimizations at LLVM level.
3. **Diagnostic power** — IR output at every layer enables precise bug localization.
4. **Migration path** — Dual MIR building paths enable incremental improvement without breaking existing code.
5. **Semantic preservation** — Each lowering is semantics-preserving, enabling formal verification of the pipeline.

The depth of the pipeline is justified by the language's semantic complexity: ownership, generics, behaviors, closures, and algebraic data types all require careful treatment that benefits from intermediate representations. Simpler languages (Go, C) can afford shallower pipelines because they have fewer semantic constructs to lower.
