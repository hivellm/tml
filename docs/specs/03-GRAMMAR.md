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
     | BehaviorDecl
     | ExtendDecl
     | ConstDecl
     | ClassDecl
     | InterfaceDecl
     | NamespaceDecl

Visibility = 'pub' | 'private' | 'protected'
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
Effect     = Ident ('::' Ident)*
```

**Examples:**
```tml
func add(a: I32, b: I32) -> I32 {
    return a + b
}

pub func first[T](list: List[T]) -> Maybe[T] {
    return list.get(0)
}

func sqrt(x: F64) -> F64
pre: x >= 0.0
post(r): r >= 0.0
{
    return x.sqrt_impl()
}

func read_file(path: String) -> Outcome[String, IoError]
effects: [io::file::read]
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

> **Implementation Note (v0.4.0):** Generic types are fully implemented via
> monomorphization. `Pair[I32]` becomes `Pair__I32` in LLVM IR. Both generic
> structs and enums with pattern matching are supported.

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
pub const VERSION: String = "1.0.0"
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
// Simple class
class Point {
    x: F64
    y: F64

    func new(x: F64, y: F64) -> Point {
        return Point { x: x, y: y }
    }
}

// Abstract class with virtual methods
abstract class Animal {
    protected name: Str

    abstract func speak(this) -> Str

    virtual func move(this) {
        println("Moving")
    }
}

// Class with inheritance
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

// Sealed class (cannot be extended)
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
// Simple interface
interface Speakable {
    func speak(this) -> Str
}

// Generic interface
interface Comparable[T] {
    func compare(this, other: T) -> I32
}

// Interface extending another
interface Orderable extends Comparable[This] {
    func less_than(this, other: This) -> Bool {
        return this.compare(other) < 0
    }
}

// Multiple methods
interface Drawable {
    func draw(this)
    func get_bounds(this) -> (F64, F64, F64, F64)
}
```

### 3.8 Namespaces (OOP)

```ebnf
NamespaceDecl = 'namespace' NamespacePath '{' Item* '}'

NamespacePath = Ident ('::' Ident)*
```

**Examples:**
```tml
namespace graphics::shapes {
    class Circle {
        radius: F64
    }

    class Rectangle {
        width: F64
        height: F64
    }

    interface Drawable {
        func draw(this)
    }
}

// Nested namespaces
namespace app {
    namespace models {
        class User { }
    }

    namespace controllers {
        class UserController { }
    }
}
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
Expr = AssignExpr

AssignExpr  = TernaryExpr (AssignOp TernaryExpr)?
AssignOp    = '=' | '+=' | '-=' | '*=' | '/=' | '%='
            | '&=' | '|=' | '^=' | '<<=' | '>>='

TernaryExpr = OrExpr ('?' Expr ':' Expr)?   // Ternary conditional

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
            | WhileExpr
            | ForExpr
            | CatchExpr
            | DoExpr
            | StructExpr
            | ArrayExpr

GroupExpr = '(' Expr ')'

BlockExpr = Block
Block     = '{' Statement* Expr? '}'

Literal = IntLit | FloatLit | StringLit | InterpolatedString | BoolLit | CharLit | NullLit
BoolLit = 'true' | 'false'
NullLit = 'null'

// Interpolated strings: "Hello {name}!" where {expr} are embedded expressions
InterpolatedString = InterpStart (Expr InterpMiddle)* Expr? InterpEnd
InterpStart  = '"' StringChars? '{'
InterpMiddle = '}' StringChars? '{'
InterpEnd    = '}' StringChars? '"' | '"'  // no expressions case is just StringLit
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
WhenArm  = Pattern '=>' Expr ','?

Pattern = RangePattern
        | LiteralPattern
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

**Pattern Types:**

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
- `to` - exclusive end: `0 to 10` matches 0-9
- `through` - inclusive end: `0 through 9` matches 0-9

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

// Struct patterns with destructuring
when point {
    Point { x: 0, y: 0 } => "origin",
    Point { x, y } => "at " + x.to_string(),
}

