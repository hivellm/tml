# Tasks: Build std::intern — String Interning Module

**Status**: Planned (0/14)
**Depends on**: None (can start immediately)
**Blocks**: phase12e (serializers use InternedStr for symbol names)
**Duration**: 3–5 days
**Risk**: Low

---

## Phase 1: Design & API (3 items)

- [ ] 1.1 Search existing stdlib for interning (`mcp__tml__docs_search query="intern"`) — confirm nothing exists
- [ ] 1.2 Design API: `Interner` type with `intern(Str) -> InternedStr`, `get(InternedStr) -> Str`, `len() -> I64`
- [ ] 1.3 Design `InternedStr` as opaque handle (I64 index into internal table)

## Phase 2: Core Implementation (5 items)

- [ ] 2.1 Create `lib/std/src/intern/mod.tml` — module declaration with `pub use`
- [ ] 2.2 Create `lib/std/src/intern/interner.tml` — `Interner` type backed by `HashMap[Str, I64]` + `List[Str]`
- [ ] 2.3 Implement `intern(ref self, s: Str) -> InternedStr` — return existing or insert new
- [ ] 2.4 Implement `get(ref self, id: InternedStr) -> Str` — lookup by index
- [ ] 2.5 Implement `len(ref self) -> I64`, `contains(ref self, s: Str) -> Bool`

## Phase 3: Tests (4 items)

- [ ] 3.1 Create `lib/std/tests/intern/basic.test.tml` — intern, get, contains, len
- [ ] 3.2 Test deduplication: intern same string twice returns same InternedStr
- [ ] 3.3 Test stress: intern 10,000 unique strings, verify all retrievable
- [ ] 3.4 Test empty string and single-char strings edge cases

## Phase 4: Benchmark (2 items)

- [ ] 4.1 Benchmark: intern 100K strings — compare with plain `HashMap[Str, I64]` baseline
- [ ] 4.2 Verify overhead < 5% vs direct HashMap (interning adds index indirection)
