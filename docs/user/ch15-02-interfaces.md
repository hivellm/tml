# Interfaces

An interface defines a named set of method signatures that a class can agree to
implement. When a class implements an interface, it promises that instances of that
class will support all methods the interface declares. Code that accepts an interface
type can then work with any implementing class, without knowing the concrete type.

## Defining an Interface

An interface declaration uses the `interface` keyword, followed by the interface name
and a body containing method signatures:

```tml
interface Serializable {
    func serialize(this) -> Str
    func deserialize(data: Str) -> Self
}
```

Method signatures in an interface have no body. The implementing class provides the
body. The special type `Self` refers to whichever concrete type is implementing the
interface at a given site.

## Implementing an Interface

A class opts into an interface using the `implements` keyword:

```tml
class User implements Serializable {
    pub name: Str
    pub age: I32

    func new(name: Str, age: I32) -> User {
        return User { name: name, age: age }
    }

    func serialize(this) -> Str {
        return this.name + ":" + this.age.to_string()
    }

    func deserialize(data: Str) -> User {
        let parts = data.split(":")
        let name = parts.get(0).unwrap_or("")
        let age = parts.get(1).unwrap_or("0").parse_i32().unwrap_or(0)
        return User { name: name, age: age }
    }
}
```

The compiler verifies that `User` provides implementations for every method declared in
`Serializable`. A missing implementation is a compile error.

## Default Method Implementations

An interface can provide a default implementation for a method. Implementing classes
inherit the default and may override it if a different behavior is needed:

```tml
interface Printable {
    func to_string(this) -> Str

    func print(this) {
        println(this.to_string())
    }

    func print_with_label(this, label: Str) {
        println("{label}: {this.to_string()}")
    }
}

class Product implements Printable {
    pub name: Str
    pub price: F64

    func new(name: Str, price: F64) -> Product {
        return Product { name: name, price: price }
    }

    func to_string(this) -> Str {
        return this.name + " ($" + this.price.to_string() + ")"
    }

    // print() and print_with_label() are inherited from Printable
}

func main() {
    let p = Product::new("Keyboard", 89.99)
    p.print()                          // Keyboard ($89.99)
    p.print_with_label("Item")        // Item: Keyboard ($89.99)
}
```

Only `to_string` is required. The other two methods are inherited from the interface
with no extra code in `Product`.

A class can override a default implementation by providing its own:

```tml
class VerboseProduct implements Printable {
    pub name: Str
    pub price: F64
    pub sku: Str

    func new(name: Str, price: F64, sku: Str) -> VerboseProduct {
        return VerboseProduct { name: name, price: price, sku: sku }
    }

    func to_string(this) -> Str {
        return "[{this.sku}] {this.name} (${this.price})"
    }

    override func print(this) {
        println("=== PRODUCT ===")
        println(this.to_string())
        println("===============")
    }
}
```

## Generic Interfaces

Interfaces can be parameterized with type parameters using `[T]` syntax:

```tml
interface Comparable[T] {
    func compare_to(this, other: ref T) -> I32
}

class Score implements Comparable[Score] {
    pub points: I32

    func new(points: I32) -> Score {
        return Score { points: points }
    }

    func compare_to(this, other: ref Score) -> I32 {
        return this.points - other.points
    }
}

func main() {
    let a = Score::new(85)
    let b = Score::new(92)

    let cmp = a.compare_to(ref b)
    if cmp < 0 {
        println("a is lower")
    } else if cmp > 0 {
        println("a is higher")
    } else {
        println("tied")
    }
    // a is lower
}
```

The `Comparable[T]` interface parameterizes the type being compared against. When `Score`
implements `Comparable[Score]`, the `T` in the interface is bound to `Score` throughout.

## The `This` Keyword in Interfaces

The special type `This` inside an interface body refers to the implementing class. It is
distinct from a type parameter because it is automatically bound to the concrete
implementing type without being declared explicitly:

```tml
interface Cloneable {
    func clone(this) -> This
}

class Config implements Cloneable {
    pub host: Str
    pub port: I32

    func new(host: Str, port: I32) -> Config {
        return Config { host: host, port: port }
    }

    func clone(this) -> Config {
        return Config { host: this.host, port: this.port }
    }
}

func main() {
    let original = Config::new("localhost", 8080)
    let copy = original.clone()
    println("{copy.host}:{copy.port}")  // localhost:8080
}
```

`This` is useful when an interface method must return or receive the concrete type of the
implementor, rather than a fixed type.

## Interface Inheritance

An interface can extend one or more other interfaces. An implementing class must satisfy
the requirements of all interfaces in the hierarchy:

```tml
interface Readable {
    func read(this) -> Str
}

interface Writable {
    func write(mut this, data: Str)
}

interface ReadWrite extends Readable, Writable {
    func seek(mut this, position: I32)
}

class FileBuffer implements ReadWrite {
    private data: Str
    private cursor: I32

    func new() -> FileBuffer {
        return FileBuffer { data: "", cursor: 0 }
    }

    func read(this) -> Str {
        return this.data.slice_from(this.cursor)
    }

    func write(mut this, incoming: Str) {
        this.data = this.data + incoming
    }

    func seek(mut this, position: I32) {
        this.cursor = position
    }
}
```

`FileBuffer` must implement `read`, `write`, and `seek` — all three, because `ReadWrite`
extends both `Readable` and `Writable`.

Interface inheritance can also enrich a base interface with default implementations:

