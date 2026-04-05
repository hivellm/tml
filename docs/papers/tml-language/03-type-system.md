# 3. Type System

## 3.1 Overview

TML implements a statically-typed system with full type inference based on Hindley-Milner unification, extended with ownership semantics, behavior bounds, and algebraic data types. The type system is designed to provide the same safety guarantees as Rust's type system while using a notation that prioritizes clarity and LLM-friendliness.

The type checker operates in four sequential phases:
1. **Registration** — All type definitions, function signatures, and behavior declarations are registered into the type environment.
2. **Import resolution** — Module imports are resolved and foreign symbols are linked.
3. **Implementation binding** — Behavior implementations (`impl Behavior for Type`) are registered and checked for coherence.
4. **Body checking** — Function bodies are type-checked with full inference, using the registered type environment.

This phased approach enables forward references (a function can call another function defined later in the same module) and supports the query-based compilation model described in Section 5.

---

## 3.2 Primitive Types

TML provides a complete set of fixed-width numeric types with capitalized, self-documenting names:

| Category | Types | Size |
|----------|-------|------|
| Signed integers | `I8`, `I16`, `I32`, `I64` | 1, 2, 4, 8 bytes |
| Unsigned integers | `U8`, `U16`, `U32`, `U64` | 1, 2, 4, 8 bytes |
| Floating point | `F32`, `F64` | 4, 8 bytes |
| Boolean | `Bool` | 1 byte |
| String slice | `Str` | pointer + length |
| Character | `Char` | 4 bytes (Unicode scalar) |
| Unit | `Unit` | 0 bytes |

The naming convention is notable: TML uses capital letters followed by bit width (`I32`, `F64`) rather than Rust's lowercase abbreviations (`i32`, `f64`) or C's inconsistent naming (`int`, `float`, `long long`). This convention:

1. Visually distinguishes types from variables (types are capitalized, variables are lowercase).
2. Immediately communicates the bit width without requiring knowledge of platform-specific defaults.
3. Follows the pattern established by `Bool`, `Str`, `Char` — all types are capitalized identifiers.

---

## 3.3 Type Inference

TML's type inference is based on Algorithm W (Hindley-Milner), extended with:

- **Numeric literal inference**: Integer literals default to `I32`, float literals to `F64`. Explicit annotation is required for other widths: `let x: I64 = 42`.
- **Return type inference**: Function return types are inferred from the body when not explicitly annotated.
- **Generic instantiation**: Type parameters are inferred from argument types at call sites.
- **Closure parameter inference**: Closure parameter types are inferred from the context in which the closure is used.

Example of progressive inference:

```
let items = List.new()         // List[?T] — type parameter unknown
items.push(42)                 // List[I32] — inferred from literal
let doubled = items.map(do(x) x * 2)  // List[I32] — inferred through chain
```

The inference engine uses bidirectional type checking: information flows both from the expression to the expected type (checking mode) and from the expected type to the expression (synthesis mode). This enables patterns like:

```
let result: Maybe[I64] = Just(42)  // 42 inferred as I64 from context
```

### 3.3.1 Comparison with Other Inference Systems

| Language | Inference Level | Limitations |
|----------|----------------|-------------|
| TML | Full HM with ownership | Return types optional, generics inferred |
| Rust | Full HM with lifetimes | Return types required, lifetimes sometimes explicit |
| Go | Limited (`:=` syntax) | No generic inference (until 1.18), return types required |
| C++ | `auto` + template deduction | Complex deduction rules, SFINAE |
| Python | None (runtime typing) | Type hints optional, not enforced by default |
| TypeScript | Structural + contextual | Full inference within functions, annotations at boundaries |

TML's inference is closest to Rust's in power but differs in one critical aspect: **lifetimes are always inferred**. There is no lifetime annotation syntax in TML. This is discussed in detail in Section 4.

---

## 3.4 Algebraic Data Types

TML supports algebraic data types through its `type` and `enum` declarations:

**Product types (structs):**
```
type Point { x: F64, y: F64 }
type Pair[A, B] { first: A, second: B }
```

