# TML v1.0 — To Machine Language

> A language designed for machines (LLMs), not adapted from human languages.

## What is TML?

**TML (To Machine Language)** is a programming language created specifically to be processed by Large Language Models. Unlike other languages designed for humans and later adapted for tools, TML was built from the ground up with deterministic parsing and code generation as priorities.

## Why Not Use Existing Languages?

| Language | Problem for LLMs |
|----------|------------------|
| **Rust** | `<T>` conflicts with comparison, lifetimes `'a` are noise, `\|x\|` closures ambiguous, `#[...]` cryptic |
| **Go** | No expressive generics, verbose error handling |
| **TypeScript** | `{}` ambiguous (object vs block), `<T>` conflicts with JSX |
| **Python** | Indentation-sensitive breaks patches, optional typing |

## TML Design Philosophy

| Principle | Implementation |
|-----------|----------------|
| **Words over symbols** | `and`, `or`, `not`, `ref`, `to` instead of `&&`, `\|\|`, `!`, `&`, `..` |
| **Explicit over implicit** | `if x then y else z`, mandatory `return` |
| **Self-documenting types** | `Maybe[T]`, `Outcome[T, E]`, `Shared[T]` |
| **Natural language directives** | `@when(os: linux)` instead of `#[cfg(...)]` |
| **No cryptic syntax** | `@auto(debug)` instead of `#[derive(Debug)]` |

## Specification Index

### Core Language

| Document | Description |
|----------|-------------|
| [01-OVERVIEW.md](./01-OVERVIEW.md) | Philosophy and principles |
| [02-LEXICAL.md](./02-LEXICAL.md) | Tokens, keywords, operators |
| [03-GRAMMAR.md](./03-GRAMMAR.md) | Complete EBNF grammar |
| [04-TYPES.md](./04-TYPES.md) | Type system |
| [05-SEMANTICS.md](./05-SEMANTICS.md) | Caps, effects, contracts |
| [06-MEMORY.md](./06-MEMORY.md) | Ownership and borrowing |
| [07-MODULES.md](./07-MODULES.md) | Module system |
| [08-IR.md](./08-IR.md) | Intermediate representation |

### Toolchain

| Document | Description |
|----------|-------------|
| [09-CLI.md](./09-CLI.md) | Commands `tml build`, `tml test`, etc. |
| [10-TESTING.md](./10-TESTING.md) | Native testing framework |
| [11-DEBUG.md](./11-DEBUG.md) | Debug and structured messages |
| [12-ERRORS.md](./12-ERRORS.md) | Error catalog |

### Reference

| Document | Description |
|----------|-------------|
| [13-BUILTINS.md](./13-BUILTINS.md) | Builtin types and functions |
| [14-EXAMPLES.md](./14-EXAMPLES.md) | Complete examples |
| [15-ERROR-HANDLING.md](./15-ERROR-HANDLING.md) | Error handling system |

## Quick Start

```tml
module hello

public func main() {
    print("Hello, TML!")
}
```

```tml
module math

public func add[T: Numeric](a: T, b: T) -> T {
    return a + b
}

public func factorial(n: U64) -> U64 {
    if n <= 1 then return 1
    return n * factorial(n - 1)
}
```

## Syntax in 5 Minutes

### Variables
```tml
let x = 42              // immutable, type inferred
let y: I32 = 100        // immutable, explicit type
var count = 0           // mutable
const PI = 3.14159      // compile-time constant
```

### Functions
```tml
func greet(name: String) -> String {
    return "Hello, " + name
}

// Generics use [] not <>
func first[T](list: List[T]) -> Maybe[T] {
    return list.get(0)
}
```

### Types
```tml
type Point {
    x: F64,
    y: F64,
}

type Color = Red | Green | Blue | Rgb(U8, U8, U8)

extend Point {
    func distance(this, other: Point) -> F64 {
        let dx = this.x - other.x
        let dy = this.y - other.y
        return (dx**2 + dy**2).sqrt()
    }
}
```

### References (Borrowing)
```tml
// Immutable reference
func length(s: ref String) -> U64 {
    return s.len()
}

// Mutable reference
func append(s: mut ref String, suffix: String) {
    s.push(suffix)
}
```

