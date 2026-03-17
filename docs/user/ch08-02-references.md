# References and Borrowing

Moving ownership into every function that needs to read a value would be impractical.
A function that prints a string should not consume it. A sorting function should
not take permanent ownership of the list it sorts. TML addresses this through
borrowing: a way to grant temporary access to a value without transferring
ownership.

A borrow is represented by a reference. References are created with the `ref`
keyword and typed as `ref T` (immutable) or `mut ref T` (mutable).

## Immutable References

An immutable reference provides read-only access to a value. Creating one does
not move the value:

```tml
func length(s: ref Str) -> U64 {
    return s.len()
}

func main() {
    let greeting = "hello, world"
    let n = length(ref greeting)   // borrow greeting for the duration of the call
    println(n.to_string())         // 12
    println(greeting)              // still valid — ownership never moved
}
```

Inside `length`, `s` is a reference to the caller's string. The function can
read through it but cannot modify the underlying data. When `length` returns,
the borrow ends and `greeting` is fully available to `main` again.

### Multiple Immutable Borrows

You can create any number of immutable references to the same value simultaneously.
Reading shared data from many places at once is safe:

```tml
let value = 42

let r1 = ref value
let r2 = ref value
let r3 = ref value

println(*r1 + *r2 + *r3)   // 126 — all three references are active at once
```

The `*` operator dereferences a reference, yielding the underlying value.

## Mutable References

A mutable reference grants write access. Create one with `mut ref`:

```tml
func append(s: mut ref Str, suffix: Str) {
    s.push_str(suffix)
}

func main() {
    var greeting = "hello"
    append(mut ref greeting, ", world")
    println(greeting)   // "hello, world"
}
```

Note that the variable being mutably borrowed must itself be declared with `var`.
You cannot take a mutable reference to a `let` binding because `let` signals that
the value will not change.

## The Borrowing Rules

The borrow checker enforces two rules that together prevent data races and
invalidated references:

**Rule 1: At any given time, you may have either any number of immutable
references or exactly one mutable reference — not both.**

**Rule 2: References must always be valid. A reference cannot outlive the
value it points to.**

These rules are checked at compile time. There is no runtime cost.

### Rule 1 in Practice

The mutual exclusion between immutable and mutable borrows prevents
a class of bugs where one part of the code reads a value while another
part is modifying it:

```tml
var x = 5

let r1 = ref x          // immutable borrow starts
let r2 = ref x          // another immutable borrow — ok

// let m = mut ref x    // ERROR: cannot borrow x as mutable while
                         // immutable borrows r1 and r2 exist

println(*r1 + *r2)      // r1 and r2 used here — borrows active

// After this point, r1 and r2 are no longer used.
// The borrows end here (non-lexical lifetimes).

let m = mut ref x       // ok: immutable borrows have ended
*m = 10
println(x)              // 10
```

TML uses non-lexical lifetimes: a borrow ends at the last point it is used,
not at the end of the enclosing block. This means the code above compiles
without introducing a nested scope to end `r1` and `r2` early.

### Rule 2 in Practice

A reference cannot outlive the value it refers to. The compiler rejects any
program where this would happen:

```tml
func make_ref() -> ref I32 {
    let x = 42
    return ref x    // ERROR: x does not live long enough
                    // x is dropped when make_ref() returns
}
```

The compiler catches this at compile time. In TML there is no lifetime annotation
syntax — the analysis runs internally and produces a diagnostic that describes
which value the reference outlives.

## Borrowing Function Parameters

The most common use of references is in function parameters. Annotating a
parameter as `ref T` expresses "this function needs to read the value" without
claiming ownership:

```tml
func sum(numbers: ref List[I32]) -> I32 {
    var total = 0
    loop i in 0 to numbers.len() {
        total = total + numbers.get(i)
    }
    return total
}

func main() {
    let nums = [1, 2, 3, 4, 5]
    let s = sum(ref nums)   // nums is borrowed, not consumed
    println(s.to_string())  // 15
    println(nums.len().to_string())  // nums is still valid: 5
}
```

