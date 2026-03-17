# Appendix A - Keywords

TML reserves 57 words that may not be used as identifiers. They are listed
below, grouped by purpose. Each entry states what the keyword does; the
relevant chapter provides full syntax and examples.

---

## Declarations

These keywords introduce named items — functions, types, modules, and so on.

| Keyword | Description |
|---------|-------------|
| `func` | Declares a function. |
| `type` | Declares a struct or type alias. |
| `enum` | Declares an enumeration. |
| `let` | Declares an immutable variable binding. |
| `var` | Declares a mutable variable binding. |
| `const` | Declares a compile-time constant. |
| `mod` | Declares a module. |
| `use` | Imports a name into the current scope. |
| `pub` | Makes an item visible outside its module. |
| `behavior` | Declares a behavior (similar to a trait or interface). |
| `extend` | Adds methods to an existing type without modifying it. |
| `impl` | Implements a behavior for a type. |
| `static` | Declares a static member that belongs to the type, not an instance. |
| `where` | Introduces generic constraints on a function or type. |

### OOP Declaration Keywords

The following keywords support the optional object-oriented programming model
(see Chapter 15). They are reserved whether or not OOP features are in use.

| Keyword | Description |
|---------|-------------|
| `class` | Declares a class with inheritance support. |
| `interface` | Declares an interface. |
| `abstract` | Marks a class or method that must be overridden. |
| `sealed` | Prevents a class from being subclassed. |
| `virtual` | Marks a method that may be overridden by a subclass. |
| `override` | Marks a method that overrides a parent method. |
| `namespace` | Declares a namespace for organizing names. |
| `new` | Constructs an instance using a constructor. |

---

## Control Flow

These keywords direct the execution path of a program.

| Keyword | Description |
|---------|-------------|
| `if` | Begins a conditional expression. |
| `then` | Separates the condition from its body in a single-line `if`. |
| `else` | Provides the alternative branch of an `if` expression. |
| `when` | Pattern-matching expression (similar to `match` in other languages). |
| `loop` | Begins an unconditional loop. |
| `while` | Adds a condition to a `loop`. |
| `for` | Iterates over a range or iterable with `in`. |
| `in` | Separates the binding from the iterable in a `for` loop. |
| `to` | Produces an exclusive range: `0 to 10` yields 0 through 9. |
| `through` | Produces an inclusive range: `1 through 10` yields 1 through 10. |
| `break` | Exits the nearest enclosing loop. |
| `continue` | Skips to the next iteration of the nearest enclosing loop. |
| `return` | Returns a value from a function. |
| `do` | Introduces a closure (lambda) expression. |
| `catch` | Catches a propagated error in a `when` arm or expression. |

---

## Types and Values

Keywords that describe or interact with types and their values.

| Keyword | Description |
|---------|-------------|
| `true` | Boolean true literal. |
| `false` | Boolean false literal. |
| `null` | Null pointer literal; has type `*Unit` (opaque pointer). |
| `this` | Refers to the current instance inside a method or extend block. |
| `This` | Refers to the type of the current instance (usable in impl blocks). |
| `Self` | Alias for the type being implemented; interchangeable with `This`. |
| `ref` | Creates a shared (immutable) reference, or annotates a reference type. |
| `mut` | Marks a value or binding as mutable. |
| `dyn` | Enables dynamic dispatch through a behavior object. |
| `as` | Casts a value to a different type. |

---

## Logic

TML uses keywords rather than symbols for logical operators, which improves
readability and removes ambiguity with bitwise operators.

| Keyword | Description |
|---------|-------------|
| `and` | Logical AND with short-circuit evaluation. |
| `or` | Logical OR with short-circuit evaluation. |
| `not` | Logical negation. |

---

## Visibility

| Keyword | Description |
|---------|-------------|
| `pub` | Public — visible to all modules (also listed under Declarations). |
| `private` | Explicitly private — visible only within the declaring module. |
| `protected` | Visible to the declaring type and its subclasses. |

---

## Memory and Safety

| Keyword | Description |
|---------|-------------|
| `lowlevel` | Introduces a block that may use memory intrinsics, raw pointers, and FFI. Signals that the programmer takes responsibility for safety. |
| `move` | Explicitly transfers ownership of a value to a new binding. |

---

## Error Handling Contracts

| Keyword | Description |
|---------|-------------|
| `requires` | Specifies a precondition that must hold when a function is called. |
| `ensures` | Specifies a postcondition that must hold when a function returns. |

---

## Other

| Keyword | Description |
|---------|-------------|
| `with` | Opens a context expression (for resource management). |
| `extends` | Declares that a class inherits from another class. |
| `implements` | Declares that a class or type implements an interface or behavior. |

---

## Compile-Time Constants

These identifiers are substituted by the compiler at the point of use. They are
not user-definable and behave like keywords.

| Constant | Type | Description |
|----------|------|-------------|
| `__FILE__` | `Str` | Absolute path of the current source file. |
| `__LINE__` | `I64` | Line number of the current source location. |
| `__DIRNAME__` | `Str` | Directory containing the current source file. |
| `__FUNC__` | `Str` | Name of the immediately enclosing function. |

```tml
func locate() {
    println("Called from ${__FUNC__} at ${__FILE__}:${__LINE__}")
}
```

---

## Naming Conventions

These are not enforced by the compiler but are followed universally in the
standard library and expected in idiomatic TML code.

| Style | Used For | Examples |
|-------|----------|---------|
| `PascalCase` | Types, behaviors, enums, enum variants | `HttpClient`, `Maybe`, `IoError` |
| `snake_case` | Functions, variables, module names | `parse_json`, `user_name`, `http_client` |
| `SCREAMING_SNAKE_CASE` | Constants | `MAX_SIZE`, `PI`, `DEFAULT_TIMEOUT` |
| `_snake_case` | Intentionally unused bindings | `_result`, `_unused` |
