# Methods and Extend Blocks

In TML, methods are not written inside the `type` block. Instead, you attach behavior to an
existing type using an `extend` block. This separation keeps data definitions readable and
lets you add methods to a type from anywhere in your codebase — including types defined in
other modules.

## The Extend Block

An `extend` block names a type and contains one or more functions associated with that type:

```tml
type Rectangle {
    width: F64,
    height: F64,
}

extend Rectangle {
    func area(this) -> F64 {
        return this.width * this.height
    }

    func perimeter(this) -> F64 {
        return 2.0 * (this.width + this.height)
    }

    func is_square(this) -> Bool {
        return this.width == this.height
    }
}

func main() {
    let r = Rectangle { width: 10.0, height: 5.0 }
    println(r.area().to_string())       // 50.0
    println(r.perimeter().to_string())  // 30.0
    println(r.is_square().to_string())  // false
}
```

## Instance Methods: The `this` Parameter

A function inside an `extend` block becomes an *instance method* when its first parameter is
named `this`. The compiler passes the receiver — the value the method is called on — as
`this` automatically.

```tml
extend Rectangle {
    func scale(this, factor: F64) -> Rectangle {
        return Rectangle {
            width: this.width * factor,
            height: this.height * factor,
        }
    }
}

func main() {
    let r = Rectangle { width: 4.0, height: 3.0 }
    let big = r.scale(2.0)
    println(big.width.to_string())   // 8.0
    println(big.height.to_string())  // 6.0
}
```

`this` has the type of the struct being extended. You access its fields with the familiar
dot notation.

## Mutable Instance Methods

To modify the receiver, annotate `this` with `mut`:

```tml
type Counter {
    value: I32,
}

extend Counter {
    func increment(mut this) {
        this.value = this.value + 1
    }

    func reset(mut this) {
        this.value = 0
    }

    func get(this) -> I32 {
        return this.value
    }
}

func main() {
    var c = Counter { value: 0 }
    c.increment()
    c.increment()
    c.increment()
    println(c.get().to_string())  // 3
    c.reset()
    println(c.get().to_string())  // 0
}
```

Calling a `mut this` method requires the receiver to be a mutable binding (`var`, not `let`).

## Static Methods: Constructors and Factories

A function in an `extend` block without a `this` parameter is a *static method*. It belongs
to the type rather than any particular instance. Static methods are called with dot notation
on the type name rather than on a value:

```tml
type Point {
    x: F64,
    y: F64,
}

extend Point {
    func new(x: F64, y: F64) -> Point {
        return Point { x: x, y: y }
    }

    func origin() -> Point {
        return Point { x: 0.0, y: 0.0 }
    }

    func distance_to(this, other: ref Point) -> F64 {
        let dx = this.x - other.x
        let dy = this.y - other.y
        return (dx * dx + dy * dy).sqrt()
    }
}

func main() {
    let p1 = Point.new(3.0, 4.0)
    let p2 = Point.origin()
    println(p1.distance_to(ref p2).to_string())  // 5.0
}
```

The `new` and `origin` functions are static: they are called on the type `Point`, not on an
instance. `distance_to` is an instance method: it is called on a `Point` value.

The convention for the primary constructor is `new`. Additional named constructors convey
intent — `origin`, `from_polar`, `default`, and so on.

## Methods That Take References

When a method does not need ownership of another struct, accept it by reference to avoid
copying. Use `ref T` for shared references and `mut ref T` for mutable references:

```tml
extend Rectangle {
    func contains(this, other: ref Rectangle) -> Bool {
        return other.width <= this.width and other.height <= this.height
    }

    func copy_from(mut this, source: ref Rectangle) {
        this.width = source.width
        this.height = source.height
    }
}
```

The `ref` keyword signals to callers that they pass a borrow, not a move. At the call site,
pass references with `ref`:

```tml
let big = Rectangle { width: 10.0, height: 8.0 }
let small = Rectangle { width: 4.0, height: 3.0 }

println(big.contains(ref small).to_string())  // true
```

## Method Chaining

Methods that return `this` type enable chaining. The pattern is common for builders:

