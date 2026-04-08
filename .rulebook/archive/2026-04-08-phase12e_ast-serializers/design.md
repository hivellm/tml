# Design: AST & TypeEnv Binary Serializers

## Overview

All serialized files start with an 8-byte header:
```
magic:         u32 (LE)   — 0x544D4C41 "TMLA" for AST, 0x544D4C45 "TMLE" for TypeEnv
version_major: u16 (LE)
version_minor: u16 (LE)
```

Current version: `1.0`. Unknown major → hard error. Unknown minor → skip unknown section (uses u32 byte-length sentinel).

## Primitive Encoding

All multi-byte integers are **little-endian**. Following the MIR serializer pattern:

| Primitive | Encoding |
|-----------|----------|
| `u8`      | 1 byte   |
| `u16`     | 2 bytes LE |
| `u32`     | 4 bytes LE |
| `u64`     | 8 bytes LE |
| `bool`    | u8 (0 = false, 1 = true) |
| `string`  | u32 length + UTF-8 bytes (no null terminator) |
| `optional<T>` | u8 present (0/1), then T if present |
| `vector<T>` | u32 count, then count × T |

Note: The TML-side `BinaryWriter` also offers `write_varint` (LEB128) for compact encoding of
counts in the AST stream. The C++ reader uses fixed `read_u32` for all counts to keep the
reader simple (varint expansion happens at the TML write side via a post-processing step).

## SourceSpan Encoding

`SourceSpan` = `SourceLocation start` + `SourceLocation end`.
Each `SourceLocation` = { file: string_view, line: u32, column: u32, offset: u32 }.

In binary: the file path is deduplicated into a per-module **file string table** (written once
in the module header). Each `SourceSpan` then stores:
```
file_idx:     u32   — index into file string table
start_offset: u32   — byte offset from start of file
start_line:   u32   — 1-based line
start_col:    u32   — 1-based column
end_offset:   u32
end_line:     u32
end_col:      u32
```
Total: 28 bytes per span. Future minor versions may compress this with varint encoding.

## Module Structure

```
Header (8 bytes)
File string table: u32 count, then count × string
Module name:       string
Module docs:       vector<string>
Decl count:        u32
Decl nodes:        count × Decl
```

## Node Tag Values

Tags are single-byte discriminants matching the `std::variant` index in each AST struct.

### DeclTag (u8)
```
Func         = 0
Struct       = 1
Union        = 2
Enum         = 3
Trait        = 4
Impl         = 5
TypeAlias    = 6
Const        = 7
Use          = 8
Mod          = 9
Class        = 10
Interface    = 11
Namespace    = 12
```

### ExprTag (u8)
```
Literal      = 0
Ident        = 1
Unary        = 2
Binary       = 3
Call         = 4
MethodCall   = 5
Field        = 6
Index        = 7
Tuple        = 8
Array        = 9
Struct       = 10
If           = 11
Ternary      = 12
IfLet        = 13
When         = 14
Loop         = 15
While        = 16
For          = 17
Block        = 18
Return       = 19
Break        = 20
Continue     = 21
Closure      = 22
Range        = 23
Cast         = 24
Is           = 25
Try          = 26
Await        = 27
Throw        = 28
Path         = 29
Lowlevel     = 30
InterpStr    = 31
TemplateLit  = 32
Base         = 33
New          = 34
```

### StmtTag (u8)
```
Let          = 0
Var          = 1
LetElse      = 2
Expr         = 3
Decl         = 4    // nested DeclPtr
```

### TypeTag (u8)
```
Named        = 0
Ref          = 1
Ptr          = 2
Array        = 3
Slice        = 4
Tuple        = 5
Func         = 6
Infer        = 7
Dyn          = 8
ImplBehavior = 9
```

### PatternTag (u8)
```
Wildcard     = 0
Ident        = 1
Literal      = 2
Tuple        = 3
Struct       = 4
Enum         = 5
Or           = 6
Range        = 7
Array        = 8
```

