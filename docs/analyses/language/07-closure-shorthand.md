# F07: Closure Shorthand Syntax

**Priority**: Medium
**Impact**: Iterator API usability
**Complexity**: Medium (parser + type inference)

## Problem

Closures require full type annotations, making functional-style code verbose:

```tml
// Current
list.iter().filter(do(x: I64) -> Bool { return x > 0 })
list.iter().map(do(x: I64) -> Str { return x.to_string() })

// Desired
list.iter().filter(do(x) { x > 0 })
list.iter().map(do(x) { x.to_string() })
```

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `lib/core/src/iter/traits/iterator.tml` | 56-284 | 15 methods taking `func(T) -> U` |
| `lib/std/src/cli.tml` | builder methods | Callback-heavy API |

## Proposal

### A. Type inference for closure parameters — DEBATABLE

> **Ambiguity note**: ADR-008 mandates "mandatory type annotations on all
> declarations." Omitting closure param types relaxes this principle. However,
> the grammar production `ClosureParam <- 'mut'? Ident (':' Type)?` makes
> the `:` optional, so **LL(1) parsing is preserved** — the parser peeks
> `,` or `)` after `Ident` to know there's no type. The tension is with
> the *design philosophy*, not the grammar.
>
> **Decision needed**: Allow type inference in closures only when passed to
> a function with a known signature? This is a scoped exception, not a
> blanket removal of mandatory annotations.

When the closure is passed to a function with known parameter types, infer
the closure's parameter and return types:

```tml
// 'filter' expects func(This::Item) -> Bool
// So do(x) { x > 0 } infers x: This::Item, return: Bool
list.iter().filter(do(x) { x > 0 })
```

### B. Implicit return for single-expression closures

```tml
do(x) { x + 1 }       // equivalent to do(x) -> I64 { return x + 1 }
do(x, y) { x == y }   // equivalent to do(x, y) -> Bool { return x == y }
```

### ~~C. Placeholder syntax~~ — REJECTED (AMBIGUOUS)

> **REJECTED**: `_` is already the wildcard/discard pattern in TML (RFC-0002
> §2.5: `WildcardPattern <- '_'`). Using `_` as an anonymous closure parameter
> in expression position creates ambiguity — the parser cannot determine whether
> `_` means "discard" or "implicit parameter" without unbounded context.
> This violates LL(1).

```tml
// REJECTED — DO NOT IMPLEMENT
list.iter().filter(_ > 0)      // _ = discard or param?
list.iter().map(_.to_string()) // ambiguous
```

## C++ Compiler Changes

1. **Type checker**: Propagate expected function type into closure type checking
2. **HIR**: Allow omitted parameter types in closures; fill in from context
3. **Implicit return**: If closure body is a single expression, treat as return value
