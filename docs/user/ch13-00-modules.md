# Modules and Code Organization

As your TML projects grow, organizing your code becomes increasingly important. TML provides a module system that lets you split your code into multiple files, making it easier to maintain, test, and reuse.

This chapter covers:
- Creating and using local modules
- The module resolution system
- Public and private visibility
- Best practices for code organization

## What is a Module?

A module in TML is simply a `.tml` file that can be imported into another file. When you import a module, you gain access to all of its public (`pub`) functions and types.

## Creating a Local Module

Let's say you're building a calculator application. Instead of putting all code in one file, you can split it into modules.

**math.tml** (your module):
```tml
// Mathematical utility functions

pub func add(a: I32, b: I32) -> I32 {
    return a + b
}

pub func subtract(a: I32, b: I32) -> I32 {
    return a - b
}

pub func multiply(a: I32, b: I32) -> I32 {
    return a * b
}

pub func divide(a: I32, b: I32) -> I32 {
    if b == 0 {
        return 0  // Avoid division by zero
    }
    return a / b
}

// Private helper - not accessible from outside
func validate_input(n: I32) -> Bool {
    return n >= 0
}
```

**main.tml** (your main program):
```tml
use math  // Import the math module

func main() -> I32 {
    let sum: I32 = math::add(10, 5)
    let diff: I32 = math::subtract(10, 5)
    let product: I32 = math::multiply(10, 5)
    let quotient: I32 = math::divide(10, 5)

    print("10 + 5 = ")
    println(sum)
    print("10 - 5 = ")
    println(diff)
    print("10 * 5 = ")
    println(product)
    print("10 / 5 = ")
    println(quotient)

    return 0
}
```

Build and run:
```bash
tml build main.tml -o calculator
./calculator
```

Output:
```
10 + 5 = 15
10 - 5 = 5
10 * 5 = 50
10 / 5 = 2
```

## The `use` Declaration

The `use` keyword imports a local module. When you write:

```tml
use math
```

TML looks for a file named `math.tml` in the same directory as your source file. All public symbols from that module become available using the `module::function` syntax.

### Multiple Imports

You can import multiple modules:

```tml
use math
use strings
use utils

func main() -> I32 {
    let x: I32 = math::add(1, 2)
    let len: I32 = strings::length("hello")
    utils::log("Starting program")
    return 0
}
```

## Public vs Private

By default, all functions and types in a module are **private** - they can only be used within that module. To make something accessible from outside, mark it with `pub`:

```tml
// Private - only usable in this file
func internal_helper() -> I32 {
    return 42
}

// Public - can be imported and used by other modules
pub func public_api() -> I32 {
    return internal_helper()  // Can call private functions internally
}
```

Trying to call a private function from another module will result in a compilation error.

## Module Resolution

When you use `use module_name`, TML searches for the module file in the following order:

1. **Same directory**: Looks for `module_name.tml` in the same directory as the importing file
2. **Standard library**: Checks the built-in standard library modules

For example, if your file structure is:
```
project/
├── main.tml
├── math.tml
└── utils.tml
```

From `main.tml`, you can import both `math` and `utils`:
```tml
use math
use utils
```

## Practical Example: Algorithm Library

Here's a more complete example showing how to organize a collection of algorithms.

**algorithms.tml**:
```tml
// Algorithm implementations

pub func factorial(n: I32) -> I32 {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

pub func fibonacci(n: I32) -> I32 {
    if n <= 1 {
        return n
    }
    var a: I32 = 0
    var b: I32 = 1
    var i: I32 = 2
    loop {
        if i > n {
            break
        }
        let temp: I32 = a + b
        a = b
        b = temp
        i = i + 1
    }
    return b
}

pub func gcd(a: I32, b: I32) -> I32 {
    loop {
        if b == 0 {
            break
        }
        let temp: I32 = b
        b = a % b
        a = temp
    }
    return a
}

pub func is_prime(n: I32) -> Bool {
    if n <= 1 {
        return false
    }
    if n <= 3 {
        return true
    }
    if n % 2 == 0 {
        return false
    }
    var i: I32 = 3
    loop {
        if i * i > n {
            break
        }
        if n % i == 0 {
            return false
        }
        i = i + 2
    }
    return true
}
```

**main.tml**:
```tml
use algorithms

func main() -> I32 {
    println("=== Algorithm Demo ===\n")

    print("Factorial(10): ")
    println(algorithms::factorial(10))

    print("Fibonacci(20): ")
    println(algorithms::fibonacci(20))

    print("GCD(48, 18): ")
    println(algorithms::gcd(48, 18))

    print("Is 17 prime? ")
    if algorithms::is_prime(17) {
        println("Yes")
    } else {
        println("No")
    }

    return 0
}
```

## Standard Library Modules

TML comes with built-in modules that you can import:

```tml
use alloc      // Memory allocation (malloc, free, etc.)
use mem        // Memory operations (copy, set, etc.)
use math       // Mathematical functions
use io         // Input/output operations
```

Standard library modules are resolved after local modules, so if you have a local `math.tml`, it will take precedence over the standard library `math` module.

## Best Practices

### 1. One Responsibility Per Module

Each module should have a single, clear purpose:

```
project/
├── main.tml           # Entry point
├── parser.tml         # Parsing logic
├── validator.tml      # Validation logic
├── formatter.tml      # Output formatting
└── utils.tml          # Shared utilities
```

### 2. Use Descriptive Module Names

Choose names that clearly describe what the module does:

- Good: `string_utils.tml`, `math_ops.tml`, `file_handler.tml`
- Avoid: `stuff.tml`, `misc.tml`, `helpers.tml`

### 3. Keep Modules Focused

If a module is getting too large (hundreds of lines), consider splitting it:

```tml
// Before: one big math.tml

// After: split into focused modules
use math_basic      // add, subtract, multiply, divide
use math_trig       // sin, cos, tan
use math_stats      // mean, median, stddev
```

### 4. Minimize Public API

Only mark functions as `pub` if they need to be used outside the module. This:
- Reduces coupling between modules
- Makes it easier to change internal implementation
- Provides a cleaner API

### 5. Document Public Functions

Add comments to public functions explaining their purpose:

```tml
// Calculates the factorial of n.
// Returns 1 for n <= 1.
// Note: May overflow for n > 12 with I32.
pub func factorial(n: I32) -> I32 {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}
```

## Summary

In this chapter, you learned:

- How to create modules by placing code in separate `.tml` files
- How to import modules with the `use` declaration
- How to call module functions with `module::function` syntax
- The difference between `pub` (public) and private visibility
- How TML resolves module imports
- Best practices for organizing code into modules

Modules are fundamental to writing maintainable TML code. As your projects grow, using modules will help you keep your code organized, testable, and reusable.
