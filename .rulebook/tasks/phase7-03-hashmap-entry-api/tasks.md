# Tasks: HashMap Entry API + Missing Methods

**Status**: In Progress — Phase 1 complete, Phase 2 partially complete
**Priority**: CRITICAL
**Phase**: 7 — Rust Parity

## Phase 1: Critical
- [x] 1.1 `is_empty(this) -> Bool` — implemented, test passing
- [ ] 1.2 `Entry[K,V]` type — `Occupied` / `Vacant` variants — BLOCKED: complex codegen for enum-wrapping HashMap internals
- [ ] 1.3 `entry(this, key: K) -> Entry[K,V]` — BLOCKED: depends on 1.2
- [ ] 1.4 `Entry::or_insert(value: V) -> ref V` — BLOCKED: depends on 1.2
- [ ] 1.5 `Entry::or_insert_with(f: func() -> V) -> ref V` — BLOCKED: depends on 1.2
- [ ] 1.6 `Entry::or_default() -> ref V` where V: Default — BLOCKED: depends on 1.2
- [ ] 1.7 `Entry::and_modify(f: func(mut ref V)) -> Entry` — BLOCKED: depends on 1.2
- [x] 1.8 Tests — `hashmap_extras.test.tml` (is_empty), `hashmap_keys.test.tml` (keys/values)
- [x] 1.8+ `get_or_set(this, key: K, default: V) -> V` — practical Entry API alternative, test passing

## Phase 2: High
- [x] 2.1 `keys(this) -> List[K]` — implemented, test passing
- [x] 2.2 `values(this) -> List[V]` — implemented, test passing
- [x] 2.3 `retain(this, pred: func(K, V) -> Bool)` — implemented, test passing
- [x] 2.4 `drain_keys(this) -> List[K]` + `drain_values(this) -> List[V]` — implemented as separate methods (tuple return `(List[K], List[V])` causes codegen IR conflict when test file also instantiates `List[Str]`), tests passing
- [x] 2.5 `extend_from(this, keys: List[K], values: List[V])` — implemented with parallel lists (tuple `List[(K,V)]` not attempted due to codegen risk), test passing
- [x] 2.6 Tests — 6 new test files, all passing

## Notes
- Entry API (1.2-1.7) requires an enum type wrapping internal HashMap slot references — complex and likely blocked by enum+ref codegen bugs. `get_or_set` provides the most common Entry API use case.
- `drain` implemented as `drain_keys`/`drain_values` instead of tuple return to avoid codegen IR conflict.
- `extend_from` takes parallel Lists instead of `List[(K,V)]` tuples.
- Multi-test files timeout in suite mode (pre-existing issue) — each new test uses 1 test per file.
- `ref List[K]` parameters cause IR type mismatch — use by-value instead.
