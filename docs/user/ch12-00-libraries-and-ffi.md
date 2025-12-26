# Building Libraries and C Interoperability

One of TML's powerful features is its ability to create libraries that can be used from other programming languages, particularly C and C++. This chapter will show you how to build both static and dynamic libraries, and how to make your TML code interoperable with C.

## Creating C-Compatible Libraries

TML can compile your code into libraries that other programs can use. There are two main types of libraries:

- **Static libraries** (`.lib` on Windows, `.a` on Linux/macOS): The library code is copied into your executable at compile time
- **Dynamic libraries** (`.dll` on Windows, `.so` on Linux, `.dylib` on macOS): The library code is loaded at runtime

### Writing a Library

Let's create a simple math library. Create a file called `math.tml`:

```tml
module math

// Public functions are automatically exported
pub func add(a: I32, b: I32) -> I32 {
    return a + b
}

pub func multiply(x: I32, y: I32) -> I32 {
    return x * y
}

pub func factorial(n: I32) -> I32 {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

// Private functions are not exported
func helper(n: I32) -> I32 {
    return n * 2
}
```

Only functions marked with `pub` (public) are exported from your library. Private functions are only available within your TML module.

### Building a Static Library

To build a static library:

```bash
tml build math.tml --crate-type lib
```

This creates:
- **Windows**: `build/debug/math.lib`
- **Linux/macOS**: `build/debug/libmath.a`

### Building a Dynamic Library

To build a dynamic library:

```bash
tml build math.tml --crate-type dylib
```

This creates:
- **Windows**: `build/debug/math.dll` (plus `math.lib` for linking)
- **Linux**: `build/debug/libmath.so`
- **macOS**: `build/debug/libmath.dylib`

## Auto-Generating C Headers

TML can automatically create a C header file for your library, making it easy to use from C/C++ programs:

```bash
tml build math.tml --crate-type lib --emit-header
```

This generates both the library and a header file `build/debug/math.h`:

```c
#ifndef TML_MATH_H
#define TML_MATH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// TML library: math
// Auto-generated C header for FFI

int32_t tml_add(int32_t a, int32_t b);
int32_t tml_multiply(int32_t x, int32_t y);
int32_t tml_factorial(int32_t n);

#ifdef __cplusplus
}
#endif

#endif // TML_MATH_H
```

Notice that:
- TML functions are exported with a `tml_` prefix to avoid naming conflicts
- TML types are automatically converted to C types (e.g., `I32` → `int32_t`)
- The header includes proper include guards and C++ compatibility

## Using Your TML Library from C

Here's how to use your TML library from a C program:

**main.c:**
```c
#include <stdio.h>
#include "build/debug/math.h"

int main() {
    int32_t sum = tml_add(5, 3);
    printf("5 + 3 = %d\n", sum);

    int32_t product = tml_multiply(4, 7);
    printf("4 * 7 = %d\n", product);

    int32_t fact = tml_factorial(5);
    printf("5! = %d\n", fact);

    return 0;
}
```

Compile and link your C program:

```bash
# Windows
clang main.c -o main.exe build/debug/math.lib

# Linux/macOS
clang main.c -o main build/debug/libmath.a
```

Run it:

```bash
./main
```

Output:
```
5 + 3 = 8
4 * 7 = 28
5! = 120
```

## Type Conversions

When you export TML functions, the types are automatically converted to C-compatible types:

| TML Type | C Type | Example |
|----------|--------|---------|
| `I8` to `I64` | `int8_t` to `int64_t` | Integer types |
| `U8` to `U64` | `uint8_t` to `uint64_t` | Unsigned types |
| `F32`, `F64` | `float`, `double` | Floating point |
| `Bool` | `bool` | Boolean (needs `stdbool.h`) |
| `Str` | `const char*` | String pointer |
| `ref T` | `T*` | Pointer to type T |
| `Ptr[T]` | `T*` | Raw pointer |

The auto-generated header automatically includes the necessary C headers (`stdint.h` for fixed-size integers, `stdbool.h` for booleans).

## Complete Example: A String Library

Let's create a slightly more complex library that works with strings:

**stringutils.tml:**
```tml
module stringutils

pub func string_length(s: Str) -> I32 {
    var len: I32 = 0
    var i: I32 = 0

    // Count characters until null terminator
    loop {
        if s.byte_at(i) == 0 {
            break
        }
        len = len + 1
        i = i + 1
    }

    return len
}

pub func string_compare(a: Str, b: Str) -> Bool {
    return a == b
}
```

Build with header:
```bash
tml build stringutils.tml --crate-type lib --emit-header
```

Use from C:
```c
#include <stdio.h>
#include "build/debug/stringutils.h"

int main() {
    const char* hello = "Hello, World!";
    int32_t len = tml_string_length(hello);
    printf("Length: %d\n", len);

    bool same = tml_string_compare("test", "test");
    printf("Same: %s\n", same ? "true" : "false");

    return 0;
}
```

## Best Practices

1. **Use `pub` strategically**: Only export functions that should be part of your public API. Keep implementation details private.

2. **Stick to simple types**: For maximum C compatibility, use primitive types (`I32`, `F64`, `Bool`, etc.) and pointers. Complex TML types like `Maybe[T]` or `Vec[T]` won't translate well to C.

3. **Generate headers**: Always use `--emit-header` when creating libraries for C/C++. It ensures type correctness and provides documentation.

4. **Test both ways**: Test your library from both TML and C to ensure the interface works correctly.

5. **Document your API**: Add comments to your public functions explaining their purpose, parameters, and return values.

## Summary

In this chapter, you learned:

- How to build static and dynamic libraries with `--crate-type lib` and `--crate-type dylib`
- How to auto-generate C headers with `--emit-header`
- How to use TML libraries from C programs
- Which TML types convert cleanly to C types
- Best practices for creating C-compatible libraries

This opens up many possibilities: you can write performance-critical code in TML and use it from existing C/C++ projects, or create new libraries that work across language boundaries.
