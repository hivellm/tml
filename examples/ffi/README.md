# FFI Examples

This directory contains examples of using TML's Foreign Function Interface (FFI) to create C-compatible libraries.

## Examples

### 1. Math Library - Static Linking (`math_lib.tml`)

A comprehensive math library demonstrating:
- Basic arithmetic operations (add, subtract, multiply, divide)
- Mathematical functions (factorial, power, abs)
- Comparison functions (max, min)
- Public vs private function export

**Build the library:**
```bash
tml build examples/ffi/math_lib.tml --crate-type=lib --emit-header --out-dir=examples/ffi
```

This generates:
- **Windows**: `examples/ffi/math_lib.lib` + `examples/ffi/math_lib.h`
- **Linux**: `examples/ffi/libmath_lib.a` + `examples/ffi/math_lib.h`

**Use from C (`use_math_lib.c`):**
```bash
# Navigate to examples/ffi directory
cd examples/ffi

# Windows
clang use_math_lib.c -o use_math_lib.exe math_lib.lib

# Linux/macOS
clang use_math_lib.c -o use_math_lib libmath_lib.a

# Run
./use_math_lib
```

**Output:**
```
=== TML Math Library Test ===

Arithmetic Operations:
  10 + 5 = 15
  ...

6/6 tests passed
All tests passed! ✓
```

### 2. Math Library - Dynamic Linking (`math_lib.tml`)

Same library as above, but built as a dynamic library (DLL/SO).

**Build the DLL:**
```bash
tml build examples/ffi/math_lib.tml --crate-type=dylib --emit-header --out-dir=examples/ffi
```

This generates:
- **Windows**: `examples/ffi/math_lib.dll` + `math_lib.lib` + `math_lib.h`
- **Linux**: `examples/ffi/libmath_lib.so` + `math_lib.h`

**Use from C (`use_math_dll.c`):**
```bash
cd examples/ffi

# Windows
clang use_math_dll.c -o use_math_dll.exe math_lib.lib
./use_math_dll.exe

# Linux
clang use_math_dll.c -o use_math_dll -L. -lmath_lib
LD_LIBRARY_PATH=. ./use_math_dll
```

**Output:**
```
=== TML Math Library DLL Test ===

Dynamic Library Usage:
  This program links to math_lib.dll at runtime
  The DLL must be in the same directory or in PATH

9/9 tests passed
All DLL functions working correctly! ✓
```

## Auto-Generated Headers

The `--emit-header` flag automatically generates a C header file with:
- `#include <stdint.h>` and `#include <stdbool.h>`
- Include guards (`#ifndef TML_MODULE_H`)
- `extern "C"` wrapper for C++ compatibility
- Function declarations with `tml_` prefix

**Example generated header:**
```c
#ifndef TML_MATH_LIB_H
#define TML_MATH_LIB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// TML library: math_lib
// Auto-generated C header for FFI

int32_t tml_add(int32_t a, int32_t b);
int32_t tml_subtract(int32_t a, int32_t b);
int32_t tml_multiply(int32_t x, int32_t y);
// ... more functions

#ifdef __cplusplus
}
#endif

#endif // TML_MATH_LIB_H
```

## Type Mapping

TML types are automatically mapped to C types:

| TML Type | C Type | Notes |
|----------|--------|-------|
| `I8` - `I64` | `int8_t` - `int64_t` | Signed integers |
| `U8` - `U64` | `uint8_t` - `uint64_t` | Unsigned integers |
| `F32`, `F64` | `float`, `double` | Floating point |
| `Bool` | `bool` | From `<stdbool.h>` |
| `Str` | `const char*` | C string |
| `ref T` | `T*` | Pointer to T |
| `Ptr[T]` | `T*` | Raw pointer |

## Library Types

### Static Library (`--crate-type lib`)
- **Windows**: `.lib` file
- **Linux/macOS**: `.a` file
- Code is linked into executable at compile time
- No runtime dependencies

### Dynamic Library (`--crate-type=dylib`)
- **Windows**: `.dll` + `.lib` (import library)
- **Linux**: `.so`
- **macOS**: `.dylib`
- Code is loaded at runtime
- Smaller executables, shared between programs

**Build dynamic library:**
```bash
tml build examples/ffi/math_lib.tml --crate-type=dylib --emit-header --out-dir=examples/ffi
```

**Use from C (`use_math_dll.c`):**
```bash
# Windows
clang use_math_dll.c -o use_math_dll.exe math_lib.lib

# Linux (requires -L. to find the .so at runtime)
clang use_math_dll.c -o use_math_dll -L. -lmath_lib
LD_LIBRARY_PATH=. ./use_math_dll
```

**Key differences from static libraries:**
- The DLL/SO must be available at runtime (same directory or in system PATH/LD_LIBRARY_PATH)
- Multiple programs can share the same DLL, reducing disk space
- You can update the DLL without recompiling programs that use it
- Slightly slower startup time due to dynamic linking

## Best Practices

1. **Use `pub` for exports**: Only public functions are exported to C
2. **Stick to simple types**: Primitive types work best for C FFI
3. **Always use `--emit-header`**: Ensures type safety
4. **Test from both sides**: Verify the library works from TML and C
5. **Document your API**: Add comments to public functions

## Further Reading

- User Guide: `docs/user/ch12-00-libraries-and-ffi.md`
- FFI Specification: `docs/specs/17-FFI.md`
- CLI Documentation: `docs/specs/09-CLI.md`
