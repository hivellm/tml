# Generic Functions and Types

## Generic Functions

A generic function declares one or more *type parameters* in square brackets after the function name. The type parameters can then appear anywhere in the parameter list and return type:

```tml
func identity[T](value: T) -> T {
    return value
}

func first[T](a: T, b: T) -> T {
    return a
}
```

At each call site, the compiler infers the concrete type from the arguments:

```tml
let x: I32 = identity(42)        // T = I32
let s: Str  = identity("hello")  // T = Str
```

Type inference means you rarely need to write the type parameter explicitly. When inference cannot determine the type — for example, when the function takes no parameters of type `T` — you can supply it explicitly:

```tml
func zero[T: Default]() -> T {
    return T.default()
}

let n: I32 = zero[I32]()
let v: F64 = zero[F64]()
```

### Multiple Type Parameters

A function can have more than one type parameter:

```tml
func pair[A, B](first: A, second: B) -> (A, B) {
    return (first, second)
}

func map_pair[A, B, C](p: (A, B), f: func(A) -> C) -> (C, B) {
    return (f(p.0), p.1)
}
```

### Generic Methods in Extend Blocks

Methods inside `extend` blocks can also be generic. The type parameter is declared on the method itself, not on the surrounding extend:

```tml
extend Str {
    func parse_as[T: FromStr](this) -> Outcome[T, ParseError] {
        return T.from_str(this)
    }
}

let n: I32 = "42".parse_as[I32]()!
let f: F64 = "3.14".parse_as[F64]()!
```

## Generic Structs

A type definition can be parameterized over one or more type parameters. The type parameters are declared after the type name:

```tml
type Pair[A, B] {
    first: A,
    second: B,
}

type Stack[T] {
    items: List[T],
}
```

When you use a generic type, you provide the concrete type arguments:

```tml
let p: Pair[I32, Str] = Pair { first: 42, second: "hello" }
let s: Stack[F64] = Stack { items: List.new() }
```

### Extend Blocks for Generic Structs

When writing `extend` blocks for a generic struct, repeat the type parameters from the struct definition:

```tml
extend Pair[A, B] {
    func new(first: A, second: B) -> Pair[A, B] {
        return Pair { first: first, second: second }
    }

    func swap(this) -> Pair[B, A] {
        return Pair { first: this.second, second: this.first }
    }
}

func main() {
    let p = Pair.new(10, "ten")
    let q = p.swap()
    println(q.first)              // ten
    println(q.second.to_string()) // 10
}
```

The full generic stack example:

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

    func size(this) -> I64 {
        return this.items.len()
    }
}

func main() {
    var stack: Stack[I32] = Stack.new()
    stack.push(1)
    stack.push(2)
    stack.push(3)

    println(stack.size().to_string())  // 3

    loop {
        when stack.pop() {
            Just(v) => println(v.to_string()),  // 3, 2, 1
            Nothing => break,
        }
    }
}
```

## Generic Enums

Enums can be parameterized in the same way as structs. The standard library's `Maybe[T]` and `Outcome[T, E]` are the canonical examples:

```tml
type Maybe[T] = Just(T) | Nothing

type Outcome[T, E] = Ok(T) | Err(E)
```

You can define your own:

```tml
type Either[L, R] = Left(L) | Right(R)

type Tree[T] =
    | Leaf(T)
    | Node { left: Heap[Tree[T]], value: T, right: Heap[Tree[T]] }
```

Using a generic enum:

```tml
func classify(n: I32) -> Either[Str, I32] {
    if n < 0 {
        Left("negative")
    } else {
        Right(n)
    }
}

func main() {
    when classify(-5) {
        Left(msg)  => println(msg),   // negative
        Right(val) => println(val.to_string()),
    }

    when classify(42) {
        Left(msg)  => println(msg),
        Right(val) => println(val.to_string()),  // 42
    }
}
```

## Monomorphization

When the compiler encounters a call to a generic function or a use of a generic type, it creates a concrete version for the specific types involved. This happens entirely at compile time and is transparent to the programmer.

For example, given:

```tml
func wrap[T](value: T) -> Maybe[T] {
    return Just(value)
}

let a = wrap(42)      // wrap[I32]
let b = wrap("hi")    // wrap[Str]
let c = wrap(3.14)    // wrap[F64]
```

The compiler generates three separate functions — `wrap__I32`, `wrap__Str`, and `wrap__F64` — each fully optimized for its concrete type. There is no boxing, no type erasure, and no runtime type checking.

The tradeoff is binary size: if a generic function is instantiated for many types, there will be many copies of its compiled code. In practice this is rarely a problem, and the compiler can eliminate dead instantiations through link-time optimization.

## Implementing Behaviors for Generic Types

You can implement a behavior for a generic type, optionally restricting when the implementation is available:

```tml
// Display is available for Pair[A, B] whenever both A and B implement Display
extend Pair[A, B] with Display where A: Display, B: Display {
    func to_string(this) -> Str {
        return "(" + this.first.to_string() + ", " + this.second.to_string() + ")"
    }
}

func main() {
    let p = Pair { first: 3, second: "hello" }
    println(p.to_string())  // (3, hello)
}
```

The `where A: Display, B: Display` clause expresses a conditional implementation: `Pair[A, B]` implements `Display` only when `A` and `B` also implement `Display`. See [Bounds and Where Clauses](ch06-02-bounds-and-where.md) for the full syntax.
