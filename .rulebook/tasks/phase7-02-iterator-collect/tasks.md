# Tasks: Iterator collect() + FromIterator

**Status**: In Progress — 4/6 done, 2 blocked by codegen
**Priority**: CRITICAL
**Phase**: 7 — Rust Parity

## Phase 1: FromIterator behavior + collect[C]() on Iterator
- [x] 1.1 Define `FromIterator[T]` behavior in `core::iter` — ALREADY EXISTED in `lib/core/src/iter/traits/from_iterator.tml`
- [x] 1.2 `impl FromIterator[T] for List[T]` — ALREADY EXISTED in `lib/std/src/collections/behaviors.tml:295`
- [ ] 1.3 `impl FromIterator[(K,V)] for HashMap[K,V]` — BLOCKED: tuple types `(K,V)` in `List` hit codegen bug (getelementptr on unsized tuple struct), and generic Iterator constraint dispatch fails cross-module
- [ ] 1.4 `impl FromIterator[Char] for Str` — BLOCKED: same tuple/generic codegen issues; also Str has no `push_char` method
- [x] 1.5 Add `collect[C: FromIterator]()` method to Iterator trait — ADDED to `lib/core/src/iter/traits/iterator.tml:305`. Type-checks but codegen blocked by generic trait dispatch bug (C::from_iter returns `()`)
- [x] 1.6 Tests: collect to List — 5 tests passing in `lib/core/tests/iter/iter_collect.test.tml` and `iter_collect_range.test.tml`

## Additional work done
- [x] Added `ListIter[T].to_list()` method in `lib/std/src/collections/behaviors.tml` — practical concrete collect that works today
- [x] Tests cover: basic collect, empty list, single element, order preservation, independent copy

## Blockers (codegen bugs preventing full implementation)
1. **Generic trait dispatch** — `C::from_iter(this)` in collect() returns `()` instead of actual type. Affects ~140 functions project-wide.
2. **Cross-module Iterator constraint** — `ListIter[T]` doesn't satisfy `I: Iterator` constraint when checked from a different module. Prevents generic `to_list[I: Iterator]()` functions.
3. **Cross-module impl resolution** — Can't add `impl Range[T] { to_list() }` from std (Range is in core). Can't add it in core either (List not available in core).
4. **Tuple in List codegen** — `List[(I64, I64)]` hits `getelementptr on unsized struct` bug, blocking HashMap pair collection.
