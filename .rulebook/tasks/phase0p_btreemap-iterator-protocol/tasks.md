## 1. Iterator implementation
- [x] 1.1 `impl Iterator for BTreeMapIter[K, V]` já existia — confirmado em btreemap.tml:376
- [x] 1.2 `fn next(&mut self) -> Maybe[MapEntry[K, V]]` já existia — btreemap.tml:382
- [x] 1.3 API cursor legada preservada (has_next/key/value/advance)

## 2. IntoIterator wiring
- [x] 2.1 `impl IntoIterator for BTreeMap[K, V]` — adicionado em btreemap.tml (type Item = MapEntry, type IntoIter = BTreeMapIter)
- [x] 2.2 Type checker: `check_for` reconhece tipos que implementam IntoIterator (control.cpp nova branch)
- [x] 2.3 Destructuring de tuplas em for-in: por design usa MapEntry { key, value } em vez de (K, V) — consistente com Rust

## 3. HashMap paridade
- [x] 3.1 HashMap já implementa IntoIterator em behaviors.tml (linha 382); nova branch em control.cpp beneficia HashMap, HashSet, e qualquer tipo futuro com IntoIterator

## 4. Testes
- [x] 4.1 for entry in map itera em ordem sorted (test_btreemap_for_in_sorted_order)
- [x] 4.2 for entry em map vazio não entra no loop (test_btreemap_for_in_empty)
- [x] 4.3 Valores e contagem corretos (test_btreemap_for_in_iteration_order)
- [x] 4.4 Backward-compat com `.iter()` (test_btreemap_explicit_iter_still_works)
- [x] 4.5 4/4 tests passando em `lib/std/tests/collections/btreemap_intoiter.test.tml`

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update or create documentation covering the implementation (CHANGELOG + docs/patches/v0.3.30.md)
- [x] 5.2 Write tests covering the new behavior (4 testes E2E)
- [x] 5.3 Run tests and confirm they pass (4/4)
