# Common Standard Behaviors

TML's core and standard libraries define a set of behaviors that types across the ecosystem implement. Understanding these behaviors lets you write generic code that composes cleanly with the standard library and with user-defined types.

## Display and Debug

`Display` and `Debug` control how a value is converted to a human-readable string.

**`Display`** is for end-user output. It is what `println` and string formatting use when you call `to_string()` or embed a value in a format string:

```tml
behavior Display {
    func to_string(this) -> Str
}
```

**`Debug`** is for developer output — diagnostics, logging, and inspection. It typically produces a more verbose, structured representation:

```tml
behavior Debug {
    func debug_string(this) -> Str
}
```

Implementing both for a custom type:

```tml
type Point {
    x: F64,
    y: F64,
}

extend Point with Display {
    func to_string(this) -> Str {
        return "(" + this.x.to_string() + ", " + this.y.to_string() + ")"
    }
}

extend Point with Debug {
    func debug_string(this) -> Str {
        return "Point { x: " + this.x.to_string() + ", y: " + this.y.to_string() + " }"
    }
}

func main() {
    let p = Point { x: 3.0, y: 4.0 }
    println(p.to_string())       // (3.0, 4.0)
    println(p.debug_string())   // Point { x: 3.0, y: 4.0 }
}
```

All primitive types implement both `Display` and `Debug`. The `@derive(Debug)` decorator (see ch14-02) can auto-generate a `Debug` implementation for structs.

## PartialEq and Eq

`PartialEq` enables equality comparison between values of the same type using `==` and `!=`:

```tml
behavior PartialEq {
    func eq(this, other: ref Self) -> Bool
}
```

`Eq` refines `PartialEq` by asserting that the equality relation is also reflexive — that every value is equal to itself. It has no additional methods; it is a marker:

```tml
behavior Eq: PartialEq {}
```

The separation exists for floating-point types: `F32` and `F64` implement `PartialEq` but not `Eq`, because `NaN != NaN` by the IEEE 754 standard. Most types that implement `PartialEq` also implement `Eq`.

```tml
type Color {
    r: U8,
    g: U8,
    b: U8,
}

extend Color with PartialEq {
    func eq(this, other: ref Color) -> Bool {
        return this.r == other.r and this.g == other.g and this.b == other.b
    }
}

extend Color with Eq {}

func main() {
    let red1 = Color { r: 255, g: 0, b: 0 }
    let red2 = Color { r: 255, g: 0, b: 0 }
    let blue = Color { r: 0, g: 0, b: 255 }

    println((red1 == red2).to_string())  // true
    println((red1 == blue).to_string())  // false
}
```

The `@derive(Eq)` decorator generates equality implementations based on field-by-field comparison.

## PartialOrd and Ord

`PartialOrd` provides ordering comparisons (`<`, `<=`, `>`, `>=`, and `cmp`):

```tml
behavior PartialOrd: PartialEq {
    func partial_cmp(this, other: ref Self) -> Maybe[Ordering]
}
```

`Ord` provides a total order — every pair of values of the type must be comparable:

```tml
behavior Ord: Eq + PartialOrd {
    func cmp(this, other: ref Self) -> Ordering
}
```

`Ordering` is an enum with three variants: `Less`, `Equal`, `Greater`. Methods like `is_less()`, `is_greater()`, and `is_equal()` make working with it convenient:

```tml
type Weight {
    grams: U32,
}

extend Weight with PartialEq {
    func eq(this, other: ref Weight) -> Bool {
        return this.grams == other.grams
    }
}

extend Weight with Eq {}

extend Weight with PartialOrd {
    func partial_cmp(this, other: ref Weight) -> Maybe[Ordering] {
        Just(this.cmp(ref other))
    }
}

extend Weight with Ord {
    func cmp(this, other: ref Weight) -> Ordering {
        if this.grams < other.grams { return Ordering.Less }
        if this.grams > other.grams { return Ordering.Greater }
        return Ordering.Equal
    }
}

func main() {
    let light = Weight { grams: 100 }
    let heavy = Weight { grams: 500 }

    println(light.cmp(ref heavy).is_less().to_string())    // true
    println(heavy.cmp(ref light).is_greater().to_string()) // true
}
```

Types that implement `Ord` can be sorted with `List::sort` and used as keys in ordered collections.

## Hash

`Hash` enables a value to be used as a key in `HashMap` and `HashSet`:

```tml
behavior Hash {
    func hash[H: Hasher](this, hasher: mut ref H)
}
```

A correct `Hash` implementation must satisfy: if `a == b` then `a.hash(h) == b.hash(h)`. The reverse is not required — hash collisions are permitted.

```tml
type UserId {
    value: U64,
}

extend UserId with Hash {
    func hash[H: Hasher](this, hasher: mut ref H) {
        this.value.hash(ref hasher)
    }
}
```

The `@derive(Hash)` decorator generates a correct hash implementation for most types automatically.

