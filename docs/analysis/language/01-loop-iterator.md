# F01: For-each Loops & Range Iteration

**Priority**: Critical — **ALREADY IN SPEC** (RFC-0002 §2.3, §3.4)
**Impact**: 30+ repetitive patterns across codebase
**Complexity**: Medium (parser + HIR desugaring + codegen)
**Status**: Compiler implementation gap — syntax is fully specified but not yet implemented

## Problem

Every loop over a collection requires manual index management:

```tml
var i: I64 = 0
loop (i < list.len() as I64) {
    let item = list.get(i)
    // ... body
    i = i + 1
}
```

This 5-line boilerplate repeats 30+ times in the parser alone and throughout
the standard library. It is error-prone (forgetting `i = i + 1` causes infinite
loops) and obscures intent.

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `compiler-tml/src/parser/parse_expr.tml` | 200-203 | Manual index loop over args |
| `lib/std/src/bigint.tml` | 226-235, 249, 290, 325 | 5+ index loops in arithmetic |
| `lib/std/src/collections/btreemap.tml` | 106-114, 118-125 | keys()/values() iteration |
| `lib/std/src/cli.tml` | 175-185 | Manual argv construction |
| `compiler-tml/src/ast/ast_writer.tml` | throughout | Repeated list traversals |

## Proposal

### A. For-each syntax

```tml
for item in list {
    process(item)
}
```

Desugars to iterator protocol: calls `.iter()` → loop calling `.next()` until `Nothing`.

### B. Range syntax (uses TML keywords `to`/`through`, NOT `..`/`..=`)

```tml
for i in 0 to n {           // exclusive (0, 1, ..., n-1)
    process(list.get(i))
}

for i in 0 through n {      // inclusive (0, 1, ..., n)
    process(i)
}
```

> **IMPORTANT**: TML uses `to` (exclusive) and `through` (inclusive) for ranges
> per RFC-0002 §1.4 and §3.3. Using `..`/`..=` would violate TML's keyword-over-
> symbol design principle and is **NOT permitted**.

### C. Enumerate

```tml
for (i, item) in list.iter().enumerate() {
    // i: I64, item: T
}
```

## C++ Compiler Changes

1. **Parser**: Implement the already-specified `ForExpr <- 'for' Pattern 'in' Expr Block` (RFC-0002 §2.3)
2. **HIR**: Implement the desugaring rules from RFC-0002 §3.4 (range loops via `to`/`through`, iterator loops via `into_iter()/.next()`)
3. **Range type**: Add `Range { start: I64, end: I64 }` with `Iterator` impl to core
4. **Behavior**: Require `IntoIterator` behavior with `into_iter()` method for for-in targets

## Alternatives Considered

- **Macro-based**: Too complex, TML has no macro system yet
- **Special loop syntax**: `loop item in list` — confuses with existing `loop (cond)` syntax
- **Method-only**: `.for_each(do(x) { ... })` — verbose, no break/continue support
