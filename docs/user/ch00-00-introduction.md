# Introduction

Welcome to *The TML Programming Language*. This book is the primary reference and learning guide
for TML — a systems programming language that compiles to native code, enforces memory safety
through ownership and borrowing, and is designed from the ground up for reliable code generation
by large language models.

## What TML Is

TML is a statically typed, compiled systems language. Programs run without a runtime or garbage
collector. Memory is managed through an ownership system: every value has a single owner, and the
compiler verifies at compile time that memory is never accessed after it is freed and never aliased
through mutable references.

Underneath, TML uses an embedded LLVM backend to produce native executables for x86-64 and ARM64
targets. The `tml` command-line tool handles compilation, linking, testing, and formatting —
there are no external toolchain dependencies to install.

## What Makes TML Different

The most visible difference from other systems languages is the syntax. TML consistently chooses
keywords and unambiguous constructs over compact symbols. A few examples illustrate this:

**Logic operators**

```tml
if x > 0 and y > 0 {
    println("both positive")
}

if not found or count == 0 {
    println("nothing here")
}
```

Rust and C use `&&`, `||`, and `!`. TML uses `and`, `or`, and `not`. The meaning is in the word.

**Generics**

```tml
func first[T](items: ref List[T]) -> Maybe[T] {
    if items.len() == 0 {
        return Nothing
    }
    return Just(items[0])
}
```

Type parameters use square brackets throughout. There is no overloading of `<` and `>` for
generic syntax — those operators are comparison only.

**Closures**

```tml
let doubled = numbers.map(do(x) x * 2).collect()

let evens = numbers.filter(do(x) x % 2 == 0).collect()
```

Closures are introduced with the `do` keyword followed by a parameter list. The pipe character
`|` is used only for bitwise OR.

**Pattern matching**

```tml
let description = when value {
    0       => "zero",
    1 to 9  => "single digit",
    10..    => "large",
    _       => "negative",
}
```

`when` replaces `match`. Range patterns use `to` and `through` rather than `..` and `..=`.

**Error handling**

```tml
func read_config(path: Str) -> Outcome[Config, IoError] {
    let contents = File::read_to_string(path)?
    let config = Json::parse[Config](contents)?
    return Just(config)
}
```

`Outcome[T, E]` replaces `Result<T, E>`. `Maybe[T]` replaces `Option<T>`. `Just` and `Nothing`
replace `Some` and `None`. The `?` propagation operator works identically to Rust's.

**Lifetimes**

There are no lifetime annotations in TML. The borrow checker enforces the same rules as Rust,
but lifetimes are always inferred. You will never write `'a` or annotate a function with lifetime
parameters.

## The Standard Library

TML's standard library (`std`) covers a wide range of domains:

| Module | Purpose |
|--------|---------|
| `std/collections` | `List`, `HashMap`, `HashSet`, `BTreeMap`, queue types |
| `std/json` | JSON serialization and deserialization |
| `std/crypto` | Hashing (SHA-2, BLAKE3), HMAC, AES, ChaCha20 |
| `std/http` | HTTP/1.1 client and server, middleware, routing |
| `std/net` | TCP and UDP sockets, DNS |
| `std/fs` | File I/O, directory traversal |
| `std/compression` | zlib, gzip, deflate |
| `std/regex` | Regular expressions |
| `std/thread` | Threads, channels, atomics, mutex, Arc |
| `std/time` | Instant, Duration, system clock |

The core library (`core`) provides the primitives the standard library builds on: memory
allocation, iterators, string types, formatting, and error handling.

## Programming Paradigms

TML supports two complementary styles:

**Struct and behavior** — the primary style, familiar from Rust. Structs hold data, `extend`
blocks add methods, and `behavior` declarations (analogous to Rust's `trait`) define shared
interfaces.

```tml
behavior Display {
    func to_string(ref self) -> Str
}

struct Point {
    x: F64,
    y: F64,
}

extend Point : Display {
    func to_string(ref self) -> Str {
        return "({self.x}, {self.y})"
    }
}
```

**Class and interface** — an OOP style, familiar from C# or Java. Classes encapsulate data and
methods together, and interfaces define contracts. Both styles can coexist and interoperate
within a single codebase.

```tml
interface Drawable {
    func draw(ref self)
}

class Circle : Drawable {
    radius: F64

    func draw(ref self) {
        println("Drawing circle with radius {self.radius}")
    }
}
```

## How This Book Is Organized

The book is divided into five parts:

**Getting Started** (Chapters 1) introduces the toolchain, walks through a Hello World program,
and covers the basic development workflow.

**Language Fundamentals** (Chapters 2 through 6) covers variables, types, functions, control
flow, structs, enums, behaviors, and generics. These chapters build the vocabulary you need
for everything that follows.

**Error Handling and Safety** (Chapter 7) explains `Maybe` and `Outcome`, the `?` propagation
operator, and how to design error types that make failure paths clear.

**Ownership and Memory** (Chapter 8) covers TML's ownership model, borrowing rules, and smart
pointer types. If you are coming from a garbage-collected language, this chapter requires the
most careful reading.

**Functional Programming** (Chapters 9 through 11) covers closures, the `do` syntax, iterators,
and the collection types.

**Code Organization** (Chapters 12 and 13) covers the module system, visibility rules, and the
testing framework.

**Advanced Features** (Chapters 14 through 19) covers decorators, object-oriented programming,
concurrency, the foreign function interface, conditional compilation, and bitwise operations.

**Standard Library Guide** (Chapters 20 through 24) walks through the most important standard
library modules with practical examples: JSON, cryptography, compression, and networking.

**Appendix** provides a complete keyword reference, operator table, and builtin function list.

## Conventions Used in This Book

Code examples appear in blocks labeled with the language:

```tml
func main() {
    println("Hello, World!")
}
```

Shell commands appear in `bash` blocks:

```bash
tml run hello.tml
```

When a concept from Rust or another language is being compared, the comparison appears as a table
or side-by-side block. TML is not a clone of any existing language, but many ideas carry over
with modified syntax, and naming the connection helps readers with prior experience.

## A Note on the Syntax Design

Every syntactic choice in TML has a rationale. The full set of decisions is documented in
[RFC-0002: Syntax Design](../rfcs/RFC-0002-SYNTAX.md) for those who want to understand the
reasoning behind specific choices. The short version: if a construct could be misread by a
human or misgenerated by a language model, TML uses a more explicit alternative.

---

*Let's begin: [Chapter 1 — Getting Started](ch01-00-getting-started.md)*
