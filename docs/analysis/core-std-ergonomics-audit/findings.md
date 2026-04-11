# Findings -- core-std-ergonomics-audit

## F-001 -- Manual index loops replaceable by for-in

- **Evidence**: 250+ instances across all libraries
- **Impact**: HIGH
- **Confidence**: HIGH
- **Discovered by**: Explore agents (core, std, compiler-tml)

Manual `var i = 0; loop (i < N) { ... i = i + 1 }` patterns are the single most common anti-pattern. All are mechanically replaceable by `for i in 0 to N { ... }`.

### Hotspots by library

**core** (~15 instances):
- `array/mod.tml` (lines 256, 342, 364) -- array iteration
- `traits/clone.tml` -- primitive type loops

**std** (~130 instances):
- `collections/list.tml` (40+) -- list operations
- `regex.tml` (40+) -- regex engine
- `bigint.tml` (40+) -- big integer math
- `text.tml` (29+) -- text processing
- `http/server/parse.tml` (32+) -- HTTP parser
- `http/h2/hpack.tml` (16+) -- HPACK codec
- `ia/tensor/ops.tml` (23+), `reshape.tml` (26+), `reduce.tml` (22+) -- tensor ops
- `collections/hashmap.tml` (13+) -- hash map internals
- `json/types.tml` (15+) -- JSON operations

**compiler-tml** (~105 instances):
- `ast/ast_writer.tml` (40+) -- AST serialization
- `serial/typeenv.tml` (23) -- type environment serialization
- `types/module_binary.tml` (15) -- module binary format
- `lexer/lexer.tml` (14) -- lexer
- `types/ty.tml` (13) -- type operations
- `types/register.tml` (12) -- type registration
- `types/imports.tml` (8) -- import resolution

---

## F-002 -- Nested when patterns replaceable by let-else and pattern guards

- **Evidence**: 80+ instances across all libraries
- **Impact**: HIGH
- **Confidence**: HIGH
- **Discovered by**: Explore agents

Deeply nested `when expr { Just(x) => { when ... { Just(y) => { ... } } } }` patterns are common for Maybe unwrapping. Two features address this:

1. **let-else** for early-return unwrapping: `let Just(x) = expr else { return }`
2. **Pattern guards** for conditional arms: `Just(x) if x > 0 => ...`

### Hotspots

**core** (~50 instances):
- `types/option.tml` -- `one_of()`, `zip()`, `unzip()`, `transpose()` (lines 429, 612, 803, 540)
- `types/result.tml` -- `map_ok()`, `map_err()`, `transpose()` (lines 204, 217, 542)
- `async/task.tml` -- Poll equality, waker handling (lines 204, 250)

**std** (~20 instances):
- `types.tml` -- `filter()` (line 93), `or_else()` (line 106)
- `json/types.tml` -- `get()`, `key_at()`, `get_path_string()` (lines 232, 287, 812)
- `thread/mod.tml` -- result unwrapping (line 590)
- `sync/mpsc.tml` -- channel receive (line 58)

**compiler-tml** (~20 instances):
- `types/imports.tml` -- module resolution chains (lines 99-147)
- `types/register.tml` -- symbol dispatch (lines 88-103, 327-345)
- `types/checker/check_expr.tml` -- symbol resolution (lines 85-450)

---

## F-003 -- Manual Duplicate/PartialEq impls on simple data structs

- **Evidence**: 50+ manual impls
- **Impact**: HIGH
- **Confidence**: HIGH
- **Discovered by**: Explore agents

Simple data structs with field-by-field `duplicate()` or `==` comparison can use `@auto(duplicate, equal)` instead.

### Candidates

**core** (~30 impls):
- `net/ip.tml` -- Ipv4Addr (4 fields), Ipv6Addr (8 fields), IpAddr (enum)
- `net/socket.tml` -- SocketAddrV4, SocketAddrV6, SocketAddr
- `time.tml` -- Duration (secs + nanos)
- `types/any.tml` -- TypeId (1 field)
- `traits/clone.tml` -- 12 primitive type Duplicate impls (I8..U64, F32, F64, Bool, Str)
- `traits/hash.tml` -- DefaultHasher, RandomState
- `async/task.tml` -- Waker

**std** (~8 impls):
- `thread/mod.tml` -- Thread (simple field copy)
- `ffi/os_str.tml` -- OsString
- `ffi/cstring.tml` -- CString
- `uuid.tml` -- Uuid

**compiler-tml** (~20 types, no impls yet but could benefit):
- `types/ty.tml` -- PrimitiveType, RefType, PtrType, ArrayType, SliceType, TupleType
- `types/env.tml` -- BoundConstraint, WhereConstraint, ConstGenericParam, StructFieldDef, EnumVariantDef, AssociatedTypeDef

