# TML v1.0 — EBNF Grammar

## 1. Notation

```
=       definition
|       alternative
?       optional (0 or 1)
*       repetition (0 or more)
+       repetition (1 or more)
()      grouping
'x'     literal terminal
X       non-terminal
```

## 2. Program

```ebnf
Program = ModDecl? Use* Item*

ModDecl = 'mod' ModulePath

Use = 'use' UsePath ('as' Ident)?
    | 'pub' 'use' UsePath

ModulePath = Ident ('::' Ident)*
UsePath = ModulePath ('::' '{' Ident (',' Ident)* '}')?
        | ModulePath '::' '*'
```

**Examples:**
```tml
mod http::client

use std::io
use std::collections::{List, Map}
use utils as u
pub use types::*
```

## 3. Items

```ebnf
Item = Function
     | TypeDecl
     | UnionDecl
     | BehaviorDecl
     | ImplDecl
     | ConstDecl
     | TypeAliasDecl
     | ClassDecl
     | InterfaceDecl
     | NamespaceDecl

Visibility = 'pub' | 'pub' '(' 'crate' ')' | 'private'
```

### 3.1 Functions

```ebnf
Function = Directive* Visibility? FuncModifiers? 'func' Ident
           GenericParams? '(' Params? ')' ('->' Type)?
           WhereClause?
           Block

FuncModifiers = 'async' | 'lowlevel'

GenericParams = '[' GenericParam (',' GenericParam)* ']'
GenericParam  = Ident (':' TypeBound)?
TypeBound     = Type ('+' Type)*

WhereClause = 'where' WhereConstraint (',' WhereConstraint)*
WhereConstraint = Type ':' TypeBound

Params = Param (',' Param)*
Param  = Ident ':' Type
```

**Examples:**
```tml
func add(a: I32, b: I32) -> I32 {
    return a + b
}

pub func first[T](list: List[T]) -> Maybe[T] {
    return list.get(0)
}

async func fetch_data(url: Str) -> Outcome[Str, Error] {
    let response = http::get(url).await!
    return Ok(response.body())
}
```

### 3.2 Types

```ebnf
TypeDecl = Directive* Visibility? ('type' | 'enum') Ident
           GenericParams? TypeBody

TypeBody = StructBody | EnumBody

StructBody = '{' (Field (',' Field)* ','?)? '}'
Field      = Visibility? Ident ':' Type

EnumBody = '{' Variant (',' Variant)* ','? '}'
Variant  = Ident VariantData?
VariantData = '(' Type (',' Type)* ')'

TypeAliasDecl = Visibility? 'type' Ident GenericParams? '=' Type

UnionDecl = Directive* Visibility? 'union' Ident
            '{' (Field (',' Field)* ','?)? '}'
```

> **Note:** `enum` is a keyword alias for `type` — both produce the same token.
> Enum variants use positional data only (e.g. `Ok(T)`), not named fields.

**Examples:**
```tml
// Struct
type Point {
    x: F64,
    y: F64,
}

// Struct with visibility
type Config {
    pub name: Str,
    pub value: I32,
}

// Enum (no data)
type Color {
    Red,
    Green,
    Blue,
}

// Enum with data
type Outcome[T, E] {
    Ok(T),
    Err(E),
}

type Maybe[T] {
    Just(T),
    Nothing,
}

type JsonValue {
    Null,
    Bool(Bool),
    Number(F64),
    Text(Str),
    Array(List[JsonValue]),
    Object(HashMap[Str, JsonValue]),
}

// Alias
type UserId = U64
type Handler = func(Request) -> Response

// Union (C-style, all fields share memory)
union IntOrPtr {
    int_val: I32,
    ptr_val: I64,
}
```

### 3.3 Behaviors

```ebnf
BehaviorDecl  = Directive* Visibility? 'behavior' Ident GenericParams?
                (':' TypeBound)? '{' BehaviorItem* '}'
BehaviorAlias = Directive* Visibility? 'behavior' Ident GenericParams?
                '=' TypeBound ('+' TypeBound)*

BehaviorItem = BehaviorFunc | AssociatedType

BehaviorFunc = Visibility? 'func' Ident GenericParams? '(' Params? ')' ('->' Type)?
               (Block | ';')

AssociatedType = 'type' Ident (':' TypeBound)? ('=' Type)?
```

