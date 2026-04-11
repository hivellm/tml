# Proposal: phase30i_destructuring-let

## Why
30+ times in the parser, `let result = func()!; let a = result.field1; let b = result.field2` pattern repeats where a single destructuring let would suffice. The grammar already supports struct patterns in let bindings (`LetStmt = 'let' Pattern ...`, `StructPattern = TypePath '{' FieldPatterns '}'`) but the compiler doesn't implement it beyond enum patterns.

Source: docs/analyses/language/15-destructuring.md

## What Changes
1. **Parser**: Ensure struct patterns work in let bindings (may already parse, needs verification)
2. **HIR/MIR**: Desugar `let Foo { a, b } = expr` into individual field extractions
3. **Codegen**: Emit GEP/extractvalue for each destructured field

## Impact
- Affected specs: 03-GRAMMAR.md (already specified), RFC-0002 (already specified)
- Affected code: `compiler/src/parser/parser_stmt.cpp`, `compiler/src/mir/thir_mir_builder.cpp`
- Breaking change: NO (additive)
- User benefit: Cleaner result handling, fewer intermediate variables
