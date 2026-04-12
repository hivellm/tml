## 1. @auto(duplicate, equal) -- core
- [x] 1.1 core/net/ip.tml -- Ipv4Addr, Ipv6Addr, IpAddr converted
- [x] 1.2 core/net/socket.tml -- SocketAddrV4, SocketAddrV6, SocketAddr converted
- [x] 1.3 core/time.tml -- Duration converted
- [x] 1.4 core/types/any.tml -- TypeId converted; AnyValue not eligible (*Unit field)
- [x] 1.5 core/traits/hash.tml -- DefaultHasher, RandomState converted
- [x] 1.6 core/async/task.tml -- Waker not eligible (RawPtr/ref fields)

## 2. @auto(duplicate, equal) -- std
- [x] 2.1 std/thread/mod.tml -- Thread not eligible (Maybe[Str] + *Unit fields)
- [x] 2.2 std/uuid.tml -- Uuid converted
- [x] 2.3 std/ffi/os_str.tml -- OsString not eligible (*U8 field)
- [x] 2.4 std/ffi/cstring.tml -- CString not eligible (*U8 field)

## 3. @repr(U8) -- core
- [x] 3.1 core/reflect/mod.tml -- TypeKind reverted (K001 codegen: ptr vs i8 mismatch)

## 4. @repr(U8) -- std
- [x] 4.1 std/sync/ordering.tml -- Ordering converted
- [x] 4.2 std/db/query/expression.tml -- CompareOp, LogicOp, SortDir converted
- [x] 4.3 std/db/orm/relation.tml -- RelationType converted

## 5. @repr(U8) -- compiler-tml
- [x] 5.1 compiler-tml/token.tml -- TokenKind preserved (wire compat requirement)
- [x] 5.2 compiler-tml/ast/common.tml -- Visibility, Mutability, LiteralKind converted
- [x] 5.3 compiler-tml/ast/exprs.tml -- UnaryOp, BinaryOp converted
- [x] 5.4 compiler-tml/types/ty.tml -- PrimitiveKind already had @repr(U8)

## 6. Tail
- [x] 6.1 Reflect tests 19/19 passed after TypeKind revert
- [x] 6.2 No regressions from @auto/@repr changes
- [x] 6.3 Committed
