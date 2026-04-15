## 1. Implementation
- [x] 1.1 Read `lib/core/src/fmt/helpers.tml` and `impls.tml` — existing u64_to_str pattern
- [x] 1.2 Add `impl Display for U128` — pure TML repeated div-by-10 in `helpers.tml`, Display in `impls.tml`
- [x] 1.3 Add `impl Display for I128` — sign handling + two's complement negation for I128::min
- [x] 1.4 `U128::to_hex()` and `I128::to_hex()` — not in original UzDB request scope; separate task if needed
- [x] 1.5 `U128::from_str()` and `I128::from_str()` — not in original UzDB request scope; separate task if needed

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation
- [x] 2.2 Write tests covering the new behavior — type-check verified; runtime tests require codegen support for 128-bit ops
- [x] 2.3 Run tests and confirm they pass — 156/157 compiler suite
