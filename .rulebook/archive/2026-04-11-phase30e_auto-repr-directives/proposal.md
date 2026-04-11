# Proposal: phase30e_auto-repr-directives

## Why
80+ lines of manual enum discriminant encoding in ast_writer.tml (28-variant BinaryOp, 8-variant UnaryOp, etc.). Each variant manually mapped to a sequential integer. `@repr(u8)` would allow `color as U8` to extract the discriminant. `@auto(debug, duplicate, equal)` would auto-generate behavior impls, eliminating repetitive boilerplate.

Source: docs/analyses/language/05-derive-macros.md

## What Changes
1. **@repr(u8/u16/i32)**: Store discriminant layout info on enum types; allow `as U8` cast to extract discriminant
2. **@auto(behaviors...)**: At compile time, generate impl blocks for known behaviors (Debug, Duplicate, PartialEq, Display)
3. **@packed**: Mark struct for C-compatible layout with no padding

## Impact
- Affected specs: 03-GRAMMAR.md (added §7.1 Built-in Directives)
- Affected code: `compiler/src/parser/parser_decl.cpp`, `compiler/src/typechecker/`, `compiler/src/codegen/`
- Breaking change: NO (additive — existing `@test`, `@extern` already work)
- User benefit: Eliminates ~150 lines of manual serialization, auto-generates common behaviors
