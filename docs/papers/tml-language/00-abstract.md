# TML: A Systems Programming Language Designed for LLM Code Generation

## Abstract

We present TML (To Machine Language), a statically-typed, compiled systems programming language that introduces LLM-friendliness as a first-class design constraint alongside the traditional goals of safety, performance, and expressiveness. TML combines Rust-inspired ownership semantics with a novel keyword-based syntax designed to minimize the ambiguity that causes errors in LLM-generated code. The language features an LL(1) grammar where each token has a single meaning, self-documenting type names (`Maybe[T]`, `Outcome[T,E]`, `Heap[T]`), and English keywords for operators and control flow (`and`, `or`, `not`, `when`, `behavior`).

The TML compiler implements a query-based, demand-driven compilation pipeline with five intermediate representation layers (AST, HIR, THIR, MIR, LLVM IR), 52 MIR optimization passes, embedded LLVM and LLD, and incremental compilation with fingerprint-based cache invalidation. The standard library provides over 500 types and 5,000 functions across core, standard, and test libraries, including built-in HTTP, JSON, cryptography, database drivers, and SIMD support.

TML introduces several innovations for AI-assisted development: a Model Context Protocol (MCP) server exposing all compiler operations as structured tool calls; a debug layers feature that emits HIR, MIR, and LLVM IR for failing tests with diagnosis hints identifying the error's compilation layer; and a systematic Rust-as-Reference methodology for evaluating generated code quality against rustc output.

We describe TML's syntax decisions and their rationale, analyze the compiler architecture with comparisons to rustc, GCC, Clang, and the Go compiler, present the multi-layer IR pipeline design, evaluate the 52-pass MIR optimization suite, and provide comprehensive feature comparisons against Rust, C++, Go, Python, Zig, Swift, and Kotlin across 30+ dimensions. We argue that the emergence of LLMs as code generators creates a new design space for programming languages — one that TML is the first to systematically explore.

**Keywords:** programming language design, compiler architecture, LLM code generation, ownership type system, intermediate representation, LLVM, systems programming
