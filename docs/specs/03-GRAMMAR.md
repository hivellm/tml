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
Program = ModuleDecl? Import* Item*

ModuleDecl = 'module' ModulePath

Import = 'import' ImportPath ('as' Ident)?
       | 'public' 'import' ImportPath

ModulePath = Ident ('.' Ident)*
ImportPath = ModulePath ('.' '{' Ident (',' Ident)* '}')?
           | ModulePath '.' '*'
```

**Examples:**
```tml
module http.client

import std.io
import std.collections.{List, Map}
import utils as u
public import types.*
```

## 3. Items

```ebnf
Item = Function
     | TypeDecl
     | BehaviorDecl
     | ExtendDecl
     | ConstDecl

Visibility = 'public' | 'private'
```

### 3.1 Functions

```ebnf
Function = Directive* Visibility? 'func' Ident StableId?
           GenericParams? '(' Params? ')' ('->' Type)?
           WhereClause?
           Contract? EffectDecl? Block

GenericParams = '[' GenericParam (',' GenericParam)* ']'
GenericParam  = Ident (':' TypeBound)?
TypeBound     = Type ('+' Type)*

WhereClause = 'where' WhereConstraint (',' WhereConstraint)*
WhereConstraint = Type ':' TypeBound

Params = Param (',' Param)*
Param  = Ident ':' Type

Contract   = PreCond? PostCond?
PreCond    = 'pre' ':' Expr
PostCond   = 'post' ('(' Ident ')')? ':' Expr

EffectDecl = 'effects' ':' '[' Effect (',' Effect)* ']'
Effect     = Ident ('.' Ident)*
```

**Examples:**
```tml
func add(a: I32, b: I32) -> I32 {
    return a + b
}

public func first[T](list: List[T]) -> Maybe[T] {
    return list.get(0)
}

func sqrt(x: F64) -> F64
pre: x >= 0.0
post(r): r >= 0.0
{
    return x.sqrt_impl()
}

func read_file(path: String) -> Outcome[String, IoError]
effects: [io.file.read]
{
    // ...
}
```

### 3.2 Types

```ebnf
TypeDecl = Directive* Visibility? 'type' Ident StableId?
           GenericParams? TypeBody

TypeBody = StructBody | EnumBody | AliasBody

StructBody = '{' (Field (',' Field)* ','?)? '}'
Field      = Ident ':' Type

EnumBody = '{' Variant (',' Variant)* ','? '}'
Variant  = Ident VariantData?
VariantData = '(' Type (',' Type)* ')'
            | '{' Field (',' Field)* '}'

AliasBody = '=' Type
```

**Examples:**
```tml
// Struct
type Point {
    x: F64,
    y: F64,
}

// Simple Enum (no data)
type Color {
    Red,
    Green,
    Blue,
}

type Direction {
    North,
    South,
    East,
    West,
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
    Text(String),
    Array(List[JsonValue]),
    Object(Map[String, JsonValue]),
}

// Alias
type UserId = U64
type Handler = func(Request) -> Response
```

### 3.3 Behaviors

```ebnf
BehaviorDecl = Directive* Visibility? 'behavior' Ident GenericParams?
               (':' TypeBound)? '{' BehaviorItem* '}'

BehaviorItem = BehaviorFunc | BehaviorType

BehaviorFunc = 'func' Ident GenericParams? '(' Params? ')' ('->' Type)?
               (Block | ';')

BehaviorType = 'type' Ident (':' TypeBound)? ('=' Type)? ';'
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
```

### 3.4 Extend

```ebnf
ExtendDecl = 'extend' Type ('with' TypeBound)? '{' ExtendItem* '}'

ExtendItem = Function
```

**Examples:**
```tml
extend Point {
    func new(x: F64, y: F64) -> This {
        return This { x: x, y: y }
    }

    func origin() -> This {
        return This.new(0.0, 0.0)
    }
}

extend Point with Equal {
    func eq(this, other: This) -> Bool {
        return this.x == other.x and this.y == other.y
    }
}

extend List[T] with Iterable {
    type Item = T;

    func next(this) -> Maybe[T] {
        // ...
    }
}
```

### 3.5 Constants

```ebnf
ConstDecl = Visibility? 'const' Ident (':' Type)? '=' Expr
```

**Examples:**
```tml
const PI: F64 = 3.14159265359
const MAX_SIZE: I32 = 1024
public const VERSION: String = "1.0.0"
```

## 4. Statements

```ebnf
Statement = LetStmt
          | VarStmt
          | ExprStmt
          | ReturnStmt
          | BreakStmt
          | ContinueStmt

LetStmt = 'let' Pattern ':' Type '=' Expr
VarStmt = 'var' Ident ':' Type '=' Expr

ExprStmt = Expr

ReturnStmt   = 'return' Expr?
BreakStmt    = 'break'
ContinueStmt = 'continue'
```

**Examples:**
```tml
let x: I32 = 42
let y: I64 = 100
let Point { x, y } = get_point()

