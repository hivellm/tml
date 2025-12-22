# TML Stability Annotations Guide

## Overview

TML uses stability annotations to manage API evolution and provide clear signals about which functions are safe to use in production code.

## Stability Levels

### `@stable` - Stable API
- **Guarantee**: Function signature and behavior will not change
- **Use case**: Production code, long-term projects
- **Version**: Marked with the version when it became stable
- **Example**: `print()`, `println()`, `Instant::now()`

### `@unstable` - Unstable API (default)
- **Guarantee**: Function may change in future versions
- **Use case**: Experimental features, new additions
- **Warning**: None (default state)
- **Example**: New concurrency primitives, experimental features

### `@deprecated` - Deprecated API
- **Guarantee**: Function will be removed in a future major version
- **Use case**: Legacy code migration
- **Warning**: Compiler warning with migration suggestion
- **Example**: `print_i32()`, `time_ms()`

## Annotation Format

```cpp
// In env_builtins.cpp
functions_["function_name"] = FuncSig{
    "function_name",
    {params},
    return_type,
    {},
    is_async,
    builtin_span,
    StabilityLevel::Stable,      // or Unstable, Deprecated
    "Deprecation message",        // Optional, for deprecated functions
    "1.0"                          // Version number
};
```

## Examples

### Stable Function
```cpp
// print(s: Str) -> Unit - @stable since v1.0
functions_["print"] = FuncSig{
    "print",
    {make_primitive(PrimitiveKind::Str)},
    make_unit(),
    {},
    false,
    builtin_span,
    StabilityLevel::Stable,
    "",
    "1.0"
};
```

### Deprecated Function
```cpp
// print_i32(n: I32) -> Unit - @deprecated since v1.2
functions_["print_i32"] = FuncSig{
    "print_i32",
    {make_primitive(PrimitiveKind::I32)},
    make_unit(),
    {},
    false,
    builtin_span,
    StabilityLevel::Deprecated,
    "Use toString(value) + print() instead for better type safety",
    "1.2"
};
```

## Current Stability Status

### Core I/O - Stable (v1.0)
- `print(s: Str)` ✅
- `println(s: Str)` ✅

### Time API - Stable (v1.1)
- `Instant::now()` ✅
- `Instant::elapsed(start: I64)` ✅
- `Duration::as_secs_f64(us: I64)` ✅
- `Duration::as_millis_f64(us: I64)` ✅

### Benchmarking - Stable (v1.0)
- `black_box(value: I32)` ✅
- `black_box_i64(value: I64)` ✅

### Deprecated Functions (will be removed in v2.0)
- `print_i32()` ❌ → Use `toString()` + `print()`
- `print_bool()` ❌ → Use `toString()` + `print()`
- `time_ms()` ❌ → Use `Instant::now()`
- `time_us()` ❌ → Use `Instant::now()`
- `time_ns()` ❌ → Use `Instant::now()`

### Unstable Functions (subject to change)
- Memory allocation functions
- Threading primitives
- Channel operations
- Mutex/WaitGroup APIs
- List/HashMap operations
- Buffer operations
- Most string utilities
- Math functions

## Migration Guide

### From `print_i32()` to `print()`:
```tml
// Old (deprecated)
print_i32(42)

// New (stable)
print(toString(42))
```

### From `time_ms()` to `Instant::now()`:
```tml
// Old (deprecated)
let start: I32 = time_ms()
// ... code ...
let elapsed: I32 = time_ms() - start

// New (stable)
let start: I64 = Instant::now()
// ... code ...
let elapsed: I64 = Instant::elapsed(start)
let elapsed_ms: Str = Duration::as_millis_f64(elapsed)
```

## Type Checker Integration

The type checker will emit warnings when deprecated functions are used:

```
warning: function 'print_i32' is deprecated since v1.2
  --> test.tml:5:1
   |
 5 | print_i32(42)
   | ^^^^^^^^^^^^ deprecated function call
   |
   = note: Use toString(value) + print() instead for better type safety
   = help: This function will be removed in v2.0
```

## Versioning Policy

- **Patch versions (1.0.x)**: No API changes, bug fixes only
- **Minor versions (1.x.0)**: New functions can be added, existing stable APIs unchanged
- **Major versions (x.0.0)**: Deprecated functions removed, breaking changes allowed

## Adding New Functions

1. Start as **Unstable** (no annotation)
2. After 2 minor versions without changes → Mark as **Stable**
3. If function needs to change → Mark as **Deprecated** and create new version
4. Remove deprecated functions in next major version

## Compiler Flags

- `--forbid-deprecated`: Treat deprecation warnings as errors
- `--allow-unstable`: Suppress warnings about using unstable APIs
- `--stability-report`: Generate report of all API usage with stability levels

