# Object-Oriented Programming

TML supports two distinct programming paradigms for structuring types and behavior: the
struct-plus-behaviors model that forms the core of the language, and a class-based
object-oriented system with inheritance, interfaces, and virtual dispatch. You can use
either paradigm, or mix both, depending on what your problem calls for.

This chapter covers the OOP side of TML. The struct-plus-behaviors model is covered in
[Structs and Methods](ch03-00-structs.md) and [Behaviors](ch05-00-behaviors.md).

## Two Paradigms, One Language

Most TML code uses the struct-plus-behaviors model:

```tml
// Struct-plus-behaviors: composition over inheritance
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

behavior Drawable {
    func draw(this)
}

impl Drawable for Point {
    func draw(this) {
        println("Point at ({this.x}, {this.y})")
    }
}
```

TML also supports a class-based OOP model:

```tml
// Class-based OOP: inheritance hierarchies and virtual dispatch
class Shape {
    pub name: Str

    func new(name: Str) -> Shape {
        return Shape { name: name }
    }

    virtual func area(this) -> F64 {
        return 0.0
    }
}

class Circle extends Shape {
    radius: F64

    func new(radius: F64) -> Circle {
        return Circle { name: "Circle", radius: radius }
    }

    override func area(this) -> F64 {
        return 3.14159 * this.radius * this.radius
    }
}
```

Both paradigms are first-class. There is no pressure to choose one universally. The
right choice depends on the structure of your problem.

## When to Use Behaviors (struct + extend + impl)

The struct-plus-behaviors model is the default choice in TML. Prefer it when:

- **You need zero-cost abstractions.** Behavior dispatch can be fully resolved at compile
  time, producing no vtable overhead. Generic functions constrained by behaviors are
  monomorphized to concrete types.

- **You are adding capabilities to existing types.** You can implement a behavior for any
  type, including types you did not write. This is not possible with class inheritance.

- **You want composition over inheritance.** A struct can implement any number of
  behaviors independently. Behaviors can be combined through bounds without creating
  deep hierarchies.

- **You are implementing operators or standard library contracts.** The standard library
  uses behaviors for `Display`, `Hash`, `Eq`, `Ord`, `Iterator`, and so on. These
  compose cleanly with generic code.

- **Your types are primarily data containers.** Structs with `extend` blocks are lightweight.
  The compiler has full visibility into the type layout and can optimize aggressively.

## When to Use Classes

Choose the class system when:

- **You need true runtime polymorphism through inheritance.** If a variable must hold
  either a `Dog` or a `Cat` and dispatch to the correct `speak()` at runtime based on
  which concrete type it holds, a class hierarchy with virtual methods is the natural fit.

- **You are modeling strict IS-A relationships.** When `Dog` is genuinely a specialization
  of `Animal` — sharing its fields, inheriting its behavior, and extending it — class
  inheritance expresses that directly.

- **You need protected members.** The `protected` visibility modifier is only available
  on classes. It lets a base class expose internal state to subclasses without exposing
  it to the world.

- **You are building a framework or plugin system.** Class-based APIs are natural for
  systems where users subclass a base type to hook into behavior — GUI toolkits, web
  framework middleware, game engine components.

- **You are integrating with OOP-oriented external code.** If you are wrapping a C++
  library that uses inheritance, or building an API meant to be consumed from a
  class-oriented language, using classes in TML makes the mapping clearer.

- **You need abstract base classes.** Abstract classes enforce a contract that subclasses
  must fulfill, while still providing shared implementation for methods that do not need
  to be overridden.

## Quick Comparison

| Consideration | Behaviors | Classes |
|---|---|---|
| Dispatch cost | Zero (monomorphized) or one pointer (dyn) | Always one vtable pointer |
| Multiple "inheritance" | Any number of behaviors | Single base class |
| Retroactive implementation | Yes — impl for any type | No |
| Protected members | No | Yes |
| Abstract methods | Via behavior (no default) | `abstract func` |
| Constructor inheritance | Not applicable | `base:` call |
| Common use | General-purpose TML code | Framework APIs, OOP hierarchies |

## Combining Both Paradigms

The two models are not mutually exclusive. A class can implement a behavior, and a
generic function can accept a class type:

```tml
behavior Display {
    func to_string(this) -> Str
}

class Animal {
    pub name: Str

    func new(name: Str) -> Animal {
        return Animal { name: name }
    }

    virtual func speak(this) -> Str {
        return "..."
    }
}

class Dog extends Animal {
    override func speak(this) -> Str {
        return "Woof!"
    }
}

impl Display for Dog {
    func to_string(this) -> Str {
        return "Dog(" + this.name + ")"
    }
}

func print_display[T: Display](value: ref T) {
    println(value.to_string())
}

func main() {
    let dog = Dog::new("Rex")
    print_display(ref dog)  // Dog(Rex)
}
```

An interface can also coexist with behaviors. Interfaces follow OOP semantics (implemented
by classes, used through class hierarchies), while behaviors follow the structural model.
Both can appear in the same codebase.

## What This Chapter Covers

- [Classes and Inheritance](ch15-01-classes.md) — class definitions, access modifiers,
  static members, inheritance with `extends`, virtual and override methods, abstract
  classes, sealed classes, constructors with base calls, and properties
- [Interfaces](ch15-02-interfaces.md) — interface definitions, default method
  implementations, generic interfaces, interface inheritance, implementing multiple
  interfaces, and how interfaces compare to behaviors

---

*Previous: [Decorators and Derive](ch14-00-decorators.md)*
*Next: [Classes and Inheritance](ch15-01-classes.md)*