```tml
type QueryBuilder {
    table: Str,
    limit_value: I32,
    offset_value: I32,
    where_clause: Str,
}

extend QueryBuilder {
    func new(table: Str) -> QueryBuilder {
        return QueryBuilder {
            table: table,
            limit_value: 100,
            offset_value: 0,
            where_clause: "",
        }
    }

    func limit(this, n: I32) -> QueryBuilder {
        return QueryBuilder { limit_value: n, ..this }
    }

    func offset(this, n: I32) -> QueryBuilder {
        return QueryBuilder { offset_value: n, ..this }
    }

    func where_clause(this, clause: Str) -> QueryBuilder {
        return QueryBuilder { where_clause: clause, ..this }
    }

    func build(this) -> Str {
        var sql = "SELECT * FROM " + this.table
        if this.where_clause != "" {
            sql = sql + " WHERE " + this.where_clause
        }
        sql = sql + " LIMIT " + this.limit_value.to_string()
        sql = sql + " OFFSET " + this.offset_value.to_string()
        return sql
    }
}

func main() {
    let query = QueryBuilder.new("users")
        .where_clause("active = true")
        .limit(10)
        .offset(20)
        .build()

    println(query)
    // SELECT * FROM users WHERE active = true LIMIT 10 OFFSET 20
}
```

Each method returns a new `QueryBuilder` (using the struct update syntax `..this`), so the
original value is not mutated. Chains can be split across lines for readability.

## Multiple Extend Blocks

You can write more than one `extend` block for the same type. This is useful for organizing
methods by concern, or for providing conditional implementations:

```tml
type Vector2 {
    x: F64,
    y: F64,
}

// Arithmetic
extend Vector2 {
    func new(x: F64, y: F64) -> Vector2 {
        return Vector2 { x: x, y: y }
    }

    func add(this, other: ref Vector2) -> Vector2 {
        return Vector2 { x: this.x + other.x, y: this.y + other.y }
    }

    func scale(this, factor: F64) -> Vector2 {
        return Vector2 { x: this.x * factor, y: this.y * factor }
    }
}

// Geometry
extend Vector2 {
    func length(this) -> F64 {
        return (this.x * this.x + this.y * this.y).sqrt()
    }

    func normalize(this) -> Vector2 {
        let len = this.length()
        return Vector2 { x: this.x / len, y: this.y / len }
    }

    func dot(this, other: ref Vector2) -> F64 {
        return this.x * other.x + this.y * other.y
    }
}

func main() {
    let a = Vector2.new(3.0, 4.0)
    let b = Vector2.new(1.0, 0.0)

    println(a.length().to_string())               // 5.0
    println(a.dot(ref b).to_string())             // 3.0
    println(a.normalize().length().to_string())   // 1.0
}
```

All methods from all `extend` blocks for a type are available equally. The split is purely
organizational.

## Extend with Generic Structs

`extend` blocks work with generic types. Specify the same type parameters as the struct
definition:

```tml
type Stack[T] {
    items: List[T],
}

extend Stack[T] {
    func new() -> Stack[T] {
        return Stack { items: List.new() }
    }

    func push(mut this, value: T) {
        this.items.push(value)
    }

    func pop(mut this) -> Maybe[T] {
        return this.items.pop()
    }

    func peek(this) -> Maybe[ref T] {
        return this.items.last()
    }

    func is_empty(this) -> Bool {
        return this.items.len() == 0
    }

    func size(this) -> I32 {
        return this.items.len()
    }
}

func main() {
    var s: Stack[I32] = Stack.new()
    s.push(1)
    s.push(2)
    s.push(3)

    println(s.size().to_string())  // 3

    when s.pop() {
        Just(v) => println(v.to_string()),  // 3
        Nothing => println("empty"),
    }
}
```

The type parameter `T` is in scope throughout the `extend` block, just as it is in the struct
definition.

## A Complete Example: 2D Point

The following example combines a struct definition with a full set of methods to show the
complete picture:

```tml
type Point {
    x: F64,
    y: F64,
}

extend Point {
    func new(x: F64, y: F64) -> Point {
        return Point { x: x, y: y }
    }

    func origin() -> Point {
        return Point { x: 0.0, y: 0.0 }
    }

    func distance_to(this, other: ref Point) -> F64 {
        let dx = this.x - other.x
        let dy = this.y - other.y
        return (dx * dx + dy * dy).sqrt()
    }

    func translate(this, dx: F64, dy: F64) -> Point {
        return Point { x: this.x + dx, y: this.y + dy }
    }

    func to_string(this) -> Str {
        return "(" + this.x.to_string() + ", " + this.y.to_string() + ")"
    }
}

func main() {
    let a = Point.new(0.0, 0.0)
    let b = Point.new(3.0, 4.0)

    println(a.to_string())                    // (0.0, 0.0)
    println(b.to_string())                    // (3.0, 4.0)
    println(a.distance_to(ref b).to_string()) // 5.0

    let c = b.translate(1.0, -1.0)
    println(c.to_string())                    // (4.0, 3.0)
}
```

The `new` and `origin` static methods serve as named constructors. `distance_to`,
`translate`, and `to_string` are instance methods. All are defined in one `extend` block,
keeping related behavior together.
