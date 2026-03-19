---
name: Thread Spawn Implementation
description: How thread spawning works in TML — trampoline pattern, type-specialized handles, known blockers
type: project
---

## Thread Spawn — Implemented 2026-03-19

**Key Files**:
- `lib/std/src/thread/mod.tml` — main thread module
- `compiler/runtime/concurrency/sync.c` — C runtime `tml_thread_spawn`

### C ABI
`tml_thread_spawn(void* func_ptr, void* arg, uint64_t stack_size) -> uint64_t`
- The C runtime allocates `{func_ptr, arg}`, creates OS thread with `tml_thread_entry` which calls `func(arg)`
- Return value is raw HANDLE (Windows) or pthread_t (Unix) as uint64_t
- `@extern` must use `*Unit` params and `U64` return (NOT RawPtr[T] or struct types)

### Trampoline Pattern
1. `spawn_fn`/`spawn_i64` allocates 40-byte state block on heap
2. Stores user's function pointer (cast to I64) at offset 0
3. Stores result pointer at offset 16
4. Passes module-level `thread_entry_unit`/`thread_entry_i64` as thread entry point
5. Entry wrapper reads fn_ptr from state, casts to `func() -> T`, calls it, stores result
6. `join()` waits for thread, reads result from state block, frees memory

### Why Type-Specialized (not generic)
- `impl[T] JoinHandle[T]` methods fail with "Unknown method" — generic type dispatch bug
- `Builder::spawn[T]` also fails — generic methods on non-generic types don't resolve
- Workaround: `UnitJoinHandle` and `I64JoinHandle` as concrete types with concrete impls
- Generic `spawn[T]`/`JoinHandle[T]` exist in code but are currently unusable

### Import Pattern (CRITICAL)
Tests MUST explicitly import the concrete types:
```tml
use std::thread::{spawn_fn, UnitJoinHandle}
let handle: UnitJoinHandle = spawn_fn(do() { ... })
```
Without explicit type annotation, method resolution fails.

### Known Blockers
- `Thread.name()` crashes — `Maybe[Str]` field access from struct causes ACCESS_VIOLATION
- Old thread tests crash with stale incremental cache after module refactor
- Generic spawn[T] blocked by compiler generic method dispatch
