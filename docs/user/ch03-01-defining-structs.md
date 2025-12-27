# Defining and Instantiating Structs

Structs are defined using the `type` keyword.

## Defining a Struct

```tml
type User {
    username: Str,
    email: Str,
    age: I32,
    active: Bool,
}
```

Each piece of data is called a *field*. Fields have a name and a type.

## Creating Instances

Create an instance by specifying values for all fields:

```tml
func main() {
    let user = User {
        username: "alice",
        email: "alice@example.com",
        age: 30,
        active: true,
    }

    println(user.username)  // alice
}
```

## Accessing Fields

Use dot notation to access fields:

```tml
func main() {
    let user = User {
        username: "bob",
        email: "bob@example.com",
        age: 25,
        active: true,
    }

    println(user.username)  // bob
    println(user.email)     // bob@example.com
    println(user.age)       // 25
    println(user.active)    // true
}
```

## Mutable Instances

To modify fields, the instance must be mutable:

```tml
func main() {
    let mut user = User {
        username: "charlie",
        email: "charlie@example.com",
        age: 28,
        active: true,
    }

    user.email = "charlie.new@example.com"
    println(user.email)  // charlie.new@example.com
}
```

Note: The entire instance must be mutable. You cannot make just one field
mutable.

## Struct Update Syntax

Create a new struct based on an existing one:

```tml
func main() {
    let user1 = User {
        username: "alice",
        email: "alice@example.com",
        age: 30,
        active: true,
    }

    let user2 = User {
        email: "bob@example.com",
        ..user1  // Copy remaining fields from user1
    }

    println(user2.username)  // alice (copied from user1)
    println(user2.email)     // bob@example.com (new value)
}
```

## Tuple Structs

Structs without named fields, just types:

```tml
type Point(I32, I32)
type Color(I32, I32, I32)

func main() {
    let origin = Point(0, 0)
    let black = Color(0, 0, 0)

    println(origin.0, ", ", origin.1)  // 0, 0
}
```

## Unit Structs

Structs with no fields (useful for implementing behaviors):

```tml
type AlwaysEqual
```

## Nested Structs

Structs can contain other structs:

```tml
type Point {
    x: I32,
    y: I32,
}

type Rectangle {
    top_left: Point,
    bottom_right: Point,
}

func main() {
    let rect = Rectangle {
        top_left: Point { x: 0, y: 10 },
        bottom_right: Point { x: 20, y: 0 },
    }

    println(rect.top_left.x)      // 0
    println(rect.bottom_right.y)  // 0
}
```
