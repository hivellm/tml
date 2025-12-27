# Variables and Mutability

Variables are fundamental to any programming language. In TML, variables
are immutable by default, which helps prevent bugs and makes code easier
to reason about.

## Declaring Variables

Use the `let` keyword to declare a variable:

```tml
func main() {
    let x = 5
    println(x)  // 5
}
```

## Immutability by Default

By default, variables in TML are immutable. This means once you assign
a value, you cannot change it:

```tml
func main() {
    let x = 5
    x = 6  // Error: cannot assign to immutable variable
}
```

This is intentional. Immutability prevents accidental changes and makes
code easier to understand.

## Mutable Variables

When you need a variable that can change, use `let mut`:

```tml
func main() {
    let mut x = 5
    println(x)  // 5
    x = 6
    println(x)  // 6
}
```

The `mut` keyword explicitly marks the variable as mutable, making it
clear to anyone reading the code that this variable may change.

## Constants

For values that never change and are known at compile time, use `const`:

```tml
const MAX_POINTS = 100000

func main() {
    println(MAX_POINTS)
}
```

Constants:
- Must be uppercase by convention
- Must have a type that can be computed at compile time
- Cannot use `mut`

## Shadowing

You can declare a new variable with the same name as a previous variable.
The new variable *shadows* the previous one:

```tml
func main() {
    let x = 5
    let x = x + 1  // x is now 6
    let x = x * 2  // x is now 12
    println(x)     // 12
}
```

Shadowing is different from mutation because we're creating a new variable
each time. This lets us change the type:

```tml
func main() {
    let spaces = "   "      // String
    let spaces = 3          // Now an integer (shadows the string)
    println(spaces)         // 3
}
```

## Variable Naming

Variable names in TML:
- Must start with a letter or underscore
- Can contain letters, numbers, and underscores
- Are case-sensitive (`x` and `X` are different)
- Should use `snake_case` by convention

```tml
let user_name = "Alice"
let age_in_years = 30
let _private = true
```
