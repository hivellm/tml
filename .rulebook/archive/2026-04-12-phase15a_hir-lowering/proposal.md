# Proposal: HIR Lowering — Rewrite in TML (Phase 15a)

## Why

HIR (High-level Intermediate Representation) is the essential bridge between the parsed AST and
the MIR builder. It takes the raw syntax tree and a fully-resolved TypeEnv from the type checker
(phase14d) and produces a typed, desugared tree where every expression node carries its concrete
type, all syntactic sugar has been eliminated, and generic functions have been monomorphized into
concrete specializations. Without HIR, the MIR builder would need to re-implement type resolution,
desugaring, and monomorphization — the entire point of the pipeline split is that HIR does this
work once, cleanly, so MIR can focus on control flow and SSA construction.

This phase ports ~15,207 LOC of C++ across 8 source files and 4 serialization files to ~9,900
lines of TML, completing the first major pass of the ERA 1 compiler port.

## What Changes

Port the following C++ files to TML:

- `compiler/src/hir/hir_builder.cpp` (1,511 LOC) — main HIR builder, entry point
- `compiler/src/hir/hir_builder_expr.cpp` (1,485 LOC) — expression lowering (largest single file)
- `compiler/src/hir/hir_builder_stmt.cpp` (433 LOC) — statement lowering
- `compiler/src/hir/hir_builder_pattern.cpp` (362 LOC) — pattern lowering
- `compiler/src/hir/hir_pass.cpp` (728 LOC) — HIR optimization passes
- `compiler/src/hir/hir_pass_inline.cpp` (1,292 LOC) — HIR inlining pass
- `compiler/src/hir/hir_printer.cpp` (619 LOC) — HIR pretty-printer
- `compiler/src/hir/hir_expr.cpp` (225 LOC), `hir_pattern.cpp` (142 LOC),
  `hir_module.cpp` (90 LOC), `hir_stmt.cpp` (76 LOC) — HIR node type definitions
- `compiler/src/hir/serializer/` (4 files, ~3,561 LOC) — binary serialization for pipeline bridge

New TML modules produced:

- `compiler-tml/src/hir/mod.tml` — module root
- `compiler-tml/src/hir/expr.tml` — `HirExpr` enum (~40 variants, each carrying resolved type)
- `compiler-tml/src/hir/stmt.tml` — `HirStmt` enum
- `compiler-tml/src/hir/pattern.tml` — `HirPattern` enum
- `compiler-tml/src/hir/module.tml` — `HirModule` struct
- `compiler-tml/src/hir/builder.tml` — `HirBuilder` struct and `lower_module` entry point
- `compiler-tml/src/hir/lower_expr.tml` — expression lowering (largest module)
- `compiler-tml/src/hir/monomorph.tml` — generic instantiation engine
- `compiler-tml/src/hir/printer.tml` — HIR pretty-printer

## Key Decisions

**HirExpr as enum with resolved types on every node.** Every `HirExpr` variant carries its
concrete `Type` as a field. This is the central design decision of the HIR: type resolution is
done once here, and every downstream pass (THIR, MIR, codegen) can read the type directly from
the node without consulting the TypeEnv. This matches the C++ implementation where `HirExpr`
nodes carry a `resolved_type` field populated during lowering.

**Monomorphization as a separate pass, not interleaved.** When expression lowering encounters a
call to `foo[I32](x)`, it does not immediately generate the specialized `foo$I32$` function.
Instead, it records the instantiation in a queue and lowers the call using the mangled name as
a placeholder. A separate monomorphization pass then drains the queue, generating each
specialization exactly once (handling recursive generics by checking the queue before adding).
This matches the C++ design and avoids infinite recursion on recursive generic types like
`List[List[T]]`.

**Closure capture analysis generates closure struct types.** When a closure `do(x) expr` is
encountered, capture analysis walks the body and identifies all free variables. For each closure,
a synthetic struct type is generated (e.g., `__Closure_42`) with one field per captured variable,
using the capture mode (ref, value, or move) determined by how the variable is used. The closure
body is lowered as a separate `HirFunc` with the closure struct as its first parameter. This
matches the C++ `hir_builder.cpp` approach and produces the struct layout that MIR codegen
expects.

**Desugaring is exhaustive and irreversible.** After HIR lowering, `for`/`while`/`var` syntax
does not exist in the output. All `for x in iter` loops become explicit iterator protocol calls
(`iter.next()` in a loop). All `var` declarations become `let mut`. All `if let Just(x) = e`
patterns become explicit `when` expressions. Downstream passes never need to handle these sugar
forms.

## Architecture

```
compiler-tml/src/hir/
  mod.tml        -- module root, re-exports HirModule, HirExpr, HirBuilder
  expr.tml       -- HirExpr enum: Lit, Var, Field, Index, Call, When, Loop, Closure, ...
  stmt.tml       -- HirStmt enum: Let, Expr, Return, ...
  pattern.tml    -- HirPattern enum: Wildcard, Bind, Struct, Enum, Tuple, ...
  module.tml     -- HirModule: List[HirFunc] + List[HirTypeDef] + List[HirImpl]
  builder.tml    -- HirBuilder + lower_module() entry point
  lower_expr.tml -- lower_expr(), lower_call(), lower_closure(), lower_when(), ...
  monomorph.tml  -- MonomorphQueue, drain_queue(), mangle_name()
  printer.tml    -- print_module(), print_func(), print_expr() for debug output
```

## Pipeline Integration

After phase15a completes, the pipeline is:

```
AST + TypeEnv (from phase14d)
 | phase15a: type resolution, desugaring, capture analysis, monomorphization
 v
HirModule  →  THIR lowerer (Phase 15b)
```

The `HirModule` output is a complete, typed, desugared representation of the source. Every
expression carries its concrete type. No generic functions remain — only their specializations.
No syntactic sugar remains. This is what THIR consumes.

## Success Criteria

Differential HIR comparison: run the TML HIR builder on all 1,700+ test files and all stdlib
modules. Serialize the resulting `HirModule` (using the binary serializer from phase 5) and
compare field-by-field with the C++ HIR builder output. Zero diffs required before phase 15b
begins.

## Risk Assessment

High. Monomorphization has well-known edge cases: recursive generics (`List[List[T]]`),
mutually recursive generic functions, generic types appearing only in associated type positions,
and trait object erasure (where monomorphization does not apply). The C++ implementation handles
these via a worklist algorithm with cycle detection; the TML port must replicate this exactly.

Closure capture analysis is also subtle: the capture mode (ref vs value vs move) affects the
generated closure struct layout and must match what the MIR builder expects. A mismatch here
produces runtime use-after-free bugs that are difficult to trace.

Plan: implement data types and builder core first, test against simple non-generic modules,
then add monomorphization and test with generic stdlib modules, then add closure support last.

## Dependencies

- **Requires**: phase14d complete (full TypeEnv with behavior dispatch and coercion annotations)
- **Blocks**: phase15b (THIR lowerer consumes HirModule)
- **Blocks**: phase15c (MIR builder HIR→MIR path consumes HirModule directly)
