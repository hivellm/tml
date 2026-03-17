# Behavior Objects and Dynamic Dispatch

## Static vs Dynamic Dispatch

When you write a generic function with a behavior bound, the compiler resolves which implementation to call at compile time. This is *static dispatch*, and it is the default in TML:

```tml
func print_area[S: Shape](shape: S) {
    println(shape.area().to_string())
}
```

For each concrete type `S` that is ever passed to `print_area`, the compiler generates a specialized copy of the function. This is called *monomorphization*. The resulting code is fast — every call is direct, with no indirection — but it can increase binary size when the same function is instantiated for many types.

Sometimes you do not know the concrete type at compile time. You might store a list of shapes where each element could be a `Circle` or a `Rectangle`, and the mix is decided at runtime. This is where *dynamic dispatch* and behavior objects come in.

## The `dyn` Keyword

A behavior object is written as `dyn BehaviorName`. It is a pointer-sized fat pointer that contains two components: a pointer to the data and a pointer to a *vtable* — a table of function pointers for that type's implementation of the behavior. Method calls through a `dyn` reference are resolved by looking up the appropriate function pointer in the vtable at runtime.

```tml
func print_area_dyn(shape: ref dyn Shape) {
    println(shape.area().to_string())
}
```

The function no longer has a type parameter. It accepts any `Shape` implementation through the common `dyn Shape` reference, and the correct `area` method is dispatched at runtime.

## Storing Mixed Types in a Collection

The most common use case for behavior objects is collections that need to hold values of different concrete types that share a behavior:

```tml
behavior Shape {
    func area(this) -> F64
    func perimeter(this) -> F64
}

type Circle {
    radius: F64,
}

type Rectangle {
    width: F64,
    height: F64,
}

extend Circle with Shape {
    func area(this) -> F64 { 3.14159 * this.radius * this.radius }
    func perimeter(this) -> F64 { 2.0 * 3.14159 * this.radius }
}

extend Rectangle with Shape {
    func area(this) -> F64 { this.width * this.height }
    func perimeter(this) -> F64 { 2.0 * (this.width + this.height) }
}

func print_all_shapes(shapes: ref List[dyn Shape]) {
    loop shape in *shapes {
        println("Area: " + shape.area().to_string())
    }
}

func main() {
    let circle = Circle { radius: 5.0 }
    let rect = Rectangle { width: 10.0, height: 5.0 }

    let shapes: List[dyn Shape] = [circle, rect]
    print_all_shapes(ref shapes)
    // Area: 78.53975
    // Area: 50.0
}
```

Without `dyn Shape`, a `List[S]` can only hold values of one concrete type `S`. With `List[dyn Shape]`, you can mix any number of types that implement `Shape`.

## Behavior Object Safety

Not every behavior can be used as a behavior object. A behavior is *object-safe* — usable as `dyn Behavior` — only if the compiler can build a vtable for it. The restrictions are:

1. **No methods that return `Self` by value.** `Self` in a vtable context has no concrete size.
2. **No generic methods.** A method like `func compare[T](this, other: T) -> Bool` would require a separate vtable entry per `T`, which is impossible.
3. **No static methods (methods without `this`).** Static methods are not dispatched through an instance.

If you try to use a non-object-safe behavior with `dyn`, the compiler produces an error explaining which method violates the rules.

The following behavior is object-safe:

```tml
behavior Serializable {
    func serialize(this) -> Str
    func byte_size(this) -> U64
}
```

This one is not, because of the generic method:

```tml
// NOT object-safe
behavior Codec {
    func encode[W: Writer](this, writer: mut ref W)
}
```

To make `Codec` usable for dynamic dispatch, you would need to redesign it — for example, by making `Writer` a behavior object itself.

## When to Use Dynamic Dispatch

Use `dyn Behavior` when:

- You need a **heterogeneous collection** — a list or map of values of different concrete types that share a behavior.
- The concrete type is determined at **runtime** (e.g., it is read from configuration, selected by user input, or returned from a factory function).
- **Binary size** is a concern and you are willing to pay for runtime dispatch overhead to avoid monomorphizing the same code for dozens of types.

Use generic bounds (`T: Behavior`) when:

- The type is known at compile time.
- **Performance** is critical — static dispatch inlines better and avoids the vtable indirection.
- You need to call methods that are not object-safe (e.g., generic methods).
- The function needs to return a value of the same type (`T`) rather than working through a reference.

## Dynamic Dispatch Through Function Arguments

Behavior objects can be passed as function arguments using `ref dyn Behavior`:

```tml
func describe(item: ref dyn Describable) {
    println(item.describe())
}

func main() {
    let alice = Person { name: "Alice", age: 30 }
    let circle = Circle { radius: 2.0 }

    describe(ref alice)   // Alice is 30 years old
    describe(ref circle)  // circle with radius 2.0
}
```

Owned behavior objects — storing the value by value rather than by reference — require heap allocation through `Heap[dyn Behavior]`:

```tml
use core::alloc::heap::Heap

let shape: Heap[dyn Shape] = Heap::new(Circle { radius: 3.0 })
println(shape.area().to_string())  // 28.27431
```

`Heap[dyn Shape]` stores the `Circle` on the heap and attaches the `Shape` vtable. The value is freed when `shape` goes out of scope.
