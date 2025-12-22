# Using Structs to Structure Related Data

A *struct*, or structure, lets you group related values together. Structs
are similar to classes in other languages, but without inheritance.

## Why Structs?

Consider representing a user with separate variables:

```tml
let username = "alice"
let email = "alice@example.com"
let age = 30
```

This works, but the data isn't connected. What if you need multiple users?

Structs solve this by grouping related data:

```tml
type User {
    username: Str,
    email: Str,
    age: I32,
}
```

Now you can create instances:

```tml
let user = User {
    username: "alice",
    email: "alice@example.com",
    age: 30,
}
```

## What You'll Learn

In this chapter, you'll learn:

- How to define structs
- How to create instances
- How to access and modify fields
- Practical examples using structs