**Examples:**
```tml
behavior Equal {
    func eq(this, other: This) -> Bool;
}

behavior Ordered: Equal {
    func compare(this, other: This) -> Ordering;

    // Default implementation
    func less_than(this, other: This) -> Bool {
        return this.compare(other) == Less
    }
}

behavior Iterable {
    type Item;
    func next(this) -> Maybe[This.Item];
}

// Behavior alias — names a combination of bounds
behavior Numeric = Add[Self] + Sub[Self] + Mul[Self] + Div[Self]
```

#### 3.3.1 Operator Behaviors

Binary and unary operators on non-primitive types dispatch to behavior method calls:

| Operator | Behavior | Method |
|----------|----------|--------|
| `+` | `Add[Rhs]` | `add(this, rhs)` |
| `-` | `Sub[Rhs]` | `sub(this, rhs)` |
| `*` | `Mul[Rhs]` | `mul(this, rhs)` |
| `/` | `Div[Rhs]` | `div(this, rhs)` |
| `%` | `Rem[Rhs]` | `rem(this, rhs)` |
| `-x` | `Neg` | `neg(this)` |
| `!x` | `Not` | `not(this)` |
| `==` | `PartialEq` | `eq(this, rhs)` |
| `<` | `PartialOrd` | `lt(this, rhs)` |
| `a[i]` | `Index[Idx]` | `index(this, idx)` |

Primitive types (I8–I64, U8–U64, F32, F64) use built-in LLVM arithmetic — no behavior dispatch.

### 3.4 Impl

```ebnf
ImplDecl = 'impl' GenericParams? TypePath GenericArgs?
           ('for' Type)? WhereClause?
           '{' ImplItem* '}'

ImplItem = AssociatedType | Visibility? Function
```

Two forms:
- **Inherent impl:** `impl Type { methods }` — adds methods to a type
- **Behavior impl:** `impl Behavior for Type { methods }` — implements a behavior

**Examples:**
```tml
// Inherent impl (add methods to a type)
impl Point {
    pub func new(x: F64, y: F64) -> This {
        return This { x, y }
    }

    pub func origin() -> This {
        return This.new(0.0, 0.0)
    }

    pub func distance(this, other: Point) -> F64 {
        let dx: F64 = this.x - other.x
        let dy: F64 = this.y - other.y
        return (dx**2 + dy**2).sqrt()
    }
}

// Behavior impl (implement a behavior for a type)
impl Equal for Point {
    func eq(this, other: This) -> Bool {
        return this.x == other.x and this.y == other.y
    }
}

// Generic impl
impl[T] Iterable for List[T] {
    type Item = T;

    func next(this) -> Maybe[T] {
        // ...
    }
}
```

### 3.5 Constants

```ebnf
ConstDecl = Visibility? 'const' Ident ':' Type '=' Expr
```

**Examples:**
```tml
const PI: F64 = 3.14159265359
const MAX_SIZE: I32 = 1024
pub const VERSION: Str = "1.0.0"
```

### 3.6 Classes (OOP)

```ebnf
ClassDecl = Directive* ClassModifiers? 'class' Ident GenericParams?
            ExtendsClause? ImplementsClause?
            '{' ClassMember* '}'

ClassModifiers = 'abstract' | 'sealed'

ExtendsClause = 'extends' TypePath
ImplementsClause = 'implements' TypePath (',' TypePath)*

ClassMember = ClassField | ClassMethod | ClassConstructor

ClassField = FieldModifiers? Ident ':' Type ('=' Expr)?
FieldModifiers = MemberVisibility? 'static'?
MemberVisibility = 'private' | 'protected' | 'pub'

ClassMethod = MethodModifiers? 'func' Ident GenericParams?
              '(' Params? ')' ('->' Type)?
              BaseCall?
              (Block | ';')

MethodModifiers = MemberVisibility? MethodKind*
MethodKind = 'static' | 'virtual' | 'override' | 'abstract'

BaseCall = 'base' ':' TypePath '::' Ident '(' Args? ')'
```

