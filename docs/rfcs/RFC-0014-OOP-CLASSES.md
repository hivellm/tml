# RFC-0014: C#-Style Object-Oriented Programming

## Status
Accepted (Implemented)

## Summary

This RFC defines C#-style object-oriented programming for TML, including classes with single inheritance, interfaces with multiple implementation, virtual methods, abstract classes, sealed classes, and namespaces. This supersedes the "OO Sugar" approach from RFC-0006.

## Motivation

While RFC-0006 proposed syntactic sugar for OO patterns, real-world demand from developers and LLMs familiar with C#, Java, and TypeScript has driven the need for full OOP support:

1. **Familiar mental model**: Developers from C#/Java backgrounds expect inheritance and polymorphism
2. **Virtual dispatch**: Runtime polymorphism is essential for plugin architectures and extensibility
3. **Design patterns**: Factory, Strategy, Observer, and other patterns are natural with inheritance
4. **Interop**: C# and Java libraries use classes extensively
5. **LLM code generation**: LLMs trained on C#/Java code generate OOP patterns more naturally

---

## 1. Classes

### 1.1 Basic Class

```tml
class Point {
    x: F64
    y: F64

    func new(x: F64, y: F64) -> Point {
        return Point { x: x, y: y }
    }

    func distance(this, other: ref Point) -> F64 {
        let dx: F64 = this.x - other.x
        let dy: F64 = this.y - other.y
        return sqrt(dx * dx + dy * dy)
    }
}
```

### 1.2 Field Visibility

```tml
class Person {
    private id: I64           // Only accessible within this class
    protected name: Str       // Accessible within class and subclasses
    pub age: I32              // Accessible everywhere
}
```

### 1.3 Static Members

```tml
class Counter {
    static count: I32 = 0     // Shared across all instances

    static func get_count() -> I32 {
        return Counter::count
    }

    func new() -> Counter {
        Counter::count = Counter::count + 1
        return Counter { }
    }
}
```

---

## 2. Inheritance

### 2.1 Extending Classes

TML uses **single inheritance** for classes:

```tml
class Animal {
    protected name: Str

    func new(name: Str) -> Animal {
        return Animal { name: name }
    }

    virtual func speak(this) -> Str {
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
}
```

### 2.2 Base Constructor Call

The `base:` clause calls the parent constructor:

```tml
func new(name: Str, breed: Str) -> Dog
    base: Animal::new(name)   // Call parent constructor
{
    return Dog { breed: breed }
}
```

### 2.3 Abstract Classes

Abstract classes cannot be instantiated directly:

```tml
abstract class Shape {
    protected x: F64
    protected y: F64

    abstract func area(this) -> F64
    abstract func perimeter(this) -> F64

    // Non-abstract methods allowed
    func move(this, dx: F64, dy: F64) {
        this.x = this.x + dx
        this.y = this.y + dy
    }
}

class Circle extends Shape {
    private radius: F64

    override func area(this) -> F64 {
        return 3.14159 * this.radius * this.radius
    }

    override func perimeter(this) -> F64 {
        return 2.0 * 3.14159 * this.radius
    }
}
```

### 2.4 Sealed Classes

Sealed classes cannot be extended:

```tml
sealed class GermanShepherd extends Dog {
    // This class cannot be inherited from
}

// ERROR: Cannot extend sealed class
class SuperDog extends GermanShepherd { }
```

---

## 3. Interfaces

### 3.1 Basic Interface

Interfaces define method contracts:

```tml
interface Drawable {
    func draw(this)
    func get_bounds(this) -> (F64, F64, F64, F64)
}

interface Serializable {
    func to_json(this) -> Str
    func from_json(json: Str) -> This
}
```

### 3.2 Multiple Implementation

Classes can implement multiple interfaces:

```tml
class Sprite implements Drawable, Serializable {
    func draw(this) {
        // Drawing implementation
    }

    func get_bounds(this) -> (F64, F64, F64, F64) {
        return (this.x, this.y, this.width, this.height)
    }

    func to_json(this) -> Str {
        return format("{\"x\": {}, \"y\": {}}", this.x, this.y)
    }

    func from_json(json: Str) -> This {
        // Parsing implementation
    }
}
```

