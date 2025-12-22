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
let mut status = Status::Pending

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
    let mut state = TaskState::Created

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

## Limitations

Current TML enums are simple tagged enums (like C enums). They:

- Map directly to integer values (0, 1, 2, ...)
- Can be compared with `==` and `!=`
- Can be stored in variables and passed to functions
- Cannot currently hold associated data (planned for future)

For more complex data structures, consider using structs or behaviors.