### LiteralKind (u8)  — for LiteralExpr / LiteralPattern
```
Int     = 0   — followed by i64 value + u8 bit_width + u8 is_signed
Float   = 1   — followed by f64 value + u8 is_f64
Bool    = 2   — followed by u8 value
Str     = 3   — followed by string
Char    = 4   — followed by u32 codepoint
Null    = 5   — no further data
```

### Visibility (u8)
```
Private  = 0
Public   = 1
PubCrate = 2
```

## Field-by-Field Schema

### Common: SourceSpan
```
file_idx:     u32
start_offset: u32
start_line:   u32
start_col:    u32
end_offset:   u32
end_line:     u32
end_col:      u32
```

### Common: Decorator
```
name:     string
arg_count: u32
args:     arg_count × Expr
span:     SourceSpan
```

### Common: GenericParam
```
name:          string
is_const:      u8
is_lifetime:   u8
bound_count:   u32
bounds:        bound_count × Type
has_const_type: u8
const_type:    Type  (if has_const_type)
has_default:   u8
default_type:  Type  (if has_default)
has_lifetime_bound: u8
lifetime_bound: string (if has_lifetime_bound)
span:          SourceSpan
```

### Common: WhereClause
```
constraint_count: u32
constraints: count × { Type, bound_count u32, bound_count × Type }
eq_count:    u32
equalities:  eq_count × { Type, Type }
span:        SourceSpan
```

### Common: FuncParam
```
pattern:     Pattern
type:        Type
deco_count:  u32
decorators:  deco_count × Decorator
span:        SourceSpan
```

### Decl encoding
```
tag:  DeclTag u8
span: SourceSpan
<kind-specific fields>
```

#### FuncDecl (tag=0)
```
has_doc:        u8
doc:            string (if has_doc)
deco_count:     u32
decorators:     deco_count × Decorator
vis:            Visibility u8
name:           string
generic_count:  u32
generics:       generic_count × GenericParam
param_count:    u32
params:         param_count × FuncParam
has_return_type: u8
return_type:    Type (if has_return_type)
has_where:      u8
where:          WhereClause (if has_where)
is_async:       u8
is_unsafe:      u8
has_extern_abi: u8
extern_abi:     string (if has_extern_abi)
has_extern_name: u8
extern_name:    string (if has_extern_name)
link_lib_count: u32
link_libs:      link_lib_count × string
contract_count: u32
contracts:      contract_count × ContractClause
has_body:       u8
body:           BlockExpr (if has_body)
```

#### ContractClause
```
is_pre:          u8
has_result_bind: u8
result_bind:     string (if has_result_bind)
condition:       Expr
span:            SourceSpan
```

#### StructDecl (tag=1)
```
has_doc:       u8; doc: string (if has_doc)
deco_count:    u32; decorators × Decorator
vis:           Visibility
name:          string
generic_count: u32; generics × GenericParam
field_count:   u32; fields × StructField
has_where:     u8; where: WhereClause (if has_where)
```

#### StructField
```
has_doc:     u8; doc: string (if has_doc)
vis:         Visibility
deco_count:  u32; decorators × Decorator
name:        string
type:        Type
has_default: u8; default: Expr (if has_default)
span:        SourceSpan
```

#### UnionDecl (tag=2)
```
has_doc:     u8; doc: string (if has_doc)
deco_count:  u32; decorators × Decorator
vis:         Visibility
name:        string
field_count: u32; fields × StructField
```

#### EnumDecl (tag=3)
```
has_doc:       u8; doc: string (if has_doc)
deco_count:    u32; decorators × Decorator
vis:           Visibility
name:          string
generic_count: u32; generics × GenericParam
variant_count: u32; variants × EnumVariant
has_where:     u8; where: WhereClause (if has_where)
```

