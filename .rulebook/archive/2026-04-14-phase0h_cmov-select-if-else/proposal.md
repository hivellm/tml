# Proposal: phase0h_cmov-select-if-else

## Why
Simple if-else expressions that return a scalar value on each branch generate an LLVM `br`+`phi` pair instead of a single `select` instruction (which maps directly to a CMOV on x86-64). Every `br` instruction is a potential branch misprediction; `select` is data-flow and executes in a single cycle with no prediction penalty. Benchmarks show TML if-else chains at 1 ns/op (509M ops/sec) vs Rust at <1 ns/op (4.3B ops/sec) — an 8.4x gap. Rust emits `select` for branchless scalar conditionals automatically. TML must detect this pattern and do the same.

## What Changes
The codegen path for `IfExpr` in the MIR→LLVM emission layer will be extended with a branchless-eligible check. An if-else is eligible when: (1) both branches contain exactly one expression with no side effects, (2) both branches return the same scalar type (I64, I32, F64, bool, pointer), and (3) there is no nested control flow. When eligible, the codegen evaluates both branch expressions unconditionally and emits `select i1 %cond, <then_val>, <else_val>`. When not eligible (multi-statement body, side effects, different types, nested control flow), the existing `br`+`phi` form is preserved. Chained if-else-if is handled by nesting `select` instructions.

## Impact
- Affected specs: codegen/if-expression
- Affected code: MIR→LLVM emission for `IfExpr`/conditional expressions, likely in `compiler/src/codegen/instructions.cpp` or `compiler/src/codegen/mir_llvm_builder.cpp`
- Breaking change: NO
- User benefit: Up to 8x improvement for branchless scalar selection patterns (ternary-style expressions, min/max, clamping, lookup fallbacks). Zero source changes required.
