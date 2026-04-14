# Proposal: phase0d_codegen-switch-when-dense

## Why
When/match with dense integer patterns (0–9) generates cascading if-else chains (icmp+br) instead of a single LLVM `switch` instruction. Benchmarks show TML at 3 ns/op (331M ops/sec) vs Rust at <1 ns/op (3.16B ops/sec) — a 9.5x gap. The LLVM `switch` instruction is directly lowered to a CPU jump table by the backend, eliminating the branch-predictor pressure of chained comparisons. This is a pure codegen quality issue; the semantics are identical.

## What Changes
The MIR→LLVM emission path for `WhenInst`/`MatchInst` will be extended to detect integer discriminants and emit an LLVM `switch` instruction instead of a sequence of `icmp eq` + conditional branch pairs. For dense patterns (consecutive integers) LLVM will fold to a jump table. For sparse patterns the `switch` will still be emitted with a default arm and LLVM will choose binary search or jump table based on density heuristics. Non-integer patterns (string, struct, enum with payload) keep the existing if-else fallback.

## Impact
- Affected specs: codegen/control-flow
- Affected code: `compiler/src/codegen/instructions.cpp` (MIR→LLVM WhenInst emission), possibly `compiler/src/codegen/mir_llvm_builder.cpp`
- Breaking change: NO
- User benefit: Up to 9x throughput improvement for integer dispatch code (parsers, state machines, bytecode interpreters). Zero source-level changes required.