#### EnumVariant
```
has_doc:     u8; doc: string (if has_doc)
name:        string
kind:        u8  (0=unit, 1=tuple, 2=struct)
  if kind==1: field_count u32, tuple_fields × Type
  if kind==2: field_count u32, struct_fields × StructField
has_discrim: u8; discrim: Expr (if has_discrim)
span:        SourceSpan
```

#### TraitDecl (tag=4)
```
has_doc:       u8; doc: string (if has_doc)
deco_count:    u32; decorators × Decorator
vis:           Visibility
name:          string
generic_count: u32; generics × GenericParam
super_count:   u32; super_traits × Type
assoc_count:   u32; assoc_types × AssociatedType
method_count:  u32; methods × FuncDecl (tag omitted—always FuncDecl)
has_where:     u8; where: WhereClause (if has_where)
```

#### AssociatedType
```
name:          string
generic_count: u32; generics × GenericParam
bound_count:   u32; bounds × Type
has_default:   u8; default: Type (if has_default)
span:          SourceSpan
```

#### ImplDecl (tag=5)
```
has_doc:      u8; doc: string (if has_doc)
generic_count: u32; generics × GenericParam
has_trait:    u8; trait_type: Type (if has_trait)
self_type:    Type
binding_count: u32; type_bindings × AssocTypeBinding
const_count:  u32; constants × ConstDecl
method_count: u32; methods × FuncDecl
has_where:    u8; where: WhereClause (if has_where)
```

#### AssocTypeBinding
```
name:          string
generic_count: u32; generics × GenericParam
type:          Type
span:          SourceSpan
```

#### TypeAliasDecl (tag=6)
```
has_doc:       u8; doc: string (if has_doc)
vis:           Visibility
name:          string
generic_count: u32; generics × GenericParam
type:          Type
```

#### ConstDecl (tag=7)
```
has_doc: u8; doc: string (if has_doc)
vis:     Visibility
name:    string
type:    Type
value:   Expr
```

#### UseDecl (tag=8)
```
vis:          Visibility
path_segments: u32; segments × string
has_alias:    u8; alias: string (if has_alias)
has_symbols:  u8; symbol_count: u32; symbols × string (if has_symbols)
is_glob:      u8
span:         SourceSpan
```

#### ModDecl (tag=9)
```
vis:       Visibility
name:      string
has_items: u8
  if has_items: item_count u32; items × Decl
span:      SourceSpan
```

### Expr encoding
```
tag:  ExprTag u8
span: SourceSpan
<kind-specific fields>
```

#### LiteralExpr (tag=0)
```
kind:  LiteralKind u8
value: (per LiteralKind)
```

#### IdentExpr (tag=1)
```
name: string
```

#### UnaryExpr (tag=2)
```
op:      u8  (UnaryOp enum index)
operand: Expr
```

#### BinaryExpr (tag=3)
```
op:    u8  (BinaryOp enum index)
left:  Expr
right: Expr
```

#### CallExpr (tag=4)
```
callee:    Expr
arg_count: u32
args:      arg_count × Expr
```

#### MethodCallExpr (tag=5)
```
receiver:      Expr
method:        string
generic_count: u32; type_args × Type
arg_count:     u32; args × Expr
```

#### FieldExpr (tag=6)
```
object: Expr
field:  string
```

#### IndexExpr (tag=7)
```
object: Expr
index:  Expr
```

#### TupleExpr (tag=8)
```
elem_count: u32
elements:   elem_count × Expr
```

#### ArrayExpr (tag=9)
```
kind:  u8  (0=list, 1=repeat)
  if kind==0: elem_count u32; elements × Expr
  if kind==1: element Expr; count Expr
```

#### StructExpr (tag=10)
```
path_segments: u32; segments × string
field_count:   u32; fields × { string, Expr }
has_base:      u8; base: Expr (if has_base)
```

#### IfExpr (tag=11)
```
condition:  Expr
then_block: BlockExpr
else_count: u32
else_ifs:   else_count × { Expr, BlockExpr }
has_else:   u8; else_block: BlockExpr (if has_else)
```