Annotating a parameter as `mut ref T` expresses "this function needs to modify
the value in place":

```tml
func double_all(numbers: mut ref List[I32]) {
    loop i in 0 to numbers.len() {
        let current = numbers.get(i)
        numbers.set(i, current * 2)
    }
}

func main() {
    var nums = [1, 2, 3]
    double_all(mut ref nums)
    // nums is now [2, 4, 6]
}
```

## Method Receiver Syntax

Methods use `this`, `ref this`, and `mut ref this` to express the same
distinctions for their receiver:

```tml
type Counter {
    value: I32,
}

extend Counter {
    // Takes ownership — consumes the counter
    func into_value(this) -> I32 {
        return this.value
    }

    // Immutable borrow — reads the counter
    func get(ref this) -> I32 {
        return this.value
    }

    // Mutable borrow — modifies the counter
    func increment(mut ref this) {
        this.value = this.value + 1
    }
}

func main() {
    var c = Counter { value: 0 }

    c.increment()           // mut ref this — c is mutably borrowed
    c.increment()
    println(c.get().to_string())  // ref this — c is immutably borrowed: 2

    let v = c.into_value()  // this — c is moved, c is no longer valid
}
```

## Slices: References to Contiguous Data

A slice is a reference to a contiguous portion of a collection. Slices allow
functions to work on a part of a list without the function knowing or caring
about the full list's bounds:

```tml
func first_three(data: ref Slice[I32]) {
    loop i in 0 to data.len() {
        println(data.get(i).to_string())
    }
}

func main() {
    let nums = [10, 20, 30, 40, 50]
    first_three(ref nums.slice(0, 3))  // borrows [10, 20, 30]
    // nums is still valid
}
```

A `MutSlice[T]` is a mutable slice that allows modification through the reference:

```tml
func zero_out(data: mut ref MutSlice[I32]) {
    loop i in 0 to data.len() {
        data.set(i, 0)
    }
}
```

## References and Smart Pointers

References work naturally alongside smart pointers. When you hold a `Heap[T]`,
`Shared[T]`, or `Sync[T]`, you can borrow a reference to the inner value through
them:

```tml
use std::alloc::Heap

let boxed = Heap::new(42)
let r: ref I32 = boxed.deref()   // borrow the inner I32
println(r.to_string())            // 42
// boxed still owns the allocation; r borrows it
```

The borrow rules apply equally: you cannot have a mutable reference to the inner
value through a shared smart pointer at the same time as any other reference.

## Common Patterns

### Returning Data Without Copying

Pass a mutable reference in so the callee can write results directly into
caller-owned storage, avoiding a heap allocation for the return value:

```tml
func build_message(name: ref Str, out: mut ref Str) {
    out.push_str("Hello, ")
    out.push_str(name)
    out.push_str("!")
}

func main() {
    var msg = ""
    build_message(ref "Alice", mut ref msg)
    println(msg)   // "Hello, Alice!"
}
```

### Borrowing Through Conditionals

Because non-lexical lifetimes are used, borrows can span if/else branches
as long as they do not conflict with a mutable borrow in the same scope:

```tml
func describe(value: ref Maybe[I32]) -> Str {
    when value {
        Just(ref n) => "contains " + n.to_string()
        Nothing     => "empty"
    }
}
```

### Holding a Reference in a Struct

Struct fields can be reference types, but the struct then cannot outlive the
value it references. The compiler enforces this automatically:

```tml
type Excerpt {
    text: ref Str,
}

func make_excerpt(source: ref Str) -> Excerpt {
    return Excerpt { text: source }
}
```

The returned `Excerpt` is only valid as long as the `source` it borrows from.
The compiler will reject any use of the `Excerpt` after `source` is dropped.
