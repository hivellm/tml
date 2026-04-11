# F15: Destructuring Assignment

**Priority**: Low — **ALREADY IN SPEC** (RFC-0002 §2.4-2.5: `LetStmt <- 'let' ... Pattern ...`, `StructPattern <- TypePath '{' FieldPatterns? '}'`)
**Impact**: Result handling readability
**Complexity**: Low-Medium
**Status**: Compiler implementation gap — struct patterns in let already in grammar

## Problem

Unwrapping results into multiple variables requires intermediate bindings:

```tml
let result: ParsedExpr = parse_prefix_expr(tokens, pos)!
var p: I64 = result.pos
var left: Heap[Expr] = result.expr
```

This 3-line pattern repeats 30+ times in the parser.

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `compiler-tml/src/parser/parse_expr.tml` | 103-105 | Result destructure |
| `compiler-tml/src/parser/parse_stmt.tml` | throughout | Same pattern |
| `compiler-tml/src/parser/parse_decl.tml` | throughout | Same pattern |

## Proposal

### Struct destructuring in let

```tml
let ParsedExpr { expr: left, pos: p } = parse_prefix_expr(tokens, pos)!
```

### Tuple destructuring

```tml
let (a, b, c) = compute_triple()
```

## C++ Compiler Changes

1. **Parser**: Allow struct patterns in `let` bindings
2. **HIR**: Desugar destructuring into individual field accesses
3. **MIR**: Emit field extractions as individual instructions
