# Comments

Comments let you annotate your code with explanations that the compiler ignores. TML supports
three styles: line comments, block comments, and documentation comments.

## Line Comments

A line comment begins with `//` and extends to the end of the line:

```tml
func main() {
    // This entire line is a comment.
    let x: I32 = 5   // This comment follows code on the same line.
    println(x.to_string())
}
```

Line comments are the most common form and are appropriate for most in-code annotations.

## Block Comments

A block comment begins with `/*` and ends with `*/`. It can span multiple lines:

```tml
func main() {
    /*
       This is a block comment.
       It can span as many lines as needed.
    */
    let x: I32 = 5
    println(x.to_string())
}
```

Block comments can be nested:

```tml
/*
   Outer comment.
   /* Inner comment — still valid. */
   Back in the outer comment.
*/
```

Block comments are useful for temporarily disabling a section of code during development or for
writing a long explanation that spans multiple lines in the middle of a function.

## Documentation Comments

A documentation comment begins with `///` and documents the item immediately below it. The TML
documentation generator uses these comments to produce HTML API references:

```tml
/// Returns the factorial of `n`.
///
/// Panics if `n` is negative.
func factorial(n: I32) -> I32 {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}
```

Documentation comments support a small set of Markdown conventions: paragraphs, code blocks
(fenced with triple backticks), bold, and bullet lists.

```tml
/// Classifies an integer as negative, zero, or positive.
///
/// # Examples
///
/// ```tml
/// let result = classify(-5)
/// assert_eq(result, "negative")
/// ```
func classify(n: I32) -> Str {
    if n < 0 {
        return "negative"
    }
    if n == 0 {
        return "zero"
    }
    return "positive"
}
```

Write documentation comments on all public functions, types, and constants. For private
implementation details, line comments are sufficient.

## What to Put in Comments

Comments should explain *why*, not *what*. The code already shows what it does. A comment that
simply restates the code adds noise without value:

```tml
// Poor comment — restates the obvious
x = x + 1  // increment x by 1

// Good comment — explains the reasoning
x = x + 1  // adjust for zero-based indexing before passing to the C API
```

Comments that become outdated are worse than no comments. When you change code, update the
comments at the same time.

Good uses for comments:

- Explaining a non-obvious algorithm or formula
- Documenting a constraint or assumption that cannot be expressed in the type system
- Noting a workaround for a known external limitation
- Providing a reference to the specification or algorithm the code implements

```tml
// FNV-1a hash — fast, good distribution for short strings.
// See: http://www.isthe.com/chongo/tech/comp/fnv/
func fnv1a(data: ref Str) -> U64 {
    var hash: U64 = 14695981039346656037
    loop i in 0 to data.len() {
        hash = hash ^ (data.byte_at(i) as U64)
        hash = hash * 1099511628211
    }
    return hash
}
```