**Examples:**
```tml
abstract class Animal {
    protected name: Str

    abstract func speak(this) -> Str

    virtual func move(this) {
        println("Moving")
    }
}

class Dog extends Animal implements Speakable {
    private breed: Str

    func new(name: Str, breed: Str) -> Dog
        base: Animal::new(name)
    {
        return Dog { breed: breed }
    }

    override func speak(this) -> Str {
        return "Woof!"
    }
}

sealed class GermanShepherd extends Dog {
    static count: I32 = 0
}
```

### 3.7 Interfaces (OOP)

```ebnf
InterfaceDecl = Directive* Visibility? 'interface' Ident GenericParams?
                InterfaceExtends?
                '{' InterfaceMethod* '}'

InterfaceExtends = 'extends' TypePath (',' TypePath)*

InterfaceMethod = 'func' Ident GenericParams?
                  '(' Params? ')' ('->' Type)?
                  (Block | ';')
```

**Examples:**
```tml
interface Speakable {
    func speak(this) -> Str
}

interface Orderable extends Comparable[This] {
    func less_than(this, other: This) -> Bool {
        return this.compare(other) < 0
    }
}
```

### 3.8 Namespaces (OOP)

```ebnf
NamespaceDecl = 'namespace' NamespacePath '{' Item* '}'

NamespacePath = Ident ('::' Ident)*
```

## 4. Statements

```ebnf
Statement = LetStmt
          | LetElseStmt
          | VarStmt
          | ExprStmt
          | ReturnStmt
          | BreakStmt
          | ContinueStmt

LetStmt     = 'let' Pattern (':' Type)? '=' Expr
LetElseStmt = 'let' Pattern (':' Type)? '=' Expr 'else' Block
VarStmt     = 'var' Ident ':' Type '=' Expr

ExprStmt = Expr

ReturnStmt   = 'return' Expr?
BreakStmt    = 'break'
ContinueStmt = 'continue'
```

**Examples:**
```tml
let x: I32 = 42
let Point { x, y } = get_point()

// Let-else: unwrap or early exit
let Just(value) = maybe_result else { return }
let Ok(data) = fetch(url) else { return Err(e) }

var count: I32 = 0

return result
break
continue
```

## 5. Expressions

### 5.1 Precedence Hierarchy

```ebnf
Expr = AssignExpr

AssignExpr  = PipeExpr (AssignOp PipeExpr)?
AssignOp    = '=' | '+=' | '-=' | '*=' | '/=' | '%='
            | '&=' | '|=' | '^=' | '<<=' | '>>='

PipeExpr    = OrExpr ('|>' OrExpr)*

OrExpr      = AndExpr ('or' AndExpr)*
AndExpr     = NotExpr ('and' NotExpr)*
NotExpr     = 'not' NotExpr | CompareExpr

CompareExpr = BitOrExpr (CompareOp BitOrExpr)?
CompareOp   = '==' | '!=' | '<' | '>' | '<=' | '>='

BitOrExpr   = BitXorExpr ('|' BitXorExpr)*
BitXorExpr  = BitAndExpr ('^' BitAndExpr)*
BitAndExpr  = ShiftExpr ('&' ShiftExpr)*
ShiftExpr   = AddExpr (('<<' | '>>') AddExpr)*

AddExpr     = MulExpr (('+' | '-') MulExpr)*
MulExpr     = PowExpr (('*' | '/' | '%') PowExpr)*
PowExpr     = CastExpr ('**' CastExpr)*

CastExpr    = UnaryExpr (('as' | 'is') Type)*

UnaryExpr   = ('-' | '~' | 'not' | 'ref') UnaryExpr
            | PostfixExpr

PostfixExpr = PrimaryExpr Postfix*
Postfix     = '.' Ident                         // field access
            | '.' Ident GenericArgs? '(' Args? ')'  // method call
            | '?.' Ident                         // optional field access
            | '?.' Ident '(' Args? ')'          // optional method call
            | '(' Args? ')'                      // function call
            | '[' Expr ']'                       // index
            | '!'                                // error propagation (try)
            | '++'                               // postfix increment
            | '--'                               // postfix decrement
            | '.' 'await'                        // async await
```

