# Defining and Instantiating Structs

Structs are defined with the `type` keyword followed by a name and a block of named fields.
Each field has a name and a type, separated by a colon, and fields are separated by commas.

## Basic Struct Definition

```tml
type User {
    username: Str,
    email: Str,
    age: I32,
    active: Bool,
}
```

The struct name uses `PascalCase` by convention. Field names use `snake_case`.

## Creating Instances

To create an instance, write the struct name followed by field initializers in curly braces.
All fields must be provided:

```tml
func main() {
    let user = User {
        username: "alice",
        email: "alice@example.com",
        age: 30,
        active: true,
    }

    println(user.username)  // alice
    println(user.age)       // 30
}
```

If you forget a field or provide a field that does not exist on the struct, the compiler
produces an error at the call site.

## Accessing Fields

Use dot notation to read a field:

```tml
func print_user(u: User) {
    println(u.username)
    println(u.email)
    println(u.age.to_string())
    println(u.active.to_string())
}
```

## Mutable Instances

By default, a `let` binding is immutable. To modify fields after creation, declare the
binding with `var`:

```tml
func main() {
    var user = User {
        username: "bob",
        email: "bob@example.com",
        age: 25,
        active: true,
    }

    user.email = "bob.updated@example.com"
    user.age = 26
    println(user.email)  // bob.updated@example.com
}
```

Mutability applies to the whole instance. You cannot make a single field mutable while leaving
the rest immutable.

## Struct Update Syntax

When you want a new struct that is mostly the same as an existing one, use the `..` spread
syntax to copy the remaining fields:

```tml
func main() {
    let original = User {
        username: "alice",
        email: "alice@example.com",
        age: 30,
        active: true,
    }

    // Override email; copy everything else from original
    let updated = User {
        email: "alice.new@example.com",
        ..original
    }

    println(updated.username)  // alice
    println(updated.email)     // alice.new@example.com
    println(updated.age)       // 30
}
```

Fields listed before `..source` take priority. Fields not listed are copied from the source.
The source struct must be of the same type.

## Shorthand Field Initialization

When a local variable has the same name as a field, you can write the name once:

```tml
func make_user(username: Str, email: Str) -> User {
    let age = 0
    let active = true
    return User { username, email, age, active }
}
```

This is equivalent to `User { username: username, email: email, age: age, active: active }`.

## Tuple Structs

A tuple struct has fields accessed by position rather than by name. Use parentheses instead
of curly braces in the definition:

```tml
type Color(U8, U8, U8)
type Point2D(F64, F64)
```

Instantiate and access tuple struct fields with numeric indices:

```tml
func main() {
    let red = Color(255, 0, 0)
    let origin = Point2D(0.0, 0.0)

    println(red.0.to_string())    // 255
    println(origin.1.to_string()) // 0.0
}
```

Tuple structs are useful when the order of the fields is obvious and the type name itself
provides enough context. For example, `Color(255, 0, 0)` is self-evident, while a named
struct would be more verbose without adding clarity.

## Unit Structs

A struct with no fields is called a *unit struct*. It occupies zero bytes and is commonly used
to implement behaviors on a type that carries no data:

```tml
type Marker
type AlwaysTrue
type JsonFormatter
```

Unit structs are instantiated by writing the name alone:

```tml
let m = Marker
```

## Nested Structs

Struct fields can be of any type, including other structs:

```tml
type Point {
    x: F64,
    y: F64,
}

type Rectangle {
    top_left: Point,
    bottom_right: Point,
}

func area(r: Rectangle) -> F64 {
    let width = r.bottom_right.x - r.top_left.x
    let height = r.top_left.y - r.bottom_right.y
    return width * height
}

func main() {
    let rect = Rectangle {
        top_left: Point { x: 0.0, y: 10.0 },
        bottom_right: Point { x: 20.0, y: 0.0 },
    }

    println(area(rect).to_string())  // 200.0
}
```

Chain dot notation to access nested fields: `rect.top_left.x`.

## Generic Structs

Structs can be parameterized over one or more types using square-bracket syntax. This lets
you write a single definition that works for many types:

```tml
type Pair[A, B] {
    first: A,
    second: B,
}

type Stack[T] {
    items: List[T],
    size: I32,
}
```

When you instantiate a generic struct, the type parameters are inferred from the values you
provide:

```tml
func main() {
    let p = Pair { first: 42, second: "hello" }
    println(p.first.to_string())   // 42
    println(p.second)              // hello

    let coords = Pair { first: 3.0, second: 4.0 }
    println(coords.first.to_string())  // 3.0
}
```

You can also annotate the type explicitly when inference is ambiguous:

```tml
let p: Pair[I32, Str] = Pair { first: 1, second: "one" }
```

Generic structs are monomorphized at compile time: each combination of type arguments produces
a separate, fully optimized version of the struct. There is no runtime overhead compared to
writing a non-generic version by hand.

## A Complete Example: Task Record

The following example brings together field definition, instantiation, update syntax, and
a helper function to illustrate how structs are used in practice:

```tml
type Task {
    id: I32,
    title: Str,
    completed: Bool,
    priority: I32,
}

func complete(t: Task) -> Task {
    return Task { completed: true, ..t }
}

func describe(t: Task) -> Str {
    let status = if t.completed { "done" } else { "pending" }
    return t.title + " [" + status + "] priority=" + t.priority.to_string()
}

func main() {
    let task = Task {
        id: 1,
        title: "Write documentation",
        completed: false,
        priority: 2,
    }

    println(describe(task))  // Write documentation [pending] priority=2

    let done = complete(task)
    println(describe(done))  // Write documentation [done] priority=2
}
```

The `complete` function returns a new `Task` with `completed` overridden to `true`, leaving
all other fields unchanged. The original `task` is not modified.
