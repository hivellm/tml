# TML v1.0 — To Machine Language

> **LLM-First Design, Human-Friendly Ergonomics**
>
> A language optimized for LLM code generation with the familiar feel of Rust and C#.

## What is TML?

**TML (To Machine Language)** is a programming language designed from the ground up for **LLM code generation and analysis**, while maintaining **ergonomics that human developers expect** from modern languages like Rust and C#.

### The Dual Focus

| For LLMs | For Humans |
|----------|------------|
| Deterministic LL(1) grammar | Familiar Rust/C# patterns |
| Unique token meanings | Method chaining `.filter().map()` |
| Self-documenting keywords | Explicit types everywhere |
| Stable IDs for patches | Clean generics `List[T]` |
| No ambiguous syntax | Readable `and`/`or`/`not` |

### Inspired By

| Source | What TML Takes |
|--------|----------------|
| **Rust** | Ownership, pattern matching, `ref`/`mut`, zero-cost abstractions, traits (as `behavior`) |
| **C#** | Clean generics `[T]`, method syntax, properties, LINQ-style chains, `async`/`await` |
| **TML Innovation** | `and`/`or`/`not` keywords, `to`/`through` ranges, `Maybe[T]`/`Outcome[T,E]`, `@directives`, stable IDs |

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
| [08-IR.md](./08-IR.md) | High-level IR for semantic analysis |
| [30-MIR.md](./30-MIR.md) | Mid-level IR for optimization (SSA form) |

### Toolchain

| Document | Description |
|----------|-------------|
| [09-CLI.md](./09-CLI.md) | Commands `tml build`, `tml test`, etc. |
| [10-TESTING.md](./10-TESTING.md) | Native testing framework |
| [11-DEBUG.md](./11-DEBUG.md) | Debug and structured messages |
| [12-ERRORS.md](./12-ERRORS.md) | Error catalog |
| [18-RLIB-FORMAT.md](./18-RLIB-FORMAT.md) | RLIB library format and metadata |
| [19-MANIFEST.md](./19-MANIFEST.md) | Package manifest (tml.toml) |

### Reference

| Document | Description |
|----------|-------------|
| [13-BUILTINS.md](./13-BUILTINS.md) | Builtin types and functions |
| [14-EXAMPLES.md](./14-EXAMPLES.md) | Complete examples |
| [15-ERROR-HANDLING.md](./15-ERROR-HANDLING.md) | Error handling system |
| [25-DECORATORS.md](./25-DECORATORS.md) | Custom decorators system |

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
let x: I32 = 42         // immutable, explicit type required
let y: I64 = 100        // immutable, explicit type
var count: I32 = 0      // mutable, explicit type required
const PI: F64 = 3.14159 // compile-time constant, explicit type
```

### String Interpolation
```tml
let name: String = "World"
let greeting: String = "Hello {name}!"    // "Hello World!"
let result: String = "Sum: {a + b}"       // expressions allowed
let escaped: String = "Use \{ and \}"     // literal braces
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
        let dx: F64 = this.x - other.x
        let dy: F64 = this.y - other.y
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
let data: Outcome[String, Error] = read_file("config.tml")!

// else provides inline fallback
let config: Outcome[Data, Error] = parse(data)! else default_config()

// catch for blocks with common handling
catch {
    let file: Outcome[File, Error] = open(path)!
    let data: Outcome[String, Error] = file.read()!
    return Ok(parse(data)!)
} else |err| {
    log.error(err)
    return Err(err)
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
let add: func(I32, I32) -> I32 = do(x, y) x + y
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
| **Outcome** | `Outcome[T, E]` = `Ok(T)` \| `Err(E)` |
| **Collections** | `List[T]`, `Map[K, V]`, `Set[T]` |
| **Memory** | `Heap[T]`, `Shared[T]`, `Sync[T]` |
| **References** | `ref T`, `mut ref T` |

## Status

| Version | Status | Features |
|---------|--------|----------|
| v0.1 | ✅ Complete | Core specification |
| v0.5 | ✅ Active | Bootstrap compiler with LLVM backend |
| v1.0 | 🔄 In Progress | Self-hosted compiler |

### Implementation Status (Jan 2026)

| Feature | Status |
|---------|--------|
| Lexer | ✅ Complete |
| Parser | ✅ Complete (LL(1)) |
| Type Checker | ✅ Complete (modular) |
| Pattern Matching | ✅ Complete |
| Module System | ✅ Complete |
| Local Module Imports | ✅ Complete (`use module_name`) |
| Trait Objects | ✅ Complete |
| Generics | ✅ Complete (monomorphization) |
| Where Clauses | ✅ Complete |
| String Interpolation | ✅ Complete |
| LLVM Backend | ✅ Complete |
| Test Framework | ✅ Complete |
| FFI Support | ✅ Complete (@extern, @link) |
| Borrow Checker | ✅ Complete (reborrows, two-phase) |
| Build Cache | ✅ Complete (content-based) |
| Mid-level IR (MIR) | ✅ Complete (SSA, 6 optimization passes) |
| **C#-Style OOP** | ✅ Complete (classes, inheritance, interfaces, vtables) |
| Package Management | ✅ Complete (`tml add`, `tml update`) |

## License

Apache License 2.0
