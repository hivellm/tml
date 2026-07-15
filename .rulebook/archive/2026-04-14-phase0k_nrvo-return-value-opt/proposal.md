# Proposal: phase0k_nrvo-return-value-opt

## Why
Functions that return structs currently copy the return value into a temporary, then copy it again into the caller's slot. Named Return Value Optimization (NRVO) eliminates the intermediate copy by constructing the return value directly in the caller's `sret` slot. Benchmarks show function-returning-struct at ~5-10 ns/op vs Rust at <2 ns/op. Rust applies NRVO via MIR copy-propagation automatically. Without NRVO, every struct-returning function adds one extra `memcpy` per call. This is especially impactful for parser/compiler internals where most functions return `Outcome[T, E]` or named result structs. See `docs/analysis/benchmark/06-functions-closures.md` for context.

## What Changes
The MIR optimization pass will be extended with a simple copy-elision analysis: if a function's last MIR instruction is `return local_var` and `local_var` is a struct allocated in the function body (not a parameter), its allocation site will be rewritten to use the `sret` pointer directly, eliminating the copy. For functions where the return path is branching (multiple `return` sites), NRVO applies only to the common case; uncommon paths fall back to memcpy. The LLVM `sret` attribute must already be in place (expected from phase0j work).

## Impact
- Affected specs: codegen/function-returns, codegen/struct-passing
- Affected code: MIR copy-elision pass in `compiler/src/mir/` or codegen lowering in `compiler/src/codegen/`
- Breaking change: NO
- User benefit: Eliminates one memcpy per struct-returning function call. Critical for parser, type-checker, and compiler code paths that return large structs.
