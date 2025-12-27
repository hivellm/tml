# An Example Program Using Structs

Let's write a program that calculates the area of a rectangle. We'll
start with simple variables and refactor to use structs.

## Starting with Variables

```tml
func main() {
    let width = 30
    let height = 50
    let area = width * height
    println("Area: ", area)  // Area: 1500
}
```

This works, but `width` and `height` aren't connected. Let's improve it.

## Using a Struct

```tml
type Rectangle {
    width: I32,
    height: I32,
}

func area(rect: Rectangle) -> I32 {
    rect.width * rect.height
}

func main() {
    let rect = Rectangle {
        width: 30,
        height: 50,
    }

    println("Area: ", area(rect))  // Area: 1500
}
```

Now the width and height are clearly related.

## Adding More Functionality

Let's add more operations:

```tml
type Rectangle {
    width: I32,
    height: I32,
}

func area(rect: Rectangle) -> I32 {
    rect.width * rect.height
}

func perimeter(rect: Rectangle) -> I32 {
    2 * (rect.width + rect.height)
}

func is_square(rect: Rectangle) -> Bool {
    rect.width == rect.height
}

func main() {
    let rect = Rectangle {
        width: 30,
        height: 50,
    }

    println("Width: ", rect.width)
    println("Height: ", rect.height)
    println("Area: ", area(rect))
    println("Perimeter: ", perimeter(rect))
    println("Is square: ", is_square(rect))
}
```

Output:
```
Width: 30
Height: 50
Area: 1500
Perimeter: 160
Is square: false
```

## Another Example: A Point System

```tml
type Point {
    x: I32,
    y: I32,
}

func distance_from_origin(p: Point) -> I32 {
    // Simplified: using Manhattan distance
    let abs_x = if p.x < 0 { -p.x } else { p.x }
    let abs_y = if p.y < 0 { -p.y } else { p.y }
    abs_x + abs_y
}

func add_points(a: Point, b: Point) -> Point {
    Point {
        x: a.x + b.x,
        y: a.y + b.y,
    }
}

func main() {
    let p1 = Point { x: 3, y: 4 }
    let p2 = Point { x: -1, y: 2 }

    println("P1 distance: ", distance_from_origin(p1))  // 7

    let p3 = add_points(p1, p2)
    println("P3: (", p3.x, ", ", p3.y, ")")  // P3: (2, 6)
}
```

## When to Use Structs

Use structs when:

1. **Data belongs together**: Width and height of a rectangle
2. **You need multiple instances**: Multiple users, multiple points
3. **You want to pass related data as a unit**: One parameter instead of many
4. **You want to add meaning**: `Point` is clearer than two `I32` values
