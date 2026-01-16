# TML Examples

This directory contains example TML code files demonstrating the language syntax and features.

## File List

| File | Description |
|------|-------------|
| [01-hello-world.tml](01-hello-world.tml) | Basic program structure, module declaration, main function |
| [02-variables.tml](02-variables.tml) | Variable declarations (`let`, `var`, `const`), type inference, numeric types |
| [03-functions.tml](03-functions.tml) | Function definitions, generics with `[T]`, references (`ref`, `mut ref`) |
| [04-control-flow.tml](04-control-flow.tml) | `if-then-else`, `when` pattern matching, `loop` (unified iteration) |
| [05-types.tml](05-types.tml) | Structs, enums with data, type aliases, generics |
| [06-error-handling.tml](06-error-handling.tml) | `!` operator, `else` recovery, `catch` blocks, `Outcome` |
| [07-traits.tml](07-traits.tml) | Behavior definitions, `extend X with T` syntax, behavior bounds |
| [08-closures.tml](08-closures.tml) | `do()` closure syntax, captures, higher-order functions |
| [09-modules.tml](09-modules.tml) | Module system, `import`, visibility (`public`/`private`) |
| [10-collections.tml](10-collections.tml) | Arrays, List, Map, Set, iterators, ranges |
| [11-async.tml](11-async.tml) | `async`/`await`, channels, concurrent execution |
| [12-memory.tml](12-memory.tml) | Ownership, borrowing, smart pointers (`Heap`, `Shared`, `Sync`) |
| [13-caps-effects.tml](13-caps-effects.tml) | Capabilities system (`with io, net`), effects, contracts |
| [14-stable-ids.tml](14-stable-ids.tml) | `@id` annotations for LLM integration and precise patches |
| [15-namespaces.tml](15-namespaces.tml) | Namespace organization, classes, interfaces |
| [16-oop-benchmark.tml](16-oop-benchmark.tml) | Virtual dispatch benchmarks, sealed/@value classes |

## Key Syntax Differences from Rust

| Feature | Rust | TML |
|---------|------|-----|
| Generics | `<T>` | `[T]` |
| Closures | `\|x\| expr` | `do(x) expr` |
| Logical operators | `&&`, `\|\|`, `!` | `and`, `or`, `not` |
| Function keyword | `fn` | `func` |
| Pattern matching | `match` | `when` |
| Loops | `for`, `while`, `loop` | `loop` (unified) |
| Conditionals | `if cond { }` | `if cond then { }` |
| Self reference | `self`, `Self` | `this`, `This` |
| Trait impl | `impl T for X` | `extend X with T` |
| Modules | `mod`, `use` | `module`, `import` |
| Mutable binding | `let mut` | `var` |

## Quick Start

```tml
// hello.tml
mod hello

pub func main() {
    println("Hello, TML!")
}
```

Run with:
```bash
tml run hello.tml
```
