# RFC-0002: Surface Syntax

## Status
Draft

## Summary

This RFC defines the human-readable surface syntax of TML and the desugaring rules that transform it into the core IR (RFC-0001). The surface syntax prioritizes readability and LLM comprehension.

## Motivation

LLMs process code as text. The surface syntax should:
1. Be visually unambiguous (no `<>` vs comparison confusion)
2. Use words over symbols where clarity improves
3. Support incremental parsing (LL(1) friendly)
4. Desugar deterministically to core IR

---

## 1. Lexical Structure

### 1.1 Keywords (32 total)

```
and         as          async       await       behavior
break       const       continue    do          else
enum        false       for         func        if
impl        in          let         loop        mod
mut         not         or          pub         ref
return      then        this        through     to
true        type        when        with
```

### 1.2 Additional Keywords

```
decorator   quote
```

### 1.3 Reserved (future use)

```
class       state       macro       yield       effect
where       dyn         box         move        try
catch       throw       virtual     override    super
```

### 1.4 Operators

```
Arithmetic:   +  -  *  /  %  **
Comparison:   == != < > <= >=
Logical:      and or not
Bitwise:      & | ^ ~ << >>
Assignment:   = += -= *= /= %= &= |= ^= <<= >>=
Access:       .  ::  ->
Range:        to  through
Other:        !  ?  @  #  ..  ...
```

### 1.4 Delimiters

```
Grouping:     ( ) [ ] { }
Separators:   , : ; |
```

### 1.5 Literals

```
// Integers
42          // I32 (default)
42_u64      // U64 explicit
0xFF        // Hex
0o77        // Octal
0b1010      // Binary
1_000_000   // Underscores allowed

// Floats
3.14        // F64 (default)
3.14_f32    // F32 explicit
1e10        // Scientific
1.5e-3      // Negative exponent

// Strings
"hello"             // Basic string
"line1\nline2"      // Escape sequences
"""
multiline
string
"""                 // Raw multiline

// Characters
'a'
'\n'
'\u{1F600}'         // Unicode

// Boolean
true
false
```

---

## 2. Grammar (PEG)

### 2.1 Top-Level

```peg
Module      <- Item*
Item        <- Visibility? (FuncDef / TypeDef / ConstDef / BehaviorDef / ImplBlock / ModDef)

Visibility  <- 'pub' ('(' PubScope ')')?
PubScope    <- 'crate' / 'super' / 'self' / Path

FuncDef     <- Decorator* 'func' Ident GenericParams? '(' Params? ')' ('->' Type)? Effects? Block
TypeDef     <- 'type' Ident GenericParams? '=' TypeBody
ConstDef    <- 'const' Ident ':' Type '=' Expr
BehaviorDef <- 'behavior' Ident GenericParams? '{' BehaviorItem* '}'
ImplBlock   <- 'impl' GenericParams? Type ('for' Type)? '{' ImplItem* '}'
ModDef      <- 'mod' Ident ('{' Item* '}' / ';')
```

### 2.2 Types

```peg
Type        <- FuncType / SimpleType

FuncType    <- 'func' '(' TypeList? ')' '->' Type Effects?
SimpleType  <- ReferenceType / BaseType
ReferenceType <- ('mut'? 'ref') Type
BaseType    <- TypePath GenericArgs? / TupleType / ArrayType
TupleType   <- '(' TypeList? ')'
ArrayType   <- '[' Type ';' Expr ']'
TypeList    <- Type (',' Type)*
GenericArgs <- '[' TypeList ']'
TypePath    <- Ident ('::' Ident)*
```

### 2.3 Expressions

