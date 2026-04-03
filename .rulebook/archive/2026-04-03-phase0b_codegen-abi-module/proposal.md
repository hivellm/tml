# Proposal: Centralized ABI Module for MIR Codegen

## Why

Win64 ABI decisions for struct passing (by-value vs by-pointer, sret, self/this handling) are scattered across 10+ sites in the codegen, each using string prefix matching (`starts_with("%struct.")`). This causes monthly bugs: structs passed by value when Win64 requires by-pointer, sret mismatches between caller/callee, and self parameters not converted to pointer. Rust, Go, and Clang all centralize ABI decisions in one module computed once per function — the codegen never inspects types to decide passing convention.

## What Changes

1. **New ABI module** (`compiler/include/codegen/abi.hpp` + `abi.cpp`) with:
   - `PassMode` enum: `Direct`, `Indirect`, `Ignore`, `Pair`
   - `ArgABI` struct: per-argument passing info `{mode, llvm_type, sret}`
   - `FnABI` struct: per-function ABI info `{ret, args[]}`
   - `compute_fn_abi()`: classifies each argument using MirType (not string prefix)

2. **Win64 rules centralized**: aggregates >8 bytes → Indirect, small power-of-2 → Direct, Unit → Ignore, fat pointers → Pair. Implemented once in `classify_argument()`.

3. **Codegen simplified**: `emit_function_declaration()`, `emit_function()`, and `emit_call_inst()` read pre-computed FnABI instead of ad-hoc string checks. The `func_param_types_` side-table is removed.

## Impact

- Affected specs: None (internal compiler change)
- Affected code: `compiler/include/codegen/abi.hpp` (new), `compiler/src/codegen/abi.cpp` (new), `compiler/src/codegen/mir_codegen.cpp` (declarations), `compiler/src/codegen/mir/instructions_call.cpp` (call sites)
- Breaking change: NO — same LLVM IR output, cleaner internal architecture
- User benefit: Fewer struct-passing crashes; easier to add new aggregate types
