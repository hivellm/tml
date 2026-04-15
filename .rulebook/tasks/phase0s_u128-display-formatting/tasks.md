## 1. Implementation
- [ ] 1.1 Read `lib/core/src/num.tml` — check existing U128/I128 methods and Display behavior
- [ ] 1.2 Add `impl Display for U128` — decimal formatting via 64-bit division chaining (hi/lo split)
- [ ] 1.3 Add `impl Display for I128` — handle sign, then delegate to U128 decimal formatter
- [ ] 1.4 Add `U128::to_hex() -> Str` and `I128::to_hex() -> Str`
- [ ] 1.5 Add `U128::from_str(s: Str) -> Maybe[U128]` and `I128::from_str`

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior — `U128::max().to_string()`, `I128::min().to_string()`, round-trip `from_str(to_string(x)) == Just(x)`
- [ ] 2.3 Run tests and confirm they pass
