# Tasks: Fix Struct Codegen Blockers

**Status**: COMPLETE
**Updated**: 2026-03-20

## Bug 1: struct-with-generic-field GEP
- [x] 1.1 Verified: struct with List[I64] field compiles and runs correctly
- [x] 1.2 Verified: nested structs (outer.inner.items) work

## Bug 2: ptr_read/ptr_write for multi-field structs
- [x] 2.1 Diagnose: emit IR for ptr_read[MyStruct] with 2-4 I64 fields
- [x] 2.2 Fix: register ptr_read/write with type_params={"T"} in mem.cpp
- [x] 2.3 Fix: get_expr_type handles LowlevelExpr and generic substitution
- [x] 2.4 Fix: add type_args field to CallInst, propagate through MIR
- [x] 2.5 Fix: ptr_read/write handlers use type_args[0] for element type
- [x] 2.6 Test: ptr_read/ptr_write roundtrip 3-field struct (I64 x3)
- [x] 2.7 Test: ptr_read/ptr_write mixed types (I64 + I32 + Bool)

## Bug 3: struct field mutation codegen
- [x] 3.1 Diagnose: mutable struct alloca was dead code in thir_mir_builder.cpp
- [x] 3.2 Fix: moved alloca check inside binding pattern block
- [x] 3.3 Test: mutate I32 field (p.x=99, p.y=20)
- [x] 3.4 Test: mutate field of struct read via ptr_read (count=99, active=true)

## Bug 4: integer literal coercion in function pointer calls
- [x] 4.1 Diagnose: f(42) generated call i64 %fn(i32 42) — no sext coercion
- [x] 4.2 Fix: added sext at 3 call sites in call.cpp (field fat, ident fat, ident thin)
- [x] 4.3 Test: closure + List in same function works
- [x] 4.4 Test: apply(do(x) { x * 2 }, 21) returns 42

## Bug 5: List[func(T)] stride
- [x] 5.1 Verified: List[I64] with function pointers cast to/from I64 works

## Bug 6: Iterator::fold[B] method-level generic monomorphization
- [x] 6.1 Diagnose: generate_default_method rejected methods with generics
- [x] 6.2 Fix: allow method-level generics when all params have substitutions
- [x] 6.3 Fix: type param inference miscounted impl_param_count
- [x] 6.4 Fix: added GenericType handling and dynamic param_offset
- [x] 6.5 Test: fold(0, closure) prints OK (sum=60)
