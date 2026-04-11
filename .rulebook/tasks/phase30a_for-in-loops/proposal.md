# Proposal: phase30a_for-in-loops

## Why
30+ manual index loops (`var i = 0; loop (i < len) { ...; i = i + 1 }`) across parser, stdlib, and compiler-tml. This boilerplate is error-prone (infinite loops from forgotten increment) and obscures intent. The syntax is already fully specified in RFC-0002 §2.3 and §3.4 but not implemented in the C++ compiler.

Source: docs/analyses/language/01-loop-iterator.md

## What Changes
Implement `for <pattern> in <expr> { body }` in the C++ compiler:
1. **Parser**: Parse `for` keyword → Pattern → `in` → Expr → Block
2. **HIR desugaring**: Range loops (`for i in 0 to 10`) → counter loop. Iterator loops (`for x in collection`) → `.into_iter()` + `.next()` loop. Direct index loops (`for x in list`) → index-based loop.
3. **Range types**: Add `Range`/`RangeInclusive` to core with `to`/`through` operators
4. **IntoIterator behavior**: Types implementing `IntoIterator` work with for-in

## Impact
- Affected specs: RFC-0002 (already specified), 03-GRAMMAR.md (already has ForExpr)
- Affected code: `compiler/src/parser/`, `compiler/src/hir/`, `compiler/src/mir/thir_mir_builder.cpp`
- Breaking change: NO (additive)
- User benefit: Eliminates ~150 lines of boilerplate, prevents infinite loop bugs
