## 1. Implementation
- [ ] 1.1 Read `lib/std/src/collections/btreemap.tml` — understand `BTreeMapIter` cursor API
- [ ] 1.2 Check `core::iter` for the `Iterator[T]` behavior signature
- [ ] 1.3 Implement `Iterator[Pair[K,V]]` on `BTreeMapIter[K,V]` with `next() -> Maybe[Pair[K,V]]`
- [ ] 1.4 Verify `for entry in map.iter()` compiles and `entry.key`/`entry.value` are accessible

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update CHANGELOG.md and bump VERSION
- [ ] 2.2 Write tests: for-in over BTreeMap, empty map, key/value access
- [ ] 2.3 Run `tml test --suite=compiler` and `tml test --suite=std` — confirm no regressions
