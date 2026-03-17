# Bounds and Where Clauses

A type parameter on its own is completely unconstrained — the compiler allows you to pass any type for it, but as a result you can only perform operations that work on every type in existence. That means you cannot call methods, compare values, or print them. Bounds solve this by specifying which behaviors a type parameter must implement. Where clauses provide a clean place to write complex sets of bounds without cluttering a function signature.

## Single Behavior Bounds

The most common form places a single bound directly in the type parameter list:

```tml
func print_item[T: Display](item: T) {
    println(item.to_string())
}
```

The `T: Display` constraint tells the compiler two things: first, that `print_item` may only be called with a type `T` that implements `Display`; second, that inside the function body, any value of type `T` has a `to_string()` method available. Without the bound, calling `item.to_string()` would be a compile error.

At each call site the compiler checks the constraint is satisfied:

```tml
print_item(42)          // ok — I32 implements Display
print_item("hello")     // ok — Str implements Display
print_item(3.14)        // ok — F64 implements Display

type Opaque { data: I32 }
print_item(Opaque { data: 0 })  // error: Opaque does not implement Display
```

The bound also enables the called function to be statically dispatched — the compiler generates a separate, fully optimized version of `print_item` for each concrete type that appears at a call site. See [Generic Functions and Types](ch06-01-generic-functions-and-types.md) for more on monomorphization.

## Multiple Bounds with `+`

When a type parameter must satisfy more than one behavior, join the bounds with `+`:

```tml
func describe[T: Display + Debug](item: T) {
    println("Display: " + item.to_string())
    println("Debug:   " + item.debug_string())
}
```

The type `T` must implement both `Display` and `Debug`. A type that implements only one of them cannot be used with this function.

Multiple type parameters each carry their own bounds:

```tml
func zip_display[A: Display, B: Display](a: A, b: B) -> Str {
    return a.to_string() + " | " + b.to_string()
}
```

Bounds can reference supertrait relationships. `Ord` requires `Eq + PartialOrd`, so bounding on `Ord` implicitly includes access to equality and ordering methods:

```tml
func clamp[T: Ord](value: T, low: T, high: T) -> T {
    if value.cmp(ref low).is_less() {
        return low
    }
    if value.cmp(ref high).is_greater() {
        return high
    }
    return value
}
```

## Where Clauses

When bounds become long enough to hurt readability, move them to a `where` clause after the return type. The signature stays clean and the constraints are grouped in one place:

```tml
func complex[T, U](a: T, b: U) -> T
    where T: Ord + Display + Default,
          U: Into[T] + Display
{
    let converted: T = b.into()
    let result = if a.cmp(ref converted).is_greater() { a } else { converted }
    println("Comparing " + a.to_string() + " with " + b.to_string())
    return result
}
```

The function above has two type parameters with several bounds each. Writing all of this inline — `func complex[T: Ord + Display + Default, U: Into[T] + Display](a: T, b: U) -> T` — is harder to scan. The `where` clause spreads the bounds across lines and makes each parameter's requirements immediately visible.

Where clauses are also required when a bound involves an associated type of a behavior:

```tml
func sum_items[I](iter: I) -> I64
    where I: Iterator,
          I.Item: Into[I64]
{
    var total: I64 = 0
    loop item in iter {
        total += item.into()
    }
    return total
}
```

The `I.Item: Into[I64]` bound cannot be expressed in the inline `[I: Iterator]` position because `I.Item` is not a type parameter itself — it is an associated type resolved when `I` is known.

### Where Clauses on Behavior Implementations

`where` clauses apply equally to `extend` blocks. Here, `Pair[A, B]` implements `Display` only when both components also implement `Display`:

```tml
type Pair[A, B] {
    first: A,
    second: B,
}

extend Pair[A, B] with Display
    where A: Display,
          B: Display
{
    func to_string(this) -> Str {
        return "(" + this.first.to_string() + ", " + this.second.to_string() + ")"
    }
}
```

This is a *conditional implementation*: `Pair[I32, Str]` implements `Display` because both `I32` and `Str` implement `Display`, but `Pair[I32, Opaque]` does not if `Opaque` lacks `Display`.

Another example: implementing `Ord` for a wrapper type that delegates to its inner type:

