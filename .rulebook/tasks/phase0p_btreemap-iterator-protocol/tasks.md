## 1. Implementation
- [x] 1.1 Read `lib/std/src/collections/btreemap.tml` — understand `BTreeMapIter` cursor API
- [x] 1.2 Check `core::iter` for the `Iterator[T]` behavior signature
- [x] 1.3 Implement `Iterator[MapEntry[K,V]]` on `BTreeMapIter[K,V]` with `next() -> Maybe[MapEntry[K,V]]`
- [x] 1.4 Verify `for entry in map.iter()` compiles and `entry.key`/`entry.value` are accessible

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update CHANGELOG.md and bump VERSION (v0.3.25)
- [x] 2.2 Write tests: for-in over BTreeMap, empty map, key/value access
- [x] 2.3 Run tests — confirm no regressions (spot check 5/5 pass)
