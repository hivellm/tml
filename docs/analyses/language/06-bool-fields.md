# F06: Bool Struct Fields (i1 Layout Bug)

**Priority**: Medium
**Impact**: 15+ workarounds using I64 for booleans
**Complexity**: Medium (LLVM type layout fix)

## Problem

Bool fields in structs cause LLVM layout bugs (i1 type is 1 bit but struct
alignment expects byte-sized fields). The workaround is using `I64` for all
boolean fields, losing type safety:

```tml
pub type FuncParam {
    is_this: I64,   // Should be Bool
    is_mut: I64,    // Should be Bool
}

pub type ServerResponse {
    headers_sent: I64,  // 0=false, 1=true (Bool i1 layout workaround)
}
```

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `compiler-tml/src/ast/decls.tml` | 31-45 | FuncParam with I64 bools |
| `compiler-tml/src/ast/exprs.tml` | 234-236 | ClosureExpr.is_move: I64 |
| `compiler-tml/src/ast/exprs.tml` | 287-291 | RangeExpr.inclusive: I64 |
| `lib/std/src/http/server/server_response.tml` | 93 | Explicit workaround comment |

## Proposal

Fix the LLVM codegen to represent Bool as `i8` in struct layouts (not `i1`):

```llvm
; Current (broken):
%FuncParam = type { %Str, i1, i1 }    ; misaligned

; Fixed:
%FuncParam = type { %Str, i8, i8 }    ; proper byte alignment
```

Then all `I64` boolean workarounds can be replaced with proper `Bool`:

```tml
pub type FuncParam {
    is_this: Bool,
    is_mut: Bool,
}
```

## C++ Compiler Changes

1. **Type lowering**: Map `Bool` → `i8` in struct contexts (keep `i1` for registers/conditions)
2. **Load/store**: Insert `zext i8 → i1` on load, `trunc i1 → i8` on store
3. **Struct layout**: Use `i8` alignment for Bool fields (1 byte, not 1 bit)
