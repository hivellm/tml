# 27. AST Structure

The parser produces an Abstract Syntax Tree with source spans for every node.

## 1. Design Principles

1. **Every node has a span** - Enables precise error messages
2. **Nodes are typed** - Union types for each category
3. **Immutable by default** - Transformations create new trees
4. **Arena allocated** - Fast allocation, single deallocation
5. **Stable IDs** - Content-addressable for diffing

## 2. Span Structure

```tml
type Span = {
    file_id: U32,       // Index into file table
    start: U32,         // Byte offset start
    end: U32,           // Byte offset end (exclusive)
}

// Extended span for error messages
type FullSpan = {
    file: String,       // File path
    start: Position,
    end: Position,
    source: String,     // Source line(s)
}

type Position = {
    line: U32,          // 1-indexed
    column: U32,        // 1-indexed, UTF-8 bytes
    offset: U32,        // Byte offset
}
```

### 2.1 Span Operations

```tml
impl Span {
    // Combine two spans (first.start to second.end)
    func merge(this: This, other: This) -> This

    // Check if position is within span
    func contains(this: ref This, offset: U32) -> Bool

    // Get source text
    func text(this: ref This, source: ref Source) -> ref str
}
```

## 3. Node ID

Every AST node has a stable ID for diffing and patching.

```tml
type NodeId = {
    hash: [U8; 12],     // Content hash (base58 encoded)
}

impl NodeId {
    func from_content(content: ref [U8]) -> This {
        let hash = sha256(content)
        This { hash: hash[0..12] }
    }

    func to_string(this: ref This) -> String {
        base58_encode(this.hash)
    }
}
```

## 4. Base Node

All AST nodes embed the base:

```tml
type Node[T] = {
    id: NodeId,
    span: Span,
    data: T,
}
```

## 5. Module Structure

```tml
type Module = Node[ModuleData]

type ModuleData = {
    name: Maybe[Ident],
    items: Vec[Item],
}
```

## 6. Items

```tml
type Item = Node[ItemData]

type ItemData =
    | Func(FuncDef)
    | Type(TypeDef)
    | Const(ConstDef)
    | Behavior(BehaviorDef)
    | Impl(ImplBlock)
    | Mod(ModDef)
    | Decorator(DecoratorDef)
    | Use(UseDecl)
```

### 6.1 Function Definition

```tml
type FuncDef = {
    decorators: Vec[Decorator],
    visibility: Maybe[Visibility],
    name: Ident,
    generic_params: Vec[GenericParam],
    params: Vec[Param],
    return_type: Maybe[Type],
    effects: Vec[Ident],
    body: Maybe[Block],
}

type Param = Node[ParamData]

type ParamData = {
    is_mut: Bool,
    name: Ident,
    ty: Type,
}
```

### 6.2 Type Definition

```tml
type TypeDef = {
    decorators: Vec[Decorator],
    visibility: Maybe[Visibility],
    name: Ident,
    generic_params: Vec[GenericParam],
    body: TypeBody,
}

type TypeBody =
    | Struct(StructDef)
    | Sum(SumDef)
    | Alias(Type)

type StructDef = {
    fields: Vec[Field],
}

type Field = Node[FieldData]

type FieldData = {
    decorators: Vec[Decorator],
    visibility: Maybe[Visibility],
    name: Ident,
    ty: Type,
    default: Maybe[Expr],
}

type SumDef = {
    variants: Vec[Variant],
}

type Variant = Node[VariantData]

type VariantData = {
    name: Ident,
    fields: VariantFields,
}

type VariantFields =
    | Unit
    | Tuple(Vec[Type])
    | Struct(Vec[Field])
```

### 6.3 Other Items

```tml
type ConstDef = {
    decorators: Vec[Decorator],
    visibility: Maybe[Visibility],
    name: Ident,
    ty: Type,
    value: Expr,
}

type BehaviorDef = {
    decorators: Vec[Decorator],
    visibility: Maybe[Visibility],
    name: Ident,
    generic_params: Vec[GenericParam],
    items: Vec[BehaviorItem],
}

type BehaviorItem = {
    decorators: Vec[Decorator],
    name: Ident,
    generic_params: Vec[GenericParam],
    params: Vec[Param],
    return_type: Maybe[Type],
    effects: Vec[Ident],
    default_body: Maybe[Block],
}

type ImplBlock = {
    generic_params: Vec[GenericParam],
    self_type: Type,
    trait_type: Maybe[Type],
    items: Vec[Item],
}

type ModDef = {
    visibility: Maybe[Visibility],
    name: Ident,
    items: Maybe[Vec[Item]>,  // None = external module
}

type UseDecl = {
    path: UsePath,
    alias: Maybe[Ident>,
}
```

## 7. Types