**Sum types (enums):**
```
enum Shape {
    Circle(F64),
    Rectangle(F64, F64),
    Triangle(F64, F64, F64),
}
```

**Pattern matching** with exhaustiveness checking:
```
func area(shape: Shape) -> F64 {
    when shape {
        Circle(r) -> 3.14159 * r * r,
        Rectangle(w, h) -> w * h,
        Triangle(a, b, c) -> {
            let s = (a + b + c) / 2.0
            (s * (s - a) * (s - b) * (s - c)).sqrt()
        },
    }
}
```

The type checker enforces exhaustiveness: if a new variant is added to `Shape`, all `when` expressions matching on `Shape` must be updated. This is identical to Rust's exhaustiveness checking for `match` expressions.

### 3.4.1 Maybe[T] and Outcome[T, E]

TML's optional and error types are algebraic data types with special compiler support:

```
enum Maybe[T] {
    Just(T),
    Nothing,
}

enum Outcome[T, E] {
    Ok(T),
    Err(E),
}
```

These types are integrated with language-level features:

- **`!` operator** — Propagates errors: `let value = risky_operation()!` returns `Err(e)` from the enclosing function if the operation fails.
- **`?.` optional chaining** — Propagates `Nothing`: `let name = user?.name` evaluates to `Nothing` if `user` is `Nothing`.
- **`let-else` guards** — `let Just(x) = maybe_value else { return Nothing }` provides flat unwrapping.
- **`else` recovery** — `let value = risky()! else default_value` provides a fallback.

This is more extensive than Rust's `?` operator, which only propagates errors. TML's `?.` operator (borrowed from JavaScript/TypeScript/Kotlin) adds optional chaining, and `let-else` (also present in Rust since 1.65) provides flat control flow for sequential unwrapping.

---

## 3.5 Behaviors (Traits)

TML's behavior system is semantically equivalent to Rust's trait system but uses different terminology and syntax:

```
behavior Display {
    func to_string(this) -> Str
}

behavior Ordered: Equal {
    func compare(this, other: ref This) -> Ordering
}
```

Key properties:

- **Behavior bounds**: `func sort[T: Ordered](items: mut ref List[T])` — the type parameter `T` must implement the `Ordered` behavior.
- **Where clauses**: `func merge[K, V](a: HashMap[K, V], b: HashMap[K, V]) -> HashMap[K, V] where K: Hash + Equal` — complex bounds are expressed in where clauses.
- **Default methods**: Behaviors can provide default implementations that types can override.
- **Associated types**: Behaviors can define associated types resolved at implementation time.
- **Behavior inheritance**: `behavior Ordered: Equal` — `Ordered` requires `Equal` as a supertrait.

### 3.5.1 Automatic Derivation

TML provides the `@auto` decorator for automatic behavior implementation:

```
@auto(equal, duplicate, debug, hash, order, default)
type Config {
    name: Str,
    version: I32,
    enabled: Bool,
}
```

This is equivalent to Rust's `#[derive(PartialEq, Clone, Debug, Hash, Ord, Default)]` but with simpler naming:

| TML @auto | Rust #[derive] | Generated Behavior |
|-----------|----------------|-------------------|
| `equal` | `PartialEq`, `Eq` | Structural equality |
| `duplicate` | `Clone` | Deep copy |
| `debug` | `Debug` | Debug formatting |
| `hash` | `Hash` | Hash computation |
| `order` | `PartialOrd`, `Ord` | Comparison ordering |
| `default` | `Default` | Default value construction |

### 3.5.2 Behavior Objects (Dynamic Dispatch)

TML supports behavior objects for runtime polymorphism:

```
func print_all(items: List[dyn Display]) {
    for item in items {
        println(item.to_string())
    }
}
```

The `dyn Display` type is a fat pointer containing a data pointer and a vtable pointer, identical to Rust's `dyn Trait` implementation. This enables heterogeneous collections and runtime dispatch at the cost of losing monomorphization and inlining optimization.

---

## 3.6 Generic Type System

### 3.6.1 Parametric Polymorphism