## Default

`Default` provides a way to construct a "zero value" of a type:

```tml
behavior Default {
    func default() -> Self
}
```

The `default()` method is a static method (no `this` parameter). It is called as `TypeName.default()`:

```tml
type Config {
    timeout_ms: U64,
    max_retries: U32,
    verbose: Bool,
}

extend Config with Default {
    func default() -> Config {
        return Config {
            timeout_ms: 5000,
            max_retries: 3,
            verbose: false,
        }
    }
}

func main() {
    let cfg = Config.default()
    println(cfg.timeout_ms.to_string())  // 5000
}
```

`Default` is used by collection types that need to fill gaps (e.g., resizing an array to a larger size), and by any code that constructs a type with mostly default values that are selectively overridden.

## Duplicate and Copy

`Duplicate` provides explicit cloning of a value. When a type implements `Duplicate`, you can call `.duplicate()` to get a deep copy:

```tml
behavior Duplicate {
    func duplicate(this) -> Self
}
```

`Copy` is a marker behavior for types that are safe to copy implicitly by value — small, stack-allocated types like integers, booleans, and floats. Types that implement `Copy` do not have move semantics; assigning or passing them copies the bits. You cannot implement `Copy` for a type that contains heap-allocated data.

Most primitive types (`I32`, `F64`, `Bool`, etc.) implement `Copy`. Structs that consist entirely of `Copy` fields can also implement `Copy` if you declare it:

```tml
type Point {
    x: F64,
    y: F64,
}

// Point contains only Copy types, so it can be Copy
extend Point with Copy {}
extend Point with Duplicate {
    func duplicate(this) -> Point {
        return Point { x: this.x, y: this.y }
    }
}
```

## From and Into

`From` and `Into` provide value-to-value conversions between types:

```tml
behavior From[T] {
    func from(value: T) -> Self
}

behavior Into[T] {
    func into(this) -> T
}
```

If a type implements `From[T]`, the compiler automatically provides the corresponding `Into` implementation. You typically only need to implement `From`:

```tml
type Celsius {
    degrees: F64,
}

type Fahrenheit {
    degrees: F64,
}

extend Fahrenheit with From[Celsius] {
    func from(c: Celsius) -> Fahrenheit {
        return Fahrenheit { degrees: c.degrees * 9.0 / 5.0 + 32.0 }
    }
}

func main() {
    let boiling = Celsius { degrees: 100.0 }
    let f = Fahrenheit.from(boiling)
    println(f.degrees.to_string())  // 212.0
}
```

`From` is also important for error handling: when a function returns `Outcome[T, E]` and you use `!` to propagate an error of a different type `F`, the compiler calls `E.from(f: F)` automatically. See [Designing Error Types](ch07-03-error-types.md) for details.

## Drop

`Drop` provides a destructor — code that runs when a value goes out of scope:

```tml
behavior Drop {
    func drop(mut this)
}
```

The `drop` method is called automatically when the value's lifetime ends. You rarely need to implement `Drop` explicitly; resource management is typically handled by wrapper types in the standard library. The primary use case is types that wrap raw resources (file handles, network connections, manually allocated memory) that need explicit cleanup:

```tml
type FileHandle {
    fd: I64,
}

extend FileHandle with Drop {
    func drop(mut this) {
        if this.fd >= 0 {
            close_file(this.fd)
            this.fd = -1
        }
    }
}
```

When `FileHandle` goes out of scope — whether by returning from a function, by a `when` arm completing, or by an early return — `drop` is called. The resource is always released, even if the path to scope exit is through an error propagation with `!`.

You cannot call `drop` explicitly. If you need to release a resource before the end of the scope, use a block expression to create an inner scope, or reassign the variable.

## Iterator

`Iterator` enables types to be used with the `loop ... in ...` syntax and with the iterator adapter methods (`map`, `filter`, `fold`, etc.):

```tml
behavior Iterator {
    type Item

    func next(mut this) -> Maybe[This.Item]
}
```

Each call to `next` returns `Just(value)` for the next item, or `Nothing` when the sequence is exhausted. Here is a simple range iterator:

```tml
type Range {
    current: I32,
    end: I32,
}

extend Range {
    func new(start: I32, end: I32) -> Range {
        return Range { current: start, end: end }
    }
}

extend Range with Iterator {
    type Item = I32

    func next(mut this) -> Maybe[I32] {
        if this.current < this.end {
            let value = this.current
            this.current = this.current + 1
            return Just(value)
        }
        return Nothing
    }
}

func main() {
    var r = Range.new(0, 5)
    loop value in r {
        println(value.to_string())  // 0, 1, 2, 3, 4
    }
}
```

Types that implement `Iterator` automatically gain access to the adapter methods defined in the standard library: `map`, `filter`, `take`, `skip`, `fold`, `collect`, and more. See [Iterators](ch10-00-iterators.md) for the full picture.
