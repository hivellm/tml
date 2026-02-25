# Enums

Enums allow you to define a type by enumerating its possible values. TML supports simple enums that map to integer values.

## Defining an Enum

Use the `type` keyword with curly braces to define an enum:

```tml
type Color {
    Red,
    Green,
    Blue
}

type Direction {
    North,
    South,
    East,
    West
}

type Status {
    Pending,
    Active,
    Completed,
    Failed
}
```

Each variant is automatically assigned an integer value starting from 0.

## Using Enum Values

Access enum variants using the `::` path syntax:

```tml
let color = Color::Red
let direction = Direction::North
let status = Status::Active
```

## Enum Values as Integers

Enum variants are integers under the hood:

```tml
let red = Color::Red       // 0
let green = Color::Green   // 1
let blue = Color::Blue     // 2
```

You can compare enum values with integers or with other enum variants:

```tml
if color == Color::Red {
    println("The color is red")
}

if color == 0 {
    println("The color is the first variant")
}
```

## Comparing Enum Values

Use comparison operators to compare enum values:

```tml
let color1 = Color::Red
let color2 = Color::Blue

if color1 == Color::Red {
    println("color1 is red")
}

if color1 != color2 {
    println("Colors are different")
}
```

## Mutable Enum Variables

You can change enum values using mutable variables:

```tml
var status = Status::Pending

// Later, change the status
status = Status::Active

if status == Status::Active {
    println("Status is now active")
}

// Mark as completed
status = Status::Completed
```

## Example: State Machine

Enums are excellent for representing states:

```tml
type TaskState {
    Created,
    Running,
    Paused,
    Completed,
    Failed
}

func process_task() -> I32 {
    var state = TaskState::Created

    // Start the task
    state = TaskState::Running

    // Simulate work...
    if state == TaskState::Running {
        println("Task is running...")
    }

    // Complete the task
    state = TaskState::Completed

    if state == TaskState::Completed {
        println("Task completed successfully!")
        return 0
    }

    return 1
}
```

## Example: Direction Navigation

```tml
type Direction {
    North,
    South,
    East,
    West
}

func describe_direction(dir: I32) {
    if dir == Direction::North {
        println("Going north")
    }
    if dir == Direction::South {
        println("Going south")
    }
    if dir == Direction::East {
        println("Going east")
    }
    if dir == Direction::West {
        println("Going west")
    }
}

func main() -> I32 {
    let dir = Direction::East
    describe_direction(dir)  // Prints: Going east
    return 0
}
```

## Pattern Matching with `when`

Use `when` for exhaustive pattern matching on enums:

```tml
type Direction {
    North,
    South,
    East,
    West
}

func describe_direction(dir: Direction) -> String {
    return when dir {
        Direction::North => "Going north",
        Direction::South => "Going south",
        Direction::East => "Going east",
        Direction::West => "Going west"
    }
}

func main() -> I32 {
    let dir = Direction::East
    println(describe_direction(dir))  // Going east
    return 0
}
```

This is cleaner than multiple `if` statements and ensures all cases are handled.

## Algebraic Data Types (ADTs)

TML supports enums with associated data (algebraic data types):

```tml
type Maybe[T] {
    Just(T),
    Nothing
}

type Outcome[T, E] {
    Ok(T),
    Err(E)
}
```

Match and extract data with `when`:

```tml
func get_value(m: Maybe[I32]) -> I32 {
    return when m {
        Just(value) => value,
        Nothing => 0
    }
}

func handle_result(r: Outcome[I32, String]) -> I32 {
    return when r {
        Ok(value) => value,
        Err(msg) => {
            println("Error: " + msg)
            -1
        }
    }
}
```

## Recursive Enums

Some data structures are naturally recursive -- a linked list node contains another list, a tree node contains child trees. In TML, an enum variant cannot directly contain its own type because the compiler would need infinite memory to lay out the value. You must introduce **indirection** using `Heap[T]`, which allocates the inner value on the heap through a pointer.

### Linked List

A classic linked list stores a value and a pointer to the rest of the list:

```tml
use core::alloc::heap::Heap

type IntList {
    Cons(I32, Heap[IntList]),
    Nil
}
```

`Cons` holds an `I32` value and a heap-allocated pointer to the next node. `Nil` marks the end of the list. Without `Heap`, the compiler would reject this definition because `IntList` would contain itself directly, requiring infinite size.

#### Constructing a list

Build lists from the tail forward, wrapping each recursive element in `Heap::new`:

```tml
// The list [1, 2, 3]
let list = IntList::Cons(1, Heap::new(
    IntList::Cons(2, Heap::new(
        IntList::Cons(3, Heap::new(IntList::Nil))
    ))
))
```

#### Pattern matching on a recursive enum

Use `when` to destructure recursive variants:

```tml
func head(list: IntList) -> I32 {
    return when list {
        Cons(val, rest) => val,
        Nil => 0
    }
}
```

The binding `val` receives the `I32` payload, and `rest` receives the `Heap[IntList]` tail.

### Binary Tree

A generic binary tree demonstrates recursive enums with type parameters:

```tml
use core::alloc::heap::Heap

type Tree[T] {
    Leaf(T),
    Branch(Heap[Tree[T]], Heap[Tree[T]])
}
```

Each `Branch` holds two heap-allocated child trees. `Leaf` holds a value with no children.

```tml
// Build a small tree:
//       *
//      / \
//    10   20
let tree = Tree::Branch(
    Heap::new(Tree::Leaf(10)),
    Heap::new(Tree::Leaf(20))
)
```

### Why `Heap[T]` is Required

An enum's size must be known at compile time. It is the size of its largest variant. If a variant directly contains the enum itself, the size equation becomes circular -- the type would need infinite memory.

`Heap[T]` solves this by storing a fixed-size pointer (8 bytes on 64-bit systems) instead of the value inline. The actual data lives on the heap.

**Without indirection, the compiler rejects the definition with error T085:**

```tml
// ERROR T085: recursive enum has infinite size
type Bad {
    Loop(Bad)
}
```

The fix is to wrap the self-reference in `Heap[T]`:

```tml
type Good {
    Loop(Heap[Good])
}
```

Other pointer types also provide valid indirection: `Shared[T]`, `Sync[T]`, `List[T]`, raw pointers (`*T`), and references (`ref T`).

## Limitations

TML enums support two forms:

- **Simple enums** (no associated data) are tagged integers. Variants map to values 0, 1, 2, ... and can be compared with `==` and `!=`.
- **Algebraic data types** (variants with payloads) support full pattern matching with `when` and data extraction via bindings.

Both forms can be stored in variables, passed to functions, and returned from functions. Recursive enum variants must use `Heap[T]` or another pointer type for indirection.
