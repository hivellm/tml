# Object-Oriented Programming

TML supports C#-style object-oriented programming with classes, interfaces, inheritance, and virtual methods. This provides a familiar OOP paradigm alongside TML's behavior/trait system.

## Classes

Classes define types with fields, methods, and constructors:

```tml
class Point {
    x: F64
    y: F64

    func new(x: F64, y: F64) -> Point {
        return Point { x: x, y: y }
    }

    func distance(this, other: Point) -> F64 {
        let dx: F64 = this.x - other.x
        let dy: F64 = this.y - other.y
        return (dx * dx + dy * dy).sqrt()
    }
}

func main() {
    let p1: Point = Point::new(0.0, 0.0)
    let p2: Point = Point::new(3.0, 4.0)
    println("Distance: {p1.distance(p2)}")  // 5.0
}
```

### Member Visibility

Classes support three visibility levels:

| Modifier | Access |
|----------|--------|
| `private` | Only within the class |
| `protected` | Class and subclasses |
| `pub` | Everywhere |

```tml
class Account {
    private balance: F64
    protected owner: Str
    pub id: I64

    func new(owner: Str, initial: F64) -> Account {
        return Account {
            balance: initial,
            owner: owner,
            id: generate_id(),
        }
    }

    pub func deposit(mut this, amount: F64) {
        this.balance += amount
    }

    pub func get_balance(this) -> F64 {
        return this.balance
    }
}
```

### Static Members

Static fields and methods belong to the class, not instances:

```tml
class Counter {
    static count: I32 = 0

    static func increment() {
        Counter::count += 1
    }

    static func get_count() -> I32 {
        return Counter::count
    }
}

func main() {
    Counter::increment()
    Counter::increment()
    println("Count: {Counter::get_count()}")  // Count: 2
}
```

## Inheritance

Classes can extend other classes using `extends`:

```tml
class Animal {
    protected name: Str

    func new(name: Str) -> Animal {
        return Animal { name: name }
    }

    func speak(this) -> Str {
        return "..."
    }
}

class Dog extends Animal {
    private breed: Str

    func new(name: Str, breed: Str) -> Dog
        base: Animal::new(name)
    {
        return Dog { breed: breed }
    }

    override func speak(this) -> Str {
        return "Woof!"
    }

    func get_breed(this) -> Str {
        return this.breed
    }
}

func main() {
    let dog: Dog = Dog::new("Rex", "German Shepherd")
    println("{dog.name} says: {dog.speak()}")  // Rex says: Woof!
}
```

### Virtual and Override

Methods can be marked `virtual` to allow overriding in subclasses:

```tml
class Shape {
    virtual func area(this) -> F64 {
        return 0.0
    }

    virtual func perimeter(this) -> F64 {
        return 0.0
    }
}

class Rectangle extends Shape {
    width: F64
    height: F64

    override func area(this) -> F64 {
        return this.width * this.height
    }

    override func perimeter(this) -> F64 {
        return 2.0 * (this.width + this.height)
    }
}

class Circle extends Shape {
    radius: F64

    override func area(this) -> F64 {
        return 3.14159 * this.radius * this.radius
    }

    override func perimeter(this) -> F64 {
        return 2.0 * 3.14159 * this.radius
    }
}
```

### Abstract Classes

Abstract classes cannot be instantiated directly and may contain abstract methods:

```tml
abstract class Vehicle {
    protected speed: F64 = 0.0

    abstract func accelerate(mut this, amount: F64)

    virtual func stop(mut this) {
        this.speed = 0.0
    }

    func get_speed(this) -> F64 {
        return this.speed
    }
}

class Car extends Vehicle {
    override func accelerate(mut this, amount: F64) {
        this.speed += amount
        if this.speed > 200.0 {
            this.speed = 200.0  // Max speed limit
        }
    }
}

class Bicycle extends Vehicle {
    override func accelerate(mut this, amount: F64) {
        this.speed += amount * 0.5  // Slower acceleration
        if this.speed > 40.0 {
            this.speed = 40.0
        }
    }
}
```

### Sealed Classes

Sealed classes cannot be extended:

```tml
sealed class ImmutableConfig {
    pub name: Str
    pub value: I32

    func new(name: Str, value: I32) -> ImmutableConfig {
        return ImmutableConfig { name: name, value: value }
    }
}

// Error: Cannot extend sealed class
// class CustomConfig extends ImmutableConfig { }
```

## Interfaces

Interfaces define contracts that classes can implement:

```tml
interface Drawable {
    func draw(this)
    func get_bounds(this) -> (F64, F64, F64, F64)
}

interface Moveable {
    func move_to(mut this, x: F64, y: F64)
}

class Sprite implements Drawable, Moveable {
    x: F64
    y: F64
    width: F64
    height: F64

    func draw(this) {
        println("Drawing sprite at ({this.x}, {this.y})")
    }

    func get_bounds(this) -> (F64, F64, F64, F64) {
        return (this.x, this.y, this.width, this.height)
    }

    func move_to(mut this, x: F64, y: F64) {
        this.x = x
        this.y = y
    }
}
```

