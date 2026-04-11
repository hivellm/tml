# Proposal: phase30f_bool-layout-fix

## Why
15+ struct fields use `I64` instead of `Bool` as a workaround for the LLVM i1 layout bug. Bool (i1) in struct fields causes misaligned access because i1 is 1 bit but struct alignment expects byte-sized fields. This forces explicit `0`/`1` encoding and loses type safety.

Source: docs/analyses/language/06-bool-fields.md

## What Changes
1. **Type lowering**: Map `Bool` to `i8` in struct contexts (keep `i1` for registers/conditions)
2. **Load/store**: Insert `zext i8 → i1` on load from struct field, `trunc i1 → i8` on store
3. **Struct layout**: Use `i8` alignment for Bool fields (1 byte, not 1 bit)

## Impact
- Affected specs: None (implementation fix)
- Affected code: `compiler/src/codegen/` (type lowering, struct layout)
- Breaking change: NO (binary layout changes but Bool fields weren't usable before)
- User benefit: Bool struct fields work correctly; removes 15+ I64 workarounds
