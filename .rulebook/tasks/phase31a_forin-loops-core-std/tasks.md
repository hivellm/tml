## 1. Core library
- [ ] 1.1 core/array/mod.tml -- replace manual loops (~3 sites)
- [ ] 1.2 core/traits/clone.tml -- replace manual loops if present

## 2. Std library -- collections
- [ ] 2.1 std/collections/list.tml -- replace manual loops (~40 sites)
- [ ] 2.2 std/collections/hashmap.tml -- replace manual loops (~13 sites)

## 3. Std library -- text & parsing
- [ ] 3.1 std/text.tml -- replace manual loops (~29 sites)
- [ ] 3.2 std/regex.tml -- replace manual loops (~40 sites)
- [ ] 3.3 std/json/types.tml -- replace manual loops (~15 sites)
- [ ] 3.4 std/bigint.tml -- replace manual loops (~40 sites)

## 4. Std library -- networking & IO
- [ ] 4.1 std/http/server/parse.tml -- replace manual loops (~32 sites)
- [ ] 4.2 std/http/h2/hpack.tml -- replace manual loops (~16 sites)

## 5. Std library -- tensor/math
- [ ] 5.1 std/ia/tensor/ops.tml -- replace manual loops (~23 sites)
- [ ] 5.2 std/ia/tensor/reshape.tml -- replace manual loops (~26 sites)
- [ ] 5.3 std/ia/tensor/reduce.tml -- replace manual loops (~22 sites)

## 6. Std library -- remaining files
- [ ] 6.1 Scan remaining std files for any missed manual loops and convert

## 7. Tail (mandatory -- enforced by rulebook v5.3.0)
- [ ] 7.1 Update CHANGELOG.md with for-in migration entry
- [ ] 7.2 Run /check on all modified files
- [ ] 7.3 Run tests on affected suites and confirm they pass