### Interface Inheritance

Interfaces can extend other interfaces:

```tml
interface Comparable[T] {
    func compare(this, other: T) -> I32
}

interface Orderable extends Comparable[This] {
    func less_than(this, other: This) -> Bool {
        return this.compare(other) < 0
    }

    func greater_than(this, other: This) -> Bool {
        return this.compare(other) > 0
    }

    func equals(this, other: This) -> Bool {
        return this.compare(other) == 0
    }
}
```

### Default Implementations

Interfaces can provide default implementations:

```tml
interface Printable {
    func to_string(this) -> Str

    func print(this) {
        println(this.to_string())
    }

    func print_with_prefix(this, prefix: Str) {
        println("{prefix}: {this.to_string()}")
    }
}

class User implements Printable {
    name: Str

    func to_string(this) -> Str {
        return "User({this.name})"
    }

    // print() and print_with_prefix() are inherited from Printable
}
```

## Combining Inheritance and Interfaces

Classes can extend a base class and implement multiple interfaces:

```tml
interface Serializable {
    func serialize(this) -> Str
}

interface Cloneable {
    func clone(this) -> This
}

abstract class Entity {
    protected id: I64

    func get_id(this) -> I64 {
        return this.id
    }
}

class Player extends Entity implements Serializable, Cloneable {
    name: Str
    score: I32

    func new(name: Str) -> Player {
        return Player {
            id: generate_id(),
            name: name,
            score: 0,
        }
    }

    func serialize(this) -> Str {
        return "{\"id\": {this.id}, \"name\": \"{this.name}\", \"score\": {this.score}}"
    }

    func clone(this) -> Player {
        return Player {
            id: generate_id(),
            name: this.name,
            score: this.score,
        }
    }
}
```

## Properties

Properties provide controlled access to fields with getter and setter:

```tml
class Temperature {
    private celsius: F64

    prop value: F64 {
        get {
            return this.celsius
        }
        set {
            if value < -273.15 {
                this.celsius = -273.15  // Absolute zero
            } else {
                this.celsius = value
            }
        }
    }

    prop fahrenheit: F64 {
        get {
            return this.celsius * 9.0 / 5.0 + 32.0
        }
        set {
            this.celsius = (value - 32.0) * 5.0 / 9.0
        }
    }
}

func main() {
    let mut temp: Temperature = Temperature { celsius: 0.0 }
    temp.value = 100.0
    println("Celsius: {temp.value}")       // 100.0
    println("Fahrenheit: {temp.fahrenheit}") // 212.0
}
```

## Constructors

Constructors use the `new` keyword and can call base constructors:

```tml
class Base {
    protected value: I32

    new(value: I32) {
        this.value = value
    }
}

class Derived extends Base {
    private extra: Str

    new(value: I32, extra: Str) : base(value) {
        this.extra = extra
    }
}

func main() {
    let obj: Derived = Derived::new(42, "hello")
}
```

## OOP vs Behaviors

TML offers both OOP (classes/interfaces) and Rust-style behaviors. Here's when to use each:

| Use OOP (Classes) When | Use Behaviors When |
|------------------------|-------------------|
| Modeling hierarchical relationships | Adding capabilities to existing types |
| Need protected members | Zero-cost abstractions |
| Migrating from C#/Java | Implementing operators |
| Complex object lifecycles | Extension methods |
| Need constructors with inheritance | Multiple unrelated implementations |

```tml
// OOP approach - inheritance hierarchy
class Animal { }
class Dog extends Animal { }
class Cat extends Animal { }

// Behavior approach - capability-based
behavior Speakable {
    func speak(this) -> Str
}

impl Speakable for Dog {
    func speak(this) -> Str { "Woof!" }
}

impl Speakable for Cat {
    func speak(this) -> Str { "Meow!" }
}
```

Both approaches can be combined:

```tml
interface Drawable {
    func draw(this)
}

behavior Printable {
    func to_string(this) -> Str
}

class Widget implements Drawable {
    func draw(this) {
        println("Drawing widget")
    }
}

impl Printable for Widget {
    func to_string(this) -> Str {
        return "Widget"
    }
}
```

## See Also

- [Structs](ch03-00-structs.md) - Lightweight data types
- [Enums](ch09-00-enums.md) - Sum types and pattern matching
- [Standard Library](ch10-00-standard-library.md) - Built-in classes and interfaces

---

*Previous: [ch14-00-json.md](ch14-00-json.md)*
*Next: [ch16-00-crypto.md](ch16-00-crypto.md)*