### 5.2 Primary Expressions

```ebnf
PrimaryExpr = Literal
            | Ident
            | TypePath
            | 'this'
            | 'This'
            | 'ref' Expr
            | 'await' Expr
            | GroupExpr
            | BlockExpr
            | IfExpr
            | WhenExpr
            | LoopExpr
            | WhileExpr
            | ForExpr
            | DoExpr
            | ThrowExpr
            | LowlevelExpr
            | StructExpr
            | ArrayExpr

GroupExpr = '(' Expr ')'

BlockExpr = Block
Block     = '{' Statement* Expr? '}'

Literal = IntLit | FloatLit | StringLit | InterpolatedString
        | TemplateLiteral | BoolLit | CharLit | NullLit
BoolLit = 'true' | 'false'
NullLit = 'null'

// Interpolated strings: "Hello {name}!" where {expr} are embedded expressions
InterpolatedString = InterpStart (Expr InterpMiddle)* Expr? InterpEnd

// Template literals: `Hello, {name}!` — produces Text type
TemplateLiteral = '`' (TemplateChar | '{' Expr '}')* '`'
```

### 5.3 If Expression

```ebnf
IfExpr    = 'if' IfCond IfBody ('else' ElseBody)?
IfCond    = 'let' Pattern '=' Expr  // if-let pattern matching
          | Expr                     // regular condition
IfBody    = 'then' Expr               // expression form
          | Block                     // block form
ElseBody  = 'if' IfCond IfBody ('else' ElseBody)?  // else-if chain
          | 'then' Expr                             // expression form
          | Block                                   // block form
```

**Two syntaxes supported:**

1. **Expression form** (with `then` keyword):
```tml
if x > 0 then x else -x

if x < 0 then "negative"
else if x == 0 then "zero"
else "positive"
```

2. **Block form** (with braces):
```tml
let result: I32 = if x > 0 {
    x * 2
} else {
    0
}
```

3. **If-let pattern matching**:
```tml
if let Just(value) = maybe_x {
    println(value)
} else {
    println("nothing")
}
```

### 5.4 When Expression (Pattern Matching)

```ebnf
WhenExpr = 'when' Expr '{' WhenArm+ '}'
WhenArm  = Pattern Guard? '=>' (Expr | Block) ','?

Guard = 'if' Expr
```

**Pattern Types:**

```ebnf
Pattern = LiteralPattern
        | RangePattern
        | IdentPattern
        | WildcardPattern
        | StructPattern
        | EnumPattern
        | TuplePattern
        | ArrayPattern

LiteralPattern   = Literal
RangePattern     = Literal ('to' | 'through') Literal
IdentPattern     = Ident
WildcardPattern  = '_'
StructPattern    = TypePath '{' FieldPattern (',' FieldPattern)* '}'
FieldPattern     = Ident (':' Pattern)?
EnumPattern      = TypePath ('(' Pattern (',' Pattern)* ')')?
TuplePattern     = '(' Pattern (',' Pattern)+ ')'
ArrayPattern     = '[' Pattern (',' Pattern)* ('..' Ident?)? ']'
```

| Pattern | Syntax | Example |
|---------|--------|---------|
| Literal | `value` | `42`, `"hello"`, `true` |
| Range | `start to/through end` | `0 through 9`, `'a' to 'z'` |
| Ident | `name` | `x`, `value` |
| Wildcard | `_` | `_` (matches anything) |
| Struct | `Type { fields }` | `Point { x, y }` |
| Enum | `Variant(patterns)` | `Just(x)`, `Ok(value)` |
| Tuple | `(patterns)` | `(a, b, c)` |
| Array | `[patterns]` | `[first, second, _]` |

**Range Keywords:**
- `to` — exclusive end: `0 to 10` matches 0–9
- `through` — inclusive end: `0 through 9` matches 0–9

