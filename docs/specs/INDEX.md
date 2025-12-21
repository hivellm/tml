# TML v1.0 — To Machine Language

> A language designed for machines (LLMs), not adapted from human languages.

## What is TML?

**TML (To Machine Language)** is a programming language created specifically to be processed by Large Language Models. Unlike other languages designed for humans and later adapted for tools, TML was built from the ground up with deterministic parsing and code generation as priorities.

## Why Not Use Rust/Go/TypeScript?

| Language | Problem for LLMs |
|----------|------------------|
| **Rust** | `<T>` conflicts with comparison, lifetimes `'a` are noise, closures `\|x\|` ambiguous |
| **Go** | No expressive generics, verbose and repetitive error handling |
| **TypeScript** | `{}` ambiguous (object vs block), `<T>` conflicts with JSX |
| **Python** | Indentation-sensitive breaks patches, optional typing |

## Specification Index

### Core Language

| Document | Description |
|----------|-------------|
| [01-OVERVIEW.md](./01-OVERVIEW.md) | Philosophy, principles, and comparison with Rust |
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
func first[T](list: List[T]) -> Option[T] {
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

### Control Flow
```tml
// if-then-else (always with then)
if x > 0 then positive() else negative()

// when (pattern matching)
when value {
    Some(x) -> process(x),
    None -> default(),
}

// unified loop
loop item in items {
    process(item)
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
    return Ok(parse(data)!)
} else |err| {
    log.error(err)
    return Err(err)
}
```

## Design Decisions

### 1. Generics with `[]` not `<>`
```tml
// TML - no ambiguity
let list: List[Int] = List.new()
if a < b then ...

// Rust - ambiguous
let list: Vec<i32> = Vec::new();
if a < b { ... }  // is < comparison or generic?
```

### 2. Logical Keywords
```tml
// TML - clear words
if a and b or not c then ...

// Rust - symbols
if a && b || !c { ... }
```

### 3. Closures with `do()`
```tml
// TML - no ambiguity with |
let add = do(x, y) x + y
items.map(do(x) x * 2)

// Rust - | is also bitwise OR
let add = |x, y| x + y;
```

### 4. Unified Loop
```tml
// TML - one keyword
loop i in 0..10 { ... }
loop while running { ... }
loop { ... }

// Rust - three keywords
for i in 0..10 { ... }
while running { ... }
loop { ... }
```

### 5. No Explicit Lifetimes
```tml
// TML - inferred
func longest(a: &String, b: &String) -> &String {
    if a.len() > b.len() then a else b
}

// Rust - verbose
fn longest<'a>(a: &'a str, b: &'a str) -> &'a str {
    if a.len() > b.len() { a } else { b }
}
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

## Status

| Version | Status | Features |
|---------|--------|----------|
| v0.1 | In development | Core specification |
| v1.0 | Planned | Reference implementation |

## License

MIT License
