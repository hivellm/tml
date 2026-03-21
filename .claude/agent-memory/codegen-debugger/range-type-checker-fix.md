---
name: range-type-checker-fix
description: Range type checker fix - check_range() returned Slice[I64] instead of Range[T], causing method return types to resolve as ()
type: project
---

## Range Type Checker Fix (2026-03-20)

### Bug
`check_range()` in `compiler/src/types/checker/control.cpp` returned `Slice[I64]` for ALL range expressions.
This caused method calls on Range types (size_hint, count, next) to resolve return types as `()`.

### Root Cause
Line 252 of control.cpp: `return make_slice(make_primitive(PrimitiveKind::I64));`
All range expressions (`0..10`, `1..=5`) returned `Slice[I64]` regardless of actual element type.

### Fix
- `check_range()`: Returns `Range[T]` for `..` and `RangeInclusive[T]` for `..=`/`through`
- `check_for()`: Added `Range[T]`, `RangeInclusive[T]`, `RangeFrom[T]` handling for element type extraction
- `gen_range()`: Added to AST codegen `llvm_ir_gen_expr.cpp` for standalone range struct construction

### Files Changed
- `compiler/src/types/checker/control.cpp` (check_range, check_for)
- `compiler/src/codegen/llvm/llvm_ir_gen_expr.cpp` (gen_range, gen_expr dispatch)
- `compiler/include/codegen/llvm/llvm_ir_gen.hpp` (gen_range declaration)

### What Works Now
- Type checker correctly infers `Range[I32]` for `0..10`
- `Range::size_hint()`, `Range::count()` etc. type check correctly (were () before)
- For-in loops continue to work in AST codegen path

### What Still Doesn't Work
- **AST codegen Range method dispatch**: `r.size_hint()` type checks but codegen can't emit the call.
  The `gen_range()` stores range as anonymous struct `{ i32, i32 }`, not `%struct.Range__I32`.
  The method dispatcher (`method.cpp`) can't find `Range::size_hint` because `infer_expr_type` doesn't handle RangeExpr.
- **MIR path for-loops**: Pre-existing bug. `for i in 0..10` in standalone files generates `%struct.Range undef` but struct type isn't declared. This existed before the fix.

### Key Discovery
`Array[I32, 3].hash()` was NOT broken (despite task description). It already worked correctly.
The actual issue was Range method return types, not Array behavior dispatch.

**Why:** check_range returned wrong type → Slice method lookup failed → fallback to make_unit()
**How to apply:** When method calls return (), first check what TYPE the receiver is. The receiver type drives method lookup.
