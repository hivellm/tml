# Foreword

Programming languages are tools for thought. They shape how we express ideas, how we reason about
correctness, and — increasingly — how AI systems generate and analyze code. TML was designed with
all three of those goals in mind.

## The Problem with Existing Languages

The explosive growth of AI-assisted programming has exposed a structural mismatch. Languages like
C++, Rust, and Java were designed for human typists working in text editors. Their syntax reflects
decades of tradition rather than the requirements of probabilistic text generation. Consider a few
examples:

- Generic type parameters use angle brackets: `Vec<HashMap<String, Vec<i32>>>`. The `<` character
  is also the less-than operator, creating parsing ambiguity that humans resolve through experience
  and that language models must learn to navigate through statistical pattern matching.
- Boolean operators are symbolic: `&&`, `||`, `!`. These are compact for keyboard entry but carry
  no inherent meaning — a model must learn that `&&` means "and" rather than some other operation.
- Closures in Rust use pipe characters as delimiters: `|x| x + 1`. The same character serves as
  bitwise OR, requiring context to disambiguate.
- Lifetime annotations (`'a`, `'b`) appear inline with types throughout Rust code, adding visual
  noise that carries semantics invisible in the generated output.

None of these design choices are mistakes — they were sensible tradeoffs for their time. But they
impose unnecessary cognitive load on language models, contributing to generation errors that are
subtle and difficult to detect.

## The TML Approach

TML eliminates these ambiguities through a consistent principle: **keywords over symbols, clarity
over brevity**. Every syntactic choice was evaluated against the question: *will a language model
generate this correctly and consistently?*

Generics use square brackets: `List[T]`, `HashMap[K, V]`. Square brackets appear nowhere else in
TML syntax, so they unambiguously signal a type parameter.

Boolean logic uses English words: `and`, `or`, `not`. These carry their meaning without
memorization and cannot be confused with punctuation.

Closures use the `do` keyword: `do(x) x + 1`. The pipe character is freed for bitwise operations
exclusively.

Lifetimes are always inferred. They appear nowhere in the source language, eliminating an entire
category of annotation noise.

The result is a language that is not only easier for models to generate, but easier for humans to
read — because the code says what it means.

## Safety Without Ceremony

TML is a systems programming language. It compiles to native code via an embedded LLVM backend,
has no garbage collector, and gives you full control over memory layout and allocation. At the same
time, it enforces memory safety through an ownership and borrowing system with the same guarantees
as Rust.

The difference is in presentation. TML uses `ref T` and `mut ref T` where Rust uses `&T` and
`&mut T`. Lifetimes are inferred rather than annotated. The borrow checker's rules are identical,
but the surface syntax is designed to make those rules legible to someone reading unfamiliar code —
human or AI.

## A Production-Ready Standard Library

TML ships with a comprehensive standard library covering everything you need to build real
software: JSON serialization, cryptographic primitives, HTTP client and server, compression,
regular expressions, concurrent data structures, and more. The standard library is written in TML
itself, serving as both a practical toolkit and a demonstration of the language's capabilities at
scale.

## Who This Book Is For

This book is for programmers who want to understand TML from the ground up. It assumes you have
experience in at least one other programming language — the concepts of functions, variables, and
types should be familiar. Experience with Rust or C# will help you recognize certain patterns, but
neither is required.

If you are building a code generation system and evaluating TML as a target language, the
foreword and introduction will orient you quickly. The language reference chapters give you
precise definitions of every construct.

If you are a developer looking to write systems software in a language that is readable,
safe, and backed by a rich standard library, start at Chapter 1 and work through sequentially.

---

*Proceed to the [Introduction](ch00-00-introduction.md) for an overview of the book's
organization and what you will build along the way.*
