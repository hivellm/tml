# Proposal: phase24b_fix-k001-bool-i32-i1-mismatch

## Why

Boolean comparison codegen emits `i32` where LLVM expects `i1`, causing a type mismatch error. This blocks the JSON benchmark and any code with complex boolean expressions in certain code paths.

**Error reproduced by**:
```
tml run benchmarks/profile_tml/json_bench.tml --stage=parser:cpp
```

**Error message**:
```
K001: '%t4892' defined with type 'i32' but expected 'i1'
  %t4893 = icmp eq i1 %t4892, 1
```

The `icmp eq i1 %t4892, 1` instruction expects its first operand to be `i1` (boolean), but the codegen produced an `i32` value. This is a type lowering bug where a boolean result is being stored/loaded as `i32` instead of `i1`.

**Benchmark impact**: Blocks the entire `std::json` module (4 files, SIMD parser, builder, serializer). JSON is critical for HTTP APIs, configuration, and data exchange.

## What Changes

1. Trace the IR around line 13243 of the json_bench generated IR to find which expression produces `i32` instead of `i1`
2. Fix the boolean lowering in the codegen — likely in `emit_binary_op` for comparison operators, or in boolean coercion for struct fields
3. Known related issue: T6 gotcha documents "bool/i1 struct fields must be I64 to avoid layout bugs" — this may be a case where a `Bool` return from a function is being treated as `I32` (the enum discriminant type) instead of `i1`

## Impact
- Affected specs: std::json, any code with complex boolean expressions
- Affected code: `compiler/src/codegen/binary_ops.cpp` or `compiler/src/mir/instructions.cpp` (boolean emission)
- Breaking change: NO
- User benefit: JSON parsing works; std::json module fully operational
