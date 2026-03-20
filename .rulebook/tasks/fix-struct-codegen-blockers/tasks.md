# Tasks: Fix Struct Codegen Blockers

**Status**: Not Started
**Priority**: High — blocks 15 refactor items across HTTP, streams, async, events
**Updated**: 2026-03-20
**Origin**: Extracted from `refactor-async-use-existing-apis` — items that cannot be implemented without compiler C++ fixes

---

## Bug 1: struct-with-generic-field GEP (5 blocked items)

When a struct has a field of a generic type (e.g., `List[I64]`, `HashMap[Str, Str]`), accessing
that field via GEP generates invalid LLVM IR. The compiler produces wrong type indices
for the struct layout when generic types are involved.

**Repro**: Any struct with `field: List[I64]` as a field, then accessing `this.field.push(x)`.

- [ ] 1.1 Diagnose: emit IR for a minimal struct-with-List-field test case, identify the invalid GEP
- [ ] 1.2 Fix GEP generation in MIR codegen for structs containing generic-instantiated fields
- [ ] 1.3 Test: struct with List[I64] field — construct, push, get, destroy
- [ ] 1.4 Test: struct with HashMap[Str, Str] field — construct, set, get, destroy
- [ ] 1.5 Test: nested struct (outer has inner which has List field)

**Unblocks**: 2.8 (App route tables), 2.9 (queue with Mutex[RingBuffer]), 6.1-6.10 (EventLoop List fields), 8.1-8.2 (events HashMap[Str, List[I64]]), 11.1 (H2StreamTable with List[I64])

## Bug 2: ptr_read/ptr_write for multi-field structs (4 blocked items)

`ptr_read[T]` where T is a struct with 2+ fields fails with "Type mismatch: expected T, found I32".
The codegen generates an I32 load instead of a struct load. Only single-field structs work.

**Repro**: `let s: MyState = lowlevel { ptr_read[MyState](ptr as *MyState) }` where MyState has 2+ I64 fields.

- [ ] 2.1 Diagnose: emit IR for ptr_read[MyStruct] with 2-4 I64 fields
- [ ] 2.2 Fix codegen to emit correct LLVM struct load for multi-field ptr_read
- [ ] 2.3 Fix codegen to emit correct LLVM struct store for multi-field ptr_write
- [ ] 2.4 Test: ptr_read/ptr_write roundtrip for 2, 4, 8 field structs
- [ ] 2.5 Test: ptr_read/ptr_write for struct with mixed field types (I64 + I32 + Bool)

**Unblocks**: 2.1-2.4 (SharedState), 2.5 (ConnectionSlot), 5.2 (ReadableStream handle), 5.4 (WritableStream handle)

## Bug 3: struct field mutation codegen (3 blocked items)

`var s: MyStruct = ...; s.field = new_value` generates invalid IR — the store target type
mismatches (`store i32 999, ptr %v37` where %v37 is i64).

**Repro**: `var s: MyState = MyState { a: 10 }; s.a = 999` — crashes with LLVM IR parse error.

- [ ] 3.1 Diagnose: emit IR for struct field mutation, identify the type mismatch
- [ ] 3.2 Fix codegen to emit correct typed store for struct field assignment
- [ ] 3.3 Test: mutate I64 field, I32 field, Bool field, nested struct field
- [ ] 3.4 Test: mutate field of struct read via ptr_read (after Bug 2 is fixed)

**Unblocks**: 2.1-2.4 (SharedState field updates), 2.9 (RingBuffer head/tail mutation), 5.2/5.4 (stream handle field updates)

## Bug 4: cross-module closure symbol emission (1 blocked item)

Closures passed as function parameters across module boundaries don't emit their LLVM symbols.
The closure body is compiled but the symbol is not exported from the defining module's object file.

**Repro**: Module A defines `func map[T](items: List[T], f: func(T) -> T)`. Module B calls `map(list, do(x) x + 1)`. The closure `do(x) x + 1` compiles but its LLVM symbol is not found at link time.

- [ ] 4.1 Diagnose: emit IR for both modules, check symbol tables
- [ ] 4.2 Fix symbol emission for cross-module closures in MIR codegen
- [ ] 4.3 Test: closure parameter across module boundary
- [ ] 4.4 Test: generic function with closure parameter across modules

**Unblocks**: 8.6 (iterator adapters with closures)

## Bug 5: List[func(T)] stride calculation (1 blocked item)

When a List stores function pointers `func(T)` as elements, the stride calculation is incorrect.
Function pointers should be 8 bytes (I64) but the codegen computes a different stride.

- [ ] 5.1 Diagnose: emit IR for List[func(I32)] push/get, check stride
- [ ] 5.2 Fix stride calculation for function pointer types in List/collection codegen
- [ ] 5.3 Test: List of function pointers — push, get, call

**Unblocks**: 8.3-8.5 (observable with List[func(T)] subscribers)

## Downstream: Refactor items unblocked by these fixes

Once the bugs above are fixed, these refactor items from `refactor-async-use-existing-apis` become implementable:

| Bug Fixed | Refactor Items Unblocked |
|-----------|--------------------------|
| Bug 1 (GEP) | 2.8, 2.9, 6.1-6.10, 8.1-8.2, 11.1 |
| Bug 2 (ptr_read) | 2.1-2.5, 5.2, 5.4 |
| Bug 3 (mutation) | 2.1-2.4, 2.9, 5.2, 5.4 |
| Bug 4 (closures) | 8.6 |
| Bug 5 (stride) | 8.3-8.5 |

## Note: Items NOT blocked by codegen

These items from the refactor task are architectural constraints, not compiler bugs:
- **7.1-7.9** (executor) — state lives in C runtime, requires ROADMAP Phase 4 migration
- **10.4** (async_udp) — FFI interop pattern, typed struct adds no value
- **5.8-5.12** (async_buffered) — needs Buffer compact/drain API (library feature, not compiler bug)
