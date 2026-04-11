## 1. @auto(duplicate, equal) -- core
- [ ] 1.1 core/net/ip.tml -- Ipv4Addr, Ipv6Addr, IpAddr
- [ ] 1.2 core/net/socket.tml -- SocketAddrV4, SocketAddrV6, SocketAddr
- [ ] 1.3 core/time.tml -- Duration
- [ ] 1.4 core/types/any.tml -- TypeId
- [ ] 1.5 core/traits/hash.tml -- DefaultHasher, RandomState
- [ ] 1.6 core/async/task.tml -- Waker

## 2. @auto(duplicate, equal) -- std
- [ ] 2.1 std/thread/mod.tml -- Thread
- [ ] 2.2 std/uuid.tml -- Uuid
- [ ] 2.3 std/ffi/os_str.tml -- OsString
- [ ] 2.4 std/ffi/cstring.tml -- CString

## 3. @repr(U8) -- core
- [ ] 3.1 core/reflect/mod.tml -- TypeKind (remove manual tag() function)

## 4. @repr(U8) -- std
- [ ] 4.1 std/sync/ordering.tml -- Ordering
- [ ] 4.2 std/db/query/expression.tml -- CompareOp, LogicOp, SortDir
- [ ] 4.3 std/db/orm/relation.tml -- RelationType

## 5. @repr(U8) -- compiler-tml
- [ ] 5.1 compiler-tml/token.tml -- TokenKind (139 variants, verify wire compat)
- [ ] 5.2 compiler-tml/ast/common.tml -- Visibility, Mutability, LiteralKind
- [ ] 5.3 compiler-tml/ast/exprs.tml -- UnaryOp, BinaryOp
- [ ] 5.4 compiler-tml/types/ty.tml -- PrimitiveKind (remove manual tag function)

## 6. Tail (mandatory -- enforced by rulebook v5.3.0)
- [ ] 6.1 Update CHANGELOG.md
- [ ] 6.2 Run /check on all modified files
- [ ] 6.3 Run tests on affected suites and confirm they pass
