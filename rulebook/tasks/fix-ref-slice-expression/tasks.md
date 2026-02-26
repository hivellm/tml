# Tasks: Fix ref slice expression (C026)

**Status**: Complete (100%)

## Phase 1: AST Codegen Fix

- [x] 1.1 Add IndexExpr handling in Ref case of gen_unary (unary.cpp)
- [x] 1.2 Handle RangeExpr index: create fat pointer {ptr, i64} for ref arr[start to end]
- [x] 1.3 Handle simple index: return GEP element pointer for ref arr[i]
- [x] 1.4 Rebuild compiler and verify IR output

## Phase 2: MIR Codegen Fix (related)

- [x] 2.1 Add used_struct_types_ tracking in mir_codegen.hpp
- [x] 2.2 Collect struct types from StructInitInst in generate() first pass
- [x] 2.3 Collect struct types from StructInitInst in generate_cgu() first pass
- [x] 2.4 Emit type declarations for undeclared imported structs in emit_type_defs()

## Phase 3: Testing

- [x] 3.1 Verify ref buf[0 to 3] static range from start
- [x] 3.2 Verify ref buf[2 to 5] static range from offset
- [x] 3.3 Verify ref buf[start to end] dynamic range with variables
- [x] 3.4 Verify ref buf[i] single element reference with mutation
- [x] 3.5 Verify IR correctness via emit-ir

## Phase 4: Type Checker Fixes

- [x] 4.1 MIR bounds-check: skip icmp when idx_type is struct (instructions.cpp)
- [x] 4.2 Unwrap RefType in check_index() for ref [T] indexing (expr.cpp)
- [x] 4.3 Unwrap RefType before SliceType method resolution for ref [T].len() (expr_call_method.cpp)
- [x] 4.4 Verify .len() returns I64 and data[0] returns T via legacy IR
- [x] 4.5 Full test suite — no regressions