**Examples:**
```tml
// Literal patterns
when value {
    0 => "zero",
    1 => "one",
    n => "other: " + n.to_string(),
}

// Range patterns
when n {
    0 through 9 => "single digit",
    10 to 100 => "two digits",
    _ => "large",
}

// Enum patterns with payload extraction
when result {
    Ok(value) => use(value),
    Err(e) => log(e),
}

// With guards
when token {
    Ident(name) if name == "self" => handle_self(),
    Ident(name) => handle_ident(name),
    _ => skip(),
}

// Block bodies in arms
when n {
    0 => {
        let result: I32 = compute()
        result * 2
    },
    _ => n,
}
```

### 5.5 Loop Expressions

#### 5.5.1 Conditional Loop

```ebnf
LoopExpr = 'loop' '(' Expr ')' Block
         | 'loop' '(' 'true' ')' Block      // infinite loop
```

**Examples:**
```tml
// Conditional loop
loop (count < 10) {
    print(count)
    count = count + 1
}

// Infinite loop
loop (true) {
    if done then break
    work()
}
```

#### 5.5.2 While Loop

```ebnf
WhileExpr = 'while' Expr Block
```

The `while` keyword is an alias for `loop (condition)`.

**Examples:**
```tml
while running {
    tick()
}

while count < 10 {
    print(count)
    count = count + 1
}
```

#### 5.5.3 For Loop (Iteration)

```ebnf
ForExpr = 'for' Pattern 'in' Expr Block
```

**Examples:**
```tml
// Range iteration
for i in 0 to 10 {
    print(i)  // 0, 1, 2, ..., 9
}

for i in 1 through 5 {
    print(i)  // 1, 2, 3, 4, 5
}

// Collection iteration
for item in list {
    process(item)
}

// With pattern destructuring
for Point { x, y } in points {
    draw(x, y)
}
```

### 5.6 Error Propagation with `!`

```ebnf
TryExpr = Expr '!'
```

Propagates errors automatically. If the expression is `Err(e)`, the enclosing function returns `Err(e)`. If `Ok(v)`, unwraps to `v`.

**Examples:**
```tml
let file = File.open(path)!
let data = file.read()!
```

### 5.7 Throw Expression

```ebnf
ThrowExpr = 'throw' Expr
```

**Examples:**
```tml
throw Error::new("invalid input")
```

### 5.8 Do Expression (Closures/Lambdas)

```ebnf
DoExpr = 'move'? 'do' DoParams? ('->' Type)? DoBody

DoParams = '(' Param (',' Param)* ')'
         | Ident

DoBody = Expr
       | Block
```

**Examples:**
```tml
let add = do(x: I32, y: I32) x + y

let double = do(x) x * 2

let complex = do(x: I32, y: I32) -> I32 {
    let sum: I32 = x + y
    return sum * 2
}

// Move closure (captures by value)
let callback = move do(x) {
    captured_value.process(x)
}

// Usage with higher-order functions
items.map(do(x) x * 2)
items.filter(do(x) x > 0)
items.fold(0, do(acc, x) acc + x)
```

### 5.9 Lowlevel Expression

```ebnf
LowlevelExpr = 'lowlevel' Block
```

Enables low-level operations (raw pointers, intrinsics) within the block.

**Examples:**
```tml
lowlevel {
    let ptr = mem_alloc(size)
    ptr_write(ptr, value)
    ptr_read(ptr)
}
```

### 5.10 Pipe Forward

```ebnf
PipeExpr = Expr '|>' Expr
```

Desugars `a |> f(b)` to `f(a, b)` and `a |> f` to `f(a)`.

**Examples:**
```tml
let result = data
    |> parse()
    |> transform(config)
    |> serialize()
```

### 5.11 Optional Chaining

```ebnf
OptionalCall   = Expr '?.' Ident '(' Args? ')'
OptionalField  = Expr '?.' Ident
```

If the left-hand side is `Nothing`, the entire expression evaluates to `Nothing`.
Auto-flattening: if the method returns `Maybe[V]`, result is `Maybe[V]` (not `Maybe[Maybe[V]]`).

**Examples:**
```tml
let name = parse(json_str)?.get_string("name")
let age = user?.profile?.age
```

### 5.12 Type Operations

```ebnf
CastExpr  = Expr 'as' Type     // type cast
TypeCheck = Expr 'is' Type     // type check (returns Bool)
```

