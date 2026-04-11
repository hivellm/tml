# F03: Pattern Guards in When/Match

**Priority**: High — **ALREADY IN SPEC** (RFC-0002 §2.5: `AndPattern <- PrimaryPattern ('if' Expr)?`)
**Impact**: 40+ nested when-expressions
**Complexity**: Medium (parser + pattern matching codegen)
**Status**: Compiler implementation gap — `if` guards and `if let` already defined in grammar

## Problem

When-expressions cannot have guard conditions, forcing nested patterns:

```tml
when ty {
    Nothing => {
        when init {
            Nothing => return Err(error),
            _ => {}
        }
    },
    _ => {}
}
```

Should be:

```tml
when (ty, init) {
    (Nothing, Nothing) => return Err(error),
    _ => {}
}
```

Or with guards:

```tml
when value {
    Just(x) if x > 0 => handle_positive(x),
    Just(x) => handle_nonpositive(x),
    Nothing => handle_missing(),
}
```

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `compiler-tml/src/parser/parse_stmt.tml` | 132-140 | Nested when for two Maybe values |
| `compiler-tml/src/types/infer/unify.tml` | 145-237 | 80+ lines of nested when |
| `compiler-tml/src/parser/parse_expr.tml` | throughout | Nested when for precedence checks |
| `lib/core/src/iter/traits/iterator.tml` | 186-189 | let-else workaround for guards |

## Proposal

### A. Pattern guards with `if`

```tml
when expr {
    Pattern(x) if condition => body,
    Pattern(x) => fallback,
}
```

### B. Tuple pattern matching

```tml
when (a, b) {
    (Just(x), Just(y)) => combine(x, y),
    (Just(x), Nothing) => use_x(x),
    (Nothing, _) => default(),
}
```

### C. If-let expressions

```tml
if let Just(x) = expr {
    use(x)
} else {
    handle_none()
}
```

## C++ Compiler Changes

1. **Parser**: Allow `if <expr>` after pattern in when-arms
2. **HIR**: Desugar guard into conditional branch after successful pattern match
3. **MIR**: Emit guard condition check between pattern match and arm body
4. **Tuple patterns**: Already partially supported; extend to when-expressions