### 3.3 Interface Inheritance

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
}
```

### 3.4 Default Implementations

Interfaces can provide default method implementations:

```tml
interface Equatable {
    func equals(this, other: This) -> Bool

    func not_equals(this, other: This) -> Bool {
        return not this.equals(other)  // Default implementation
    }
}
```

---

## 4. Virtual Methods and Polymorphism

### 4.1 Virtual Methods

Methods marked `virtual` can be overridden:

```tml
class Animal {
    virtual func speak(this) -> Str {
        return "..."
    }

    virtual func move(this) {
        println("Moving")
    }
}
```

### 4.2 Override Methods

Use `override` to redefine virtual methods:

```tml
class Dog extends Animal {
    override func speak(this) -> Str {
        return "Woof!"
    }

    override func move(this) {
        base.move()           // Call parent method
        println("Running")
    }
}
```

### 4.3 Base Calls

Use `base` to call parent methods:

```tml
override func move(this) {
    base.move()               // Calls Animal::move
    println("Running faster!")
}
```

---

## 5. Namespaces

### 5.1 Basic Namespace

```tml
namespace graphics.shapes {
    class Circle {
        radius: F64
    }

    class Rectangle {
        width: F64
        height: F64
    }
}
```

### 5.2 Nested Namespaces

```tml
namespace app {
    namespace models {
        class User {
            name: Str
            email: Str
        }
    }

    namespace controllers {
        class UserController {
            func create(data: Str) -> models::User {
                // Access models namespace
            }
        }
    }
}
```

### 5.3 Using Namespaces

```tml
use graphics.shapes.Circle
use graphics.shapes.*         // Import all

func main() {
    let c: Circle = Circle::new(5.0)
}
```

---

## 6. Memory Layout

### 6.1 Class Memory Structure

Classes are heap-allocated with a vtable pointer:

```
Class Instance Layout:
+----------------+
| vtable_ptr     |  -> Points to class vtable
+----------------+
| base fields    |  -> Parent class fields (if any)
+----------------+
| own fields     |  -> This class's fields
+----------------+
```

### 6.2 Vtable Structure

```
Vtable Layout:
+----------------+
| method_0       |  -> Virtual method pointers
| method_1       |
| method_2       |
| ...            |
+----------------+
```

### 6.3 Example

```tml
class Animal {           // Animal vtable: [speak]
    name: Str
    virtual func speak(this) -> Str
}

class Dog extends Animal {  // Dog vtable: [Dog::speak]
    breed: Str
    override func speak(this) -> Str
}
```

```
Dog Instance:
+----------------+
| vtable_ptr     |  -> Dog_vtable
+----------------+
| name: Str      |  // From Animal
+----------------+
| breed: Str     |  // From Dog
+----------------+

Dog_vtable:
+----------------+
| Dog::speak     |
+----------------+
```

---

## 7. Codegen Details

### 7.1 Constructor Codegen

```tml
class Dog extends Animal {
    func new(name: Str, breed: Str) -> Dog
        base: Animal::new(name)
    {
        return Dog { breed: breed }
    }
}
```

Generates:

```llvm
define ptr @Dog_new(ptr %name, ptr %breed) {
    ; Allocate Dog instance
    %obj = call ptr @malloc(i64 24)  ; vtable + name + breed

    ; Set vtable pointer
    store ptr @Dog_vtable, ptr %obj

    ; Initialize parent fields (via base call)
    %name_field = getelementptr %Dog, ptr %obj, i32 0, i32 1
    store ptr %name, ptr %name_field

    ; Initialize own fields
    %breed_field = getelementptr %Dog, ptr %obj, i32 0, i32 2
    store ptr %breed, ptr %breed_field

    ret ptr %obj
}
```

### 7.2 Virtual Dispatch

```tml
let animal: Animal = Dog::new("Rex", "Shepherd")
let sound: Str = animal.speak()  // Virtual call
```

Generates:

```llvm
; Load vtable pointer
%vtable = load ptr, ptr %animal

; Get speak method from vtable (index 0)
%speak_ptr = getelementptr %Animal_vtable, ptr %vtable, i32 0, i32 0
%speak = load ptr, ptr %speak_ptr

