## 1. Destructuring let -- core
- [ ] 1.1 core/net/ip.tml -- octet extraction patterns
- [ ] 1.2 core/reflect/mod.tml -- FieldInfo, InterfaceInfo construction

## 2. Destructuring let -- std
- [ ] 2.1 std/bigint.tml -- digit/sign extraction
- [ ] 2.2 std/json/types.tml -- key/value extraction
- [ ] 2.3 std/collections/hashmap.tml -- header field extraction (only non-lowlevel code)

## 3. Destructuring let -- compiler-tml
- [ ] 3.1 compiler-tml/ast/ast_writer.tml -- field/segment extraction
- [ ] 3.2 compiler-tml/types/checker/check_expr.tml -- path/field extraction
- [ ] 3.3 compiler-tml/main_frontend.tml -- span/location extraction

## 4. Struct update syntax (..)
- [ ] 4.1 std/db/orm/relation.tml -- CascadeOptions builders (with_insert, with_update, with_delete)
- [ ] 4.2 std/db/query/expression.tml -- Expr construction
- [ ] 4.3 compiler-tml/lexer/lexer.tml -- Lexer variant construction

## 5. Tail (mandatory -- enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md
- [ ] 5.2 Run /check on all modified files
- [ ] 5.3 Run tests on affected suites and confirm they pass