```peg
Expr        <- Assignment
Assignment  <- LogicalOr (AssignOp Assignment)?
LogicalOr   <- LogicalAnd ('or' LogicalAnd)*
LogicalAnd  <- Comparison ('and' Comparison)*
Comparison  <- BitwiseOr (CompareOp BitwiseOr)?
BitwiseOr   <- BitwiseXor ('|' BitwiseXor)*
BitwiseXor  <- BitwiseAnd ('^' BitwiseAnd)*
BitwiseAnd  <- Shift ('&' Shift)*
Shift       <- Additive (ShiftOp Additive)*
Additive    <- Multiplicative (AddOp Multiplicative)*
Multiplicative <- Power (MulOp Power)*
Power       <- Unary ('**' Power)?
Unary       <- UnaryOp* Postfix
Postfix     <- Primary PostfixOp*

Primary     <- Literal / Ident / 'this' / ParenExpr / BlockExpr
             / IfExpr / WhenExpr / LoopExpr / ReturnExpr / BreakExpr
             / Closure / StructInit / ArrayInit

PostfixOp   <- Call / Index / FieldAccess / MethodCall / Propagate / Await
Call        <- '(' ArgList? ')'
Index       <- '[' Expr ']'
FieldAccess <- '.' Ident
MethodCall  <- '.' Ident GenericArgs? '(' ArgList? ')'
Propagate   <- '!'
Await       <- '.await'
```

### 2.4 Statements

```peg
Block       <- '{' Statement* Expr? '}'
Statement   <- LetStmt / ExprStmt / Item

LetStmt     <- 'let' 'mut'? Pattern (':' Type)? '=' Expr ';'?
ExprStmt    <- Expr ';'
```

### 2.5 Patterns

```peg
Pattern     <- OrPattern
OrPattern   <- AndPattern ('|' AndPattern)*
AndPattern  <- PrimaryPattern ('if' Expr)?
PrimaryPattern <- LiteralPattern / IdentPattern / WildcardPattern
               / TuplePattern / StructPattern / VariantPattern
               / RangePattern

IdentPattern <- 'mut'? Ident ('@' Pattern)?
WildcardPattern <- '_'
TuplePattern <- '(' PatternList? ')'
StructPattern <- TypePath '{' FieldPatterns? '}'
VariantPattern <- TypePath ('(' PatternList ')' / '{' FieldPatterns '}')?
RangePattern <- Literal ('to' / 'through') Literal
```

---

## 3. Desugaring Rules

Surface syntax transforms to core IR via these rules.

### 3.1 Method Calls

```tml
// Surface
obj.method(arg1, arg2)

// Desugars to
Type::method(obj, arg1, arg2)
```

**IR:**
```json
{
  "kind": "call",
  "func": { "kind": "path", "segments": ["Type", "method"] },
  "args": [
    { "kind": "ident", "name": "obj" },
    { "kind": "ident", "name": "arg1" },
    { "kind": "ident", "name": "arg2" }
  ]
}
```

### 3.2 Field Access Sugar

```tml
// Surface
point.x

// Desugars to
Point::x(point)  // if getter defined

// Or direct field access if public
```

### 3.3 Range Expressions

```tml
// Surface
1 to 10        // exclusive end
1 through 10   // inclusive end

// Desugars to
Range::new(1, 10)           // to
RangeInclusive::new(1, 10)  // through
```

### 3.4 Loop Expressions

```tml
// Surface: for-in loop
for item in collection {
    process(item)
}

// Desugars to
{
    let mut __iter = collection.into_iter()
    loop {
        when __iter.next() {
            Just(item) -> process(item),
            Nothing -> break,
        }
    }
}
```

```tml
// Surface: while loop
loop condition {
    body
}

// Desugars to
loop {
    if not condition then break
    body
}
```

### 3.5 String Interpolation

```tml
// Surface
"Hello, {name}! You have {count} messages."

// Desugars to
String::concat([
    "Hello, ",
    name.to_string(),
    "! You have ",
    count.to_string(),
    " messages."
])
```

### 3.6 Decorator Application

```tml
// Surface
@test
@timeout(5000)
func my_test() { ... }

// Desugars to metadata in IR
{
  "kind": "func_def",
  "name": "my_test",
  "decorators": [
    { "name": "test", "args": [] },
    { "name": "timeout", "args": [{ "kind": "literal", "value": 5000 }] }
  ],
  "body": { ... }
}
```

