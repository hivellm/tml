# Proposal: Table-Driven Intrinsic Dispatch

## Why

`instructions_call.cpp` is 1,357 lines of if/else chains matching function names as strings. The same type-resolution logic is duplicated across 12+ intrinsic handlers, and the same `ensure_ptr` lambda is copied 12 times. Adding a new intrinsic requires copying a 50-line block. Rust uses symbol matching, Go uses SSA rewrite rules, Clang uses `BuiltinID` switch — all are enum/table-driven with shared helpers.

## What Changes

1. **Intrinsic registry** (`compiler/include/codegen/intrinsic_table.hpp`):
   - `IntrinsicKind` enum with ~30 entries (PtrRead, PtrWrite, Memcpy, Sqrt, etc.)
   - `IntrinsicInfo` struct: `{kind, min_args, has_result}`
   - `lookup_intrinsic(func_name)`: O(1) lookup with alias handling

2. **Shared helpers**:
   - `resolve_element_type()`: single implementation replacing 12 duplicated 5-level fallback chains
   - `ensure_ptr_value()`: single method replacing 12 copied lambdas
   - `emit_memcpy()`, `emit_memset()`, `emit_memmove()`: typed emit helpers

3. **Dispatch refactor**: `emit_call_inst()` uses `switch(intrinsic.kind)` instead of if/else chain. Each intrinsic becomes a separate method.

## Impact

- Affected specs: None (internal compiler change)
- Affected code: `compiler/include/codegen/intrinsic_table.hpp` (new), `compiler/src/codegen/intrinsic_table.cpp` (new), `compiler/src/codegen/mir/instructions_call.cpp` (~515 lines removed, ~165 added)
- Breaking change: NO — same LLVM IR output, cleaner dispatch
- User benefit: Easier to add new intrinsics; fewer duplication-related bugs; ~40% smaller file
