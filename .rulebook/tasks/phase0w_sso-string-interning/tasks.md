## 1. Diagnosis
- [ ] 1.1 Read `lib/core/src/text.tml` — document current `Text` struct layout (ptr + len + cap)
- [ ] 1.2 Read Rust `smol_str` or `compact_str` source (via web) for SSO layout reference — document the tagged-union approach
- [ ] 1.3 Write a TML micro-benchmark: create 1M short strings (5-10 chars) — measure allocs/sec and ns/op with `check_leaks=true`

## 2. Implementation: SSO Layout
- [ ] 2.1 Update `Text` struct in `lib/core/src/text.tml` to use tagged-union SSO layout: inline data (len ≤ 15) vs heap pointer (len > 15)
- [ ] 2.2 Update `Text::new(str)`: if `str.len() <= 15`, copy bytes inline; otherwise allocate heap buffer
- [ ] 2.3 Update `Text::len()`, `Text::as_str()`, `Text::get_byte(i)` to check SSO tag and dispatch accordingly
- [ ] 2.4 Update `Text::drop()` (from phase0v): only free heap variant, no-op for inline variant
- [ ] 2.5 Audit all `lowlevel` code that reads `Text` struct fields directly — update to use SSO-aware accessors

## 3. Implementation: String Interning
- [ ] 3.1 Create `compiler-tml/src/intern.tml`: `InternTable` with `HashMap[Str, I64]` + `List[Text]` storage
- [ ] 3.2 Implement `intern(s: Str) -> I64`: look up in table, insert if absent, return integer ID
- [ ] 3.3 Implement `lookup(id: I64) -> Str`: returns the interned string for a given ID
- [ ] 3.4 Wire `InternTable` into the TML lexer: all identifier tokens use interned IDs instead of heap `Text` copies

## 4. Benchmark Gate
- [ ] 4.1 Run short-string creation benchmark — measure ns/op before and after SSO
- [ ] 4.2 Run the compiler's parse phase on a 1000-line TML file — measure parse time with and without interning
- [ ] 4.3 Compare vs Rust `String` heap allocation benchmark from `docs/analysis/benchmark/14-text-stringbuilder.md`
- [ ] 4.4 GATE: Short string (≤15 chars) creation must require zero `malloc` calls. Parse phase must improve ≥30%. Do NOT proceed if gate fails.

## 5. Validation
- [ ] 5.1 Run `tml test --suite=core` — all string operations must produce correct results with SSO layout
- [ ] 5.2 Run `tml test --suite=compiler` — no regressions (lexer/parser use strings heavily)
- [ ] 5.3 Run `mcp__tml__debug(check_leaks=true)` on a compile — zero leaks for SSO strings

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 6.1 Update CHANGELOG.md with `perf(core): SSO for Text (≤15 chars inline, zero malloc) + string interning for compiler identifiers`
- [ ] 6.2 Write tests: SSO round-trip (short and long strings), intern table lookup, equality via ID
- [ ] 6.3 Run tests and confirm they pass
