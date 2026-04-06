# Proposal: THIR Lowering — Rewrite in TML (Phase 15b)

## Why

THIR (Typed High-level Intermediate Representation) is a thin transformation over HIR that
inserts the remaining implicit semantic content that the parser and type checker left out:
coercions, concrete method resolution, operator desugaring, and pattern exhaustiveness
verification. It exists as a distinct pass because these transformations require the full
TypeEnv (available after phase14d) and the typed HIR (available after phase15a), but they
must happen before MIR construction, which assumes all method calls are already resolved to
concrete functions and all operators are already desugared to their trait method equivalents.

Without THIR, the MIR builder would need to re-run method resolution and coercion insertion
mid-construction, tangling two fundamentally different concerns. The separation is documented
as a hard invariant from phase12c.

## What Changes

Port the following C++ files to TML:

- `compiler/src/thir/thir_lower.cpp` (1,138 LOC) — main THIR lowering pass
- `compiler/src/thir/exhaustiveness.cpp` (623 LOC) — pattern exhaustiveness analysis
- `compiler/src/thir/thir_module.cpp` (112 LOC) — THIR module and node type definitions
- 6 header files (1,169 LOC combined) — `ThirExpr`, `ThirModule`, `ThirLower`, exhaustiveness

New TML modules produced:

- `compiler-tml/src/thir/mod.tml` — module root
- `compiler-tml/src/thir/expr.tml` — `ThirExpr` enum extending `HirExpr` with coercion nodes
- `compiler-tml/src/thir/module.tml` — `ThirModule` struct
- `compiler-tml/src/thir/lower.tml` — `ThirLower` struct and `lower` entry point
- `compiler-tml/src/thir/exhaustiveness.tml` — exhaustiveness checker

## Key Decisions

**Coercions as explicit nodes in THIR, not implicit annotations.** When an `I8` value is used
where `I32` is expected, THIR inserts a `ThirExpr::Coerce { inner, from_ty, to_ty }` node
wrapping the original expression. Downstream passes (MIR builder) see a concrete coercion
instruction and generate the appropriate `sext` or `zext` LLVM instruction. Making coercions
implicit (e.g., via a side table or metadata annotation) would require the MIR builder to
re-inspect types at every use site — the explicit node model is simpler and matches the C++
`ThirLower::insert_coercion` design.

**Method resolution uses the trait solver from phase14d, not a local lookup.** When THIR
encounters `obj.method(args)` in the HIR, it calls the `BehaviorSolver` from phase14d to
resolve the concrete impl. The result is a `ThirExpr::MethodCall { recv, method_path, args }`
node where `method_path` is the fully-qualified path to the impl method (e.g.,
`core::iter::Iterator::next` rather than just `next`). This eliminates all ambiguity before
MIR construction, which cannot perform method resolution itself.

**Exhaustiveness uses matrix decomposition (usefulness algorithm).** The exhaustiveness
checker works by constructing a pattern matrix from all `when` arms and computing whether the
set of patterns is exhaustive via the standard "usefulness" algorithm (the same approach as
rustc's `check_match`). For each constructor (enum variant, literal range, struct), it expands
the matrix and checks recursively. This is the algorithm used in `exhaustiveness.cpp` and
must be replicated rather than approximated — the C++ test suite includes edge cases for
or-patterns, range patterns, and nested struct patterns that simpler approaches fail on.

**Operator desugaring is finalized here, not in phase14d.** Phase14d partially desugars
operators via the coercion pass (resolving the trait impl), but THIR rewrites the AST node
itself. After THIR, no `BinOp(Add, a, b)` nodes exist — only
`MethodCall(a, "std::ops::Add::add", [b])`. This two-step design is a documented phase12c
invariant: inference uses the syntactic form, THIR rewrites to the semantic form.

## Architecture

```
compiler-tml/src/thir/
  mod.tml            -- module root, re-exports ThirModule, ThirExpr, ThirLower
  expr.tml           -- ThirExpr enum: all HirExpr variants + Coerce, MethodCall (resolved)
  module.tml         -- ThirModule: List[ThirFunc] + List[ThirTypeDef]
  lower.tml          -- ThirLower { solver: ref BehaviorSolver, type_env: ref TypeEnv }
                     --   lower(hir: HirModule) -> ThirModule
                     --   lower_expr(), insert_coercion(), resolve_method(), desugar_op()
  exhaustiveness.tml -- PatternMatrix, is_exhaustive(), find_missing_patterns()
```

## Pipeline Integration

After phase15b completes, the pipeline is:

```
HirModule (from phase15a)
 | phase15b: coercion insertion, method resolution, operator desugaring, exhaustiveness check
 v
ThirModule  →  MIR builder THIR path (Phase 15c)
```

The `ThirModule` output is consumed exclusively by the THIR→MIR builder path in phase15c.
The HIR→MIR legacy path in phase15c consumes `HirModule` directly and does not use `ThirModule`.
Both paths must produce equivalent MIR — differential testing in phase15c verifies this.

## Success Criteria

Two-level differential testing:

1. THIR node comparison: lower 20 stdlib modules through the TML THIR pass. Serialize the
   `ThirModule` and compare field-by-field with C++ THIR output. Zero diffs required on method
   resolution, coercion insertion, and operator desugaring.

2. Exhaustiveness parity: run the TML exhaustiveness checker on all `when` expressions in the
   full 1,700+ file test suite. Compare reported exhaustiveness errors and unreachable pattern
   warnings with C++ output. Zero disagreements required.

## Risk Assessment

Medium. The trait solver (phase14d) already handles the most complex resolution logic. THIR
wiring is largely mechanical: call the solver, wrap results in THIR nodes, recurse through the
tree. The main risk is the exhaustiveness checker — matrix decomposition is a well-specified
algorithm but has many edge cases (or-patterns, guard interaction, range patterns on integers).
The C++ `exhaustiveness.cpp` (623 LOC) is the reference; implement against it directly.

A secondary risk is coercion double-insertion: phase14d already partially inserts coercions via
its coercion pass. THIR must detect and skip coercions already inserted rather than wrapping
them again. The C++ implementation checks for existing `CoercionExpr` nodes before inserting.

## Dependencies

- **Requires**: phase15a complete (HirModule is the input to THIR lowering)
- **Requires**: phase14d complete (BehaviorSolver is called during method resolution)
- **Blocks**: phase15c (MIR builder THIR path consumes ThirModule)
