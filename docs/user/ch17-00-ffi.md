# Foreign Function Interface

TML programs do not exist in isolation. They run on operating systems that
expose C APIs, use libraries written in C and C++, and are sometimes required
to provide functionality to programs written in other languages. The Foreign
Function Interface (FFI) is the mechanism TML provides for crossing these
language boundaries safely and explicitly.

## What FFI Covers

This chapter covers three directions of interoperability:

1. **Calling C from TML** — Declaring and invoking functions that live in
   external C libraries using `@extern` and `@link` decorators.

2. **Lowlevel blocks and memory intrinsics** — Using TML's `lowlevel` keyword
   to perform raw pointer operations, manual memory management, and other
   operations that require explicit programmer attention.

3. **Building TML libraries for C consumption** — Compiling TML code into
   static or dynamic libraries and generating C headers so that C programs
   can call TML functions.

## The Design Philosophy

FFI in TML is deliberate and visible. Every call to a C function must be
explicitly declared with `@extern`. Every use of raw pointer arithmetic must
appear inside a `lowlevel` block. There are no hidden unsafe operations.

This design serves two goals:

- **Auditability** — searching a codebase for `@extern` or `lowlevel`
  immediately shows all the places where TML's usual safety guarantees do not
  apply.

- **Clarity of intent** — calling `strlen` looks different from calling a
  pure TML function, making it impossible to mistake C interop for TML code.

## Prerequisites

Before reading the detailed chapters, you should be familiar with:

- TML's basic type system (chapter 2)
- Pointers and raw memory (chapter 8)
- The `lowlevel` keyword is introduced fully in chapter 17-02

If you only need to call a standard system library function — `getenv`,
`fopen`, `clock_gettime` — chapter 17-01 is sufficient. If you are
implementing a data structure using raw memory, or wrapping a C library that
passes ownership of heap-allocated data, read chapter 17-02 as well.
