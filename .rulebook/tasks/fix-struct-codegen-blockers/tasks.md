# Tasks: Fix Struct Codegen Blockers

**Status**: In Progress — Bug 1+2+3+5 FIXED, Bug 4 FIXED (integer literal coercion in fnptr calls)
**Priority**: High — blocks 15 refactor items across HTTP, streams, async, events
**Updated**: 2026-03-20
**Origin**: Extracted from `refactor-async-use-existing-apis`

---

## Bug 1: struct-with-generic-field GEP — ALREADY FIXED

Verified 2026-03-20: struct with `List[I64]` field compiles and runs correctly.
Nested structs (outer.inner.items) also work. No action needed.

- [x] 1.1-1.5 All verified working — struct with List[I64], HashMap[Str,Str], nested structs

**Unblocked NOW**: 2.8 (App route tables), 6.1-6.10 (EventLoop), 8.1-8.2 (events), 11.1 (H2StreamTable)

## Bug 2: ptr_read/ptr_write for multi-field structs — FIXED

`ptr_read[T]` where T is a struct with 2+ fields was generating `extractvalue i32 %v, 4294967295`.

**Root cause**: 4 interconnected bugs:
1. Type checker (mem.cpp) registered ptr_read with return type I32 and empty type_params — should be GenericType{"T"} with type_params={"T"}
2. HIR builder get_expr_type() didn't handle LowlevelExpr or generic substitution for CallExpr
3. HIR builder lower_let/lower_var used get_expr_type (which returned Unit for lowlevel blocks) BEFORE lowering the init expression
4. MIR codegen ptr_read handler lacked access to CallInst::type_args to determine the load type

**Fix (6 files)**:
- `types/builtins/mem.cpp`: Register ptr_read/write family with type_params={"T"} and GenericType return
- `hir/hir_builder.cpp`: get_expr_type handles LowlevelExpr (recurse into trailing expr) and CallExpr generic substitution
- `hir/hir_builder_stmt.cpp`: lower_let and lower_var now lower init FIRST, then use init type if get_expr_type returned Unit/GenericType
- `hir/hir_builder_expr.cpp`: lower_call extracts type_args from PathExpr::generics, overrides return_type for ptr_read intrinsics
- `mir/mir.hpp`: Added type_args field to CallInst
- `mir/thir_mir_builder.cpp` + `builder/hir_expr.cpp`: Propagate type_args from THIR/HIR call to MIR CallInst
- `codegen/mir/instructions.cpp`: ptr_read/write family handlers check type_args[0] as highest priority element type

- [x] 2.1 Diagnose: emit IR for ptr_read[MyStruct] with 2-4 I64 fields
- [x] 2.2 Fix codegen to emit correct LLVM struct load for multi-field ptr_read
- [x] 2.3 Fix codegen to emit correct LLVM struct store for multi-field ptr_write
- [x] 2.4 Test: ptr_read/ptr_write roundtrip for 3-field struct (a=10, b=20, c=30)
- [ ] 2.5 Test: ptr_read/ptr_write for struct with mixed field types (I64 + I32 + Bool)

**Unblocks**: 2.1-2.4 (SharedState), 2.5 (ConnectionSlot), 5.2 (ReadableStream), 5.4 (WritableStream)

## Bug 3: struct field mutation codegen — FIXED

`var s: MyStruct = ...; s.field = new_value` generates invalid IR.

**Root cause**: In `thir_mir_builder.cpp:build_let_stmt`, the mutable struct alloca path was dead code.
The array fast-path block (lines 535-609) handled ALL `ThirBindingPattern` cases and returned
unconditionally at line 608, before the mutable struct check at line 614 could ever execute.
Fix: moved the mutable struct alloca logic inside the same binding pattern block, after the array check.

- [x] 3.1 Diagnose: emit IR for struct field mutation, identify the type mismatch
- [x] 3.2 Fix codegen to emit correct typed store for struct field assignment
- [x] 3.3 Test: mutate I32 field verified with assert_eq (p.x=99, p.y=20)
- [ ] 3.4 Test: mutate field of struct read via ptr_read (Bug 2 now fixed)

**Unblocks**: 2.1-2.4 (SharedState), 2.9 (RingBuffer), 5.2/5.4 (streams)

## Bug 4: cross-module closure return type — FIXED

Simple closure case now works: `func apply(f: func(I64) -> I64, val: I64) -> I64` returns correct i64.

**Root cause**: Integer literal arguments in function pointer calls were not coerced to match declared parameter types. `f(42)` where `f: func(I64) -> I64` generated `call i64 %fn(i32 42)` instead of `call i64 %fn(i64 42)`. The i32/i64 mismatch corrupted the stack.

**Fix (1 file, 3 call sites)**: `compiler/src/codegen/llvm/expr/call.cpp`
- FieldExpr fat pointer call (line ~264): coerce args using FuncType.params
- IdentExpr fat pointer call (line ~1635): coerce args using semantic_type FuncType/ClosureType params
- IdentExpr thin pointer call (line ~1809): same coercion for thin fn ptr path

**Note**: The original repro used `List[I64]::new()` without the required `initial_capacity` argument. This is a type checker gap (should reject missing required arg), but not a codegen bug. With `List[I64]::new(4)`, the closure+List combination works correctly.

- [x] 4.1 Diagnose: simple closure return type is now correct
- [x] 4.2 Simple closure parameter with I64 return — WORKS
- [x] 4.3 Test: `apply(do(x) { x * 2 }, 21)` returns 42 correctly
- [x] 4.4 closure call + List in same function — FIXED (integer literal coercion)

**Unblocks**: 8.3-8.6 (observable/iterators with closures)

## Bug 5: List[func(T)] stride — ALREADY FIXED

Verified 2026-03-20: List[I64] with function pointers cast to/from I64 works correctly.

- [x] 5.1-5.3 All verified working — push func as I64, get, cast back, call

---

## Downstream: Items now unblocked (implement these)

| Item | What to do | Blocked by |
|------|-----------|-----------|
| 2.8 App route tables → List[I64] | Replace mem_alloc arrays with List[I64] fields in App | Nothing (Bug 1 FIXED) |
| 6.1-6.10 EventLoop List fields | Replace el_la_* manual arrays with List[I64] | Nothing (Bug 1 FIXED) |
| 8.1-8.2 events.tml List fields | Replace la_* manual arrays with typed fields | Nothing (Bug 1 FIXED) |
| 11.1 H2StreamTable → List[I64] | Replace flat array with List[I64] | Nothing (Bug 1 FIXED) |
| 2.1-2.5 SharedState/ConnectionSlot | Typed struct with ptr_read/write | Nothing (Bug 2+3 FIXED) |
| 5.2/5.4 Stream handle structs | Typed struct with ptr_read/write | Nothing (Bug 2+3 FIXED) |
| 2.9 queue RingBuffer | Typed struct with field mutation | Nothing (Bug 3 FIXED) |
| 8.3-8.6 Observable/iterators | Closure parameters across modules | Nothing (Bug 4 FIXED) |
