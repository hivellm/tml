# User-Defined Decorators with Quote/Splice

**Category**: language
**Tags**: language, decorators, metaprogramming

## Description

TML replaces Rust-style macros with decorators. User-defined decorators use the 'decorator' keyword, take DecoratorTarget (Func, Type, Field, etc.), return DecoratorResult (Unchanged, Modified, AddItem, Remove, Error). Quote/splice syntax: 'quote { ... }' creates code templates, '${expr}' splices expressions. Composition: top-to-bottom applied, inside-out execution. Can run at compile-time (default) or @runtime.

## When to Use

When implementing metaprogramming features or code generation in TML. Decorators are the primary abstraction for compile-time code transformation.
