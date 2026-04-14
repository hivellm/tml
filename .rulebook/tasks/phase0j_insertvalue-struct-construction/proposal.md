# Proposal: phase0j_insertvalue-struct-construction

## Why
TML struct construction generates `alloca` + multiple `store` instructions + a `load` of the entire struct — a stack round-trip pattern that LLVM cannot always optimize away at O0. Rust emits `insertvalue` chains that build the struct value entirely in SSA registers, with no memory traffic. Benchmarks show TML struct access at 16-32 ns/op vs Rust at <2 ns/op — a 10-18x gap, measured at O0. Even with O2 (phase0i), the alloca pattern may block scalar replacement of aggregates (SROA) for complex structs. Using `insertvalue` directly eliminates the alloca and produces IR that is valid at both O0 and O2. See `docs/analysis/benchmark/05-memory-structs.md` for details.

## What Changes
The MIR→LLVM emission for struct literal construction (`StructLiteral` / `AggregateInst` in the MIR) will be rewritten to emit `insertvalue` chains instead of `alloca+store+load`. The pattern is:
```
%0 = insertvalue { i64, f64 } undef, i64 %field0, 0
%1 = insertvalue { i64, f64 } %0, f64 %field1, 1
```
For structs returned from functions, the `sret` convention is preserved (LLVM's ABI already handles this). For intermediate struct values that never escape to the stack, the `insertvalue` form allows LLVM to keep the value in registers throughout. The `/compare-ir` skill will be used to verify the emitted IR matches Rust's output function-by-function.

## Impact
- Affected specs: codegen/struct-construction
- Affected code: MIR→LLVM emission in `compiler/src/codegen/instructions.cpp` (aggregate construction), `compiler/src/codegen/mir_llvm_builder.cpp`
- Breaking change: NO
- User benefit: 10-18x improvement for struct-heavy code paths at O0; enables SROA to fire at O2 reducing the gap further.