#### TernaryExpr (tag=12)
```
condition: Expr
then_expr: Expr
else_expr: Expr
```

#### IfLetExpr (tag=13)
```
pattern:    Pattern
value:      Expr
then_block: BlockExpr
has_else:   u8; else_block: BlockExpr (if has_else)
```

#### WhenExpr (tag=14)
```
subject:   Expr
arm_count: u32
arms:      arm_count × WhenArm
```

##### WhenArm
```
pat_count:  u32
patterns:   pat_count × Pattern
has_guard:  u8; guard: Expr (if has_guard)
body:       Expr
span:       SourceSpan
```

#### LoopExpr (tag=15)
```
has_label:  u8; label: string (if has_label)
body:       BlockExpr
```

#### WhileExpr (tag=16)
```
has_label:  u8; label: string (if has_label)
condition:  Expr
body:       BlockExpr
```

#### ForExpr (tag=17)
```
has_label:  u8; label: string (if has_label)
pattern:    Pattern
iterable:   Expr
body:       BlockExpr
```

#### BlockExpr (tag=18)
```
stmt_count: u32
stmts:      stmt_count × Stmt
has_tail:   u8; tail_expr: Expr (if has_tail)
```

#### ReturnExpr (tag=19)
```
has_value: u8; value: Expr (if has_value)
```

#### BreakExpr (tag=20)
```
has_label: u8; label: string (if has_label)
has_value: u8; value: Expr (if has_value)
```

#### ContinueExpr (tag=21)
```
has_label: u8; label: string (if has_label)
```

#### ClosureExpr (tag=22)
```
param_count:     u32; params × ClosureParam
has_return_type: u8; return_type: Type (if has_return_type)
body:            Expr
```

##### ClosureParam
```
pattern: Pattern
has_type: u8; type: Type (if has_type)
span:     SourceSpan
```

#### RangeExpr (tag=23)
```
has_start:  u8; start: Expr (if has_start)
has_end:    u8; end: Expr (if has_end)
inclusive:  u8
```

#### CastExpr (tag=24)
```
expr:        Expr
target_type: Type
```

#### IsExpr (tag=25)
```
expr:    Expr
pattern: Pattern
```

#### TryExpr (tag=26)
```
expr: Expr
```

#### AwaitExpr (tag=27)
```
expr: Expr
```

#### ThrowExpr (tag=28)
```
expr: Expr
```

#### PathExpr (tag=29)
```
segment_count: u32; segments × string
generic_count: u32; generics × Type
```

#### LowlevelExpr (tag=30)
```
stmt_count: u32
stmts:      stmt_count × Stmt
has_tail:   u8; tail_expr: Expr (if has_tail)
```

#### InterpolatedStringExpr (tag=31)
```
part_count: u32
parts:      part_count × InterpPart
```

##### InterpPart
```
kind: u8  (0=literal string, 1=expr)
  if kind==0: value string
  if kind==1: expr Expr
```

#### TemplateLiteralExpr (tag=32)
```
raw: string
```

#### BaseExpr (tag=33)
```
(no fields beyond span)
```

#### NewExpr (tag=34)
```
type:      Type
arg_count: u32; args × Expr
```

### Stmt encoding
```
tag:  StmtTag u8
span: SourceSpan
<kind-specific fields>
```

#### LetStmt (tag=0)
```
pattern:  Pattern
has_type: u8; type: Type (if has_type)
has_init: u8; init: Expr (if has_init)
```

#### VarStmt (tag=1)
```
name:     string
has_type: u8; type: Type (if has_type)
has_init: u8; init: Expr (if has_init)
```

#### LetElseStmt (tag=2)
```
pattern:    Pattern
has_type:   u8; type: Type (if has_type)
init:       Expr
else_block: BlockExpr
```

#### ExprStmt (tag=3)
```
expr: Expr
```

#### DeclStmt (tag=4)
```
decl: Decl
```

