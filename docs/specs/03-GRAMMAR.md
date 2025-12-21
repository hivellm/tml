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
     | TraitDecl
     | ExtendDecl
     | ConstDecl

Visibility = 'public' | 'private'
```

### 3.1 Functions

```ebnf
Function = Annotation* Visibility? 'func' Ident StableId?
           GenericParams? '(' Params? ')' ('->' Type)?
           Contract? EffectDecl? Block

GenericParams = '[' GenericParam (',' GenericParam)* ']'
GenericParam  = Ident (':' TypeBound)?
TypeBound     = Type ('+' Type)*

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

public func first[T](list: List[T]) -> Option[T] {
    return list.get(0)
}

func sqrt(x: F64) -> F64
pre: x >= 0.0
post(r): r >= 0.0
{
    return x.sqrt_impl()
}

func read_file(path: String) -> Result[String, IoError]
effects: [io.file.read]
{
    // ...
}
```

### 3.2 Types

```ebnf
TypeDecl = Annotation* Visibility? 'type' Ident StableId?
           GenericParams? TypeBody

TypeBody = StructBody | EnumBody | AliasBody

StructBody = '{' (Field (',' Field)* ','?)? '}'
Field      = Ident ':' Type

EnumBody = '=' Variant ('|' Variant)*
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

// Enum
type Color = Red | Green | Blue | Rgb(U8, U8, U8)

type Result[T, E] = Ok(T) | Err(E)

type JsonValue =
    | Null
    | Bool(Bool)
    | Number(F64)
    | String(String)
    | Array(List[JsonValue])
    | Object(Map[String, JsonValue])

// Alias
type UserId = U64
type Handler = func(Request) -> Response
```

### 3.3 Traits

```ebnf
TraitDecl = Annotation* Visibility? 'trait' Ident GenericParams?
            (':' TypeBound)? '{' TraitItem* '}'

TraitItem = TraitFunc | TraitType

TraitFunc = 'func' Ident GenericParams? '(' Params? ')' ('->' Type)?
            (Block | ';')

TraitType = 'type' Ident (':' TypeBound)? ('=' Type)? ';'
```

**Examples:**
```tml
trait Eq {
    func eq(this, other: This) -> Bool;
}

trait Ord: Eq {
    func cmp(this, other: This) -> Ordering;

    // Default implementation
    func lt(this, other: This) -> Bool {
        return this.cmp(other) == Less
    }
}

trait Iterator {
    type Item;
    func next(this) -> Option[This.Item];
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

extend Point with Eq {
    func eq(this, other: This) -> Bool {
        return this.x == other.x and this.y == other.y
    }
}

extend List[T] with Iterator {
    type Item = T;

    func next(this) -> Option[T] {
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
const MAX_SIZE = 1024
public const VERSION = "1.0.0"
```

## 4. Statements

```ebnf
Statement = LetStmt
          | VarStmt
          | ExprStmt
          | ReturnStmt
          | BreakStmt
          | ContinueStmt

LetStmt = 'let' Pattern (':' Type)? '=' Expr
VarStmt = 'var' Ident (':' Type)? '=' Expr

ExprStmt = Expr

ReturnStmt   = 'return' Expr?
BreakStmt    = 'break'
ContinueStmt = 'continue'
```

**Examples:**
```tml
let x = 42
let y: I64 = 100
let Point { x, y } = get_point()

var count = 0
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

UnaryExpr   = ('-' | '~' | '&' | '*') UnaryExpr
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
IfExpr = 'if' Expr 'then' Expr ('else' Expr)?
```

**Note:** `then` is mandatory to avoid ambiguity.

**Examples:**
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
    Some(x) -> if x > 0 then x else 0,
    None -> -1,
}
```

### 5.5 Loop Expression

```ebnf
LoopExpr = 'loop' LoopKind Block

LoopKind = 'in' Expr           // for-each
         | 'while' Expr        // while
         | ε                   // infinite
```

**Examples:**
```tml
// For-each
loop item in items {
    process(item)
}

loop i in 0..10 {
    print(i)
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
let file = File.open(path)!
let data = file.read()!

// With fallback
let port = env.get("PORT")!.parse[U16]()! else 8080

// With error binding
let data = fetch(url)! else |err| {
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
    let local = load_local()!
    let remote = fetch_remote()!
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
let add = do(x, y) x + y

let double = do(x) x * 2

let complex = do(x, y) {
    let sum = x + y
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
let x = 1.0
let y = 2.0
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
[1, 2, 3, 4, 5]
["a", "b", "c"]
[0; 100]  // 100 zeros
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
     | OptionType

PrimitiveType = 'Bool' | 'I8' | 'I16' | 'I32' | 'I64' | 'I128'
              | 'U8' | 'U16' | 'U32' | 'U64' | 'U128'
              | 'F32' | 'F64' | 'String' | 'Char'

NamedType    = TypePath
GenericType  = TypePath '[' Type (',' Type)* ']'
RefType      = '&' 'mut'? Type
FuncType     = 'func' '(' (Type (',' Type)*)? ')' '->' Type
ArrayType    = '[' Type ';' Expr ']'
TupleType    = '(' Type (',' Type)+ ')'
OptionType   = Type '?'

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
Result[Data, Error]

// References
&String
&mut List[T]

// Functions
func(I32, I32) -> I32
func(String) -> Bool

// Arrays
[I32; 10]
[Byte; 256]

// Tuples
(I32, String)
(F64, F64, F64)

// Optional (sugar for Option[T])
String?
User?
```

## 7. LL(1) Verification

### 7.1 First Token Determines Production

| Token | Production |
|-------|------------|
| `module` | ModuleDecl |
| `import` | Import |
| `public` | Visibility + Item or Import |
| `private` | Visibility + Item |
| `func` | Function |
| `type` | TypeDecl |
| `trait` | TraitDecl |
| `extend` | ExtendDecl |
| `const` | ConstDecl |
| `let` | LetStmt |
| `var` | VarStmt |
| `if` | IfExpr |
| `when` | WhenExpr |
| `loop` | LoopExpr |
| `catch` | CatchExpr |
| `return` | ReturnStmt |
| `break` | BreakStmt |
| `continue` | ContinueStmt |
| `do` | DoExpr |
| `this` | ThisExpr |
| `This` | ThisType or Constructor |
| `{` | BlockExpr |
| `(` | GroupExpr or TupleExpr |
| `[` | ArrayExpr |
| Ident | VarRef or FuncCall or TypeRef |
| Literal | LiteralExpr |

### 7.2 No Ambiguities

**Generics vs Comparison:**
```tml
List[T]     // [ starts generic
a < b       // < always comparison
```

**Struct vs Block:**
```tml
Point { x: 1 }  // TypeName followed by { = struct
{ let x = 1 }   // { alone = block
```

**Closure vs OR:**
```tml
do(x) x + 1     // do starts closure
a | b           // | always bitwise OR
```

## 8. Complete Example

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
        let dx = this.x - other.x
        let dy = this.y - other.y
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

#[test]
func test_distance() {
    let p1 = Point.new(0.0, 0.0)
    let p2 = Point.new(3.0, 4.0)
    assert_eq(p1.distance(p2), 5.0)
}
```

---

*Previous: [02-LEXICAL.md](./02-LEXICAL.md)*
*Next: [04-TYPES.md](./04-TYPES.md) — Type System*
