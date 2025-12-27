# Introduction

Welcome to *The TML Programming Language*, a guide to TML - a language designed for the AI era.

## The TML Philosophy

> **LLM-First, Human-Friendly**

TML is built on a unique premise: design a language that **LLMs can generate reliably**, while keeping the **ergonomics that human developers love** from Rust and C#.

### Why This Approach?

Modern software development increasingly involves AI-generated code. But existing languages weren't designed for this:

- Ambiguous syntax confuses code generation
- Cryptic symbols (`&&`, `||`, `<T>`) make token prediction harder
- Macros break deterministic parsing

TML solves these problems while remaining pleasant for humans to read and write.

## Who TML Is For

TML is ideal for:

- **LLM developers** building code generation systems that need reliable, parseable output
- **Human developers** who appreciate clean, readable syntax inspired by Rust and C#
- **Mixed teams** where AI-generated and human-written code coexist
- **Systems programmers** who need low-level control with high-level safety

## What You'll Recognize

If you know **Rust**, you'll recognize:
- Ownership and borrowing (`ref`, `mut ref`)
- Pattern matching (`when`)
- Traits as `behavior`
- Zero-cost abstractions

If you know **C#**, you'll recognize:
- Clean generics with `[T]`
- Method syntax (`.len()`, `.push()`)
- LINQ-style chains
- Async/await patterns

## What's New in TML

- `and`, `or`, `not` instead of `&&`, `||`, `!`
- `to`/`through` for ranges instead of `..`/`..=`
- `Maybe[T]`/`Outcome[T,E]` instead of `Option`/`Result`
- `@directives` instead of cryptic macros
- Stable IDs (`@abc123`) for AI-friendly patching

## How to Use This Book

This book assumes you have programming experience. It's organized progressively:

- **Chapters 1-2**: Getting started and basic concepts
- **Chapter 3**: Data structures (structs)
- **Chapters 4-6**: Low-level operations (bitwise, pointers, memory)
- **Chapter 7**: Concurrency (atomics, channels, mutex)
- **Chapters 8-9**: Collections and enums

## Source Code

All examples in this book are tested and can be found in the `tests/tml/` directory.

Let's begin your TML journey!