### Type encoding
```
tag:  TypeTag u8
span: SourceSpan
<kind-specific fields>
```

#### NamedType (tag=0)
```
path_segments: u32; segments × string
has_generics:  u8
  if has_generics: generic_count u32; generics × GenericArg
```

##### GenericArg
```
kind:    u8  (0=type, 1=const, 2=binding)
has_name: u8; name: string (if kind==2)
  if kind==0 or kind==2: type: Type
  if kind==1: expr: Expr
span:    SourceSpan
```

#### RefType (tag=1)
```
is_mut:      u8
has_lifetime: u8; lifetime: string (if has_lifetime)
inner:       Type
```

#### PtrType (tag=2)
```
is_mut: u8
inner:  Type
```

#### ArrayType (tag=3)
```
element: Type
size:    Expr
```

#### SliceType (tag=4)
```
element: Type
```

#### TupleType (tag=5)
```
elem_count: u32
elements:   elem_count × Type
```

#### FuncType (tag=6)
```
param_count: u32
params:      param_count × Type
return_type: Type
```

#### InferType (tag=7)
```
(no fields beyond span)
```

#### DynType (tag=8)
```
path_segments:   u32; segments × string
has_generics:    u8; generics (same as NamedType if present)
is_mut:          u8
```

#### ImplBehaviorType (tag=9)
```
path_segments: u32; segments × string
has_generics:  u8; generics (same as NamedType if present)
```

### Pattern encoding
```
tag:  PatternTag u8
span: SourceSpan
<kind-specific fields>
```

#### WildcardPattern (tag=0)
```
(no fields)
```

#### IdentPattern (tag=1)
```
name:        string
is_mut:      u8
has_type_ann: u8; type_ann: Type (if has_type_ann)
```

#### LiteralPattern (tag=2)
```
kind:  LiteralKind u8
value: (per LiteralKind)
```

#### TuplePattern (tag=3)
```
elem_count: u32
elements:   elem_count × Pattern
```

#### StructPattern (tag=4)
```
path_segments: u32; segments × string
field_count:   u32; fields × { string, Pattern }
has_rest:      u8
```

#### EnumPattern (tag=5)
```
path_segments: u32; segments × string
has_payload:   u8
  if has_payload: payload_count u32; payload × Pattern
```

#### OrPattern (tag=6)
```
pat_count: u32
patterns:  pat_count × Pattern
```

#### RangePattern (tag=7)
```
has_start:  u8; start: Expr (if has_start)
has_end:    u8; end: Expr (if has_end)
inclusive:  u8
```

#### ArrayPattern (tag=8)
```
elem_count: u32
elements:   elem_count × Pattern
has_rest:   u8; rest: Pattern (if has_rest)
```

## TypeEnv Binary Format (magic = 0x544D4C45 "TMLE")

TypeEnv serialization uses a two-pass scheme to handle cyclic type references.

### Pass 1 — Assign indices
Walk every `Type*` reachable from the TypeEnv's type registry. Assign a `u32` index to each
unique pointer. Cross-references are stored as u32 indices, not pointers.

### Pass 2 — Write flattened type table
```
type_count: u32
types:      type_count × TypeEntry
```

Where each TypeEntry includes the type's kind tag + fields, with any nested `Type*` written
as their u32 index.

### TypeEnv main sections (after header + type table)
```
function_count: u32
functions:      count × { name: string, type_idx: u32 }

struct_count: u32
structs:      count × { name: string, field_count u32, fields × { name: string, type_idx u32 } }

behavior_count: u32
behaviors:      count × { name: string, method_count u32, methods × { name: string, type_idx u32 } }

impl_count: u32
impls:      count × { self_type_idx: u32, trait_type_idx: u32 (0 = inherent) }
```

### Deserialization
1. Read full type table into `std::vector<Type>` (all entries, with cross-refs as indices)
2. Second pass: for all entries, replace index fields with actual pointers to type table entries
3. Populate TypeEnv maps from sections, looking up types by index
