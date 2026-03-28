# Proposal: C#-Style Object-Oriented Programming

## Status
- **Created**: 2025-12-30
- **Status**: Draft
- **Priority**: High

## Why

TML currently uses Rust-style composition with behaviors (traits) and impl blocks. While powerful, this model is unfamiliar to developers coming from C#, Java, TypeScript, or Python. Adding C#-style OOP features would:

1. **Lower barrier to entry** - Familiar syntax for majority of developers
2. **Enable classic patterns** - Factory, Strategy, Observer, etc. work naturally
3. **IDE support** - Class hierarchies are easier to visualize and navigate
4. **LLM generation** - LLMs trained on C#/Java code can generate TML more easily

### Current State vs Proposed

```tml
// CURRENT: Rust-style with behaviors
behavior Drawable {
    func draw(this)
}

type Circle {
    radius: F64
}

impl Drawable for Circle {
    func draw(this) { ... }
}
```

```tml
// PROPOSED: C#-style with classes
interface Drawable {
    func draw(this)
}

class Circle extends Shape implements Drawable {
    radius: F64

    func new(radius: F64) -> Circle {
        return Circle { radius: radius }
    }

    func draw(this) { ... }

    override func area(this) -> F64 {
        return 3.14159 * this.radius * this.radius
    }
}
```

## What Changes

### New Keywords
- `class` - Define a class (can have inheritance)
- `interface` - Define an interface (like C# interface)
- `extends` - Single inheritance from another class
- `implements` - Implement one or more interfaces
- `override` - Override a virtual/abstract method
- `virtual` - Method can be overridden
- `abstract` - Class/method must be implemented
- `sealed` - Class cannot be inherited
- `namespace` - Group related types
- `new` - Constructor (replaces manual struct creation)
- `base` - Call parent class method
- `protected` - Visible to subclasses only
- `private` - Visible only within class
- `static` - Class-level member

### Class Features
```tml
namespace Game.Entities {

abstract class Entity {
    protected id: I64
    protected position: Vec2

    func new(id: I64, pos: Vec2) -> Entity {
        return Entity { id: id, position: pos }
    }

    virtual func update(mut this, dt: F64) {
        // Default implementation
    }

    abstract func render(this)
}

class Player extends Entity implements Controllable, Damageable {
    private health: I32
    private inventory: List[Item]

    func new(id: I64, pos: Vec2) -> Player {
        return Player {
            base: Entity::new(id, pos),
            health: 100,
            inventory: List::new()
        }
    }

    override func update(mut this, dt: F64) {
        base.update(dt)  // Call parent
        this.process_input()
    }

    override func render(this) {
        draw_sprite(this.position, "player.png")
    }

    // Interface implementation
    func take_damage(mut this, amount: I32) {
        this.health = this.health - amount
    }
}

}  // namespace Game.Entities
```

### Interface Features
```tml
interface Serializable {
    func serialize(this) -> Bytes
    func deserialize(data: Bytes) -> This  // Static interface method
}

interface Comparable[T] {
    func compare(this, other: ref T) -> Ordering
}

// Interface inheritance
interface SortableCollection[T] extends Iterable[T], Comparable[T] {
    func sort(mut this)
}
```

### Namespace Features
```tml
namespace Collections.Generic {
    class List[T] { ... }
    class HashMap[K, V] { ... }
}

namespace Collections.Concurrent {
    class ConcurrentQueue[T] { ... }
}

// Usage
use Collections.Generic.List
use Collections.Concurrent.*

func main() {
    let list = List[I32]::new()
    let queue = ConcurrentQueue[I32]::new()
}
```

### Static Members
```tml
class Math {
    static PI: F64 = 3.14159265358979

    static func sqrt(x: F64) -> F64 { ... }
    static func pow(base: F64, exp: I32) -> F64 { ... }
}

// Usage
let result = Math::sqrt(Math::PI)
```

### Properties (Optional)
```tml
class Rectangle {
    private _width: F64
    private _height: F64

    // Property with getter and setter
    prop width: F64 {
        get => this._width
        set(value) => { this._width = value }
    }

    // Read-only property
    prop area: F64 {
        get => this._width * this._height
    }
}
```

## Coexistence with Behaviors

Classes and behaviors will coexist:

```tml
// Behaviors still work for composition
behavior Hashable {
    func hash(this) -> I64
}

// Classes can implement behaviors
class User implements Hashable {
    name: Str

    func hash(this) -> I64 {
        return this.name.hash()
    }
}

// Interfaces are just behaviors with different syntax
interface Drawable {
    func draw(this)
}

// Equivalent to:
behavior Drawable {
    func draw(this)
}
```

## Impact

- **Additive**: New keywords, existing code unchanged
- **Parser**: Add class/interface/namespace grammar
- **Type System**: Add inheritance hierarchy tracking
- **Codegen**: Add vtable generation for virtual methods
- **Runtime**: No changes needed (vtables are static)

## Comparison with C#

| Feature | C# | TML (Proposed) |
|---------|-----|----------------|
| Classes | `class Foo` | `class Foo` |
| Interfaces | `interface IFoo` | `interface Foo` |
| Inheritance | `: Base` | `extends Base` |
| Implementation | `: IFoo` | `implements Foo` |
| Abstract | `abstract class` | `abstract class` |
| Sealed | `sealed class` | `sealed class` |
| Virtual | `virtual void M()` | `virtual func m()` |
| Override | `override void M()` | `override func m()` |
| Properties | `int X { get; set; }` | `prop x: I32 { get; set }` |
| Namespaces | `namespace X.Y` | `namespace X.Y` |
| Static | `static int X` | `static x: I32` |
| Constructor | `Foo() { }` | `func new() -> Foo` |
| Base call | `base.M()` | `base.m()` |
| Protected | `protected` | `protected` |

## References

- C# Classes: https://docs.microsoft.com/en-us/dotnet/csharp/programming-guide/classes-and-structs/classes
- C# Interfaces: https://docs.microsoft.com/en-us/dotnet/csharp/programming-guide/interfaces/
- C# Inheritance: https://docs.microsoft.com/en-us/dotnet/csharp/programming-guide/classes-and-structs/inheritance
