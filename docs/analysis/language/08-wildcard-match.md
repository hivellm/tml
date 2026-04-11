# F08: Wildcard Match Arms & Default Cases

**Priority**: Medium
**Impact**: Large enum when-expressions
**Complexity**: Low (parser already supports `_`)

## Problem

Extracting a common field from a large enum requires exhaustive matching:

```tml
func expr_span(e: ref Expr) -> Span {
    when *e {
        Literal(l) => l.span,
        Ident(i) => i.span,
        Binary(b) => b.span,
        // ... 33 more variants, all returning .span
    }
}
```

When all variants share a common field, this is pure boilerplate.

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `compiler-tml/src/ast/exprs.tml` | 361-399 | 36-variant span extraction |
| `compiler-tml/src/ast/ast_writer.tml` | 108-152 | 28-variant op encoding |

## Proposal

### A. Ensure `_` wildcard works in all positions

```tml
when expr {
    SpecialCase(x) => handle_special(x),
    _ => default_behavior(),
}
```

### B. Common field access on enums (future)

If all variants of an enum share a field name and type, allow direct access:

```tml
// If every Expr variant has a .span field:
let s = expr.span  // no when needed
```

This requires compile-time verification that the field exists in all variants.

## C++ Compiler Changes

1. **Verify** `_` wildcard already works in all when-expression positions
2. **Common field detection**: At type-check time, detect fields shared by all variants
3. **Direct access codegen**: Generate efficient field extraction without full match
