# Execution plan -- core-std-ergonomics-audit

Based on the findings in [findings.md](./findings.md), this plan organizes
work into 6 tasks under phase 31, grouped by feature type for maximum
mechanical consistency and minimum context-switching.

## Task 31a -- for-in loops (core + std)

**Goal**: Replace all manual index loops in core and std with `for i in 0 to N`
**Depends on**: nothing
**Findings**: F-001
**Estimated sites**: ~145 instances

Scope:
- [ ] core/array/mod.tml
- [ ] core/traits/clone.tml
- [ ] std/collections/list.tml (~40)
- [ ] std/collections/hashmap.tml (~13)
- [ ] std/regex.tml (~40)
- [ ] std/bigint.tml (~40)
- [ ] std/text.tml (~29)
- [ ] std/json/types.tml (~15)
- [ ] std/http/server/parse.tml (~32)
- [ ] std/http/h2/hpack.tml (~16)
- [ ] std/ia/tensor/ops.tml, reshape.tml, reduce.tml (~71)

## Task 31b -- for-in loops (compiler-tml)

**Goal**: Replace all manual index loops in compiler-tml with `for i in 0 to N`
**Depends on**: nothing (parallel with 31a)
**Findings**: F-001
**Estimated sites**: ~105 instances

Scope:
- [ ] ast/ast_writer.tml (~40)
- [ ] serial/typeenv.tml (~23)
- [ ] types/module_binary.tml (~15)
- [ ] lexer/lexer.tml (~14)
- [ ] types/ty.tml (~13)
- [ ] types/register.tml (~12)
- [ ] types/imports.tml (~8)

## Task 31c -- let-else + pattern guards (all libraries)

**Goal**: Flatten nested when blocks using let-else and pattern guards
**Depends on**: nothing
**Findings**: F-002
**Estimated sites**: ~80 instances

Scope:
- [ ] core/types/option.tml -- zip, unzip, transpose, one_of
- [ ] core/types/result.tml -- map_ok, map_err, transpose
- [ ] core/async/task.tml -- Poll equality, waker
- [ ] std/types.tml -- filter, or_else
- [ ] std/json/types.tml -- get, key_at, get_path_*
- [ ] std/thread/mod.tml -- result unwrapping
- [ ] compiler-tml/types/imports.tml -- module resolution
- [ ] compiler-tml/types/register.tml -- symbol dispatch
- [ ] compiler-tml/types/checker/check_expr.tml -- symbol resolution

## Task 31d -- @auto directives + @repr (all libraries)

**Goal**: Replace manual Duplicate/PartialEq impls with @auto; add @repr(U8) to small enums
**Depends on**: nothing
**Findings**: F-003, F-004
**Estimated sites**: ~50 manual impls, ~15 enums

Scope -- @auto:
- [ ] core/net/ip.tml -- Ipv4Addr, Ipv6Addr, IpAddr
- [ ] core/net/socket.tml -- SocketAddrV4, SocketAddrV6, SocketAddr
- [ ] core/time.tml -- Duration
- [ ] core/types/any.tml -- TypeId
- [ ] core/traits/hash.tml -- DefaultHasher, RandomState
- [ ] core/async/task.tml -- Waker
- [ ] std/thread/mod.tml -- Thread
- [ ] std/uuid.tml -- Uuid

Scope -- @repr(U8):
- [ ] core/reflect/mod.tml -- TypeKind (remove manual tag())
- [ ] std/sync/ordering.tml -- Ordering
- [ ] std/db/query/expression.tml -- CompareOp, LogicOp, SortDir
- [ ] std/db/orm/relation.tml -- RelationType
- [ ] compiler-tml/token.tml -- TokenKind
- [ ] compiler-tml/ast/common.tml -- Visibility, Mutability, LiteralKind
- [ ] compiler-tml/ast/exprs.tml -- UnaryOp, BinaryOp
- [ ] compiler-tml/types/ty.tml -- PrimitiveKind (remove manual tag function)

## Task 31e -- destructuring let + struct update (all libraries)

**Goal**: Replace field-by-field extraction with destructuring; use ..base for partial updates
**Depends on**: nothing
**Findings**: F-005, F-006
**Estimated sites**: ~70 destructuring + ~20 struct update

Scope -- destructuring:
- [ ] core/net/ip.tml -- octet extraction
- [ ] core/reflect/mod.tml -- FieldInfo
- [ ] std/collections/hashmap.tml -- header fields
- [ ] std/bigint.tml -- digit/sign
- [ ] std/json/types.tml -- key/value
- [ ] compiler-tml/ast/ast_writer.tml -- field/segment
- [ ] compiler-tml/types/checker/check_expr.tml -- path/field
- [ ] compiler-tml/main_frontend.tml -- span/location

Scope -- struct update:
- [ ] std/db/orm/relation.tml -- CascadeOptions builders
- [ ] std/db/query/expression.tml -- Expr construction
- [ ] compiler-tml/lexer/lexer.tml -- Lexer init

## Task 31f -- optional chaining + behavior aliases (all libraries)

**Goal**: Apply optional chaining for Maybe method calls; define common behavior aliases
**Depends on**: nothing
**Findings**: F-007, F-008
**Estimated sites**: ~15 optional chaining + ~5 aliases

Scope:
- [ ] std/json/types.tml -- get_path_string, get_path_i64
- [ ] compiler-tml/types/imports.tml -- module lookup chains
- [ ] compiler-tml/types/module.tml -- module lookup chains
- [ ] Define behavior aliases in core (Copyable, Hashable, Serializable)

## Open questions

- @auto(duplicate) on types with `Heap[T]` fields may trigger K001 codegen bug -- verify before applying
- @repr(U8) on TokenKind (139 variants) is fine (fits in U8), but verify wire format compatibility
- Destructuring let on lowlevel pointer-based structs (hashmap internals) may not apply -- these use raw pointer arithmetic, not struct fields