```tml
type Type = Node[TypeData]

type TypeData =
    | Path(PathType)
    | Tuple(Vec[Type])
    | Array(ArrayType)
    | Slice(Type)
    | Reference(RefType)
    | Pointer(PtrType)
    | Function(FuncType)
    | Never
    | Inferred  // _

type PathType = {
    path: Path,
    generic_args: Vec[Type],
}

type ArrayType = {
    element: Heap[Type],
    size: Heap[Expr],
}

type RefType = {
    is_mut: Bool,
    inner: Heap[Type],
}

type PtrType = {
    is_mut: Bool,
    inner: Heap[Type],
}

type FuncType = {
    params: Vec[Type],
    return_type: Heap[Type],
    effects: Vec[Ident],
}
```

## 8. Expressions

```tml
type Expr = Node[ExprData]

type ExprData =
    // Literals
    | IntLit(IntLiteral)
    | FloatLit(FloatLiteral)
    | StringLit(StringLiteral)
    | CharLit(Char)
    | BoolLit(Bool)
    | Unit

    // Identifiers
    | Ident(Ident)
    | Path(Path)
    | This

    // Operations
    | Binary(BinaryExpr)
    | Unary(UnaryExpr)
    | Call(CallExpr)
    | MethodCall(MethodCallExpr)
    | Index(IndexExpr)
    | Field(FieldExpr)
    | Cast(CastExpr)

    // Control flow
    | If(IfExpr)
    | Match(MatchExpr)
    | Loop(LoopExpr)
    | For(ForExpr)
    | Block(Block)
    | Return(Maybe[Heap[Expr]>)
    | Break(BreakExpr)
    | Continue(Maybe[Ident>)

    // Other
    | Closure(ClosureExpr)
    | StructInit(StructInitExpr)
    | ArrayInit(ArrayInitExpr)
    | TupleInit(Vec[Expr])
    | Propagate(Heap[Expr])
    | Await(Heap[Expr])
    | Ref(RefExpr)
    | Deref(Heap[Expr])
    | Assign(AssignExpr)
    | Let(LetExpr)
    | Quote(QuoteExpr)
```

### 8.1 Expression Details

```tml
type BinaryExpr = {
    op: BinaryOp,
    left: Heap[Expr],
    right: Heap[Expr],
}

type BinaryOp =
    // Arithmetic
    | Add | Sub | Mul | Div | Mod | Pow
    // Comparison
    | Eq | Ne | Lt | Le | Gt | Ge
    // Logical
    | And | Or
    // Bitwise
    | BitAnd | BitOr | BitXor | Shl | Shr
    // Assignment
    | Assign | AddAssign | SubAssign | MulAssign
    | DivAssign | ModAssign | BitAndAssign
    | BitOrAssign | BitXorAssign | ShlAssign | ShrAssign

type UnaryExpr = {
    op: UnaryOp,
    expr: Heap[Expr],
}

type UnaryOp =
    | Neg      // -
    | Not      // not
    | BitNot   // ~
    | Ref      // ref
    | MutRef   // mut ref
    | Deref    // *

type CallExpr = {
    func: Heap[Expr],
    generic_args: Vec[Type],
    args: Vec[Arg],
}

type Arg = {
    name: Maybe[Ident],
    value: Expr,
}

type MethodCallExpr = {
    receiver: Heap[Expr],
    method: Ident,
    generic_args: Vec[Type],
    args: Vec[Arg],
}

type IfExpr = {
    condition: Heap[Expr],
    then_branch: Heap[Expr],
    else_branch: Maybe[Heap[Expr]>,
}

type MatchExpr = {
    scrutinee: Heap[Expr],
    arms: Vec[MatchArm],
}

type MatchArm = {
    pattern: Pattern,
    guard: Maybe[Expr],
    body: Expr,
}

type LoopExpr = {
    label: Maybe[Ident],
    condition: Maybe[Heap[Expr]>,  // while condition
    body: Block,
}

type ForExpr = {
    label: Maybe[Ident],
    pattern: Pattern,
    iterable: Heap[Expr],
    body: Block,
}

type ClosureExpr = {
    params: Vec[Param],
    return_type: Maybe[Type>,
    body: Heap[Expr],
}

type StructInitExpr = {
    path: Path,
    fields: Vec[FieldInit],
    spread: Maybe[Heap[Expr]>,
}

type FieldInit = {
    name: Ident,
    value: Maybe[Expr>,  // None = shorthand { x } instead of { x: x }
}
```

## 9. Patterns

