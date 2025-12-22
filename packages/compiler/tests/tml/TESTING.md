# TML Testing Framework

## Current Status

✅ **Implemented:**
- Test file discovery (`*.test.tml`)
- Test runner (`tml test`)
- Decorator parsing (`@test`)

⏳ **Pending Implementation:**
- @test decorator support in codegen
- Automatic test runner generation
- Assertion macros

## Test File Format

Test files must use the `*.test.tml` naming convention:
```
basics.test.tml
closures_generics.test.tml
control_flow.test.tml
```

## Current Testing Approach

Until @test decorators are fully implemented, use this pattern:

```tml
// basics.test.tml

func test_arithmetic() -> I32 {
    println("--- Arithmetic Test ---")

    let result: I32 = 2 + 2
    if result != 4 {
        println("FAIL: 2 + 2 should be 4")
        return 1
    }

    println("Arithmetic: OK")
    return 0
}

func test_variables() -> I32 {
    println("--- Variables Test ---")

    let x: I32 = 10
    if x != 10 {
        println("FAIL: x should be 10")
        return 1
    }

    println("Variables: OK")
    return 0
}

func main() -> I32 {
    println("=== Test Suite ===")

    let mut result: I32 = 0

    result = test_arithmetic()
    if result != 0 { return result }

    result = test_variables()
    if result != 0 { return result }

    println("=== All tests passed! ===")
    return 0
}
```

## Running Tests

```bash
# Run all tests
tml test

# Run specific test
tml test basics

# Run with verbose output
tml test --verbose
```

## Future @test Decorator Support

When fully implemented, tests will look like:

```tml
@test
func arithmetic() {
    assert_eq(2 + 2, 4)
}

@test
func variables() {
    let x: I32 = 10
    assert_eq(x, 10)
}

// No main() needed - generated automatically
```

## Test Organization

Tests are grouped by feature:

- `basics.test.tml` - Core language (arithmetic, variables, conditionals)
- `control_flow.test.tml` - Loops, functions, recursion
- `closures_generics.test.tml` - Closures and generic constraints
- `enums.test.tml` - Enum types and pattern matching
- `structs.test.tml` - Struct types and methods
- `collections.test.tml` - Lists, HashMaps, arrays
- `concurrency.test.tml` - Threads, channels, atomics

## Test Patterns

### Simple Test
```tml
func test_feature() -> I32 {
    if condition {
        return 0  // Success
    }
    return 1  // Failure
}
```

### Test with Multiple Assertions
```tml
func test_multiple() -> I32 {
    if !condition1 { return 1 }
    if !condition2 { return 2 }
    if !condition3 { return 3 }
    return 0
}
```

### Test Suite Pattern
```tml
func main() -> I32 {
    let mut r: I32 = 0

    r = test_one()
    if r != 0 { return r }

    r = test_two()
    if r != 0 { return r }

    return 0
}
```

## Implementation TODO

- [ ] Implement @test codegen support
- [ ] Generate test runner from @test functions
- [ ] Add assert! macro
- [ ] Add assert_eq! macro
- [ ] Add should_panic support
- [ ] Add benchmark support (@bench)
- [ ] Parallel test execution