var count: I32 = 0
var name: String = "default"

return result
break
continue
```

## 5. Expressions

### 5.1 Precedence Hierarchy

```ebnf
Expr = OrExpr

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
PowExpr     = UnaryExpr ('**' UnaryExpr)*

UnaryExpr   = ('-' | '~') UnaryExpr
            | PostfixExpr

PostfixExpr = PrimaryExpr Postfix*
Postfix     = '.' Ident
            | '(' Args? ')'
            | '[' Expr ']'
            | '!'
            | '?'
```

### 5.2 Primary Expressions

```ebnf
PrimaryExpr = Literal
            | Ident
            | 'this'
            | 'This'
            | 'ref' Expr
            | GroupExpr
            | BlockExpr
            | IfExpr
            | WhenExpr
            | LoopExpr
            | CatchExpr
            | DoExpr
            | StructExpr
            | ArrayExpr

GroupExpr = '(' Expr ')'

BlockExpr = Block
Block     = '{' Statement* Expr? '}'

Literal = IntLit | FloatLit | StringLit | BoolLit | CharLit
BoolLit = 'true' | 'false'
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

if valid then
    process()
else
    error()

// Chained
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

if let Ok(data) = result {
    process(data)
}
```

### 5.4 When Expression (Pattern Matching)

```ebnf
WhenExpr = 'when' Expr '{' WhenArm+ '}'
WhenArm  = Pattern '->' Expr ','?

Pattern = LiteralPattern
        | IdentPattern
        | WildcardPattern
        | StructPattern
        | EnumPattern
        | TuplePattern

LiteralPattern   = Literal
IdentPattern     = Ident
WildcardPattern  = '_'
StructPattern    = TypePath '{' FieldPattern (',' FieldPattern)* '}'
FieldPattern     = Ident (':' Pattern)?
EnumPattern      = TypePath ('(' Pattern (',' Pattern)* ')')?
TuplePattern     = '(' Pattern (',' Pattern)+ ')'
```

**Examples:**
```tml
when value {
    0 -> "zero",
    1 -> "one",
    n -> "other: " + n.to_string(),
}

when result {
    Ok(value) -> use(value),
    Err(e) -> log(e),
}

when point {
    Point { x: 0, y: 0 } -> "origin",
    Point { x, y } -> "at " + x.to_string(),
}

// No guards - use inline if
when opt {
    Just(x) -> if x > 0 then x else 0,
    Nothing -> -1,
}
```

### 5.5 Loop Expression

```ebnf
LoopExpr = 'loop' LoopKind Block

LoopKind = Ident 'in' Expr       // for-each with binding
         | 'in' Expr             // for-each anonymous
         | 'while' Expr          // while
         | ε                     // infinite
```

**Examples:**
```tml
// For-each
loop item in items {
    process(item)
}

loop i in 0 to 10 {
    print(i)
}

loop i in 1 through 5 {
    print(i)  // 1, 2, 3, 4, 5
}

// While
loop while running {
    tick()
}

// Infinite
loop {
    if done then break
    work()
}
```

### 5.6 Error Propagation with `!`

```ebnf
PropagateExpr = Expr '!'
ElseExpr      = Expr '!' 'else' (Expr | '|' Ident '|' Expr)
```

Propagates errors automatically or recovers with else.

**Examples:**
```tml
let file: Outcome[File, Error] = File.open(path)!
let data: Outcome[String, Error] = file.read()!

// With fallback
let port: Outcome[String, Error] = env.get("PORT")!.parse[U16]()! else 8080

// With error binding
let data: Outcome[Data, Error] = fetch(url)! else |err| {
    log.warn(err.to_string())
    Data.default()
}
```

### 5.7 Catch Expression

```ebnf
CatchExpr = 'catch' Block 'else' ('|' Ident '|')? Block
```

**Examples:**
```tml
catch {
    let local: Data = load_local()!
    let remote: Outcome[Data, Error] = fetch_remote()!
    return Ok(merge(local, remote)!)
} else |err| {
    log.error(err.to_string())
    return Err(err)
}
```

### 5.8 Do Expression (Closures/Lambdas)

```ebnf
DoExpr = 'do' DoParams? DoBody

DoParams = '(' Param (',' Param)* ')'
         | Ident

DoBody = Expr
       | Block
```

**Examples:**
```tml
let add: func(I32, I32) -> I32 = do(x, y) x + y

let double: func(I32) -> I32 = do(x) x * 2

let complex: func(I32, I32) -> I32 = do(x, y) {
    let sum: I32 = x + y
    return sum * 2
}

// Usage
items.map(do(x) x * 2)
items.filter(do(x) x > 0)
items.fold(0, do(acc, x) acc + x)
```

### 5.9 Struct Expression

```ebnf
StructExpr = TypePath '{' FieldInit (',' FieldInit)* ','? '}'
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

### 5.10 Array Expression

