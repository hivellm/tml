TML_MODULE("compiler")

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_type_explanations_part2() {
    static const std::unordered_map<std::string, std::string> db = {

        {"T050", R"EX(
Iterator type error [T050]

A `for` loop's iterator expression does not implement the `IntoIterator`
behavior, or the iterator's `Item` type is incompatible.

Example of erroneous code:

    for item in 42 {           // I32 is not iterable
        println(item)
    }

How to fix:

    for item in [1, 2, 3] {    // arrays are iterable
        println(item)
    }

    for i in 0 to 10 {        // ranges are iterable
        println(i)
    }
)EX"},

        {"T051", R"EX(
Range type error [T051]

A range expression (`to` or `through`) uses non-integer types.
Range bounds must be integer types.

Example of erroneous code:

    for x in "a" to "z" {     // strings cannot be range bounds
        println(x)
    }

How to fix:

    for x in 0 to 26 {        // use integer bounds
        println(x)
    }
)EX"},

        {"T052", R"EX(
Division by zero [T052]

A division or modulo operation has a literal zero as the divisor.
Division by zero is undefined behavior.

Example of erroneous code:

    let x = 42 / 0             // division by zero
    let y = 10 % 0             // modulo by zero

How to fix:

    let x = 42 / 2             // valid division
    if divisor != 0 {
        let result = value / divisor   // check before dividing
    }
)EX"},

        {"T054", R"EX(
Lifetime error [T054]

A reference or borrow has a lifetime that is incompatible with its
usage. This typically occurs when returning references that could
outlive their source data.

Related: B010 (return local reference)
)EX"},

        {"T055", R"EX(
Const initializer type mismatch [T055]

The type of a constant's initializer expression does not match its declared
type. Constants require exact type matches — no implicit conversions.

Example of erroneous code:

    const MAX: I32 = "hello"     // error: expected I32, found Str
    const PI: F64 = 3            // error: expected F64, found I32

How to fix:

    const MAX: I32 = 100         // matching types
    const PI: F64 = 3.14         // matching types

Related: T056 (variable binding type mismatch)
)EX"},

        {"T056", R"EX(
Variable binding type mismatch [T056]

The type of a variable's initializer does not match its declared type
annotation. This applies to `let`, `var`, and `let-else` bindings.

Example of erroneous code:

    let x: I32 = "hello"         // error: expected I32, found Str
    var y: Bool = 42             // error: expected Bool, found I32
    let Some(v): I32 = expr else { return }  // type mismatch in let-else

How to fix:

    let x: I32 = 42              // matching types
    var y: Bool = true           // matching types

    // Or remove the type annotation to let the compiler infer:
    let x = 42                   // inferred as I32

Related: T055 (const initializer type mismatch)
)EX"},

        {"T057", R"EX(
Pointer argument type mismatch [T057]

A pointer method was called with an argument of the wrong type. Pointer
methods like `write()` and `offset()` require specific argument types.

Example of erroneous code:

    let p: Ptr[I32] = mem_alloc[I32](1)
    p.write("hello")             // error: expected I32, got Str
    p.offset("two")              // error: offset() requires I32 or I64

How to fix:

    p.write(42)                  // write matching type (I32)
    p.offset(2)                  // offset with integer argument

- `write(value)`: value must match the pointer's element type
- `offset(n)`: n must be I32 or I64
)EX"},

        {"T058", R"EX(
Override parameter type mismatch [T058]

A method override or behavior implementation has a parameter with a different
type than the base class method or interface declaration.

Example of erroneous code:

    class Animal {
        virtual func speak(this, volume: I32) -> Str { return "" }
    }
    class Dog extends Animal {
        func speak(this, volume: Str) -> Str {  // error: expected I32, got Str
            return "woof"
        }
    }

    behavior Comparable {
        func cmp(this, other: ref Self) -> I32
    }
    impl Comparable for MyType {
        func cmp(this, other: Str) -> I32 {     // error: expected ref Self
            return 0
        }
    }

How to fix:

    // Parameter types must exactly match the base/interface declaration
    class Dog extends Animal {
        func speak(this, volume: I32) -> Str { return "woof" }
    }
)EX"},

        {"T059", R"EX(
Unknown field in struct/union literal [T059]

A field name used in a struct or union literal does not exist in the type
definition.

Example of erroneous code:

    type Point { x: I32, y: I32 }
    let p = Point { x: 1, z: 2 }    // error: unknown field 'z'

How to fix:

    let p = Point { x: 1, y: 2 }    // use correct field name

Check the struct definition for available field names. Field names are
case-sensitive.
)EX"},

        {"T060", R"EX(
Union literal field count error [T060]

A union literal must initialize exactly one field. Unions represent a value
that can be one of several types, so only one field can be active at a time.

Example of erroneous code:

    union Value { i: I32, f: F64 }
    let v = Value { }                // error: requires exactly one field
    let v = Value { i: 1, f: 2.0 }  // error: only one field at a time

How to fix:

    let v = Value { i: 42 }          // initialize exactly one field
    let v = Value { f: 3.14 }        // or the other field
)EX"},

        {"T061", R"EX(
Missing required field in struct literal [T061]

A struct literal is missing a required field that has no default value.
All fields without defaults must be provided when constructing a struct.

Example of erroneous code:

    type User { name: Str, age: I32, active: Bool }
    let u = User { name: "Alice" }   // error: missing 'age' and 'active'

How to fix:

    let u = User { name: "Alice", age: 30, active: true }

    // Or add default values in the type definition:
    type User { name: Str, age: I32, active: Bool = true }
    let u = User { name: "Alice", age: 30 }  // 'active' uses default
)EX"},

        {"T062", R"EX(
Struct update base type mismatch [T062]

A struct update expression (`..base`) uses a base value whose type does not
match the struct being constructed.

Example of erroneous code:

    type Point2D { x: I32, y: I32 }
    type Point3D { x: I32, y: I32, z: I32 }
    let p2 = Point2D { x: 1, y: 2 }
    let p3 = Point3D { z: 3, ..p2 }  // error: base is Point2D, not Point3D

How to fix:

    let p3_base = Point3D { x: 0, y: 0, z: 0 }
    let p3 = Point3D { z: 3, ..p3_base }  // base matches target type
)EX"},

        {"T063", R"EX(
Override without base class [T063]

A method was marked with `override` but the class does not extend any base
class. The `override` keyword requires a parent class with a virtual method
to override.

Example of erroneous code:

    class Standalone {
        override func speak(this) -> Str {    // error: no base class
            return "hello"
        }
    }

How to fix:

    // Either remove 'override':
    class Standalone {
        func speak(this) -> Str { return "hello" }
    }

    // Or extend a base class:
    class Standalone extends Animal {
        override func speak(this) -> Str { return "hello" }
    }
)EX"},

        {"T064", R"EX(
Cannot override non-virtual method [T064]

A method is marked as `override` but the corresponding method in the base
class is not declared as `virtual`. Only virtual methods can be overridden.

Example of erroneous code:

    class Base {
        func greet(this) -> Str { return "hi" }    // not virtual
    }
    class Child extends Base {
        override func greet(this) -> Str {          // error: not virtual
            return "hello"
        }
    }

How to fix:

    class Base {
        virtual func greet(this) -> Str { return "hi" }  // add 'virtual'
    }
    class Child extends Base {
        override func greet(this) -> Str { return "hello" }  // now valid
    }
)EX"},

        {"T065", R"EX(
Override method not found in base [T065]

A method is marked as `override` but no method with that name exists in any
base class in the inheritance chain.

Example of erroneous code:

    class Animal {
        virtual func speak(this) -> Str { return "" }
    }
    class Dog extends Animal {
        override func bark(this) -> Str {     // error: 'bark' not in Animal
            return "woof"
        }
    }

How to fix:

    class Dog extends Animal {
        override func speak(this) -> Str {    // override the correct method
            return "woof"
        }
    }

Check the base class for the exact method name and spelling.
)EX"},

        {"T066", R"EX(
Invalid class name in new expression [T066]

The class name in a `new` expression could not be resolved. The type must
be a known class that can be instantiated.

Example of erroneous code:

    let obj = new UnknownClass()      // error: unknown class

How to fix:

    class MyClass { value: I32 }
    let obj = new MyClass()           // use a defined class name

Check that the class is defined and imported (via `use`) if in another module.
)EX"},

        {"T067", R"EX(
Field not found in base class [T067]

A `base.field` expression references a field that does not exist in the
parent class.

Example of erroneous code:

    class Animal { name: Str }
    class Dog extends Animal {
        func info(this) -> Str {
            return base.species        // error: 'species' not in Animal
        }
    }

How to fix:

    class Dog extends Animal {
        func info(this) -> Str {
            return base.name           // use a field that exists in Animal
        }
    }

Related: T005 (unknown field on type)
)EX"},

        {"T068", R"EX(
Tuple pattern on non-tuple type [T068]

A tuple destructuring pattern was used on a value that is not a tuple.

Example of erroneous code:

    let x: I32 = 42
    let (a, b) = x              // I32 is not a tuple

How to fix: use a tuple pattern only on tuple types:

    let pair: (I32, I32) = (1, 2)
    let (a, b) = pair           // correct

Related: T036 (tuple arity mismatch), T035 (general pattern mismatch)
)EX"},

        {"T069", R"EX(
Enum pattern on non-enum type [T069]

An enum destructuring pattern was used on a value that is not an enum.

Example of erroneous code:

    let x: I32 = 42
    when x {
        Just(v) => ...          // I32 is not Maybe or any enum
    }

How to fix: use enum patterns only on enum types:

    let m: Maybe[I32] = Just(42)
    when m {
        Just(v) => println("{v}"),
        Nothing => println("empty"),
    }

Related: T035 (general pattern mismatch)
)EX"},

        {"T070", R"EX(
Struct pattern on non-struct type [T070]

A struct destructuring pattern was used on a value that is not a struct.

Example of erroneous code:

    let x: I32 = 42
    let Point { x, y } = x     // I32 is not a struct

How to fix: use struct patterns only on struct types:

    let p = Point { x: 1, y: 2 }
    let Point { x, y } = p     // correct

Related: T035 (general pattern mismatch)
)EX"},

        {"T071", R"EX(
Array pattern on non-array type [T071]

An array destructuring pattern was used on a value that is not an array.

Example of erroneous code:

    let x: I32 = 42
    let [a, b, c] = x          // I32 is not an array

How to fix: use array patterns only on array types:

    let arr: [I32; 3] = [1, 2, 3]
    let [a, b, c] = arr        // correct

Related: T035 (general pattern mismatch)
)EX"},

        {"T072", R"EX(
Unknown field in struct pattern [T072]

A struct destructuring pattern references a field that does not exist
in the struct definition.

Example of erroneous code:

    struct Point { x: I32, y: I32 }
    let p = Point { x: 1, y: 2 }
    let Point { x, z } = p     // 'z' is not a field of Point

How to fix: use only fields that exist in the struct:

    let Point { x, y } = p     // correct

Related: T059 (unknown field in struct literal), T005 (unknown field)
)EX"},

        {"T073", R"EX(
Field not found on class [T073]

Attempted to access a field that does not exist on a class type,
including its base classes.

Example of erroneous code:

    class Dog extends Animal {
        let breed: Str
    }
    let d = new Dog()
    print(d.color)              // 'color' not in Dog or Animal

How to fix: access only fields defined in the class or its parents:

    print(d.breed)              // correct
    print(d.name)               // if defined in Animal

Related: T005 (unknown field), T067 (base field not found)
)EX"},

        {"T074", R"EX(
Field not found through pointer [T074]

Attempted to access a field through a pointer type, but the pointed-to
type does not have that field.

Example of erroneous code:

    struct Point { x: I32, y: I32 }
    let p: Ptr[Point] = ...
    let z = p.z                 // 'z' not in Point

How to fix: access only fields that exist on the pointed-to type:

    let x = p.x                // correct (auto-deref)
    let y = p.y                // correct (auto-deref)

Related: T005 (unknown field), T073 (class field not found)
)EX"},

        {"T075", R"EX(
Class not found [T075]

A `new` expression references a class name that is not defined
or not in scope.

Example of erroneous code:

    let obj = new FakeClass()   // 'FakeClass' doesn't exist

How to fix: ensure the class is defined and imported:

    class MyClass {
        let value: I32
    }
    let obj = new MyClass()     // correct

Related: T022 (unknown struct), T046 (base class not found)
)EX"},

        {"T076", R"EX(
Class has no base class [T076]

The `base` keyword was used in a class that does not extend any
base class.

Example of erroneous code:

    class Dog {
        func speak() -> Str {
            return base.speak() // Dog has no base class
        }
    }

How to fix: only use `base` in classes that extend a parent:

    class Dog extends Animal {
        func speak() -> Str {
            return base.speak() // correct - Animal.speak()
        }
    }

Related: T063 (override: no base class), T046 (base class not found)
)EX"},

        {"T077", R"EX(
Method not found in base class [T077]

A `base.method()` call references a method that does not exist
in the base class.

Example of erroneous code:

    class Animal {
        func speak() -> Str { return "..." }
    }
    class Dog extends Animal {
        func bark() -> Str {
            return base.fly()   // Animal has no fly() method
        }
    }

How to fix: call methods that exist in the base class:

    return base.speak()         // correct

Related: T006 (unknown method), T078 (class method not found)
)EX"},

        {"T078", R"EX(
Method not found on class [T078]

A method was called on a class instance, but the method does not
exist in the class or any of its base classes.

Example of erroneous code:

    class Dog {
        func bark() -> Str { return "Woof!" }
    }
    let d = new Dog()
    d.fly()                     // Dog has no fly() method

How to fix: call methods that exist on the class:

    d.bark()                    // correct

Related: T006 (unknown method), T077 (base method not found)
)EX"},

        {"T079", R"EX(
Method not found on behavior [T079]

A method was called on a dynamic behavior (trait object), but the
method does not exist in the behavior definition.

Example of erroneous code:

    behavior Printable {
        func to_str() -> Str
    }
    func show(p: dyn Printable) {
        p.display()             // Printable has no display()
    }

How to fix: call methods defined in the behavior:

    func show(p: dyn Printable) {
        p.to_str()              // correct
    }

Related: T006 (unknown method), T025 (unknown behavior)
)EX"},

        {"T080", R"EX(
Pointer read() takes no arguments [T080]

The pointer `read()` method was called with arguments, but it
takes no arguments.

Example of erroneous code:

    let p: Ptr[I32] = ...
    let v = p.read(42)          // read() takes no arguments

How to fix: call read() without arguments:

    let v = p.read()            // correct

Related: T049 (general pointer method error)
)EX"},

        {"T081", R"EX(
Pointer write() requires exactly one argument [T081]

The pointer `write()` method was called with the wrong number
of arguments.

Example of erroneous code:

    let p: Ptr[I32] = ...
    p.write()                   // missing value argument
    p.write(1, 2)               // too many arguments

How to fix: pass exactly one value to write:

    p.write(42)                 // correct

Related: T049 (general pointer method error)
)EX"},

        {"T082", R"EX(
Pointer is_null() takes no arguments [T082]

The pointer `is_null()` method was called with arguments, but it
takes no arguments.

Example of erroneous code:

    let p: Ptr[I32] = ...
    let n = p.is_null(true)     // is_null() takes no arguments

How to fix: call is_null() without arguments:

    let n = p.is_null()         // correct

Related: T049 (general pointer method error)
)EX"},

        {"T083", R"EX(
Pointer offset() requires exactly one argument [T083]

The pointer `offset()` method was called with the wrong number
of arguments.

Example of erroneous code:

    let p: Ptr[I32] = ...
    let q = p.offset()          // missing count argument
    let r = p.offset(1, 2)     // too many arguments

How to fix: pass exactly one offset count:

    let q = p.offset(3)         // correct

Related: T049 (general pointer method error)
)EX"},

        {"T084", R"EX(
Unknown pointer method [T084]

A method was called on a pointer type that is not one of the
supported pointer methods.

Example of erroneous code:

    let p: Ptr[I32] = ...
    p.deref()                   // not a valid pointer method

Available pointer methods:
    p.read()         — read the value at the pointer
    p.write(value)   — write a value through the pointer
    p.is_null()      — check if the pointer is null
    p.offset(count)  — return a pointer offset by count elements

Related: T049 (general pointer method error)
)EX"},

        {"T085", R"EX(
Recursive enum has infinite size [T085]

An enum type directly contains itself as a variant payload without
pointer indirection, which would require infinite memory.

Example of erroneous code:

    enum Expr {
        Num(I32),
        Add(Expr, Expr),         // ERROR: Expr contains itself directly
    }

To fix, wrap the self-reference in Heap[T] (or another pointer type)
to introduce indirection:

    enum Expr {
        Num(I32),
        Add(Heap[Expr], Heap[Expr]),   // OK: Heap provides indirection
    }

Allowed indirection types: Heap[T], Shared[T], Sync[T], List[T], *T, ref T.
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
