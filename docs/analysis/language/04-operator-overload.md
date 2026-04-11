# F04: Operator Overloading & Index Syntax

**Priority**: High
**Impact**: 20+ method-based operations that should be operators
**Complexity**: Medium (behavior resolution + codegen)

## Problem

No operator overloading means numeric types use method calls:

```tml
let result = a.add(b).mul(c)   // instead of: a + b * c
let val = map.get(key)          // instead of: map[key]
```

This makes mathematical code unreadable and deviates from user expectations.

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `lib/std/src/bigint.tml` | 271-292 | `.add()`, `.sub()` instead of `+`, `-` |
| `lib/std/src/collections/hashmap.tml` | 227-250 | `.get()` instead of `[]` |
| `lib/std/src/collections/btreemap.tml` | 60-80 | `.get()`, `.set()` methods |
| `lib/core/src/str/*.tml` | throughout | String concatenation via methods |

## Proposal

### A. Arithmetic operator behaviors

```tml
behavior Add[Rhs = This] {
    type Output
    func add(this, rhs: Rhs) -> This::Output
}

// Usage: a + b desugars to a.add(b)
impl Add for BigInt {
    type Output = BigInt
    func add(this, rhs: BigInt) -> BigInt { ... }
}
```

Standard operator → behavior mapping:
| Operator | Behavior | Method |
|----------|----------|--------|
| `+` | `Add` | `add` |
| `-` | `Sub` | `sub` |
| `*` | `Mul` | `mul` |
| `/` | `Div` | `div` |
| `%` | `Rem` | `rem` |
| `==` | `PartialEq` | `eq` |
| `<` | `PartialOrd` | `partial_cmp` |
| `[]` | `Index` | `index` |
| `[]=` | `IndexMut` | `index_mut` |

### B. Index syntax

```tml
behavior Index[Idx] {
    type Output
    func index(ref this, idx: Idx) -> ref This::Output
}

// Usage: map[key] desugars to map.index(key)
```

## C++ Compiler Changes

1. **Type checker**: Resolve binary operators to behavior method calls
2. **HIR**: Desugar `a + b` → `Add::add(a, b)` when `a` is not a primitive
3. **Precedence**: Maintain existing operator precedence; operator behaviors just provide the method
4. **Primitives**: Keep I32, I64, F64 etc. using built-in ops (no behavior dispatch overhead)