```ebnf
ArrayExpr = '[' (Expr (',' Expr)* ','?)? ']'
          | '[' Expr ';' Expr ']'
```

**Examples:**
```tml
// Array literals (dynamic lists)
let numbers: List[I32] = [1, 2, 3, 4, 5]
let names: List[String] = ["alice", "bob", "charlie"]
let empty: List[I32] = []

// Repeat syntax
let zeros: [I32; 100] = [0; 100]  // 100 zeros
let ones: [I32; 10] = [1; 10]    // 10 ones

// Indexing
let first: I32 = numbers[0]
let second: I32 = numbers[1]

// Method syntax
println(numbers.len())     // 5
numbers.push(6)
let last: Maybe[I32] = numbers.pop()
numbers.set(0, 100)
let val: Maybe[I32] = numbers.get(0)
numbers.clear()
```

### 5.11 Method Call Expression

```ebnf
MethodCall = Expr '.' Ident '(' Args? ')'
Args       = Expr (',' Expr)*
```

**Examples:**
```tml
// Collection methods
let arr: List[I32] = [1, 2, 3]
arr.push(4)
arr.pop()
arr.len()
arr.get(0)
arr.set(0, 10)
arr.clear()
arr.is_empty()
arr.capacity()

// String methods (future)
str.len()
str.to_upper()
str.contains("x")
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
     | MaybeType

PrimitiveType = 'Bool' | 'I8' | 'I16' | 'I32' | 'I64' | 'I128'
              | 'U8' | 'U16' | 'U32' | 'U64' | 'U128'
              | 'F32' | 'F64' | 'String' | 'Char'

NamedType    = TypePath
GenericType  = TypePath '[' Type (',' Type)* ']'
RefType      = 'ref' Type | 'mut' 'ref' Type
FuncType     = 'func' '(' (Type (',' Type)*)? ')' '->' Type
ArrayType    = '[' Type ';' Expr ']'
TupleType    = '(' Type (',' Type)+ ')'
MaybeType    = Type '?'

TypePath = Ident ('::' Ident)*
```

**Examples:**
```tml
// Primitives
Bool, I32, U64, F64, String, Char

// Named and Generic
Point
List[I32]
Map[String, Value]
Outcome[Data, Error]

// References
ref String
mut ref List[T]

// Functions
func(I32, I32) -> I32
func(String) -> Bool

// Arrays
[I32; 10]
[Byte; 256]

// Tuples
(I32, String)
(F64, F64, F64)

// Maybe (sugar for Maybe[T])
String?
User?
```

## 7. Directives

```ebnf
Directive     = '@' DirectiveName DirectiveArgs?
DirectiveName = Ident
DirectiveArgs = '(' (DirectiveArg (',' DirectiveArg)*)? ')'
DirectiveArg  = Ident (':' Value)?
```

**Examples:**
```tml
@test
@when(os: linux)
@auto(debug, duplicate, equal)
@deprecated("Use new_func instead")
@hint(inline: always)
@lowlevel
```

## 8. LL(1) Verification

### 8.1 First Token Determines Production

| Token | Production |
|-------|------------|
| `module` | ModuleDecl |
| `import` | Import |
| `public` | Visibility + Item or Import |
| `private` | Visibility + Item |
| `func` | Function |
| `type` | TypeDecl (struct or enum) |
| `behavior` | BehaviorDecl |
| `extend` | ExtendDecl |
| `const` | ConstDecl |
| `let` | LetStmt |
| `var` | VarStmt |
| `if` | IfExpr |
| `when` | WhenExpr |
| `loop` | LoopExpr |
| `for` | ForExpr (loop i in N) |
| `catch` | CatchExpr |
| `return` | ReturnStmt |
| `break` | BreakStmt |
| `continue` | ContinueStmt |
| `do` | DoExpr |
| `this` | ThisExpr |
| `This` | ThisType or Constructor |
| `ref` | RefExpr or RefType |
| `{` | BlockExpr |
| `(` | GroupExpr or TupleExpr |
| `[` | ArrayExpr (dynamic list) |
| `@` | Directive |
| Ident | VarRef or FuncCall or TypeRef |
| Ident `::` Ident | PathExpr (enum variant) |
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
module math.geometry

import std.math.{sqrt, PI}

public type Point {
    x: F64,
    y: F64,
}

public type Circle {
    center: Point,
    radius: F64,
}

extend Point {
    public func new(x: F64, y: F64) -> This {
        return This { x, y }
    }

    public func origin() -> This {
        return This.new(0.0, 0.0)
    }

    public func distance(this, other: Point) -> F64 {
        let dx: F64 = this.x - other.x
        let dy: F64 = this.y - other.y
        return sqrt(dx**2 + dy**2)
    }
}

extend Circle {
    public func new(center: Point, radius: F64) -> This
    pre: radius >= 0.0
    {
        return This { center, radius }
    }

    public func area(this) -> F64 {
        return PI * this.radius**2
    }

    public func contains(this, point: Point) -> Bool {
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
