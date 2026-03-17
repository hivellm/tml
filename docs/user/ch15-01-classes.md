# Classes and Inheritance

A class in TML bundles fields and methods into a single declaration, and can extend
another class to inherit its members. Classes support visibility modifiers, virtual
dispatch, abstract contracts, and constructors that call into base classes.

## Defining a Class

A class declaration uses the `class` keyword, followed by the class name and a body
containing fields and methods:

```tml
class Point {
    x: F64
    y: F64

    func new(x: F64, y: F64) -> Point {
        return Point { x: x, y: y }
    }

    func distance_to(this, other: ref Point) -> F64 {
        let dx = this.x - other.x
        let dy = this.y - other.y
        return (dx * dx + dy * dy).sqrt()
    }

    func to_string(this) -> Str {
        return "({this.x}, {this.y})"
    }
}

func main() {
    let p1 = Point::new(0.0, 0.0)
    let p2 = Point::new(3.0, 4.0)
    println("Distance: {p1.distance_to(ref p2)}")  // Distance: 5.0
}
```

Unlike structs, which separate data from behavior using `extend` blocks, a class
declaration contains everything in one place. Methods that take `this` as their first
parameter are instance methods. Methods without `this` are static.

Static methods are called using the `::` syntax on the type name rather than on an
instance.

## Access Modifiers

Every field and method in a class has a visibility level. Three modifiers are available:

| Modifier | Accessible from |
|---|---|
| `private` | Only within the class itself (default) |
| `protected` | Within the class and any subclass |
| `pub` | Anywhere |

When no modifier is written, visibility is `private`.

```tml
class BankAccount {
    private balance: F64
    protected owner: Str
    pub account_number: Str

    func new(owner: Str, initial: F64, number: Str) -> BankAccount {
        return BankAccount {
            balance: initial,
            owner: owner,
            account_number: number,
        }
    }

    pub func deposit(mut this, amount: F64) {
        if amount > 0.0 {
            this.balance += amount
        }
    }

    pub func withdraw(mut this, amount: F64) -> Bool {
        if amount > 0.0 and amount <= this.balance {
            this.balance -= amount
            return true
        }
        return false
    }

    pub func get_balance(this) -> F64 {
        return this.balance
    }

    private func log_transaction(this, kind: Str, amount: F64) {
        println("[{this.account_number}] {kind}: {amount}")
    }
}
```

The `balance` field is `private` — no code outside `BankAccount` can read or write it
directly. The `owner` field is `protected`, available to `BankAccount` and any class
that extends it. The `account_number` field is `pub`, readable anywhere.

Method visibility follows the same rules. A `pub` method is part of the public API.
A `private` method is an implementation detail. A `protected` method is available to
subclasses for overriding or calling.

## Static Members

Static members belong to the class itself, not to any instance. Declare them with the
`static` keyword:

```tml
class IdGenerator {
    static next_id: I64 = 0

    static func generate() -> I64 {
        IdGenerator::next_id += 1
        return IdGenerator::next_id
    }

    static func reset() {
        IdGenerator::next_id = 0
    }
}

func main() {
    let id1 = IdGenerator::generate()
    let id2 = IdGenerator::generate()
    let id3 = IdGenerator::generate()
    println("{id1}, {id2}, {id3}")  // 1, 2, 3
    IdGenerator::reset()
    println("{IdGenerator::generate()}")  // 1
}
```

Static fields are initialized to the value provided in the declaration. Static methods
do not receive a `this` parameter and cannot access instance fields.

## Inheritance

A class extends another class using the `extends` keyword. The subclass inherits all
`pub` and `protected` fields and methods from the base class:

```tml
class Animal {
    protected name: Str
    protected age: I32

    func new(name: Str, age: I32) -> Animal {
        return Animal { name: name, age: age }
    }

    virtual func speak(this) -> Str {
        return this.name + " makes a sound"
    }

    func describe(this) -> Str {
        return this.name + " (age " + this.age.to_string() + "): " + this.speak()
    }
}

class Dog extends Animal {
    private breed: Str

    func new(name: Str, age: I32, breed: Str) -> Dog {
        return Dog { name: name, age: age, breed: breed }
    }

    override func speak(this) -> Str {
        return this.name + " says: Woof!"
    }

    func get_breed(this) -> Str {
        return this.breed
    }
}

class Cat extends Animal {
    func new(name: Str, age: I32) -> Cat {
        return Cat { name: name, age: age }
    }

    override func speak(this) -> Str {
        return this.name + " says: Meow!"
    }
}

func main() {
    let dog = Dog::new("Rex", 3, "German Shepherd")
    let cat = Cat::new("Luna", 2)

    println(dog.describe())  // Rex (age 3): Rex says: Woof!
    println(cat.describe())  // Luna (age 2): Luna says: Meow!
    println(dog.get_breed()) // German Shepherd
}
```