// Tuple patterns
when pair {
    (a, b) => a + b,
}

// Array patterns
when arr {
    [first, _, _] => first,
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

### 5.4.1 Ternary Conditional Operator

The ternary operator provides a concise syntax for inline conditional expressions.

```ebnf
TernaryExpr = OrExpr ('?' Expr ':' Expr)?
```

**Characteristics:**
- **Right-associative**: Groups from right to left
- **Precedence**: Between assignment and logical OR
- **Type checking**: Both branches must return the same type
- **Nestable**: Can be nested for multiple conditions

**Examples:**
```tml
// Basic usage
let max: I32 = x > y ? x : y
let min: I32 = x < y ? x : y

// With expressions
let result: I32 = score > 60 ? score * 2 : score + 10

// Nested (find max of three)
let max3: I32 = a > b ? (a > c ? a : c) : (b > c ? b : c)

// In assignments
let status: String = is_valid ? "PASS" : "FAIL"

// With method calls
let value: I32 = list.is_empty() ? 0 : list.get(0)
```

**Comparison with if-then-else:**
```tml
// Ternary (concise)
let x: I32 = condition ? 10 : 20

// If-then-else (verbose)
let x: I32 = if condition then 10 else 20
```

### 5.5 Loop Expressions

#### 5.5.1 Conditional Loop

```ebnf
LoopExpr = 'loop' '(' LoopHeader ')' Block

LoopHeader = LoopVarDecl           // loop (var i: I32 < N)
           | Expr                  // loop (condition)

LoopVarDecl = 'var' Ident ':' Type '<' Expr
```

The `loop (var name: Type < limit)` syntax:
- Declares a new variable in loop scope
- Initializes the variable to `0`
- Continues while `name < limit`
- Variable must be manually incremented

**Examples:**
```tml
// Conditional loop
loop (count < 10) {
    print(count)
    count = count + 1
}

// Loop with variable declaration (auto-initialized to 0)
loop (var i: I32 < 5) {
    println(i)
    i = i + 1
}

// Infinite loop
loop (true) {
    if done then break
    work()
}

// With break/continue
loop (running) {
    if should_exit then break
    if should_skip then continue
    process()
}
```

#### 5.5.2 While Loop (Alias)

```ebnf
WhileExpr = 'while' Expr Block
```

The `while` keyword is an alias for `loop (condition)`.

**Examples:**
```tml
// Traditional while loop (alias for loop (condition))
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

// Collection iteration (List, HashMap, Buffer, Vec)
for item in list {
    process(item)
}

for value in hashmap {
    print(value)
}

// With pattern destructuring
for Point { x, y } in points {
    draw(x, y)
}
```

**Note:** The `for` loop supports:
- Range expressions: `0 to 10`, `1 through 5`
- Collection types: `List`, `HashMap`, `Buffer`, `Vec`
- Pattern matching in the loop variable

### 5.6 Error Propagation with `!`

```ebnf
PropagateExpr = Expr '!'
ElseExpr      = Expr '!' 'else' (Expr | 'do' '(' Ident ')' Expr)
```

Propagates errors automatically or recovers with else.

**Examples:**
```tml
let file: Outcome[File, Error] = File.open(path)!
let data: Outcome[String, Error] = file.read()!

// With fallback
let port: Outcome[String, Error] = env.get("PORT")!.parse[U16]()! else 8080

// With error binding
let data: Outcome[Data, Error] = fetch(url)! else do(err) {
    log.warn(err.to_string())
    Data.default()
}
```

### 5.7 Catch Expression

```ebnf
CatchExpr = 'catch' Block 'else' ('do' '(' Ident ')')? Block
```

**Examples:**
```tml
catch {
    let local: Data = load_local()!
    let remote: Outcome[Data, Error] = fetch_remote()!
    return Ok(merge(local, remote)!)
} else do(err) {
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
| `mod` | ModDecl |
| `use` | Use |
| `pub` | Visibility + Item or Use |
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

extend Point {
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

extend Circle {
    pub func new(center: Point, radius: F64) -> This
    pre: radius >= 0.0
    {
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
