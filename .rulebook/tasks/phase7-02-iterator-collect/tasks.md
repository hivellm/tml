# Tasks: Iterator collect() + FromIterator

**Status**: Proposed
**Priority**: CRITICAL
**Phase**: 7 — Rust Parity

## Phase 1: FromIterator behavior + collect[C]() on Iterator
- [ ] 1.1 Define `FromIterator[T]` behavior in `core::iter`
- [ ] 1.2 `impl FromIterator[T] for List[T]` — collect into List
- [ ] 1.3 `impl FromIterator[(K,V)] for HashMap[K,V]` — collect into HashMap
- [ ] 1.4 `impl FromIterator[Char] for Str` — collect chars into string
- [ ] 1.5 Add `collect[C: FromIterator]()` method to Iterator trait
- [ ] 1.6 Tests: collect to List, HashMap, Str
