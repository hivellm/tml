# Defining Enums

Enums are defined with the `type` keyword, just like structs. The difference is that you
provide a set of variants rather than a set of fields. Each variant is separated by `|`.

## Simple Enums

A simple enum lists named variants with no attached data. Each variant becomes a value of
the enum type:

```tml
type Direction = North | South | East | West

type Status = Pending | Active | Completed | Failed

type LogLevel = Trace | Debug | Info | Warn | Error
```

Variant names use `PascalCase` by convention.

To use a variant, write the enum type name followed by a dot and the variant name:

```tml
func main() {
    let heading = Direction.North
    let current = Status.Active

    println(heading.to_string())   // North
    println(current.to_string())   // Active
}
```

Simple enum variants are compared with `==` and `!=`:

```tml
func describe_status(s: Status) -> Str {
    if s == Status.Completed {
        return "done"
    }
    if s == Status.Failed {
        return "error"
    }
    return "in progress"
}
```

Or more cleanly with `when`:

```tml
func describe_status(s: Status) -> Str {
    when s {
        Status.Pending    => "waiting to start",
        Status.Active     => "running",
        Status.Completed  => "done",
        Status.Failed     => "error",
    }
}
```

### Mutable Enum Variables

Enum variables can be reassigned when declared with `var`:

```tml
func main() {
    var state = Status.Pending
    println(describe_status(state))  // waiting to start

    state = Status.Active
    println(describe_status(state))  // running

    state = Status.Completed
    println(describe_status(state))  // done
}
```

## Enums with Associated Data

Variants can carry data. Each variant defines its own payload, which can be different from
every other variant:

```tml
type Shape =
    | Circle { radius: F64 }
    | Rectangle { width: F64, height: F64 }
    | Triangle { base: F64, height: F64 }
```

The payload uses struct-style syntax: field names and types inside curly braces. To construct
a variant with data, provide the field values:

```tml
func main() {
    let c = Shape.Circle { radius: 5.0 }
    let r = Shape.Rectangle { width: 10.0, height: 4.0 }
    let t = Shape.Triangle { base: 6.0, height: 3.0 }

    println(area(c).to_string())  // 78.53975
    println(area(r).to_string())  // 40.0
    println(area(t).to_string())  // 9.0
}

func area(s: Shape) -> F64 {
    when s {
        Circle { radius }        => 3.14159 * radius * radius,
        Rectangle { width, height } => width * height,
        Triangle { base, height }   => 0.5 * base * height,
    }
}
```

When you match a variant with struct-like payload, the field names become local bindings inside
that arm. You can rename them if needed:

```tml
when s {
    Circle { radius: r } => r * r * 3.14159,
    // ...
}
```

## Tuple Variants

Variants can also carry unnamed (positional) data, written with parentheses instead of curly
braces:

```tml
type Message =
    | Text(Str)
    | Number(I32)
    | Coordinate(F64, F64)
    | Empty
```

Construct tuple variants by passing the payloads in order:

```tml
let msg1 = Message.Text("hello")
let msg2 = Message.Number(42)
let msg3 = Message.Coordinate(1.5, 2.5)
let msg4 = Message.Empty
```

Match and extract the payloads:

```tml
func describe(m: Message) -> Str {
    when m {
        Text(s)      => "text: " + s,
        Number(n)    => "number: " + n.to_string(),
        Coordinate(x, y) => "at " + x.to_string() + ", " + y.to_string(),
        Empty        => "nothing",
    }
}
```

## Mixed Variant Styles

A single enum can freely mix no-data variants, struct variants, and tuple variants:

```tml
type Event =
    | MouseClick { x: F64, y: F64 }
    | KeyPress(Str)
    | Scroll { delta: F64 }
    | WindowClose
    | Resize(I32, I32)
```

Use whichever style is clearest for each variant.

## Generic Enums

Enums can be parameterized over types. The standard library's `Maybe[T]` and `Outcome[T, E]`
are generic enums:

```tml
type Maybe[T] = Just(T) | Nothing

type Outcome[T, E] = Ok(T) | Err(E)
```

You can define your own generic enums in the same way:

