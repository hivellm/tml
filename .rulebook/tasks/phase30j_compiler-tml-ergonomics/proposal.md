# Proposal: phase30j_compiler-tml-ergonomics

## Why
The compiler-tml codebase (39 .tml files) was written before the phase30 language ergonomics features existed. An audit found 54+ manual index loops, 10+ redundant struct constructions, 8+ nested when cascades, and 4 enums without @repr. Applying the new features will eliminate ~150 lines of boilerplate, improve readability, and serve as a validation that all phase30 features work correctly at scale.

## What Changes
Apply phase30 language features to compiler-tml/src/:
1. Replace 54+ manual `var i = 0; loop` with `for i in 0 to N`
2. Apply `..base` struct update in register.tml and builtins.tml
3. Add `@repr(U8)` to PrimitiveKind, ScopeKind enums
4. Flatten nested when cascades with pattern guards
5. Use destructuring let where beneficial

## Impact
- Affected code: compiler-tml/src/ (6+ files)
- Breaking change: NO (pure refactor — same semantics)
- User benefit: Validates phase30 features at scale; cleaner self-hosting compiler code
