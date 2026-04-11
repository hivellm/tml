# F13: Safe Numeric Conversions

**Priority**: Low
**Impact**: Arithmetic code safety
**Complexity**: Low

## Problem

Numeric conversions use `as` which is unchecked:

```tml
let val: I64 = n
if n < 0 {
    val = 0 - n  // Manual negation, overflow at I64::MIN
}

let len = list.len() as I64  // U64 → I64 truncation
```

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `lib/std/src/bigint.tml` | 61-86 | Manual negation, I64::MIN special case |
| `compiler-tml/src/parser/*.tml` | throughout | `len() as I64` casts |

## Proposal

### Safe conversion methods

```tml
let val: I64 = n.abs()                    // built-in abs
let safe: Maybe[I32] = big_val.try_into() // checked narrowing
let wide: I64 = small_val.into()          // widening (always safe)
```

### ~~Compiler-checked widening~~ — REJECTED (VIOLATES NO-COERCION RULE)

> **REJECTED**: RFC-0001 and ADR-008 require explicit `as` for ALL type
> conversions. Implicit widening (I32 → I64 without `as`) introduces silent
> coercions that contradict TML's explicit-everything philosophy. It also
> makes integer literal type inference context-dependent, complicating both
> the type checker and LLM code generation.
>
> **`as` must remain mandatory for all numeric conversions.**

```tml
// REJECTED — DO NOT IMPLEMENT
let x: I64 = some_i32   // implicit coercion — FORBIDDEN
foo(42)                  // is 42 an I32 or I64? — AMBIGUOUS

// CORRECT — explicit as
let x: I64 = some_i32 as I64
foo(42 as I64)
```

## C++ Compiler Changes

1. **Built-in methods**: `.abs()`, `.min()`, `.max()` on integer types (method calls, not coercions)
2. ~~Implicit widening~~ — **REJECTED** (violates no-implicit-coercion rule)
3. **Into/TryInto behaviors**: Standard conversion behaviors (explicit `.into()` / `.try_into()` calls)
