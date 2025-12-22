# Comments

Comments let you document your code. The compiler ignores them, but they
help other developers (and your future self) understand your code.

## Line Comments

Use `//` for single-line comments:

```tml
func main() {
    // This is a comment
    let x = 5  // This is also a comment
    println(x)
}
```

## Block Comments

Use `/* */` for multi-line comments:

```tml
func main() {
    /* This is a
       multi-line
       comment */
    let x = 5
    println(x)
}
```

Block comments can be nested:

```tml
/* Outer comment
   /* Inner comment */
   Still in outer comment
*/
```

## Documentation Comments

Use `///` for documentation comments that describe functions:

```tml
/// Calculates the factorial of a number.
/// Returns 1 for n <= 1.
func factorial(n: I32) -> I32 {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}
```

## Best Practices

1. **Explain why, not what**: The code shows *what*, comments should explain *why*

```tml
// Bad: increment x by 1
x = x + 1

// Good: adjust for zero-based indexing
x = x + 1
```

2. **Keep comments up to date**: Outdated comments are worse than no comments

3. **Don't over-comment**: Self-explanatory code doesn't need comments

```tml
// Bad: set name to "Alice"
let name = "Alice"

// Good: no comment needed
let name = "Alice"
```

4. **Use comments for complex logic**:

```tml
// Using bitwise AND to check if number is even
// (faster than modulo for hot paths)
let is_even = (n & 1) == 0
```
