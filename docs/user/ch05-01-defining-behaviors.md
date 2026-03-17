# Defining and Implementing Behaviors

## Defining a Behavior

A behavior is declared with the `behavior` keyword followed by a name and a block of method signatures:

```tml
behavior Describable {
    func describe(this) -> Str
}
```

Each method signature inside the behavior block declares what conforming types must provide. The `this` parameter receives the instance the method is called on. There is no body — only the signature.

Behavior names use `PascalCase` by convention.

## Implementing a Behavior

To declare that a type satisfies a behavior, write an `extend Type with Behavior` block and provide concrete implementations for every required method:

```tml
type Person {
    name: Str,
    age: U32,
}

extend Person with Describable {
    func describe(this) -> Str {
        return this.name + " is " + this.age.to_string() + " years old"
    }
}

func main() {
    let alice = Person { name: "Alice", age: 30 }
    println(alice.describe())  // Alice is 30 years old
}
```

A type can implement any number of behaviors. Each `extend Type with Behavior` block is written separately:

```tml
behavior Printable {
    func print(this)
}

extend Person with Printable {
    func print(this) {
        println("Person: " + this.name + ", age " + this.age.to_string())
    }
}
```

## Default Implementations

A behavior can provide a default body for one or more of its methods. Types that implement the behavior inherit the default unless they choose to override it:

```tml
behavior Greetable {
    func name(this) -> Str

    // Default implementation — types may override this
    func greet(this) -> Str {
        return "Hello, " + this.name() + "!"
    }
}

extend Person with Greetable {
    func name(this) -> Str {
        return this.name
    }
    // greet() is inherited from the default
}

func main() {
    let alice = Person { name: "Alice", age: 30 }
    println(alice.greet())  // Hello, Alice!
}
```

A type can still override a method that has a default:

```tml
type Robot {
    id: I32,
}

extend Robot with Greetable {
    func name(this) -> Str {
        return "Robot-" + this.id.to_string()
    }

    func greet(this) -> Str {
        return "GREETINGS. I AM " + this.name() + "."
    }
}
```

## Associated Types

Behaviors can declare *associated types* — type placeholders that each implementing type fills in concretely:

```tml
behavior Container {
    type Item

    func get(this, index: U64) -> Maybe[This.Item]
    func len(this) -> U64
}
```

When a type implements `Container`, it specifies what `Item` is:

```tml
type IntVec {
    data: List[I32],
}

extend IntVec with Container {
    type Item = I32

    func get(this, index: U64) -> Maybe[I32] {
        if index < this.data.len() as U64 {
            Just(this.data.get(index as I64))
        } else {
            Nothing
        }
    }

    func len(this) -> U64 {
        this.data.len() as U64
    }
}
```

Associated types are useful when the method signatures of a behavior depend on a type that varies per implementation, but where there is only ever one correct choice for a given implementing type. They keep call sites clean: instead of writing `fn get[T: Container, I = T::Item](c: T) -> Maybe[I]`, callers write `fn get[T: Container](c: T) -> Maybe[T.Item]`.

## A Complete Behavior Example

Here is a full example: a `Shape` behavior with two implementing types, and a generic function that works on any shape:

```tml
behavior Shape {
    func area(this) -> F64
    func perimeter(this) -> F64

    // Default: checks if this shape's area is greater than another's
    func is_larger_than[S: Shape](this, other: ref S) -> Bool {
        return this.area() > other.area()
    }
}

type Circle {
    radius: F64,
}

type Rectangle {
    width: F64,
    height: F64,
}

extend Circle with Shape {
    func area(this) -> F64 {
        return 3.14159 * this.radius * this.radius
    }

    func perimeter(this) -> F64 {
        return 2.0 * 3.14159 * this.radius
    }
}

extend Rectangle with Shape {
    func area(this) -> F64 {
        return this.width * this.height
    }

    func perimeter(this) -> F64 {
        return 2.0 * (this.width + this.height)
    }
}

func total_area[S: Shape](shapes: ref List[S]) -> F64 {
    var total = 0.0
    loop shape in *shapes {
        total += shape.area()
    }
    return total
}

func main() {
    let circle = Circle { radius: 5.0 }
    let rect = Rectangle { width: 10.0, height: 4.0 }

    println("Circle area: " + circle.area().to_string())      // 78.53975
    println("Rectangle area: " + rect.area().to_string())     // 40.0
    println("Circle larger: " + circle.is_larger_than(ref rect).to_string())  // true
}
```

## Using Multiple Behavior Bounds

A function can require several behaviors at once using `+`:

```tml
behavior Printable {
    func print(this)
}

behavior Describable {
    func describe(this) -> Str
}

func describe_and_print[T: Printable + Describable](item: T) {
    println(item.describe())
    item.print()
}
```

The type `T` must implement both `Printable` and `Describable` for the call to compile. This is equivalent to Rust's `T: Trait1 + Trait2` syntax.

## Adding Methods to Existing Types with `extend`

The `extend` keyword without a behavior clause adds methods directly to a type. This does not implement any behavior — it simply attaches new methods to the type:

```tml
// Add methods to the built-in Str type
extend Str {
    func is_blank(this) -> Bool {
        return this.trim().is_empty()
    }

    func repeat_n(this, n: U32) -> Str {
        var result: Str = ""
        loop _ in 0 to n {
            result = result + this
        }
        return result
    }
}

// Add methods to I32
extend I32 {
    func is_even(this) -> Bool {
        return this % 2 == 0
    }

    func abs_val(this) -> I32 {
        if this < 0 then -this else this
    }
}

func main() {
    println("  hello  ".is_blank().to_string())  // false
    println("   ".is_blank().to_string())         // true
    println("*".repeat_n(5))                      // *****
    println((-42).abs_val().to_string())          // 42
    println(4.is_even().to_string())              // true
}
```

You can add methods to types you did not define — including built-in types and types from the standard library. The only restriction is that you cannot add methods to a type that has no definition accessible from the current module (foreign types from other packages require that you own either the type or the behavior).

## Behavior Implementation Rules

1. **All required methods must be provided.** A type that implements a behavior but omits a required method (one without a default) will not compile.

2. **The method signatures must match.** The parameter types, return type, and `this` receiver kind must exactly match what the behavior declares.

3. **One implementation per type per behavior.** You cannot implement the same behavior for the same type twice. Attempting to do so is a compile error.

4. **Behaviors can be implemented for generic types.** See [Generics](ch06-00-generics.md) for details on implementing behaviors for parameterized types.