**Examples:**
```tml
let wide: I64 = narrow as I64
let is_string: Bool = value is Str
```

### 5.13 Struct Expression

```ebnf
StructExpr = TypePath '{' FieldInit (',' FieldInit)* ','? (',' '..' Expr)? '}'
FieldInit  = Ident (':' Expr)?
```

**Examples:**
```tml
Point { x: 1.0, y: 2.0 }

// Shorthand when variable has same name
let x: F64 = 1.0
let y: F64 = 2.0
Point { x, y }

// Update syntax
Point { x: 5.0, ..old_point }
```

### 5.14 Array Expression

```ebnf
ArrayExpr = '[' (Expr (',' Expr)* ','?)? ']'
          | '[' Expr ';' Expr ']'
```

**Examples:**
```tml
let numbers: List[I32] = [1, 2, 3, 4, 5]
let empty: List[I32] = []
let zeros = [0; 100]  // 100 zeros
```

### 5.15 Method Call & Field Access

```ebnf
MethodCall     = Expr '.' Ident GenericArgs? '(' Args? ')'
FieldAccess    = Expr '.' Ident
Args           = Expr (',' Expr)*
```

## 6. Types

```ebnf
Type = PrimitiveType
     | NamedType
     | GenericType
     | RefType
     | FuncType
     | ArrayType
     | TupleType
     | PointerType

PrimitiveType = 'Bool' | 'I8' | 'I16' | 'I32' | 'I64' | 'I128'
              | 'U8' | 'U16' | 'U32' | 'U64' | 'U128'
              | 'F32' | 'F64' | 'Str' | 'Char'

NamedType    = TypePath
GenericType  = TypePath '[' Type (',' Type)* ']'
RefType      = 'ref' Type | 'mut' 'ref' Type
FuncType     = 'func' '(' (Type (',' Type)*)? ')' '->' Type
ArrayType    = '[' Type ';' Expr ']'
TupleType    = '(' Type (',' Type)+ ')'
PointerType  = '*' 'mut'? Type

TypePath = Ident ('::' Ident)*
GenericArgs = '[' Type (',' Type)* ']'
```

**Examples:**
```tml
// Primitives
Bool, I32, U64, F64, Str, Char

// Named and Generic
Point
List[I32]
HashMap[Str, Value]
Outcome[Data, Error]

// References
ref Str
mut ref List[T]

// Functions
func(I32, I32) -> I32
func(Str) -> Bool

// Arrays
[I32; 10]

// Tuples
(I32, Str)
(F64, F64, F64)

// Pointers (lowlevel)
*I32
*mut U8
```

## 7. Directives

```ebnf
Directive     = '@' DirectiveName DirectiveArgs?
DirectiveName = Ident
DirectiveArgs = '(' (DirectiveArg (',' DirectiveArg)*)? ')'
              | '[' (DirectiveArg (',' DirectiveArg)*)? ']'
DirectiveArg  = Ident (':' Value)?
              | Expr
```

**Examples:**
```tml
@test
@when(os: linux)
@derive(PartialEq, Hash, Debug)
@auto(duplicate, equal, debug)    // alias for @derive with lowercase names
@deprecated("Use new_func instead")
@hint(inline: always)
@lowlevel
@extern("c")
@link("libfoo")
@repr(U8)                         // enum discriminant type
@packed                           // struct with no padding
@flags(U32)                       // bitflag enum
@simd                             // LLVM vector type
@no_mangle                        // bare symbol name
@intrinsic("llvm.fence")          // compiler builtin
@interior_mutable                 // Cell/Mutex-like types
```

### 7.1 Built-in Directives

