# Proposal: phase30c_struct-update-syntax

## Why
50+ call sites where entire structs are reconstructed to modify a single field (e.g., cli.tml builder methods copy 8 fields to change 1). The `..expr` struct update syntax is already in the PEG grammar (`StructInit = Path ~ "{" ~ FieldInits? ~ (".." ~ Expr)? ~ "}"`) and shown in 03-GRAMMAR.md examples, but not implemented in the C++ compiler.

Source: docs/analyses/language/02-struct-update.md

## What Changes
1. **Parser**: Parse `..expr` as the last element in a struct literal
2. **Type checker**: Verify source expression matches target struct type
3. **Codegen**: For unspecified fields, emit field-by-field copy from source; for specified fields, use explicit values
4. **Optimization**: Use insertvalue chains for small structs, memcpy+overwrite for large

## Impact
- Affected specs: 03-GRAMMAR.md (updated with formal StructUpdate rule)
- Affected code: `compiler/src/parser/parser_expr.cpp`, `compiler/src/mir/thir_mir_builder_expr.cpp`
- Breaking change: NO (additive)
- User benefit: Eliminates ~200 lines of struct reconstruction boilerplate
