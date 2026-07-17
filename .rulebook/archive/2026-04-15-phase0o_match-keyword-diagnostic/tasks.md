## 1. Implementation
- [x] 1.1 Locate where `match` token fails in the expression parser
- [x] 1.2 Add early detection of `match` keyword before the cascading parser errors occur
- [x] 1.3 Emit `error[S001]: 'match' is not valid TML — use 'when' instead` with correct span

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation
- [x] 2.2 Write tests covering the new behavior
- [x] 2.3 Run `tml test --suite=compiler` — confirm no regressions