```tml
interface Comparable[T] {
    func compare_to(this, other: ref T) -> I32
}

interface Orderable extends Comparable[This] {
    func less_than(this, other: ref This) -> Bool {
        return this.compare_to(other) < 0
    }

    func greater_than(this, other: ref This) -> Bool {
        return this.compare_to(other) > 0
    }

    func equals(this, other: ref This) -> Bool {
        return this.compare_to(other) == 0
    }

    func clamp(this, low: ref This, high: ref This) -> This {
        if this.less_than(low) {
            return low
        }
        if this.greater_than(high) {
            return high
        }
        return this
    }
}
```

A class that implements `Orderable` only needs to supply `compare_to`. All four derived
methods — `less_than`, `greater_than`, `equals`, and `clamp` — come for free.

## Implementing Multiple Interfaces

A class can implement any number of interfaces by separating them with commas in the
`implements` clause:

```tml
interface Persistable {
    func save(this) -> Bool
    func load(mut this) -> Bool
}

interface Validatable {
    func validate(this) -> Bool
    func validation_errors(this) -> List[Str]
}

interface Auditable {
    func created_at(this) -> Str
    func updated_at(this) -> Str
}

class Order implements Persistable, Validatable, Auditable {
    pub id: I64
    pub customer: Str
    pub total: F64
    private created: Str
    private updated: Str

    func new(id: I64, customer: Str, total: F64) -> Order {
        return Order {
            id: id,
            customer: customer,
            total: total,
            created: "2026-03-17",
            updated: "2026-03-17",
        }
    }

    func save(this) -> Bool {
        // persist to storage
        return true
    }

    func load(mut this) -> Bool {
        // load from storage
        return true
    }

    func validate(this) -> Bool {
        return this.customer != "" and this.total >= 0.0
    }

    func validation_errors(this) -> List[Str] {
        var errors: List[Str] = List::new()
        if this.customer == "" {
            errors.push("Customer name is required")
        }
        if this.total < 0.0 {
            errors.push("Total cannot be negative")
        }
        return errors
    }

    func created_at(this) -> Str {
        return this.created
    }

    func updated_at(this) -> Str {
        return this.updated
    }
}
```

## Combining Class Inheritance with Interfaces

A class can both extend a base class and implement interfaces. The `extends` clause
comes before `implements`:

```tml
interface Serializable {
    func serialize(this) -> Str
}

interface Displayable {
    func display_name(this) -> Str
}

abstract class Entity {
    protected id: I64

    func new(id: I64) -> Entity {
        return Entity { id: id }
    }

    pub func get_id(this) -> I64 {
        return this.id
    }

    abstract func type_name(this) -> Str
}

class Player extends Entity implements Serializable, Displayable {
    pub username: Str
    pub score: I32

    func new(id: I64, username: Str) -> Player {
        return Player { id: id, username: username, score: 0 }
    }

    override func type_name(this) -> Str {
        return "Player"
    }

    func serialize(this) -> Str {
        return "{\"id\":" + this.id.to_string()
            + ",\"username\":\"" + this.username + "\""
            + ",\"score\":" + this.score.to_string() + "}"
    }

    func display_name(this) -> Str {
        return this.username + " #" + this.id.to_string()
    }
}

func main() {
    let p = Player::new(1001, "atlas")
    println(p.display_name())  // atlas #1001
    println(p.serialize())     // {"id":1001,"username":"atlas","score":0}
    println(p.type_name())     // Player
}
```

## Interfaces vs Behaviors

Interfaces and behaviors serve related but distinct roles. Understanding the difference
helps you choose the right tool:

| Aspect | Interface | Behavior |
|---|---|---|
| Implemented by | Classes only | Structs, enums, primitives, classes |
| Retroactive impl | No — must be in the `implements` clause | Yes — `impl Behavior for ExistingType` |
| Dispatch | Virtual (vtable) | Compile-time (monomorphized) or `dyn` |
| Default methods | Yes | Yes |
| Generic params | Yes | Yes |
| `Self` / `This` | `Self` | `Self` |
| Inheritance | Yes — interfaces extend interfaces | No hierarchy |

Use behaviors when you want zero-cost abstractions, when you need to add a capability to
types you did not write, or when you are working primarily with structs and enums. Use
interfaces when you are building a class hierarchy and want to enforce a shared API
across related classes, or when the virtual-dispatch semantics of interfaces better match
your design.

Both can appear in the same codebase. A class can implement a behavior as well as an
interface:

```tml
behavior Display {
    func to_string(this) -> Str
}

interface Configurable {
    func apply(mut this, key: Str, value: Str)
}

class AppSettings implements Configurable {
    pub theme: Str
    pub language: Str

    func new() -> AppSettings {
        return AppSettings { theme: "light", language: "en" }
    }

    func apply(mut this, key: Str, value: Str) {
        when key {
            "theme" => this.theme = value,
            "language" => this.language = value,
            _ => {},
        }
    }
}

impl Display for AppSettings {
    func to_string(this) -> Str {
        return "theme=" + this.theme + ", language=" + this.language
    }
}

func main() {
    var settings = AppSettings::new()
    settings.apply("theme", "dark")
    println(settings.to_string())  // theme=dark, language=en
}
```

`AppSettings` uses `implements Configurable` for the OOP contract and `impl Display for
AppSettings` for the behavior — the two coexist without conflict.

---

*Previous: [Classes and Inheritance](ch15-01-classes.md)*
*Next: [Concurrency](ch16-00-concurrency.md)*
