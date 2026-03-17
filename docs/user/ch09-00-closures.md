# Closures

A closure is an anonymous function that can capture values from the scope in which it is defined. Closures are useful for passing short pieces of behavior to other functions, building data-processing pipelines, and writing concise functional-style code.

TML uses the `do` keyword to introduce a closure. This distinguishes closures from named functions defined with `func`, and avoids the symbol noise found in languages that use `|x|` or `\x ->` syntax.

```tml
let double = do(x: I32) x * 2
println(double(21).to_string())  // 42
```

Closures in TML are first-class values. You can:

- Assign them to variables
- Pass them as arguments to functions
- Return them from functions
- Store them as function-typed fields in structs (non-capturing only)

## What This Chapter Covers

This chapter is split into three sections:

- **The Do Syntax** (ch09-01) — the full syntax for writing closures: type annotations, expression bodies, block bodies, multi-parameter closures, and variable capture.
- **Closures as Arguments** (ch09-02) — how to write functions that accept closures, how to return closures from functions, and the distinction between function pointers and capturing closures.

If you have used Rust, closures in TML map closely to Rust's `Fn` closures. The key difference is syntax: where Rust writes `|x| x * 2`, TML writes `do(x) x * 2`.

## Closures vs. Named Functions

Both named functions and closures define callable units of code. The difference is that closures can refer to variables from their enclosing scope, while named functions cannot.

```tml
let threshold = 10

// Named function — cannot access threshold (compile error)
func is_above(x: I32) -> Bool {
    return x > threshold  // Error: threshold not in scope
}

// Closure — captures threshold from the surrounding scope
let is_above = do(x: I32) x > threshold
```

Use named functions for reusable logic that stands on its own. Use closures for short, context-specific behavior that needs access to local state.
