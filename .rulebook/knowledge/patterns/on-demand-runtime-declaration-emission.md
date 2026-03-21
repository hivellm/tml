# On-Demand Runtime Declaration Emission

**Category**: codegen
**Tags**: codegen, llvm-ir, optimization, dead-code

## Description

Instead of emitting all 500+ runtime function declarations unconditionally, use emit_line() to auto-detect runtime symbol references and emit declarations on-demand. Catalog system tracks which declarations have been emitted. Reduces IR bloat significantly.

## When to Use

When generating LLVM IR that references external runtime functions. The pattern ensures only actually-used declarations appear in the output IR.