---

## F-004 -- Small enums missing @repr(U8)

- **Evidence**: 15+ enums with <256 variants
- **Impact**: MEDIUM
- **Confidence**: HIGH
- **Discovered by**: Explore agents

Enums with few variants waste memory using default I64 discriminants. `@repr(U8)` saves 7 bytes per instance and enables more efficient pattern matching.

### Candidates

**core**:
- `reflect/mod.tml` -- TypeKind (12 variants, has manual `tag()` function)

**std**:
- `sync/ordering.tml` -- Ordering (5 variants)
- `db/query/expression.tml` -- CompareOp (7), LogicOp (2), SortDir (2)
- `db/bench/suite.tml` -- BenchOp (8)
- `db/orm/relation.tml` -- RelationType (4)

**compiler-tml**:
- `token.tml` -- TokenKind (139 variants, fits U8, has manual casting at lines 328, 347, 421)
- `ast/common.tml` -- Visibility (3), Mutability (2), LiteralKind (6)
- `ast/exprs.tml` -- UnaryOp (8), BinaryOp (18)
- `types/ty.tml` -- PrimitiveKind (17, has manual 17-arm tag function)

---

## F-005 -- Field extraction patterns replaceable by destructuring let

- **Evidence**: 70+ instances
- **Impact**: MEDIUM
- **Confidence**: HIGH
- **Discovered by**: Explore agents

Sequential `let x = s.field; let y = s.field` patterns can be replaced with `let Struct { x, y } = s`.

### Hotspots

**core** (~30 instances):
- `net/ip.tml` -- octet extraction (lines 130-139)
- `reflect/mod.tml` -- FieldInfo construction (lines 141-149)

**std** (~25 instances):
- `collections/hashmap.tml` -- header field extraction (lines 96-102)
- `bigint.tml` -- digit/sign extraction (lines 161-191)
- `json/types.tml` -- key/value extraction (lines 880-889)

**compiler-tml** (~20 instances):
- `ast/ast_writer.tml` -- field/segment extraction (lines 310-314)
- `types/checker/check_expr.tml` -- path/field extraction (lines 393-403)
- `main_frontend.tml` -- span/location extraction (lines 98-99)

---

## F-006 -- Struct update syntax opportunities

- **Evidence**: 20+ instances
- **Impact**: MEDIUM
- **Confidence**: MEDIUM
- **Discovered by**: Explore agents

Field-by-field struct reconstruction where only 1-2 fields change, while copying all others.

### Hotspots

**std**:
- `db/orm/relation.tml` -- CascadeOptions builders: `with_insert()`, `with_update()`, `with_delete()` (lines 40-80)
- `db/query/expression.tml` -- Expr type with 7+ fields (lines 90-100)

**compiler-tml**:
- `lexer/lexer.tml` -- Lexer initialization (lines 42-57, 14 fields)
- `types/env.tml` -- Scope/TypeEnv manipulation

---

## F-007 -- Optional chaining for Maybe method chains

- **Evidence**: 10-15 instances
- **Impact**: LOW
- **Confidence**: MEDIUM
- **Discovered by**: Explore agents

Nested when on Maybe where the only operation is calling a method on the inner value.

### Hotspots

- `json/types.tml` -- `get_path_string()`, `get_path_i64()` (lines 812-821)
- `types/imports.tml` -- module/function lookup chains
- `types/module.tml` -- module lookup chains

---

## F-008 -- Behavior alias opportunities

- **Evidence**: 5-8 potential aliases
- **Impact**: LOW
- **Confidence**: LOW
- **Discovered by**: Explore agents

Common bound combinations that repeat across multiple generic signatures.

### Potential aliases

- `behavior Copyable = Duplicate + PartialEq` -- very common pair
- `behavior Hashable = Hash + PartialEq` -- used in HashMap bounds
- `behavior Serializable = Display + Duplicate` -- used in serialization code

---

## F-009 -- Closure type inference opportunities

- **Evidence**: 5-10 instances
- **Impact**: LOW
- **Confidence**: MEDIUM
- **Discovered by**: Explore agents

Closures with verbose explicit type annotations where context provides sufficient inference.

Minimal impact -- most closures in stdlib already use concise syntax.

---

## F-010 -- Already-adopted patterns (positive findings)

- **Evidence**: Multiple files
- **Impact**: N/A (informational)
- **Confidence**: HIGH
- **Discovered by**: Explore agents

Some new features are already partially adopted:
- `let-else` used in 8-10 places in core (task.tml, array/mod.tml)
- Closure inference works well in existing callback patterns
- `for-in` not yet adopted anywhere (entirely new)
