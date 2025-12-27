# Foreword

## LLM-First, Human-Friendly

**TML (To Machine Language)** is a programming language with a unique philosophy:

> Designed for **LLM code generation**, with **ergonomics inspired by Rust and C#** so humans can use it too.

### Why This Matters

Modern AI assistants generate millions of lines of code daily. But existing languages were designed decades ago for human typing:
- Cryptic symbols (`&&`, `||`, `<T>`) that confuse token prediction
- Ambiguous syntax that requires context to parse
- Macros and metaprogramming that break deterministic analysis

TML solves this with **deterministic, self-documenting syntax** that LLMs can generate reliably, while keeping the **familiar patterns** that human developers expect.

### Best of Both Worlds

| From Rust | From C# | TML Innovation |
|-----------|---------|----------------|
| Ownership & borrowing | Clean generics `List[T]` | `and`/`or`/`not` keywords |
| Pattern matching (`when`) | Method syntax `.len()` | `to`/`through` for ranges |
| `ref`/`mut` semantics | LINQ-style chains | `Maybe[T]`/`Outcome[T,E]` |
| Zero-cost abstractions | `async`/`await` | `@directives` over macros |
| Traits (`behavior`) | Properties | Stable IDs `@abc123` |

### Syntax Comparison

| Concept | Rust | C# | TML | Why |
|---------|------|-----|-----|-----|
| Generics | `Vec<T>` | `List<T>` | `List[T]` | `<` is only comparison |
| Closures | `\|x\| x+1` | `x => x+1` | `do(x) x+1` | `\|` is only bitwise OR |
| Logic | `&&` `\|\|` `!` | `&&` `\|\|` `!` | `and` `or` `not` | Words are unambiguous |
| Iteration | `for i in 0..10` | `for(i=0;i<10;i++)` | `for i in 0 to 10` | Reads like English |
| Optional | `Option<T>` | `T?` | `Maybe[T]` | Self-documenting |
| Interfaces | `trait` | `interface` | `behavior` | Describes purpose |

### Who Is This For?

- **LLM developers** building code generation systems
- **Human developers** who appreciate clean, readable syntax
- **Teams** mixing AI-generated and human-written code
- **Anyone** tired of debugging parser ambiguities

This documentation will guide you from your first "Hello, World!" to advanced topics like concurrency, memory management, and low-level operations.

---

*Let's start: [Chapter 1 - Getting Started](ch01-00-getting-started.md)*
