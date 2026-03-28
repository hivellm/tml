# Tasks: HashMap Entry API + Missing Methods

**Status**: Proposed
**Priority**: CRITICAL
**Phase**: 7 — Rust Parity

## Phase 1: Critical
- [ ] 1.1 `is_empty(this) -> Bool`
- [ ] 1.2 `Entry[K,V]` type — `Occupied` / `Vacant` variants
- [ ] 1.3 `entry(this, key: K) -> Entry[K,V]`
- [ ] 1.4 `Entry::or_insert(value: V) -> ref V`
- [ ] 1.5 `Entry::or_insert_with(f: func() -> V) -> ref V`
- [ ] 1.6 `Entry::or_default() -> ref V` where V: Default
- [ ] 1.7 `Entry::and_modify(f: func(mut ref V)) -> Entry`
- [ ] 1.8 Tests

## Phase 2: High
- [ ] 2.1 `keys(this) -> List[K]` — all keys
- [ ] 2.2 `values(this) -> List[V]` — all values
- [ ] 2.3 `retain(this, pred: func(ref K, ref V) -> Bool)`
- [ ] 2.4 `drain(this) -> List[(K,V)]` — remove and return all
- [ ] 2.5 `extend(this, pairs: List[(K,V)])`
- [ ] 2.6 Tests
