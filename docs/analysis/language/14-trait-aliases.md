# F14: Trait/Behavior Aliases

**Priority**: Low
**Impact**: Generic constraint readability
**Complexity**: Low

## Problem

Complex trait bounds are repeated verbatim across multiple functions:

```tml
pub func eq[I: Iterator](mut this, mut other: I) -> Bool
    where This::Item: PartialEq, I::Item = This::Item

pub func cmp[I: Iterator](mut this, mut other: I) -> Ordering
    where This::Item: Ord, I::Item = This::Item
```

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `lib/core/src/iter/traits/iterator.tml` | 235, 259 | Repeated where-clauses |
| `lib/std/src/collections/btreemap.tml` | 32 | `K: Ord` on every impl |

## Proposal

```tml
behavior Numeric = Add + Sub + Mul + Div + PartialOrd

func sum[T: Numeric](items: List[T]) -> T { ... }
```

## C++ Compiler Changes

1. **Parser**: `behavior Name = Bound1 + Bound2` syntax
2. **Type checker**: Expand alias during constraint checking
3. **No codegen change**: Pure syntactic sugar resolved at type-check time