### Control Flow
```tml
// if-then-else (always with then)
if x > 0 then positive() else negative()

// when (pattern matching)
when value {
    Just(x) -> process(x),
    Nothing -> default(),
}

// unified loop
loop item in items {
    process(item)
}

loop i in 0 to 10 {
    print(i)
}

loop while condition {
    do_work()
}
```

### Error Handling
```tml
// ! propagates errors (visible and clear)
let data = read_file("config.tml")!

// else provides inline fallback
let config = parse(data)! else default_config()

// catch for blocks with common handling
catch {
    let file = open(path)!
    let data = file.read()!
    return Success(parse(data)!)
} else |err| {
    log.error(err)
    return Failure(err)
}
```

### Behaviors (Interfaces)
```tml
behavior Printable {
    func to_text(this) -> String
}

extend Point with Printable {
    func to_text(this) -> String {
        return "(" + this.x.to_string() + ", " + this.y.to_string() + ")"
    }
}
```

### Directives
```tml
@when(os: linux)
func linux_only() { ... }

@auto(debug, duplicate, equal)
type Config {
    name: String,
    value: I32,
}

@test
func test_addition() {
    assert(add(2, 2) == 4)
}

@lowlevel
func raw_memory(p: ptr U8) -> U8 {
    return p.read()
}
```

## Design Decisions

### 1. Generics with `[]` not `<>`
```tml
// TML - no ambiguity
let list: List[I32] = List.new()
if a < b then ...

// Other languages - ambiguous
let list: Vec<i32> = Vec::new();
if a < b { ... }  // is < comparison or generic?
```

### 2. Logical Keywords
```tml
// TML - clear words
if a and b or not c then ...

// Other languages - symbols
if a && b || !c { ... }
```

### 3. Closures with `do()`
```tml
// TML - no ambiguity with |
let add = do(x, y) x + y
items.map(do(x) x * 2)

// Other languages - | is also bitwise OR
let add = |x, y| x + y;
```

### 4. References with `ref`
```tml
// TML - clear words
func process(data: ref String) -> ref String
func modify(data: mut ref String)

// Other languages - symbols
fn process(data: &String) -> &String
fn modify(data: &mut String)
```

### 5. Natural Ranges
```tml
// TML - reads like English
loop i in 0 to 10 { ... }      // 0, 1, 2, ... 9
loop i in 0 through 10 { ... } // 0, 1, 2, ... 10

// Other languages - cryptic
for i in 0..10 { ... }
for i in 0..=10 { ... }
```

### 6. Self-Documenting Types
```tml
// TML - descriptive names
Maybe[User]           // Maybe there's a user
Outcome[Data, Error]  // Outcome is success or failure
Shared[Cache]         // Shared reference-counted
Heap[LargeData]       // Allocated on heap

// Other languages - abbreviations
Option<User>          // What option?
Result<Data, Error>   // Result of what?
Rc<Cache>             // Rc = ?
Box<LargeData>        // Box = ?
```

### 7. Directives with `@`
```tml
// TML - universal, readable
@when(os: linux)
@auto(debug, equal)
@test
@lowlevel

// Other languages - cryptic
#[cfg(target_os = "linux")]
#[derive(Debug, Eq)]
#[test]
#[unsafe]
```

## File Extensions

| Extension | Description |
|-----------|-------------|
| `.tml` | TML source code |
| `.tml.ir` | Canonical IR (text) |
| `.tml.lock` | Dependency lock file |

## CLI

```bash
tml new myproject      # create project
tml build              # compile
tml run                # execute
tml test               # run tests
tml check              # check without compiling
tml fmt                # format code
```

## Type Summary

| Category | Types |
|----------|-------|
| **Primitives** | `Bool`, `I8`-`I128`, `U8`-`U128`, `F32`, `F64`, `Char`, `String` |
| **Maybe** | `Maybe[T]` = `Just(T)` \| `Nothing` |
| **Outcome** | `Outcome[T, E]` = `Success(T)` \| `Failure(E)` |
| **Collections** | `List[T]`, `Map[K, V]`, `Set[T]` |
| **Memory** | `Heap[T]`, `Shared[T]`, `Sync[T]` |
| **References** | `ref T`, `mut ref T` |

## Status

| Version | Status | Features |
|---------|--------|----------|
| v0.1 | In development | Core specification |
| v1.0 | Planned | Reference implementation |

## License

MIT License
