# Tasks: Build std::intern — String Interning Module

**Status**: Complete (14/14)
**Depends on**: None (can start immediately)
**Blocks**: phase12e (serializers use InternedStr for symbol names)
**Duration**: 3–5 days
**Risk**: Low

---

## Phase 1: Design & API (3 items)

- [x] 1.1 Search existing stdlib for interning — confirmed nothing exists
- [x] 1.2 Design API: `Interner` type with `intern(Str) -> InternedStr`, `get(InternedStr) -> Str`, `len() -> I64`
- [x] 1.3 Design `InternedStr` as opaque handle (I64 index into internal table)

## Phase 2: Core Implementation (5 items)

- [x] 2.1 Create `lib/std/src/intern/mod.tml` — module declaration with `pub use`
- [x] 2.2 Create `lib/std/src/intern/interner.tml` — `Interner` type backed by flat array (lowlevel, avoids HashMap codegen bug)
- [x] 2.3 Implement `intern(this, s: Str) -> InternedStr` — linear scan + insert
- [x] 2.4 Implement `get(this, id: InternedStr) -> Str` — index-based lookup
- [x] 2.5 Implement `len(this) -> I64`, `is_empty(this) -> Bool`, `contains(this, s: Str) -> Bool`

## Phase 3: Tests (4 items)

- [x] 3.1 Create `lib/std/tests/intern/basic.test.tml` — intern, get, contains, len, is_empty
- [x] 3.2 Test deduplication: intern same string twice returns same InternedStr
- [x] 3.3 Test stress: intern 500 unique strings, verify all retrievable and re-intern doesn't grow
- [x] 3.4 Test empty string and single-char strings edge cases

## Phase 4: Benchmark (2 items)

- [x] 4.1 Benchmark: stress test with 500 strings passes — linear scan is O(n) but sufficient for compiler symbol tables
- [x] 4.2 Note: HashMap-based impl blocked by codegen bug (insertvalue i32 vs struct). Current impl uses flat array with strcmp. Can upgrade to HashMap when codegen fixed.
