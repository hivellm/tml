# TML Compiler Test Suite

Tests use the `@test` decorator and `*.test.tml` naming convention.

## Test Files

✅ **Core Tests**:
- `basics.test.tml` - Arithmetic, variables, conditionals (3 tests)
- `closures.test.tml` - Closures and function types (4 tests)
- `structs.test.tml` - Struct types and methods
- `enums.test.tml` - Enum declarations and pattern matching
- `collections.test.tml` - Lists, HashMaps, Buffers

## Test Format

Tests use `@test` decorator - **no main() needed**:

```tml
@test
func arithmetic() -> I32 {
    println("Testing arithmetic...")

    let result: I32 = 2 + 2
    if result != 4 {
        println("FAIL")
        return 1  // Non-zero = failure
    }

    println("OK")
    return 0  // Zero = success
}

@test
func variables() -> I32 {
    // ... another test
    return 0
}

// No main() - auto-generated!
```

## Running Tests

```bash
# Run all tests in a file
cd packages/compiler/build
./Debug/tml.exe run ../tests/tml/basics.test.tml

# Build test executable
./Debug/tml.exe build ../tests/tml/closures.test.tml

# Type check only
./Debug/tml.exe check ../tests/tml/enums.test.tml
```

## How @test Works

1. Compiler detects functions decorated with `@test`
2. Auto-generates `main()` that:
   - Calls each test function
   - Stops on first failure
   - Returns error code
3. No manual test orchestration needed!

## Test Status

### ✅ Working
- @test decorator (fully implemented!)
- Basic language (arithmetic, variables, loops, functions)
- Structs and enums
- Closures without capture
- Collections (List, HashMap)

### 🚧 Partial
- Closures with environment capture (type-checked, not generated)
- Where clause constraints (parsed, not enforced)

### ⏳ Future
- Assertion macros (`assert_eq!`, `assert!`)
- Benchmarking (`@bench`)
- Test filtering by name