TML generics use bracket syntax with monomorphization (compile-time specialization):

```
func max[T: Ordered](a: T, b: T) -> T {
    if a.compare(ref b) == Ordering.Greater {
        return a
    }
    return b
}
```

At compile time, `max[I32]`, `max[F64]`, and `max[Str]` are each compiled as separate, specialized functions — identical to Rust's monomorphization strategy and C++ template instantiation.

### 3.6.2 Const Generics

TML supports compile-time constant generic parameters:

```
type Array[T; N] {
    // Fixed-size array of N elements of type T
}

func sum_array[N](arr: ref Array[I32; N]) -> I32 { ... }
```

This enables stack-allocated fixed-size containers without heap allocation, equivalent to Rust's `[T; N]` and C++'s `std::array<T, N>`.

### 3.6.3 Comparison with Other Generic Systems

| Feature | TML | Rust | C++ | Go | Java |
|---------|-----|------|-----|----|------|
| Strategy | Monomorphization | Monomorphization | Template instantiation | Dictionary passing (gcshape stenciling) | Type erasure |
| Syntax | `[T]` | `<T>` | `<T>` | `[T]` | `<T>` |
| Bounds | `T: Behavior` | `T: Trait` | `concept` (C++20) | `~interface` | `extends/super` |
| Const generics | Yes | Yes (stable) | Yes (NTTP) | No | No |
| Specialization | No | Partial (nightly) | Full (SFINAE, if constexpr) | No | No |
| Variadic | No | No (tuple workaround) | Yes (parameter packs) | No | Yes (varargs) |

TML's generic system is closest to Rust's: same monomorphization strategy, same behavior/trait bounds, same const generics. The key difference is syntax (brackets vs angle brackets) and the absence of explicit lifetime parameters.

---

## 3.7 Structural vs Nominal Typing

TML uses **nominal typing**: two types with identical fields are distinct if they have different names. This is the same model as Rust and C++, and differs from Go's structural typing for interfaces.

```
type Meters { value: F64 }
type Seconds { value: F64 }

// These are DIFFERENT types — cannot be mixed
let distance: Meters = Meters { value: 100.0 }
let time: Seconds = Seconds { value: 9.58 }
// distance + time  // COMPILE ERROR: type mismatch
```

However, TML's behavior system uses **structural subtyping** for behavior objects: any type implementing the required methods satisfies a `dyn Behavior` bound, regardless of whether it explicitly declares the implementation. This is checked at compile time through the type checker's behavior resolution.

---

## 3.8 The Impl System

TML uses `impl` blocks for both inherent methods and behavior implementations:

```
// Inherent methods
impl Point {
    func new(x: F64, y: F64) -> This { This { x, y } }
    func distance(this, other: ref Point) -> F64 { ... }
}

// Behavior implementation
impl Display for Point {
    func to_string(this) -> Str {
        `({this.x}, {this.y})`
    }
}
```

The `This` keyword in return position refers to the implementing type, avoiding the need to repeat the type name. This is equivalent to `Self` in Rust but uses a more natural English word.

### 3.8.1 Coherence Rules

TML enforces coherence (the orphan rule): a behavior implementation `impl B for T` must be defined either in the module that defines `B` or the module that defines `T`. This prevents conflicting implementations and is identical to Rust's coherence rules.

---

## 3.9 Summary

TML's type system achieves Rust-equivalent expressiveness and safety through:

1. **Full Hindley-Milner inference** with ownership extensions — reducing annotation burden.
2. **Algebraic data types** with exhaustive pattern matching — preventing unhandled cases.
3. **Behavior bounds** on generics — ensuring type safety without runtime overhead.
4. **Monomorphization** — zero-cost generics at runtime.
5. **No explicit lifetimes** — inference handles all lifetime analysis.

The type system's innovations are primarily in naming and syntax rather than semantics: `behavior` for `trait`, `Maybe` for `Option`, `Outcome` for `Result`, `Duplicate` for `Clone`. These choices reduce the learning curve and improve LLM code generation accuracy without sacrificing any safety guarantee.