| Directive | Target | Description |
|-----------|--------|-------------|
| `@test` | func | Mark as test function |
| `@derive(...)` | type/enum | Auto-generate behavior impls (Debug, Duplicate, PartialEq, Hash, ...) |
| `@auto(...)` | type/enum | Alias for `@derive` with lowercase names (`duplicate`→`Duplicate`, `equal`→`PartialEq`) |
| `@repr(U8\|U16\|I32\|I64)` | enum | Sequential discriminants with specified integer type |
| `@packed` | type | Packed struct layout — no inter-field padding |
| `@flags(U8\|U16\|U32\|U64)` | enum | Bitflag enum with power-of-2 discriminants |
| `@simd` | type | Compile fields as LLVM vector type |
| `@extern("abi")` | func | FFI function with ABI spec ("c", "c++", "stdcall") |
| `@link("lib")` | func | Specify library to link |
| `@no_mangle` | func | Don't mangle symbol name |
| `@intrinsic("name")` | func | Compiler builtin intrinsic |
| `@interior_mutable` | type | Allows mutation through shared references (Cell, Mutex) |
| `@deprecated("msg")` | any | Mark as deprecated with message |
| `@lowlevel` | func/block | Unsafe/lowlevel context |

## 8. LL(1) Verification

### 8.1 First Token Determines Production

| Token | Production |
|-------|------------|
| `mod` | ModDecl |
| `use` | Use |
| `pub` | Visibility + Item or Use |
| `private` | Visibility + Item |
| `func` | Function |
| `async` | Async Function (`async func ...`) |
| `lowlevel` | Lowlevel function or block |
| `type` / `enum` | TypeDecl (struct, enum, or alias) |
| `union` | UnionDecl |
| `behavior` | BehaviorDecl or BehaviorAlias (peek `=` after name → alias) |
| `impl` | ImplDecl |
| `const` | ConstDecl |
| `class` | ClassDecl |
| `interface` | InterfaceDecl |
| `namespace` | NamespaceDecl |
| `let` | LetStmt / LetElseStmt |
| `var` | VarStmt |
| `if` | IfExpr |
| `when` | WhenExpr |
| `loop` | LoopExpr |
| `while` | WhileExpr |
| `for` | ForExpr |
| `return` | ReturnStmt |
| `break` | BreakStmt |
| `continue` | ContinueStmt |
| `throw` | ThrowExpr |
| `do` / `move do` | DoExpr (closure) |
| `this` | ThisExpr |
| `This` | ThisType or Constructor |
| `ref` | RefExpr or RefType |
| `await` | AwaitExpr (prefix form) |
| `{` | BlockExpr |
| `(` | GroupExpr or TupleExpr |
| `[` | ArrayExpr |
| `@` | Directive |
| Ident | VarRef or FuncCall or TypeRef |
| Ident `::` Ident | PathExpr (enum variant, module path) |
| Literal | LiteralExpr |

### 8.2 No Ambiguities

**Generics vs Comparison:**
```tml
List[T]     // [ starts generic
a < b       // < always comparison
```

**Struct vs Block:**
```tml
Point { x: 1 }  // TypeName followed by { = struct
{ let x: I32 = 1 }   // { alone = block
```

**Closure vs OR:**
```tml
do(x) x + 1     // do starts closure
a | b           // | always bitwise OR
```

**Reference vs Bitwise AND:**
```tml
ref data        // ref keyword for references
a & b           // & always bitwise AND
```

## 9. Complete Example

```tml
mod math::geometry

use std::math::{sqrt, PI}

pub type Point {
    x: F64,
    y: F64,
}

pub type Circle {
    center: Point,
    radius: F64,
}

impl Point {
    pub func new(x: F64, y: F64) -> This {
        return This { x, y }
    }

    pub func origin() -> This {
        return This.new(0.0, 0.0)
    }

    pub func distance(this, other: Point) -> F64 {
        let dx: F64 = this.x - other.x
        let dy: F64 = this.y - other.y
        return sqrt(dx**2 + dy**2)
    }
}

impl Circle {
    pub func new(center: Point, radius: F64) -> This {
        return This { center, radius }
    }

    pub func area(this) -> F64 {
        return PI * this.radius**2
    }

    pub func contains(this, point: Point) -> Bool {
        return this.center.distance(point) <= this.radius
    }
}

@test
func test_distance() {
    let p1 = Point.new(0.0, 0.0)
    let p2 = Point.new(3.0, 4.0)
    assert_eq(p1.distance(p2), 5.0)
}
```

---

*Previous: [02-LEXICAL.md](./02-LEXICAL.md)*
*Next: [04-TYPES.md](./04-TYPES.md) — Type System*
