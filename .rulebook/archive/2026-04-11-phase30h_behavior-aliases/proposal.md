# Proposal: phase30h_behavior-aliases

## Why
Complex trait bounds are repeated verbatim across multiple generic functions (e.g., `where This::Item: PartialEq, I::Item = This::Item`). Behavior aliases provide a way to name common bound combinations, reducing repetition and improving readability.

Source: docs/analyses/language/14-trait-aliases.md

## What Changes
1. **Parser**: Parse `behavior Name = Bound1 + Bound2 + Bound3` as a BehaviorAlias declaration
2. **Type checker**: When `Name` is used as a bound, expand to the constituent bounds
3. **LL(1) safe**: After `behavior Ident`, peek `=` → alias; peek `{` → full behavior definition

## Impact
- Affected specs: 03-GRAMMAR.md (added BehaviorAlias rule), PEG grammar (updated BehaviorDef)
- Affected code: `compiler/src/parser/parser_decl.cpp`, `compiler/src/typechecker/`
- Breaking change: NO (additive)
- User benefit: Cleaner generic bounds, reusable constraint sets
