# Behaviors

A *behavior* in TML is a named contract that a type can satisfy. It declares a set of methods that any conforming type must implement, and optionally provides default implementations for some of those methods. Behaviors are TML's primary mechanism for polymorphism.

## The Core Idea

Consider two types — `Circle` and `Rectangle` — that both know how to compute an area and a perimeter. Without behaviors, the only way to write a function that works on both is to duplicate it or use a union of cases. With a behavior, you define the contract once:

```tml
behavior Shape {
    func area(this) -> F64
    func perimeter(this) -> F64
}
```

Then each type independently declares that it satisfies the contract:

```tml
extend Circle with Shape {
    func area(this) -> F64 { 3.14159 * this.radius * this.radius }
    func perimeter(this) -> F64 { 2.0 * 3.14159 * this.radius }
}

extend Rectangle with Shape {
    func area(this) -> F64 { this.width * this.height }
    func perimeter(this) -> F64 { 2.0 * (this.width + this.height) }
}
```

Now any function that requires a `Shape` works with both types — and any future type that also implements `Shape` — without any modification:

```tml
func print_area[S: Shape](shape: S) {
    println("Area: " + shape.area().to_string())
}
```

## How Behaviors Compare to Similar Concepts

Behaviors cover the same role as:

- **Traits** in Rust — the syntax and semantics are closely aligned. TML uses the keyword `behavior` where Rust uses `trait`, and `extend Type with Behavior` where Rust uses `impl Trait for Type`.
- **Interfaces** in Java or C# — behaviors define a set of method signatures, and types implement them explicitly.
- **Type classes** in Haskell — behaviors are resolved at compile time and, like Haskell's type classes, support parametric polymorphism through bounds on generic type parameters.

The key difference from object-oriented interfaces is that behaviors in TML are resolved statically at compile time by default, with no runtime dispatch overhead. When you write `func f[T: Shape](s: T)`, the compiler generates a concrete version of `f` for each type `T` that is ever passed — a process called *monomorphization*. Dynamic dispatch is available explicitly through `dyn Shape` (covered in ch05-03).

## What This Chapter Covers

- **Defining and Implementing Behaviors** (ch05-01) — the `behavior` keyword, `extend Type with Behavior`, default implementations, associated types, and adding methods to existing types with plain `extend`.
- **Common Standard Behaviors** (ch05-02) — the behaviors defined by TML's core and standard libraries: `Display`, `Debug`, `PartialEq`, `Eq`, `PartialOrd`, `Ord`, `Hash`, `Default`, `Duplicate`, `Copy`, `From`, `Into`, `Drop`, and `Iterator`.
- **Behavior Objects and Dynamic Dispatch** (ch05-03) — how `dyn Behavior` creates runtime polymorphism, when to use it instead of generics, and the rules around behavior object safety.
