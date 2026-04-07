# Summary

[The TML Programming Language](title-page.md)
[Foreword](foreword.md)
[Introduction](ch00-00-introduction.md)

## Getting Started

- [Getting Started](ch01-00-getting-started.md)
    - [Installation](ch01-01-installation.md)
    - [Hello, World!](ch01-02-hello-world.md)

## Language Fundamentals

- [Common Programming Concepts](ch02-00-common-programming-concepts.md)
    - [Variables and Mutability](ch02-01-variables-and-mutability.md)
    - [Data Types](ch02-02-data-types.md)
    - [Functions](ch02-03-functions.md)
    - [Comments](ch02-04-comments.md)
    - [Control Flow](ch02-05-control-flow.md)
- [Structs and Methods](ch03-00-structs.md)
    - [Defining Structs](ch03-01-defining-structs.md)
    - [Methods and Extend Blocks](ch03-02-methods-and-extend.md)
- [Enums and Pattern Matching](ch04-00-enums.md)
    - [Defining Enums](ch04-01-defining-enums.md)
    - [Pattern Matching with When](ch04-02-pattern-matching.md)
- [Behaviors](ch05-00-behaviors.md)
    - [Defining and Implementing Behaviors](ch05-01-defining-behaviors.md)
    - [Common Behaviors](ch05-02-common-behaviors.md)
    - [Behavior Objects (Dynamic Dispatch)](ch05-03-behavior-objects.md)
- [Generics](ch06-00-generics.md)
    - [Generic Functions and Types](ch06-01-generic-functions-and-types.md)
    - [Bounds and Where Clauses](ch06-02-bounds-and-where.md)

## Error Handling and Safety

- [Error Handling](ch07-00-error-handling.md)
    - [Maybe and Outcome](ch07-01-maybe-and-outcome.md)
    - [The ! Operator and Recovery](ch07-02-propagation-and-recovery.md)
    - [Designing Error Types](ch07-03-error-types.md)

## Ownership and Memory

- [Ownership and Borrowing](ch08-00-ownership.md)
    - [Ownership Rules](ch08-01-ownership-rules.md)
    - [References and Borrowing](ch08-02-references.md)
    - [Smart Pointers](ch08-03-smart-pointers.md)

## Functional Programming

- [Closures](ch09-00-closures.md)
    - [The Do Syntax](ch09-01-do-syntax.md)
    - [Closures as Arguments](ch09-02-closures-as-arguments.md)
- [Iterators](ch10-00-iterators.md)
    - [Iterator Basics](ch10-01-iterator-basics.md)
    - [Iterator Adapters and Consumers](ch10-02-adapters-and-consumers.md)
- [Collections](ch11-00-collections.md)
    - [Lists and Arrays](ch11-01-lists-and-arrays.md)
    - [Maps and Sets](ch11-02-maps-and-sets.md)

## Code Organization

- [Modules and Imports](ch12-00-modules.md)
    - [Defining Modules](ch12-01-defining-modules.md)
    - [Use Declarations and Visibility](ch12-02-use-and-visibility.md)
- [Testing](ch13-00-testing.md)
    - [Writing Tests](ch13-01-writing-tests.md)
    - [Running Tests](ch13-02-running-tests.md)
    - [Benchmarks](ch13-03-benchmarks.md)

## Advanced Features

- [Decorators and Derive](ch14-00-decorators.md)
    - [Built-in Decorators](ch14-01-builtin-decorators.md)
    - [Derive Macros](ch14-02-derive.md)
    - [Custom Decorators](ch14-03-custom-decorators.md)
- [Object-Oriented Programming](ch15-00-oop.md)
    - [Classes and Inheritance](ch15-01-classes.md)
    - [Interfaces](ch15-02-interfaces.md)
- [Concurrency](ch16-00-concurrency.md)
    - [Threads](ch16-01-threads.md)
    - [Synchronization Primitives](ch16-02-sync.md)
    - [Channels](ch16-03-channels.md)
    - [Atomic Operations](ch16-04-atomics.md)
- [Foreign Function Interface](ch17-00-ffi.md)
    - [Calling C from TML](ch17-01-calling-c.md)
    - [Lowlevel Blocks and Intrinsics](ch17-02-lowlevel.md)
    - [Building Libraries](ch17-03-building-libraries.md)
- [Conditional Compilation](ch18-00-conditional-compilation.md)
- [Bitwise Operations](ch19-00-bitwise-operations.md)

## Standard Library Guide

- [Standard Library Overview](ch20-00-standard-library.md)
- [Working with JSON](ch21-00-json.md)
- [Cryptography](ch22-00-crypto.md)
- [Compression](ch23-00-compression.md)
- [Networking and HTTP](ch24-00-networking.md)
- [Working with Databases](ch25-00-database.md)
- [Machine Learning with TML](ch26-00-ai-ml.md)

## Appendix

- [Appendix](appendix-00.md)
    - [A - Keywords](appendix-01-keywords.md)
    - [B - Operators](appendix-02-operators.md)
    - [C - Builtin Functions](appendix-03-builtins.md)