```tml
type Wrapper[T] {
    value: T,
}

extend Wrapper[T] with PartialEq where T: PartialEq {
    func eq(this, other: ref Wrapper[T]) -> Bool {
        return this.value == other.value
    }
}

extend Wrapper[T] with Eq where T: Eq {}

extend Wrapper[T] with PartialOrd where T: Ord {
    func partial_cmp(this, other: ref Wrapper[T]) -> Maybe[Ordering] {
        Just(this.value.cmp(ref other.value))
    }
}

extend Wrapper[T] with Ord where T: Ord {
    func cmp(this, other: ref Wrapper[T]) -> Ordering {
        return this.value.cmp(ref other.value)
    }
}
```

Each `extend` block requires only the bounds it actually needs, which keeps implementations narrow and composable.

## Const Generics

Const generics allow a type to be parameterized over a constant value — an integer, boolean, or other compile-time constant — rather than just a type. The syntax places the constant parameter in the same `[...]` list as type parameters, prefixed with its type:

```tml
type FixedArray[T, N: U64] {
    data: [T; N],
}
```

`FixedArray[I32, 4]` is an array of four `I32` values. `FixedArray[F64, 16]` is an array of sixteen `F64` values. Each combination of `T` and `N` produces a distinct monomorphized type.

Extend blocks for const-generic types repeat the const parameter:

```tml
extend FixedArray[T, N: U64] {
    func new() -> FixedArray[T, N] where T: Default {
        return FixedArray { data: [T.default(); N] }
    }

    func len(this) -> U64 {
        return N
    }

    func get(this, index: U64) -> Maybe[ref T] {
        if index < N {
            Just(ref this.data[index as I64])
        } else {
            Nothing
        }
    }
}
```

The value `N` is available as a compile-time constant inside the body. The compiler evaluates `[T.default(); N]` entirely at compile time and emits the correct fixed-size array initialization.

A more complete example — a matrix type parameterized over both dimensions:

```tml
type Matrix[T, ROWS: U64, COLS: U64] {
    data: [[T; COLS]; ROWS],
}

extend Matrix[T, ROWS: U64, COLS: U64]
    where T: Default + Display
{
    func new() -> Matrix[T, ROWS, COLS] {
        return Matrix { data: [[T.default(); COLS]; ROWS] }
    }

    func rows(this) -> U64 { return ROWS }
    func cols(this) -> U64 { return COLS }

    func get(this, row: U64, col: U64) -> ref T {
        return ref this.data[row as I64][col as I64]
    }

    func set(mut this, row: U64, col: U64, value: T) {
        this.data[row as I64][col as I64] = value
    }
}

func main() {
    var m: Matrix[I32, 3, 3] = Matrix.new()
    m.set(0, 0, 1)
    m.set(1, 1, 1)
    m.set(2, 2, 1)

    println("Rows: " + m.rows().to_string())  // 3
    println("Cols: " + m.cols().to_string())  // 3
    println("m[1][1]: " + m.get(1, 1).to_string())  // 1
}
```

Const generics are particularly useful for buffer types, fixed-capacity stacks, SIMD vectors, and compile-time validated array dimensions in linear algebra.

## Combining Bounds and Where Clauses: A Complete Example

The following function merges two sorted slices using all the constructs from this chapter:

```tml
func merge_sorted[T](left: ref List[T], right: ref List[T]) -> List[T]
    where T: Ord + Duplicate
{
    var result: List[T] = List.new()
    var i: I64 = 0
    var j: I64 = 0

    loop {
        if i >= left.len() {
            loop k in j to right.len() {
                result.push(right.get(k).duplicate())
            }
            break
        }
        if j >= right.len() {
            loop k in i to left.len() {
                result.push(left.get(k).duplicate())
            }
            break
        }
        if left.get(i).cmp(ref right.get(j)).is_less_or_equal() {
            result.push(left.get(i).duplicate())
            i += 1
        } else {
            result.push(right.get(j).duplicate())
            j += 1
        }
    }

    return result
}

func main() {
    let a: List[I32] = [1, 3, 5, 7]
    let b: List[I32] = [2, 4, 6, 8]
    let merged = merge_sorted(ref a, ref b)
    println(merged.to_string())  // [1, 2, 3, 4, 5, 6, 7, 8]
}
```

The `where` clause expresses that `T` must be orderable (to compare elements) and duplicable (because the function must copy elements into the output list). The caller knows exactly what types can be passed — any type that satisfies both constraints.