```tml
type Pattern = Node[PatternData]

type PatternData =
    | Wildcard                      // _
    | Ident(IdentPattern)           // x, mut x, x @ pat
    | Literal(Literal)              // 42, "foo"
    | Tuple(Vec[Pattern])           // (x, y, z)
    | Struct(StructPattern)         // Point { x, y }
    | Variant(VariantPattern)       // Some(x)
    | Or(Vec[Pattern])              // A | B | C
    | Range(RangePattern)           // 1 to 10
    | Ref(RefPattern)               // ref x, ref mut x

type IdentPattern = {
    is_mut: Bool,
    name: Ident,
    binding: Maybe[Heap[Pattern]>,  // @ pattern
}

type StructPattern = {
    path: Path,
    fields: Vec[FieldPattern],
    rest: Bool,  // ..
}

type FieldPattern = {
    name: Ident,
    pattern: Maybe[Pattern>,  // None = shorthand { x } instead of { x: x }
}

type VariantPattern = {
    path: Path,
    fields: Vec[Pattern],
}

type RangePattern = {
    start: Maybe[Literal>,
    end: Maybe[Literal>,
    inclusive: Bool,  // to vs through
}

type RefPattern = {
    is_mut: Bool,
    pattern: Heap[Pattern],
}
```

## 10. Statements

```tml
type Stmt = Node[StmtData]

type StmtData =
    | Let(LetStmt)
    | Expr(Expr)
    | Item(Item)
    | Empty  // ;

type LetStmt = {
    is_mut: Bool,
    pattern: Pattern,
    ty: Maybe[Type>,
    value: Expr,
}

type Block = Node[BlockData]

type BlockData = {
    stmts: Vec[Stmt],
    result: Maybe[Expr>,  // Trailing expression
}
```

## 11. Other

```tml
type Ident = Node[String]

type Path = Node[PathData]

type PathData = {
    segments: Vec[PathSegment],
}

type PathSegment =
    | Ident(Ident)
    | Crate
    | Super
    | SelfValue
    | SelfType  // This

type GenericParam = Node[GenericParamData]

type GenericParamData = {
    name: Ident,
    bounds: Vec[Path],
}

type Visibility =
    | Public
    | Crate
    | Super
    | Private
    | Restricted(Path)

type Decorator = Node[DecoratorData]

type DecoratorData = {
    path: Path,
    args: Vec[Arg],
}
```

## 12. Literals

```tml
type Literal =
    | Int(IntLiteral)
    | Float(FloatLiteral)
    | String(StringLiteral)
    | Char(Char)
    | Bool(Bool)

type IntLiteral = {
    value: U128,
    suffix: Maybe[IntSuffix>,
}

type IntSuffix =
    | I8 | I16 | I32 | I64 | I128
    | U8 | U16 | U32 | U64 | U128

type FloatLiteral = {
    value: F64,
    suffix: Maybe[FloatSuffix>,
}

type FloatSuffix = F32 | F64

type StringLiteral = {
    value: String,
    is_raw: Bool,
}
```

## 13. AST Traversal

### 13.1 Visitor Pattern

```tml
behavior Visitor {
    func visit_module(this: mut ref This, node: ref Module)
    func visit_item(this: mut ref This, node: ref Item)
    func visit_expr(this: mut ref This, node: ref Expr)
    func visit_stmt(this: mut ref This, node: ref Stmt)
    func visit_type(this: mut ref This, node: ref Type)
    func visit_pattern(this: mut ref This, node: ref Pattern)
}
```

### 13.2 Walker

```tml
func walk_module(visitor: mut ref impl Visitor, module: ref Module) {
    for item in module.items {
        visitor.visit_item(item)
    }
}

func walk_expr(visitor: mut ref impl Visitor, expr: ref Expr) {
    when expr.data {
        Binary(b) -> {
            visitor.visit_expr(b.left)
            visitor.visit_expr(b.right)
        },
        Call(c) -> {
            visitor.visit_expr(c.func)
            for arg in c.args {
                visitor.visit_expr(arg.value)
            }
        },
        // ... etc
    }
}
```

## 14. Arena Allocation

AST nodes are allocated in an arena for fast allocation and deallocation:

```tml
type AstArena = {
    nodes: Arena[Node],
    strings: StringInterner,
}

impl AstArena {
    func alloc[T](this: mut ref This, data: T) -> Heap[Node[T]]

    func intern(this: mut ref This, s: ref str) -> InternedString

    // Deallocate all nodes at once
    func clear(this: mut ref This)
}
```

## 15. Source Map

Maps spans back to source files:

```tml
type SourceMap = {
    files: Vec[SourceFile>,
}

type SourceFile = {
    id: U32,
    path: String,
    content: String,
    line_starts: Vec[U32>,  // Byte offsets of line starts
}

impl SourceMap {
    func add_file(this: mut ref This, path: String, content: String) -> U32

    func lookup_span(this: ref This, span: Span) -> FullSpan

    func position_of(this: ref This, file: U32, offset: U32) -> Position
}
```
