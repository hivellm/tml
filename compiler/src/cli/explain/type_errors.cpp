//! # Type Error Explanations
//!
//! Error codes T001-T084 for type checking errors.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_type_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"T001", R"EX(
Type mismatch [T001]

The compiler found a value of one type where a different type was expected.
The two types are incompatible and no implicit conversion exists.

Example of erroneous code:

    let x: Str = 42          // expected Str, found I32

The variable `x` is annotated as `Str`, but `42` is an integer literal.

How to fix:

1. Change the annotation to match the value:
       let x: I32 = 42

2. Convert the value:
       let x: Str = 42.to_string()

3. Remove the annotation and let inference decide:
       let x = 42

Related: T016 (return type mismatch), T015 (branch type mismatch)
)EX"},

        {"T002", R"EX(
Unknown type [T002]

A type name was used that the compiler cannot find in the current scope.
This usually means the type was misspelled or not imported.

Example of erroneous code:

    let x: Strng = "hello"      // 'Strng' is not defined

How to fix:

    let x: Str = "hello"        // correct the spelling

If the type is in another module:

    use std::collections::HashMap
    let m: HashMap[Str, I32] = HashMap.new()
)EX"},

        {"T003", R"EX(
Unknown function [T003]

A function was called that does not exist in the current scope. The
function may be misspelled, not imported, or defined in a different module.

Example of erroneous code:

    let result = computeValue(42)   // function not found

How to fix:

    let result = compute_value(42)  // correct the name (TML uses snake_case)

If the function is in another module:

    use math::compute_value
    let result = compute_value(42)

Related: T006 (unknown method)
)EX"},

        {"T004", R"EX(
Argument count mismatch [T004]

A function was called with the wrong number of arguments. The number of
arguments passed must match the number of parameters declared.

Example of erroneous code:

    func add(a: I32, b: I32) -> I32 { return a + b }

    let result = add(1, 2, 3)   // expected 2 args, got 3

How to fix:

    let result = add(1, 2)      // pass the correct number of arguments
)EX"},

        {"T005", R"EX(
Unknown field [T005]

An attempt was made to access a field that does not exist on the given
struct or type.

Example of erroneous code:

    type Point { x: F64, y: F64 }
    let p = Point { x: 1.0, y: 2.0 }
    let z = p.z                  // Point has no field 'z'

How to fix:

    let x = p.x                  // use an existing field

If you need a `z` field, add it to the struct:

    type Point3D { x: F64, y: F64, z: F64 }
)EX"},

        {"T006", R"EX(
Unknown method [T006]

A method was called on a type that does not have that method. The method
may be misspelled, not implemented for this type, or requires a behavior
implementation.

Example of erroneous code:

    let s: Str = "hello"
    let n = s.to_integer()      // Str has no method 'to_integer'

How to fix:

    let n = s.parse_i32()       // use the correct method name

If the method should come from a behavior:

    impl Display for MyType {
        func to_string(self) -> Str { ... }
    }

Related: T003 (unknown function), T025 (unknown behavior)
)EX"},

        {"T007", R"EX(
Cannot infer type [T007]

The compiler cannot determine the type of an expression. This happens
when there is not enough context to infer the type, such as with empty
collections or generic function calls.

Example of erroneous code:

    let items = []               // what type of list?

How to fix:

    let items: List[I32] = []    // provide a type annotation
    let items = [1, 2, 3]       // or provide initial values
)EX"},

        {"T008", R"EX(
Duplicate definition [T008]

A name was defined more than once in the same scope. Each name (variable,
function, type, etc.) must be unique within its scope.

Example of erroneous code:

    func compute() -> I32 { return 1 }
    func compute() -> I32 { return 2 }   // duplicate

How to fix:

    func compute_a() -> I32 { return 1 }
    func compute_b() -> I32 { return 2 }
)EX"},

        {"T009", R"EX(
Undeclared variable [T009]

A variable was used that has not been declared in the current scope or
any enclosing scope.

Example of erroneous code:

    func main() {
        print(x)                // 'x' is not declared
    }

How to fix:

    func main() {
        let x = 42
        print(x)                // now 'x' is in scope
    }

Note: Variables must be declared before use. TML does not support
forward references to variables.
)EX"},

        {"T010", R"EX(
Not callable [T010]

An attempt was made to call something that is not a function or closure.
Only functions, closures, and types with a call operator can be invoked.

Example of erroneous code:

    let x: I32 = 42
    let result = x(1, 2)       // I32 is not callable

How to fix:

    func compute(a: I32, b: I32) -> I32 { return a + b }
    let result = compute(1, 2)
)EX"},

        {"T011", R"EX(
Missing type annotation [T011]

A variable declaration requires a type annotation or initializer to
determine its type. The compiler cannot infer the type without one.

Example of erroneous code:

    let x                      // no type and no initializer

How to fix:

    let x: I32 = 0             // provide type and initializer
    let x = 42                 // let the compiler infer from value
)EX"},

        {"T013", R"EX(
Immutable assignment [T013]

An attempt was made to assign a new value to an immutable variable.
In TML, variables declared with `let` are immutable by default.

Example of erroneous code:

    let x = 1
    x = 2                       // cannot assign to immutable 'x'

How to fix:

    var x = 1                   // use 'var' for mutable variables
    x = 2                       // now assignment works

Related: B003 (assign not mutable)
)EX"},

        {"T014", R"EX(
Condition not Bool [T014]

An `if` or `loop while` condition must evaluate to `Bool`. A value of
a different type was used as a condition.

Example of erroneous code:

    let x = 42
    if x {                      // I32 is not Bool
        print("yes")
    }

How to fix:

    if x > 0 {                  // use a comparison expression
        print("yes")
    }

    if x != 0 {                 // or explicit comparison
        print("yes")
    }
)EX"},

        {"T015", R"EX(
Branch type mismatch [T015]

The branches of an `if`/`else` or `when` expression return different
types. When used as an expression, all branches must produce the same type.

Example of erroneous code:

    let result = if condition then 42 else "hello"
    // 'then' branch is I32, 'else' branch is Str

How to fix:

    let result = if condition then 42 else 0         // both I32
    let result = if condition then "yes" else "no"   // both Str
)EX"},

        {"T016", R"EX(
Return type mismatch [T016]

The value returned from a function does not match the declared return type.

Example of erroneous code:

    func get_name() -> Str {
        return 42               // expected Str, found I32
    }

How to fix:

    func get_name() -> Str {
        return "Alice"          // return the correct type
    }

    // Or change the return type:
    func get_value() -> I32 {
        return 42
    }

Related: T001 (type mismatch), T015 (branch type mismatch)
)EX"},

        {"T017", R"EX(
Cannot dereference non-reference type [T017]

The dereference operator `*` was applied to a value that is not a
reference or pointer type.

Example of erroneous code:

    let x: I32 = 42
    let y = *x                 // I32 is not a reference

How to fix:

    let x: I32 = 42
    let r = ref x              // create a reference
    let y = *r                 // dereference the reference
)EX"},

        {"T020", R"EX(
Division by zero in const expression [T020]

A compile-time constant expression attempts to divide by zero.

Example of erroneous code:

    const X: I32 = 10 / 0     // division by zero

How to fix:

    const X: I32 = 10 / 2     // valid constant division
)EX"},

        {"T022", R"EX(
Unknown struct [T022]

A struct constructor or pattern references a struct type that is not
defined in the current scope.

Example of erroneous code:

    let p = Pint { x: 1.0 }   // 'Pint' is not defined (typo?)

How to fix:

    let p = Point { x: 1.0, y: 2.0 }   // use correct type name
)EX"},

        {"T023", R"EX(
Unknown enum type [T023]

A pattern or expression references an enum type that is not defined
in the current scope.

Example of erroneous code:

    when value {
        Colour::Red => ...     // 'Colour' is not defined (typo?)
    }

How to fix:

    when value {
        Color::Red => ...      // use correct enum name
    }
)EX"},

        {"T024", R"EX(
Unknown enum variant [T024]

A pattern or expression references a variant that does not exist on
the specified enum type.

Example of erroneous code:

    type Color = Red | Green | Blue
    let c = Color::Yellow      // 'Yellow' is not a variant of Color

How to fix:

    let c = Color::Red         // use an existing variant
)EX"},

        {"T025", R"EX(
Unknown behavior [T025]

An `impl` block references a behavior (trait) that is not defined or
imported in the current scope.

Example of erroneous code:

    impl Displayable for MyType { ... }  // 'Displayable' not found

How to fix:

    impl Display for MyType {            // correct behavior name
        pub func to_string(this) -> Str { ... }
    }
)EX"},

        {"T026", R"EX(
Missing behavior implementation [T026]

A type claims to implement a behavior but does not provide all required
methods, or an interface method is not implemented by a class.

Example of erroneous code:

    impl Display for Point {
        // missing to_string() method
    }

How to fix:

    impl Display for Point {
        pub func to_string(this) -> Str {
            return "({this.x}, {this.y})"
        }
    }
)EX"},

        {"T027", R"EX(
Module not found [T027]

A `use` statement references a module that cannot be found.

Example of erroneous code:

    use std::nonexistent::Module   // module does not exist

How to fix:

    use std::collections::List     // use correct module path

Check available modules with `tml doc --all` or the documentation.
)EX"},

        {"T028", R"EX(
Invalid extern function [T028]

An `@extern` function declaration has invalid attributes. Extern functions
must have a calling convention and must not have a body (they are defined
externally).

Example of erroneous code:

    @extern("c")
    func malloc(size: I64) -> Ptr[U8] {
        // extern functions cannot have a body
    }

How to fix:

    @extern("c")
    func malloc(size: I64) -> Ptr[U8]   // declaration only, no body
)EX"},

        {"T029", R"EX(
Missing return statement [T029]

A function with a non-void return type does not return a value on all
code paths.

Example of erroneous code:

    func compute(x: I32) -> I32 {
        if x > 0 {
            return x * 2
        }
        // missing return for x <= 0
    }

How to fix:

    func compute(x: I32) -> I32 {
        if x > 0 {
            return x * 2
        }
        return 0               // return on all paths
    }
)EX"},

        {"T030", R"EX(
Break outside of loop [T030]

A `break` statement was used outside of a loop body. `break` can only
be used inside `loop`, `for`, or `while` constructs.

Example of erroneous code:

    func process() {
        break                  // not inside a loop
    }

How to fix:

    func process() {
        loop {
            if done {
                break          // inside a loop
            }
        }
    }
)EX"},

        {"T032", R"EX(
Await outside async [T032]

The `.await` operator was used outside of an async function. Only
async functions can use `.await` to wait for asynchronous results.

Example of erroneous code:

    func process() {
        let result = fetch_data().await   // not an async function
    }

How to fix:

    async func process() {
        let result = fetch_data().await   // async function can await
    }
)EX"},

        {"T033", R"EX(
Invalid try operator [T033]

The `?` (try) operator was used on a type that is not `Outcome[T, E]`
or `Maybe[T]`. The try operator can only propagate errors from these types.

Example of erroneous code:

    func process() -> Outcome[I32, Str] {
        let x: I32 = 42
        let y = x?             // I32 is not Outcome or Maybe
    }

How to fix:

    func process() -> Outcome[I32, Str] {
        let x = might_fail()?  // use ? on Outcome/Maybe values
        return Ok(x)
    }
)EX"},

        {"T034", R"EX(
Wrong variant arguments [T034]

An enum variant was constructed or matched with the wrong number of
arguments. Each variant has a fixed number of associated values.

Example of erroneous code:

    type Shape = Circle(F64) | Rect(F64, F64)
    let s = Shape::Circle(1.0, 2.0)    // Circle takes 1 arg, not 2

How to fix:

    let s = Shape::Circle(1.0)         // correct: 1 argument
    let r = Shape::Rect(3.0, 4.0)     // correct: 2 arguments
)EX"},

        {"T035", R"EX(
Pattern type mismatch [T035]

A pattern in a `when` arm or `let` binding does not match the type
of the value being matched.

Example of erroneous code:

    let x: I32 = 42
    when x {
        "hello" => ...         // matching I32 against Str pattern
    }

How to fix:

    when x {
        0 => println("zero"),
        n => println("other: {n}")
    }
)EX"},

        {"T036", R"EX(
Tuple arity mismatch [T036]

A tuple pattern or field access references more elements than the
tuple actually contains.

Example of erroneous code:

    let pair = (1, 2)
    let (a, b, c) = pair       // pair has 2 elements, not 3

How to fix:

    let (a, b) = pair          // match the correct arity
)EX"},

        {"T038", R"EX(
Cannot redefine builtin type [T038]

An attempt was made to define a type with the same name as a built-in
type. Built-in types like I32, Str, Bool, etc. cannot be redefined.

Example of erroneous code:

    type I32 { value: I64 }    // cannot redefine I32

How to fix:

    type MyInt { value: I64 }  // use a different name
)EX"},

        {"T039", R"EX(
Circular dependency [T039]

A circular dependency was detected in type definitions or class
inheritance. Types cannot reference themselves in a cycle.

Example of erroneous code:

    type A { b: B }
    type B { a: A }            // circular reference

How to fix:

    type A { b: Heap[B] }     // use indirection to break cycle
    type B { a: Heap[A] }
)EX"},

        {"T040", R"EX(
Cannot instantiate abstract [T040]

An attempt was made to instantiate an abstract class. Abstract classes
cannot be created directly — use a concrete subclass instead.

Example of erroneous code:

    @abstract
    class Shape { ... }
    let s = new Shape()        // cannot instantiate abstract class

How to fix:

    class Circle extends Shape { ... }
    let s = new Circle()       // instantiate a concrete subclass
)EX"},

        {"T042", R"EX(
Value class virtual method [T042]

A `@value` class cannot have virtual (overridable) methods. Value
classes are stack-allocated and do not support polymorphism.

How to fix:

Remove the `@value` annotation or make the methods non-virtual.
)EX"},

        {"T044", R"EX(
Pool/value class conflict [T044]

A class has conflicting `@pool` and `@value` annotations, or uses
features incompatible with its allocation strategy.

How to fix:

Choose either `@pool` or `@value`, not both. `@pool` classes are
pool-allocated, `@value` classes are stack-allocated.
)EX"},

        {"T045", R"EX(
Missing abstract implementation [T045]

A concrete class extends an abstract class but does not implement all
required abstract methods.

Example of erroneous code:

    @abstract
    class Shape {
        abstract func area(this) -> F64
    }
    class Circle extends Shape {
        // missing area() implementation
    }

How to fix:

    class Circle extends Shape {
        func area(this) -> F64 {
            return 3.14159 * this.radius * this.radius
        }
    }
)EX"},

        {"T046", R"EX(
Base class not found [T046]

A class `extends` declaration references a base class that does not
exist or is not in scope.

Example of erroneous code:

    class Dog extends Animel { ... }   // 'Animel' not found (typo?)

How to fix:

    class Dog extends Animal { ... }   // correct class name
)EX"},

        {"T047", R"EX(
Interface not found [T047]

A class `implements` declaration references an interface that does not
exist or is not in scope.

Example of erroneous code:

    class MyList implements Iterable { ... }  // 'Iterable' not found

How to fix:

Import or define the interface first, then implement it.
)EX"},

        {"T048", R"EX(
Invalid base expression [T048]

The `base` keyword was used in an invalid context. `base` can only be
used inside a class method to call the parent class's method or access
its fields.

Example of erroneous code:

    func standalone() {
        base.compute()         // not inside a class method
    }

How to fix:

    class Child extends Parent {
        func compute(this) -> I32 {
            return base.compute() + 1   // valid: calls parent method
        }
    }
)EX"},

        {"T049", R"EX(
Invalid pointer method [T049]

A pointer method was called with the wrong number of arguments or
on an incompatible type.

Example of erroneous code:

    let p: Ptr[I32] = ...
    p.read(42)                 // read() takes no arguments
    p.write()                  // write() requires one argument

How to fix:

    let value = p.read()       // read: no arguments
    p.write(42)                // write: one argument
)EX"},

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

    };
    return db;
}

} // namespace tml::cli::explain