---

## 4. Custom Decorators

TML supports user-defined decorators as first-class language constructs.

### 4.1 Decorator Definition

Decorators are defined using the `decorator` keyword:

```tml
decorator log_calls {
    // Target is the decorated item (func, type, field, etc.)
    func apply(target: DecoratorTarget) -> DecoratorResult {
        // Wrap function with logging
        when target {
            Func(f) -> {
                let original = f.body
                f.body = quote {
                    println("Entering: " + ${f.name})
                    let result = ${original}
                    println("Exiting: " + ${f.name})
                    result
                }
                DecoratorResult.Modified(f)
            },
            _ -> DecoratorResult.Error("@log_calls only applies to functions"),
        }
    }
}
```

### 4.2 Parameterized Decorators

Decorators can accept parameters:

```tml
decorator retry(max_attempts: U32 = 3, delay_ms: U32 = 1000) {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Func(f) -> {
                let original = f.body
                f.body = quote {
                    let mut attempts = 0
                    loop attempts < ${max_attempts} {
                        when ${original} {
                            Ok(v) -> return Ok(v),
                            Err(e) -> {
                                attempts += 1
                                if attempts < ${max_attempts} then {
                                    sleep(Duration.millis(${delay_ms}))
                                } else {
                                    return Err(e)
                                }
                            }
                        }
                    }
                }
                DecoratorResult.Modified(f)
            },
            _ -> DecoratorResult.Error("@retry only applies to functions"),
        }
    }
}

// Usage
@retry(max_attempts: 5, delay_ms: 500)
func fetch_data(url: String) -> Outcome[Data, Error] { ... }
```

### 4.3 Decorator Targets

Decorators can apply to different targets:

```tml
type DecoratorTarget =
    | Func(FuncDef)           // Functions
    | Type(TypeDef)           // Type definitions
    | Field(FieldDef)         // Struct fields
    | Variant(VariantDef)     // Enum variants
    | Param(ParamDef)         // Function parameters
    | Impl(ImplBlock)         // Impl blocks
    | Mod(ModDef)             // Modules

// Decorator that only applies to fields
decorator validate {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Field(f) -> { ... },
            Param(p) -> { ... },
            _ -> DecoratorResult.Error("@validate applies to fields and params"),
        }
    }
}
```

### 4.4 Compile-time vs Runtime Decorators

Decorators specify when they execute:

```tml
// Compile-time decorator (default) - transforms AST
@compile_time
decorator deprecated(message: String = "This item is deprecated") {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        // Emit compiler warning
        compiler.warn(target.span, message)
        DecoratorResult.Unchanged
    }
}

// Runtime decorator - wraps at runtime
@runtime
decorator memoize {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Func(f) -> {
                f.body = quote {
                    static cache: Map[Args, Return] = Map.new()
                    let args = (${f.params.as_tuple()})
                    if let Just(cached) = cache.get(ref args) then {
                        return cached.duplicate()
                    }
                    let result = ${f.original_body}
                    cache.insert(args, result.duplicate())
                    result
                }
                DecoratorResult.Modified(f)
            },
            _ -> DecoratorResult.Error("@memoize only applies to functions"),
        }
    }
}
```

### 4.5 Decorator Composition

Multiple decorators compose top-to-bottom:

```tml
@log_calls          // Applied last (outermost)
@memoize            // Applied second
@retry(3)           // Applied first (innermost)
func expensive_fetch(id: U64) -> Outcome[Data, Error] { ... }

// Execution order: log_calls wraps memoize wraps retry wraps original
```

### 4.6 Built-in Decorators

TML provides these built-in decorators:

