---
name: Nullable Maybe[ref T] double-load crash fix
description: method.cpp:973 had spurious load for nullable-ptr-optimized Maybe, causing ACCESS_VIOLATION on is_just/is_nothing/unwrap
type: project
---

## Bug: Nullable Maybe[ref T] double-load causes ACCESS_VIOLATION

**Root cause**: In `compiler/src/codegen/llvm/expr/method.cpp:973-983`, the Maybe method handler
assumed `last_expr_type_ == "ptr"` meant "the receiver is a pointer TO a Maybe value that needs loading".
But for nullable-ptr-optimized `Maybe[ref T]`, the ptr IS the Maybe value itself. Loading from it
dereferences the pointer (reading whatever memory is there), which crashes.

**Fix**: Removed the `load ptr, ptr %receiver` for the `enum_type_name == "ptr"` case. The receiver
from `gen_expr` is always the already-loaded nullable pointer value.

**Why**: `gen_ident` at `core.cpp:209-213` always does `load ptr, ptr %alloca` when reading a local
variable. So by the time the method handler sees the receiver, it's already the value, not an alloca ptr.

**How to apply**: Any future nullable-ptr-optimized enum dispatch must NOT add an extra load.
The `last_expr_type_ == "ptr"` heuristic only works for struct-based enums where the receiver
might be a ptr-to-struct that needs loading.

**Tests fixed**: core/array (20), core/slice (21), core/cell (27), core/iter (52), core/types (7),
core/option (23/25), core/result (17/19). The remaining failures are pre-existing IR bugs
(Maybe__Unit invalid GEP, Outcome void store).
