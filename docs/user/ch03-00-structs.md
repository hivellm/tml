# Structs and Methods

A *struct* is a named collection of related values. Where primitive types like `I32` or `Bool`
represent a single value, a struct lets you group multiple pieces of data under one name and
treat them as a unit.

## Why Structs?

Suppose you are tracking a 2D point. You could use two separate variables:

```tml
let x = 1.0
let y = 2.0
```

This is fragile. Nothing stops you from accidentally passing `y` where `x` was expected, and
there is no way to write a function that "takes a point" as a single argument. A struct solves
both problems:

```tml
type Point {
    x: F64,
    y: F64,
}

func distance_from_origin(p: Point) -> F64 {
    (p.x * p.x + p.y * p.y).sqrt()
}
```

The struct gives the data a name and a shape. The function signature is now self-documenting:
it takes a `Point`, not two arbitrary `F64` values.

## Structs and Methods Together

In TML, methods are not defined inside the struct declaration. Instead, you add methods to an
existing type using an `extend` block. This separation keeps struct definitions focused on data,
while `extend` blocks focus on behavior:

```tml
type Point {
    x: F64,
    y: F64,
}

extend Point {
    func new(x: F64, y: F64) -> Point {
        return Point { x: x, y: y }
    }

    func distance_to(this, other: ref Point) -> F64 {
        let dx = this.x - other.x
        let dy = this.y - other.y
        return (dx * dx + dy * dy).sqrt()
    }
}
```

The `this` parameter marks an instance method. Methods without `this` are static — called on
the type itself rather than on an instance.

## What This Chapter Covers

- [Defining Structs](ch03-01-defining-structs.md) — field syntax, instantiation, mutability,
  update syntax, tuple structs, unit structs, nested structs, and generic structs
- [Methods and Extend Blocks](ch03-02-methods-and-extend.md) — adding methods with `extend`,
  the `this` parameter, static constructors, method chaining, and multiple extend blocks