; Call virtual method
%result = call ptr %speak(ptr %animal)
```

---

## 8. Type Checking

### 8.1 Inheritance Validation

- Single inheritance only (no multiple class inheritance)
- No circular inheritance
- Sealed classes cannot be extended
- Abstract classes cannot be instantiated directly

### 8.2 Override Validation

- `override` required when overriding virtual method
- Override signature must match exactly
- Cannot override non-virtual methods
- All abstract methods must be implemented

### 8.3 Interface Validation

- All interface methods must be implemented
- Implementation signatures must match
- Class can implement multiple interfaces

### 8.4 Visibility Rules

- `private`: Only accessible within the class
- `protected`: Accessible within class and subclasses
- `pub`: Accessible everywhere
- Overrides cannot be more restrictive than parent

---

## 9. Comparison with RFC-0006

| Aspect | RFC-0006 (OO Sugar) | RFC-0014 (C#-Style OOP) |
|--------|---------------------|-------------------------|
| Class | Desugars to type+impl | First-class construct |
| Inheritance | Rejected | Single inheritance |
| Virtual methods | Not supported | Full vtable dispatch |
| Interfaces | Use behaviors | Dedicated interface type |
| Abstract | Not supported | Abstract classes/methods |
| Sealed | Not supported | Sealed classes |
| Namespaces | Not supported | Full namespace support |

---

## 10. Performance Considerations

### 10.1 Overhead

- Each class instance has 8-byte vtable pointer overhead
- Virtual calls require indirect jump through vtable
- Heap allocation for all class instances

### 10.2 Optimizations

Future optimizations (see `oop-mir-hir-optimizations` task):

- **Devirtualization**: Convert virtual calls to direct calls when type is known
- **Escape analysis**: Stack-allocate non-escaping class instances
- **Sealed class optimization**: Direct calls for sealed class methods
- **Constructor fusion**: Inline base constructor chains

### 10.3 Value Classes (Future)

```tml
@value
sealed class Point {
    x: F64
    y: F64
}
```

Value classes have no vtable and are stack-allocated.

---

## 11. Examples

### 11.1 Factory Pattern

```tml
interface Shape {
    func area(this) -> F64
    func draw(this)
}

abstract class ShapeFactory {
    abstract func create(this) -> Shape
}

class CircleFactory extends ShapeFactory {
    override func create(this) -> Shape {
        return Circle::new(10.0)
    }
}

class RectangleFactory extends ShapeFactory {
    override func create(this) -> Shape {
        return Rectangle::new(10.0, 20.0)
    }
}
```

### 11.2 Observer Pattern

```tml
interface Observer {
    func update(this, message: Str)
}

class Subject {
    private observers: List[ref Observer]

    func attach(this, obs: ref Observer) {
        this.observers.push(obs)
    }

    func notify(this, message: Str) {
        for obs in this.observers {
            obs.update(message)
        }
    }
}
```

---

## 12. Compatibility

- **RFC-0001**: Core types remain foundation
- **RFC-0002**: New OOP keywords added
- **RFC-0003**: Contracts work on class methods
- **RFC-0005**: Namespaces extend module system
- **RFC-0006**: This RFC supersedes OO sugar approach

---

## 13. Implementation Status

| Component | Status |
|-----------|--------|
| Lexer keywords | ✅ Complete |
| Parser grammar | ✅ Complete |
| Type system | ✅ Complete |
| Codegen (vtables) | ✅ Complete |
| Virtual dispatch | ✅ Complete |
| Class instantiation | ✅ Complete |
| Static fields/methods | ✅ Complete |
| Inheritance (extends) | ✅ Complete |
| Interfaces (implements) | ✅ Complete |
| `is` operator | ✅ Complete |
| Visibility modifiers | ✅ Complete |
| Abstract classes | ✅ Complete |
| Sealed classes | ✅ Complete |
| Virtual/Override | ✅ Complete |
| Module registry | ✅ Complete |
| Namespace support | 🔄 Pending |
| Property syntax | 🔄 Pending |
| MIR/HIR optimizations | 🔄 Pending |

---

## 14. References

- [C# Classes and Objects](https://docs.microsoft.com/en-us/dotnet/csharp/fundamentals/types/classes)
- [Java Classes](https://docs.oracle.com/javase/tutorial/java/javaOO/classes.html)
- [TypeScript Classes](https://www.typescriptlang.org/docs/handbook/2/classes.html)
- [LLVM Virtual Dispatch](https://llvm.org/docs/LangRef.html#call-instruction)
