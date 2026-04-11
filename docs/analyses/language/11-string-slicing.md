# F11: String Slicing Syntax

**Priority**: Low
**Impact**: String operations
**Complexity**: Low-Medium

## Problem

String slicing requires method calls; no slice syntax exists:

```tml
let sub = str::slice(s, start, end)   // function call
// Desired: s[start..end]
```

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `lib/core/src/str/split.tml` | throughout | Manual index + slice calls |
| `compiler-tml/src/parser/*.tml` | throughout | Token text extraction |

## Proposal

> **IMPORTANT**: TML uses `to`/`through` for ranges, NOT `..`/`..=` (RFC-0002 §1.4, §3.3).

```tml
let sub = s[2 to 5]          // slice from index 2 to 5 (exclusive)
let sub = s[2 through 5]     // slice from index 2 to 5 (inclusive)
```

For open-ended ranges, a sentinel or method may be needed:
```tml
let sub = s.slice_from(2)    // from index 2 to end
let sub = s.slice_to(5)      // from start to 5
```

Desugars to `Index` behavior with `Range` argument (depends on F04 operator overloading).

## C++ Compiler Changes

1. **Range type**: Core `Range { start: I64, end: I64 }` with `to`/`through` constructors
2. **Index behavior**: Implement `Index[Range]` for `Str` (requires F04)
3. **Parser**: `to`/`through` inside `[ ]` produce Range expressions (already LL(1)-safe — `to` and `through` are keywords)
