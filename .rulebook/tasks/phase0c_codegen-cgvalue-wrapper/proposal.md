# Proposal: CGValue Wrapper — Typed Values with Pointer/Value Distinction

## Why

The codegen represents all values as strings (register names like `%v42`) with no way to know if a value is a direct SSA value, a pointer, a fat pointer, or zero-sized. This causes 61 lookups in the `value_types_` side-table, scattered spill-to-alloca patterns (10+ copies), and weekly bugs where a struct value is passed where a pointer is expected (or vice versa). Rust uses `OperandRef` with `OperandValue` enum, Clang uses `RValue`/`LValue` classes — both carry the value type and category intrinsically.

## What Changes

1. **New CGValue type** (`compiler/include/codegen/cg_value.hpp`) with:
   - `CGValueKind` enum: `Immediate`, `Address`, `FatPointer`, `ZeroSized`
   - `CGValue` struct: `{reg, llvm_type, kind, mir_type}`
   - `to_address()`: spills value to alloca if not already a pointer
   - `to_immediate()`: loads from pointer if not already a value

2. **Instruction emission produces CGValues**: each instruction handler populates a `cg_values_` map alongside the existing `value_regs_` (transition period).

3. **Call sites use CGValues**: argument processing uses `CGValue::kind` instead of string-parsing `value_types_` entries. The manual spill pattern is replaced by `CGValue::to_address()`.

4. **Side-tables removed**: `value_types_` (61 lookups) and `value_spill_allocas_` are replaced by `cg_values_`.

## Impact

- Affected specs: None (internal compiler change)
- Affected code: `compiler/include/codegen/cg_value.hpp` (new), `compiler/include/codegen/mir_codegen.hpp` (new member), `compiler/src/codegen/mir/instructions*.cpp` (all instruction handlers)
- Breaking change: NO — same LLVM IR output, cleaner internal representation
- User benefit: Fewer type mismatch crashes; eliminates entire class of value/pointer confusion bugs
