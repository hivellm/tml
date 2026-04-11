# Proposal: phase30b_pattern-guards

## Why
40+ nested when-expressions in the codebase that could be flattened with pattern guards. The grammar already specifies `GuardPattern = PrimaryPattern ('if' Expr)?` in RFC-0002 §2.5 and the PEG grammar, but the C++ compiler doesn't implement it. Or-patterns (`A | B => ...`) are also specified but not implemented.

Source: docs/analyses/language/03-pattern-guards.md

## What Changes
1. **Parser**: Parse `if <expr>` after a pattern in when-arms
2. **Parser**: Parse `|` between patterns in when-arms (or-patterns)
3. **HIR/MIR**: After successful pattern match, emit guard condition check before arm body
4. **Or-patterns**: Duplicate arm body for each alternative (or share via jump)

## Impact
- Affected specs: RFC-0002 (already specified), 03-GRAMMAR.md (updated with guard syntax)
- Affected code: `compiler/src/parser/parser_pattern.cpp`, `compiler/src/mir/thir_mir_builder.cpp`
- Breaking change: NO (additive)
- User benefit: Eliminates ~100 lines of nested when boilerplate