| Decorator | Target | Description |
|-----------|--------|-------------|
| `@test` | Func | Mark as test function |
| `@bench` | Func | Mark as benchmark |
| `@inline` | Func | Hint to inline |
| `@cold` | Func | Hint rarely called |
| `@deprecated(msg)` | Any | Emit deprecation warning |
| `@must_use` | Func/Type | Warn if result unused |
| `@derive(...)` | Type | Auto-implement behaviors |
| `@when(...)` | Any | Conditional compilation |
| `@pre(...)` | Func | Precondition (RFC-0003) |
| `@post(...)` | Func | Postcondition (RFC-0003) |
| `@invariant(...)` | Type | Type invariant (RFC-0003) |

### 4.7 Derive Decorator

`@derive` auto-generates behavior implementations:

```tml
@derive(Eq, Hash, Debug, Duplicate)
type User = {
    id: U64,
    name: String,
    email: String,
}

// Generates:
impl Eq for User { ... }
impl Hash for User { ... }
impl Debug for User { ... }
impl Duplicate for User { ... }
```

Custom derive:

```tml
decorator derive_serialize {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Type(t) -> {
                let impl_block = generate_serialize_impl(t)
                DecoratorResult.AddItem(impl_block)
            },
            _ -> DecoratorResult.Error("derive only applies to types"),
        }
    }
}

@derive_serialize
type Config = { ... }
```

### 4.8 Quote and Splice

Decorators use `quote` for code generation:

```tml
decorator timer {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Func(f) -> {
                let name = f.name
                f.body = quote {
                    let start = Instant.now()
                    let result = ${f.original_body}  // Splice original
                    let elapsed = start.elapsed()
                    println("${name} took " + elapsed.as_millis().to_string() + "ms")
                    result
                }
                DecoratorResult.Modified(f)
            },
            _ -> DecoratorResult.Error("@timer only applies to functions"),
        }
    }
}
```

Quote syntax:
- `quote { ... }` - Create code template
- `${expr}` - Splice expression into template
- `$name` - Splice identifier

### 4.9 IR Representation

Custom decorators are represented in IR:

```json
{
  "kind": "decorator_def",
  "name": "retry",
  "params": [
    { "name": "max_attempts", "type": "U32", "default": { "kind": "literal", "value": 3 } },
    { "name": "delay_ms", "type": "U32", "default": { "kind": "literal", "value": 1000 } }
  ],
  "execution": "compile_time",
  "apply_func": { ... }
}
```

Decorated items include resolved decorator info:

```json
{
  "kind": "func_def",
  "name": "fetch_data",
  "decorators": [
    {
      "name": "retry",
      "resolved": "my_module::retry",
      "args": {
        "max_attempts": { "kind": "literal", "value": 5 },
        "delay_ms": { "kind": "literal", "value": 500 }
      }
    }
  ],
  "body": { ... }
}
```

---

## 5. Desugaring Rules (continued)

### 5.1 Conditional Expressions

```tml
// Surface: if-then-else
if condition then value1 else value2

// Already core, no desugaring needed
```

```tml
// Surface: if without else (statement)
if condition then { side_effect() }

// Desugars to
if condition then { side_effect() } else { () }
```

### 3.8 Closure Syntax

```tml
// Surface: do closure
do(x, y) x + y

// Desugars to
{ |x, y| x + y }  // Anonymous function in IR

// Surface: with explicit types
do(x: I32, y: I32) -> I32 { x + y }
```

**IR:**
```json
{
  "kind": "closure",
  "params": [
    { "name": "x", "type": "I32" },
    { "name": "y", "type": "I32" }
  ],
  "return_type": "I32",
  "body": {
    "kind": "binary_op",
    "op": "+",
    "left": { "kind": "ident", "name": "x" },
    "right": { "kind": "ident", "name": "y" }
  }
}
```

### 3.9 Pattern Matching

```tml
// Surface: when expression
when value {
    Ok(x) -> process(x),
    Err(e) -> handle(e),
}

// Desugars to IR match node
```

