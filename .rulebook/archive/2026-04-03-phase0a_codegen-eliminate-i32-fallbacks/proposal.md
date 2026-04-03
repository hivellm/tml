# Proposal: Eliminate i32 Fallbacks in MIR Codegen

## Why

The MIR codegen silently falls back to `i32` at 23 sites when `inst.type` is null. This masks MIR builder bugs and causes silent data corruption — i64 values truncated to i32, structs treated as integers, wrong ABI decisions. Every established compiler (Rust, Go, Clang) guarantees all types before codegen. Missing type should be a compiler bug surfaced immediately, not a tolerated condition that produces wrong code.

## What Changes

1. **New MIR validation pass** (`mir_validate.cpp`) that runs after MIR building, before codegen. Checks that every instruction has a non-null type, every block has a terminator, and return types match signatures.

2. **Warning annotations** on all 23 `make_i32_type()` fallback sites. Each warning logs the function name and instruction ID, making MIR builder bugs immediately visible in test output.

3. **Upstream MIR builder fixes** for the top warning-producing sites in `hir_mir_builder.cpp` and `thir_mir_builder.cpp` — set types on instructions that currently leave them null.

## Impact

- Affected specs: None (internal compiler change)
- Affected code: `compiler/src/mir/mir_validate.cpp` (new), `compiler/src/codegen/mir/*.cpp` (23 sites), `compiler/src/mir/hir_mir_builder.cpp`, `compiler/src/mir/thir_mir_builder.cpp`
- Breaking change: NO — adds warnings and validation, does not change output for correct programs
- User benefit: Fewer mysterious crashes and wrong results from codegen bugs; faster bug diagnosis
