# F10: Lowlevel Block Abstractions

**Priority**: Medium
**Impact**: Collections, FFI code
**Complexity**: Medium

## Problem

Pointer operations in `lowlevel` blocks are verbose and error-prone:

```tml
let entries_addr: I64 = lowlevel { ptr_read[I64](hdr as *I64) }
let ctrl_addr: I64 = lowlevel { ptr_read[I64]((hdr + 8) as *I64) }
let capacity: I64 = lowlevel { ptr_read[I64]((hdr + 16) as *I64) }
let len: I64 = lowlevel { ptr_read[I64]((hdr + 24) as *I64) }
```

Manual offset arithmetic is repeated throughout collections code.

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `lib/std/src/collections/hashmap.tml` | 66-84 | Manual header layout |
| `lib/std/src/collections/hashmap.tml` | 156-161 | Repeated offset reads |
| `lib/std/src/bigint.tml` | 96, 106, 126 | ptr_read byte patterns |

## Proposal

### ~~A. Inline lowlevel expressions~~ — REJECTED (BREAKS LL(1))

> **REJECTED**: `lowlevel` is currently always followed by `{`, which gives the
> parser a clear block boundary. Removing the braces makes it impossible to
> determine where the lowlevel expression ends without unbounded lookahead:
> `lowlevel ptr_read[I64](p) + 1` — is `+ 1` inside or outside lowlevel?
> The `{ }` delimiter is essential for LL(1) parsing.

```tml
// REJECTED — DO NOT IMPLEMENT
let addr = lowlevel ptr_read[I64](ptr as *I64)  // where does lowlevel end?
```

### B. Pointer offset syntax

```tml
// Instead of: (hdr + 8) as *I64
// Allow: hdr.offset[I64](1)  or  ptr_at[I64](hdr, 1)
```

### C. @packed struct for layout control

```tml
@packed
pub type Header {
    entries: *Entry,    // offset 0
    ctrl: *U8,          // offset 8
    capacity: I64,      // offset 16
    len: I64,           // offset 24
}
```

## C++ Compiler Changes

1. ~~Allow lowlevel expressions without braces~~ — **REJECTED** (breaks LL(1))
2. **Pointer arithmetic helpers** as compiler built-ins (library, not syntax)
3. **@packed attribute** for C-compatible struct layout with known offsets (uses existing decorator syntax)