**IR:**
```json
{
  "kind": "match",
  "scrutinee": { "kind": "ident", "name": "value" },
  "arms": [
    {
      "pattern": { "kind": "variant", "name": "Ok", "bindings": ["x"] },
      "body": { "kind": "call", "func": "process", "args": [{ "kind": "ident", "name": "x" }] }
    },
    {
      "pattern": { "kind": "variant", "name": "Err", "bindings": ["e"] },
      "body": { "kind": "call", "func": "handle", "args": [{ "kind": "ident", "name": "e" }] }
    }
  ]
}
```

---

## 4. Examples

### 4.1 Complete Function

**Surface:**
```tml
@inline
pub func find_max[T: Ord](items: ref List[T]) -> Maybe[ref T] {
    if items.is_empty() then return Nothing

    let mut max = ref items[0]
    for item in items {
        if item > max then max = ref item
    }
    Just(max)
}
```

**After Desugaring (pseudo-IR):**
```
func find_max[T: Ord](items: ref List[T]) -> Maybe[ref T]
  decorators: [inline]
  effects: []
  body:
    if List::is_empty(items) then return Nothing
    let mut max = ref items[0]
    let mut __iter = List::iter(items)
    loop {
      match Iterator::next(__iter) {
        Just(item) -> {
          if Ord::gt(item, max) then max = ref item
        }
        Nothing -> break
      }
    }
    Just(max)
```

### 4.2 Type Definition

**Surface:**
```tml
type Result[T] = {
    value: Maybe[T],
    errors: List[String],
    warnings: List[String],
}
```

**IR:**
```json
{
  "kind": "type_def",
  "name": "Result",
  "params": ["T"],
  "body": {
    "kind": "struct",
    "fields": [
      { "name": "value", "type": { "name": "Maybe", "args": [{ "ref": "T" }] } },
      { "name": "errors", "type": { "name": "List", "args": ["String"] } },
      { "name": "warnings", "type": { "name": "List", "args": ["String"] } }
    ]
  }
}
```

---

## 5. Parser Implementations

### 5.1 Compiler Parser (PEG)

Located at `grammar/tml.peg`. Used by:
- Bootstrap compiler (C++)
- Self-hosted compiler (TML)
- Any tool needing semantic parsing

### 5.2 Editor Parser (Tree-sitter)

Located at `grammar/tree-sitter-tml/`. Used by:
- VS Code extension
- Neovim/Vim
- Any editor needing syntax highlighting

Tree-sitter grammar prioritizes:
- Error recovery (partial parse on invalid input)
- Incremental re-parsing
- Fast highlighting

The Tree-sitter grammar MAY accept a superset of valid TML for better error messages.

---

## 6. Compatibility

- **RFC-0001**: All surface syntax desugars to core IR
- **RFC-0003**: Contract decorators (`@pre`, `@post`) handled in desugaring
- **RFC-0004**: `!` operator desugars to propagate node
- **RFC-0006**: OO sugar (`class`, `state`) defined in separate RFC

---

## 7. Alternatives Rejected

### 7.1 Significant Whitespace (Python-style)

```python
# Rejected
func example()
    if condition
        action()
```

Problems:
- Copy/paste issues
- Mixed indentation bugs
- Harder for LLMs to track scope

### 7.2 Semicolon Inference (Go-style)

Automatic semicolon insertion was rejected:
- Ambiguous cases confuse LLMs
- Explicit is clearer
- TML allows trailing expression without semicolon

### 7.3 XML-style Generic Syntax

```
// Rejected
List<String>

// Chosen
List[String]
```

`<>` conflicts with comparison operators and requires context for parsing.

### 7.4 `|x|` Closure Syntax (Rust-style)

```rust
// Rejected
|x, y| x + y

// Chosen
do(x, y) x + y
```

`|` conflicts with bitwise OR and pattern matching. `do` is unambiguous.

---

## 8. References

- [PEG.js](https://peggyjs.org/) - PEG parser generator
- [Tree-sitter](https://tree-sitter.github.io/) - Parser for editors
- [Rust Reference: Expressions](https://doc.rust-lang.org/reference/expressions.html)
