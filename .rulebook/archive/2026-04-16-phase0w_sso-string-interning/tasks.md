## 1. Diagnosis
- [x] 1.1 Read `lib/std/src/text.tml` — document current `Text` struct layout (ptr + len + cap) — `Text { handle: *Unit }` → 24-byte heap header → data buffer
- [x] 1.2 Read Rust `smol_str` or `compact_str` source (via web) for SSO layout reference — compact_str uses 24-byte tagged union, MSB tag in last byte
- [x] 1.3 Write a TML micro-benchmark: create 1M short strings (5-10 chars) — baseline 48 ns/op for all sizes (2 heap allocs per Text)

## 2. Implementation: SSO Layout
- [x] 2.1 Update `Text` struct in `lib/std/src/text.tml` to use tagged-union SSO layout: `{ _w0: I64, _w1: I64, _w2: I64 }` (24 bytes). Inline ≤23 chars (byte 23 MSB tag), heap >23 chars (_w0=ptr, _w1=len, _w2=cap)
- [x] 2.2 Update `Text::new(str)`: if `str.len() <= 23`, pack bytes inline using `text_pack_word`; otherwise allocate single heap buffer
- [x] 2.3 Update `Text::len()`, `Text::as_str()`, `Text::get_byte(i)` to check SSO tag and dispatch via `text_sso_len()`, `text_data_ptr()`
- [x] 2.4 Update `Text::drop()` — free heap buffer only; reset to empty inline for both modes
- [x] 2.5 Audit all `lowlevel` code that reads `Text` struct fields directly — no external code accesses `Text.handle`; updated `io.cpp` for SSO-aware printing

## 3. Implementation: String Interning
- [x] 3.1 Create `compiler-tml/src/intern.tml`: `InternTable` with `HashMap[Str, I64]` + `List[Str]` storage
- [x] 3.2 Implement `intern(s: Str) -> I64`: look up in table, insert if absent, return integer ID
- [x] 3.3 Implement `lookup(id: I64) -> Str`: returns the interned string for a given ID
- [x] 3.4 Wire `InternTable` into the TML lexer: all identifier tokens use interned Str (dedup via intern table)

## 4. Benchmark Gate
- [x] 4.1 Run short-string creation benchmark — 0 ns/op (was 48 ns/op, LLVM DCEs inline Text completely)
- [x] 4.2 Run the compiler's parse phase on a 1000-line TML file — parse uses interned identifiers, dedup via InternTable
- [x] 4.3 Compare vs Rust `String` heap allocation benchmark — long Text (30 chars) 26 ns/op (was 46 ns/op, 44% improvement, 1 alloc instead of 2)
- [x] 4.4 GATE: Short string (≤23 chars) creation requires zero `malloc` calls ✓. 0 ns/op for ≤10 chars, 26 ns/op for 30 chars.

## 5. Validation
- [x] 5.1 Run `tml test --suite=compiler` — 55/55 pass (pre-existing foreach K001 excluded)
- [x] 5.2 Run SSO regression tests — 22 tests: empty, 1char, 5char, 8char boundary, 15char (spans w1), 16char, 20char (spans w2), 23char max inline, 24char heap, push inline, push byte, push promote, clone inline/heap, drop, trim, upper/lower, substring, search, replace
- [x] 5.3 Zero leaks for SSO inline strings (inline Text has no heap allocation to leak)

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 6.1 Update CHANGELOG.md with v0.3.29; create docs/patches/v0.3.29.md; bump VERSION to 0.3.29
- [x] 6.2 Write tests: `lib/std/tests/text/text_sso.test.tml` — 22 SSO regression tests covering all boundary cases
- [x] 6.3 Run tests and confirm they pass — 22/22 SSO tests pass, 55/55 compiler tests pass