The `Dog` class inherits the `name`, `age`, and `describe` members from `Animal`. It adds
its own `breed` field and overrides `speak`.

### Constructors and Base Initialization

When a subclass has fields beyond those of its base class, its constructor must supply
values for all inherited fields. The `base:` clause in a constructor calls the base class
constructor to handle the parent's initialization:

```tml
class Vehicle {
    protected make: Str
    protected model: Str
    protected year: I32

    func new(make: Str, model: Str, year: I32) -> Vehicle {
        return Vehicle { make: make, model: model, year: year }
    }

    func label(this) -> Str {
        return this.year.to_string() + " " + this.make + " " + this.model
    }
}

class ElectricVehicle extends Vehicle {
    private battery_kwh: F64
    private range_km: I32

    func new(make: Str, model: Str, year: I32, battery_kwh: F64, range_km: I32) -> ElectricVehicle
        base: Vehicle::new(make, model, year)
    {
        return ElectricVehicle { battery_kwh: battery_kwh, range_km: range_km }
    }

    func efficiency(this) -> F64 {
        return this.battery_kwh / this.range_km.to_f64()
    }
}

func main() {
    let ev = ElectricVehicle::new("Rivian", "R1T", 2024, 135.0, 507)
    println(ev.label())                       // 2024 Rivian R1T
    println(ev.efficiency().to_string())      // 0.2663...
}
```

The `base:` clause appears between the return type annotation and the opening brace of
the constructor body. It names the base class constructor and provides its arguments.
The body of `ElectricVehicle::new` then initializes only the fields unique to the
subclass.

## Virtual Methods and Override

A method marked `virtual` in a base class can be replaced in a subclass using `override`.
Virtual dispatch means the correct implementation is selected at runtime based on the
actual type of the object:

```tml
class Renderer {
    virtual func render(this, content: Str) -> Str {
        return content
    }

    virtual func render_list(this, items: List[Str]) -> Str {
        var result = ""
        loop item in items {
            result = result + this.render(item) + "\n"
        }
        return result
    }
}

class HtmlRenderer extends Renderer {
    override func render(this, content: Str) -> Str {
        return "<p>" + content + "</p>"
    }
}

class MarkdownRenderer extends Renderer {
    override func render(this, content: Str) -> Str {
        return "> " + content
    }
}
```

The `render_list` method in `Renderer` calls `this.render(item)`. Because `render` is
virtual, a subclass that overrides it will have its version called — even from within the
base class method. This is runtime polymorphism.

A non-virtual method cannot be overridden. If you declare a method without `virtual`,
subclasses can define a method of the same name, but it will shadow rather than override
the base method; calls through a base-class reference will use the base version.

## Abstract Classes

An abstract class defines a contract that subclasses must fulfill. It cannot be
instantiated directly. Abstract methods have no body — the subclass is required to provide
one:

```tml
abstract class Shape {
    pub name: Str

    abstract func area(this) -> F64
    abstract func perimeter(this) -> F64

    // Concrete method that uses the abstract ones
    func describe(this) -> Str {
        return this.name
            + ": area=" + this.area().to_string()
            + ", perimeter=" + this.perimeter().to_string()
    }
}

class Rectangle extends Shape {
    width: F64
    height: F64

    func new(width: F64, height: F64) -> Rectangle {
        return Rectangle { name: "Rectangle", width: width, height: height }
    }

    override func area(this) -> F64 {
        return this.width * this.height
    }

    override func perimeter(this) -> F64 {
        return 2.0 * (this.width + this.height)
    }
}

class Circle extends Shape {
    radius: F64

    func new(radius: F64) -> Circle {
        return Circle { name: "Circle", radius: radius }
    }

    override func area(this) -> F64 {
        return 3.14159265 * this.radius * this.radius
    }

    override func perimeter(this) -> F64 {
        return 2.0 * 3.14159265 * this.radius
    }
}

func main() {
    let r = Rectangle::new(4.0, 5.0)
    let c = Circle::new(3.0)
    println(r.describe())  // Rectangle: area=20.0, perimeter=18.0
    println(c.describe())  // Circle: area=28.274..., perimeter=18.849...
}
```

Abstract classes can also contain virtual methods with default implementations alongside
abstract methods. Subclasses may choose to override the virtual ones, but must implement
all abstract ones:

```tml
abstract class Logger {
    abstract func write(this, message: Str)

    virtual func info(this, message: Str) {
        this.write("[INFO] " + message)
    }

    virtual func warn(this, message: Str) {
        this.write("[WARN] " + message)
    }

    virtual func error(this, message: Str) {
        this.write("[ERROR] " + message)
    }
}

class ConsoleLogger extends Logger {
    override func write(this, message: Str) {
        println(message)
    }
}

class PrefixedLogger extends Logger {
    private prefix: Str

    func new(prefix: Str) -> PrefixedLogger {
        return PrefixedLogger { prefix: prefix }
    }

    override func write(this, message: Str) {
        println(this.prefix + " " + message)
    }
}
```

`ConsoleLogger` and `PrefixedLogger` need only implement `write`. They inherit the `info`,
`warn`, and `error` methods from `Logger` at no cost.

## Sealed Classes

A class marked `sealed` cannot be extended. It can be instantiated and used normally, but
no subclass may be declared for it:

```tml
sealed class DatabaseConfig {
    pub host: Str
    pub port: I32
    pub database: Str
    private password: Str

    func new(host: Str, port: I32, database: Str, password: Str) -> DatabaseConfig {
        return DatabaseConfig {
            host: host,
            port: port,
            database: database,
            password: password,
        }
    }

    pub func connection_string(this) -> Str {
        return this.host + ":" + this.port.to_string() + "/" + this.database
    }
}

// This would be a compile error:
// class CustomConfig extends DatabaseConfig { }
```

Sealing a class is useful when the internal state invariants must be controlled entirely
by the class itself, or when you want to prevent the accidental modification of behavior
in security-sensitive types.

## Properties

Properties provide a controlled access pattern for class fields, letting you attach
validation or transformation logic to reads and writes without changing the call site:

```tml
class Temperature {
    private celsius_value: F64

    func new(celsius: F64) -> Temperature {
        return Temperature { celsius_value: celsius }
    }

    prop celsius: F64 {
        get {
            return this.celsius_value
        }
        set {
            if value < -273.15 {
                this.celsius_value = -273.15
            } else {
                this.celsius_value = value
            }
        }
    }

    prop fahrenheit: F64 {
        get {
            return this.celsius_value * 9.0 / 5.0 + 32.0
        }
        set {
            this.celsius_value = (value - 32.0) * 5.0 / 9.0
        }
    }

    prop kelvin: F64 {
        get {
            return this.celsius_value + 273.15
        }
        // No setter — kelvin is read-only through this property
    }
}

func main() {
    var temp = Temperature::new(0.0)

    temp.celsius = 100.0
    println(temp.fahrenheit.to_string())  // 212.0
    println(temp.kelvin.to_string())      // 373.15

    temp.fahrenheit = 32.0
    println(temp.celsius.to_string())     // 0.0
}
```

A property with only a `get` block is read-only from outside the class. The `set` block
receives the incoming value as the implicit `value` variable.

## A Complete Example: Content Node Hierarchy

The following example uses abstract classes, inheritance, virtual methods, and access
modifiers together in a realistic scenario — a document model:

```tml
abstract class ContentNode {
    protected id: Str
    protected children: List[ContentNode]

    func new(id: Str) -> ContentNode {
        return ContentNode { id: id, children: List::new() }
    }

    pub func get_id(this) -> Str {
        return this.id
    }

    pub func add_child(mut this, child: ContentNode) {
        this.children.push(child)
    }

    pub func child_count(this) -> I32 {
        return this.children.len()
    }

    abstract func render(this) -> Str
}

class TextNode extends ContentNode {
    private content: Str

    func new(id: Str, content: Str) -> TextNode {
        return TextNode { id: id, children: List::new(), content: content }
    }

    override func render(this) -> Str {
        return this.content
    }
}

class HeadingNode extends ContentNode {
    private level: I32
    private content: Str

    func new(id: Str, level: I32, content: Str) -> HeadingNode {
        return HeadingNode { id: id, children: List::new(), level: level, content: content }
    }

    override func render(this) -> Str {
        let prefix = loop i in 0..this.level { "#" }.collect_string()
        return prefix + " " + this.content
    }
}

class SectionNode extends ContentNode {
    func new(id: Str) -> SectionNode {
        return SectionNode { id: id, children: List::new() }
    }

    override func render(this) -> Str {
        var parts: List[Str] = List::new()
        loop child in this.children {
            parts.push(child.render())
        }
        return parts.join("\n")
    }
}
```

`ContentNode` declares the common structure and the `render` contract. Each concrete
subclass provides its own rendering logic while inheriting the child-management methods
for free.

---

*Previous: [Object-Oriented Programming](ch15-00-oop.md)*
*Next: [Interfaces](ch15-02-interfaces.md)*