```tml
type Pair[A, B] = Both(A, B) | First(A) | Second(B) | Neither

type Tree[T] =
    | Leaf(T)
    | Node { left: Heap[Tree[T]], value: T, right: Heap[Tree[T]] }
```

The type parameters are available in every variant's payload.

### Using Maybe and Outcome

`Maybe[T]` represents a value that may or may not be present. Use it instead of null:

```tml
func find_user(id: I32, users: ref List[User]) -> Maybe[User] {
    for u in users {
        if u.id == id {
            return Just(u)
        }
    }
    return Nothing
}

func main() {
    when find_user(42, ref users) {
        Just(u)  => println("Found: " + u.name),
        Nothing  => println("Not found"),
    }
}
```

`Outcome[T, E]` represents either a successful value or an error. Use it for operations
that can fail:

```tml
func parse_port(input: Str) -> Outcome[I32, Str] {
    when input.parse_i32() {
        Ok(n) if n > 0 and n <= 65535 => Ok(n),
        Ok(_)  => Err("port out of range"),
        Err(_) => Err("not a number"),
    }
}
```

## Recursive Enums

Some data structures are naturally recursive: a linked list node contains another list, a tree
node contains child trees. An enum variant cannot directly contain its own type because the
compiler needs to know the size of the type at compile time — and a self-referential size
equation has no solution.

The solution is to introduce indirection using `Heap[T]`, which stores a pointer to a
heap-allocated value. The pointer has a fixed size (8 bytes on 64-bit systems), breaking the
circular dependency:

```tml
use core::alloc::heap::Heap

type IntList = Cons(I32, Heap[IntList]) | Nil
```

`Cons` holds an integer and a heap-allocated pointer to the rest of the list. `Nil` marks
the end.

### Building a Linked List

Construct the list from the tail backward, wrapping each recursive position in `Heap::new`:

```tml
// [1, 2, 3]
let list = IntList.Cons(1, Heap::new(
    IntList.Cons(2, Heap::new(
        IntList.Cons(3, Heap::new(
            IntList.Nil
        ))
    ))
))
```

### Traversing a Linked List

Use `when` to destructure each node recursively:

```tml
func sum(list: IntList) -> I32 {
    when list {
        Cons(value, rest) => value + sum(*rest),
        Nil               => 0,
    }
}

func length(list: ref IntList) -> I32 {
    when list {
        Cons(_, rest) => 1 + length(ref *rest),
        Nil           => 0,
    }
}
```

### Binary Tree

A generic binary tree is another classic recursive structure:

```tml
use core::alloc::heap::Heap

type Tree[T] =
    | Leaf(T)
    | Branch(Heap[Tree[T]], Heap[Tree[T]])
```

```tml
// Build a tree:  Branch
//               /       \
//           Leaf(1)   Leaf(2)
let tree = Tree.Branch(
    Heap::new(Tree.Leaf(1)),
    Heap::new(Tree.Leaf(2)),
)
```

```tml
func depth[T](t: ref Tree[T]) -> I32 {
    when t {
        Leaf(_)          => 1,
        Branch(left, right) => {
            let l = depth(ref *left)
            let r = depth(ref *right)
            1 + if l > r { l } else { r }
        },
    }
}
```

### Why Heap Is Required

Without `Heap`, the compiler rejects a self-referential definition with error T085:

```tml
// ERROR T085: recursive type has infinite size
type Bad = Loop(Bad) | Stop
```

Wrapping the self-reference in `Heap[T]` gives the compiler a concrete, pointer-sized slot
to use:

```tml
// OK
type Good = Loop(Heap[Good]) | Stop
```

Other pointer types also work as indirection: `Shared[T]`, `Sync[T]`, raw pointers (`*T`),
and references (`ref T`).

## When to Use Each Form

| Form | When to Use |
|------|-------------|
| Simple enum | Named states or options with no extra data |
| Struct variant | Variant payload has named fields (two or more related values) |
| Tuple variant | Variant payload has positional fields (one value, or positional pairs) |
| No-data variant | Represents absence or a sentinel state |
| Generic enum | Payload type varies per use site |
| Recursive enum | Tree-shaped or list-shaped data structures |
