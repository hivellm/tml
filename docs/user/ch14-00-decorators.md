# Decorators and Derive

Decorators are compile-time annotations that attach metadata or behavior to functions, types,
variables, and modules. They begin with `@` and appear immediately before the item they annotate.
TML uses decorators for a wide range of tasks: marking test functions, controlling inlining,
declaring C bindings, deriving standard behaviors, and more.

```tml
@test
func test_addition() {
    assert_eq(1 + 1, 2)
}

@derive(PartialEq, Hash, Debug)
type Point {
    x: I32,
    y: I32,
}

@inline(always)
func clamp(v: I32, lo: I32, hi: I32) -> I32 {
    if v < lo { return lo }
    if v > hi { return hi }
    return v
}
```

## How Decorators Work

A decorator is evaluated at compile time. Depending on the decorator, it may:

- **Emit code** — `@derive(Debug)` generates a `debug_string()` method.
- **Set metadata** — `@deprecated` records a warning that fires at every call site.
- **Control codegen** — `@inline(always)` emits the `alwaysinline` LLVM attribute.
- **Register items** — `@test` records the function in the test harness.
- **Alter the ABI** — `@no_mangle` suppresses symbol name mangling for FFI.

Decorators do not change the type of the annotated item. A function decorated with `@inline`
still has the same signature as an undecorated function. Decorators add information; they do
not wrap or transform items into different types.

## Placement Rules

Decorators are placed on the line immediately before the item. Multiple decorators may be
stacked; they are applied in order from top to bottom:

```tml
@test
@should_panic(expected = "index out of bounds")
func test_bad_index() {
    let arr = [1, 2, 3]
    let _ = arr[10]
}
```

Decorators cannot appear mid-expression or inside function bodies. They attach only to
top-level items: functions, types, variables declared with `var` at module scope, and modules.

## Decorator Arguments

Many decorators accept arguments in parentheses. Arguments can be positional or named:

```tml
@bench(10000)                          // positional: iteration count
@stable(since = "1.0")                 // named
@deprecated(message = "Use new_api()") // named
```

If a decorator accepts arguments but none are provided, the parentheses are omitted entirely:

```tml
@inline         // no arguments
@must_use       // no arguments
@cold           // no arguments
```

## Categories

TML's built-in decorators fall into seven categories:

| Category | Decorators |
|----------|-----------|
| Testing | `@test`, `@bench`, `@fuzz`, `@should_panic`, `@ignore` |
| Performance | `@inline`, `@cold`, `@hot`, `@simd` |
| API Stability | `@stable`, `@unstable`, `@deprecated`, `@must_use` |
| Derive | `@derive` |
| FFI | `@extern`, `@link`, `@no_mangle`, `@repr` |
| Memory Layout | `@repr`, `@flags`, `@thread_local`, `@interior_mutable` |
| Diagnostics | `@allow`, `@deny`, `@doc` |
| Contracts | `@pre`, `@post`, `@invariant` |
| Conditional | `@when` |

The following sections cover each category in detail.

## What This Chapter Covers

- [Built-in Decorators](ch14-01-builtin-decorators.md) — reference for all decorators
  provided by the compiler, organized by category with examples
- [Derive Macros](ch14-02-derive.md) — how `@derive` auto-implements standard behaviors,
  what each derivable behavior generates, and how to customize derived output
- [Custom Decorators](ch14-03-custom-decorators.md) — defining your own decorators with
  the `decorator` keyword, `quote`/`splice` for code generation, and `DecoratorTarget`

## See Also

- [Testing](ch13-00-testing.md) — `@test`, `@bench`, and `@should_panic` in context
- [Foreign Function Interface](ch17-00-ffi.md) — `@extern`, `@link`, and `@repr` in depth
- [Conditional Compilation](ch18-00-conditional-compilation.md) — `@when` and `#if` directives
