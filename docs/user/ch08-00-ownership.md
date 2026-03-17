# Ownership and Borrowing

Memory management is one of the most consequential decisions a language designer makes.
Languages like C and C++ give programmers direct control but require them to remember
when to allocate and free every byte. Languages with garbage collectors automate memory
reclamation but introduce pauses and lose predictability. TML takes a third path,
inherited from Rust: a compile-time ownership system that proves memory is managed
correctly without runtime overhead.

The result is a language where use-after-free bugs, double-frees, and data races
are ruled out by the type checker, not detected at runtime.

## The Core Idea

Every value in TML has exactly one owner — a variable that is responsible for that
value's lifetime. When the owning variable goes out of scope, the value is dropped
automatically. No garbage collector is involved. No manual `free` call is required.

```tml
func example() {
    let name = "Alice"    // name owns the string
    println(name)
}
// name goes out of scope here — the string is freed automatically
```

This simple rule, applied consistently across the language, eliminates most memory
errors at their source.

## What This Chapter Covers

Ownership alone would be too restrictive. Code frequently needs to access a value
without taking permanent possession of it. For that, TML provides borrowing: a
system of references that allows temporary access to a value without transferring
ownership. The borrow checker enforces that these references are always valid.

For cases where a single owner is not sufficient — for example, when multiple parts
of a program need to share the same data — TML provides smart pointer types that
extend the ownership model safely.

This chapter introduces all three layers:

| Section | Topic |
|---------|-------|
| [Ownership Rules](ch08-01-ownership-rules.md) | Move semantics, copy types, the drop order |
| [References and Borrowing](ch08-02-references.md) | `ref T`, `mut ref T`, and the borrow checker |
| [Smart Pointers](ch08-03-smart-pointers.md) | `Heap`, `Shared`, `Sync`, `Cell`, `RefCell` |

## TML vs. Rust

TML's ownership system is modelled directly on Rust's, with one important
simplification: **lifetimes are always inferred**. There is no lifetime annotation
syntax (`'a`, `'b`, etc.) in TML source code. The compiler tracks lifetimes
internally and rejects programs where references outlive the data they point to,
but this analysis is invisible to the programmer.

If you have prior experience with Rust, you already understand TML's ownership
model. The differences are ergonomic, not semantic.

## Stack and Heap

Understanding where values live helps explain why ownership matters.

**Stack** memory is managed automatically by the call stack. Every function call
pushes a frame; every return pops it. Stack allocation is extremely fast, but the
size of every stack-allocated value must be known at compile time, and the lifetime
of the value is tied to the function that allocated it.

**Heap** memory can be allocated in arbitrary amounts at runtime and its lifetime
is decoupled from any particular function call. The tradeoff is that heap allocation
is slower and someone must be responsible for freeing it.

TML primitive types (`I32`, `F64`, `Bool`, `Char`, etc.) live on the stack. Compound
types that own heap memory — `Str`, `List[T]`, `HashMap[K, V]`, `Heap[T]` — keep
a small stack-resident header that points into the heap. The ownership system tracks
the stack header; when the header is dropped, the heap data is freed.

```tml
func demo() {
    let x: I32 = 42          // 4 bytes on the stack
    let s: Str = "hello"     // small header on the stack, bytes on the heap
}
// x and s both go out of scope — x evaporates, s.drop() frees the heap bytes
```

This mental model — stack header + heap payload — applies uniformly to strings,
collections, and smart pointers alike.
