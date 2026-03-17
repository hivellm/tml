# Generics

Generics let you write code that works over many types without duplicating it. Instead of writing `max_i32`, `max_f64`, and `max_str` as three separate functions, you write one `max` function parameterized over any type that supports ordering:

```tml
func max[T: Ord](a: T, b: T) -> T {
    if a.cmp(ref b).is_greater() then a else b
}
```

The `[T: Ord]` declares a *type parameter* named `T` with the constraint that `T` must implement the `Ord` behavior. At call sites, the compiler fills in the concrete type automatically:

```tml
let biggest = max(3, 7)            // T = I32
let first   = max("apple", "fig")  // T = Str
```

The compiler generates a separate, fully optimized version of `max` for each concrete type that appears at a call site — a process called *monomorphization*. The result is as fast as hand-written code for each type, with no runtime overhead.

## What Generics Cover

Generics in TML apply to:

- **Functions** — type parameters on individual functions
- **Structs** — type parameters on type definitions
- **Enums** — type parameters on enum definitions (including the standard library's `Maybe[T]` and `Outcome[T, E]`)
- **Behavior implementations** — implementing a behavior for a parameterized type
- **Constants** — const generics allow array sizes and similar values to be parameterized

## What This Chapter Covers

- **Generic Functions and Types** (ch06-01) — declaring type parameters, using them in function signatures and struct definitions, and how monomorphization works
- **Bounds and Where Clauses** (ch06-02) — constraining type parameters with behavior bounds, combining multiple bounds, using `where` for complex constraints, and const generics
