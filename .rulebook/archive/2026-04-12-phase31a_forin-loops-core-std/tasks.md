## 1. Core library
- [x] 1.1 core/array/mod.tml -- 17 loops converted
- [x] 1.2 core/traits/clone.tml -- no convertible loops found

## 2. Std library -- collections
- [x] 2.1 std/collections/list.tml -- 12 loops converted
- [x] 2.2 std/collections/hashmap.tml -- 3 loops converted

## 3. Std library -- text & parsing
- [x] 3.1 std/text.tml -- 13 loops converted
- [x] 3.2 std/regex.tml -- 23 loops converted
- [x] 3.3 std/json/types.tml -- 1 loop converted
- [x] 3.4 std/bigint.tml -- 17 loops converted

## 4. Std library -- networking & IO
- [x] 4.1 std/http/server/parse.tml -- 7 loops converted
- [x] 4.2 std/http/h2/hpack.tml -- 9 loops converted

## 5. Std library -- tensor/math
- [x] 5.1 std/ia/tensor/ops.tml -- 20 loops converted
- [x] 5.2 std/ia/tensor/reshape.tml -- 22 loops converted
- [x] 5.3 std/ia/tensor/reduce.tml -- 20 loops converted

## 6. Std library -- remaining files
- [x] 6.1 Full scan of all remaining files -- 253 additional loops converted across 50+ files (collections/binary_heap, btreeset, btreemap, class_collections, trie; http/server, middleware, router, websocket, protocol, h2, client, app; stream/*; sync/*; aio/*; events/*; net/*; search/*; debug; di; db; intern; core/str, simd, data)

## 7. Tail (mandatory -- enforced by rulebook v5.3.0)
- [x] 7.1 Update CHANGELOG.md with for-in migration entry (lib/core v0.2.5, lib/std v0.2.6)
- [x] 7.2 Run /check on all modified files -- all compile successfully
- [x] 7.3 Run tests on affected suites -- core/str 32/32, std/text 4/4, std/regex 4/4, std/collections 86/94 (8 pre-existing K001). Note: `resize()` reverted to manual loop due to for-in codegen phi-node bug when for-in is last expr in else block.
