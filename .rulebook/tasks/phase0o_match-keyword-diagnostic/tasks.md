## 1. Implementation
- [ ] 1.1 Locate where `match` token fails in the expression parser
- [ ] 1.2 Add early detection of `match` keyword before the cascading parser errors occur
- [ ] 1.3 Emit `error[S001]: 'match' is not valid TML — use 'when' instead` with correct span

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update CHANGELOG.md and bump VERSION
- [ ] 2.2 Write regression test: `match x { 1 => {} _ => {} }` → S001 with correct span, no ICE
- [ ] 2.3 Run `tml test --suite=compiler` — confirm no regressions
