## 1. Destructuring let -- core
- [x] 1.1 core/net/ip.tml -- 0 eligible (inline field access, not sequential let)
- [x] 1.2 core/reflect/mod.tml -- 0 eligible (constructor params, not field extraction)

## 2. Destructuring let -- std
- [x] 2.1 std/bigint.tml -- 0 eligible (individual field access in expressions)
- [x] 2.2 std/json/types.tml -- 0 eligible (single handle field)
- [x] 2.3 std/collections/hashmap.tml -- 0 eligible (all ptr arithmetic in lowlevel)

## 3. Destructuring let -- compiler-tml
- [x] 3.1 compiler-tml/ast/ast_writer.tml -- 0 eligible (inline field args)
- [x] 3.2 compiler-tml/types/checker/check_expr.tml -- 0 eligible (inline field args)
- [x] 3.3 compiler-tml/main_frontend.tml -- 0 eligible (below 3-field threshold)

## 4. Struct update syntax (..)
- [x] 4.1 std/db/orm/relation.tml -- 3 conversions (with_cascade, join_table, inverse_side)
- [x] 4.2 std/db/query/expression.tml -- 0 eligible (all fresh constructions)
- [x] 4.3 compiler-tml/lexer/lexer.tml -- 0 eligible (no base struct to copy from)

## 5. Tail
- [x] 5.1 Only relation.tml modified -- no regressions
- [x] 5.2 Struct update syntax correctly applied
- [x] 5.3 Committed
