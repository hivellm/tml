# Building Libraries for C Consumption

TML code can be compiled into static or dynamic libraries that C programs link
against directly. The compiler generates a C header file from your public
function declarations, so consumers get accurate types without writing
bindings by hand.

## Library Types

| Type | Flag | Output |
|------|------|--------|
| Static library | `--crate-type=lib` | `.lib` (Windows) / `.a` (Linux, macOS) |
| Dynamic library | `--crate-type=dylib` | `.dll` + `.lib` (Windows) / `.so` (Linux) / `.dylib` (macOS) |

Choose a static library when you want the TML code bundled into the consumer
executable at link time. Choose a dynamic library when you want the TML code
loaded at runtime and shared between multiple processes.

## Writing the Library

Mark functions `pub` to include them in the exported API. Functions without
`pub` are compiled but not exported — they cannot be called from C.

```tml
// math.tml
mod math

/// Add two 32-bit integers.
pub func add(a: I32, b: I32) -> I32 {
    return a + b
}

/// Multiply two 32-bit integers.
pub func multiply(x: I32, y: I32) -> I32 {
    return x * y
}

/// Compute n factorial. Returns 1 for n <= 1.
pub func factorial(n: I32) -> I32 {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

// This function is NOT exported — it is internal to the library.
func internal_helper(n: I32) -> I32 {
    return n * 2
}
```

## Building

### Static Library

```bash
tml build math.tml --crate-type=lib
```

Output:
- Windows: `build/debug/math.lib`
- Linux / macOS: `build/debug/libmath.a`

### Dynamic Library

```bash
tml build math.tml --crate-type=dylib
```

Output:
- Windows: `build/debug/math.dll` and `build/debug/math.lib` (import library)
- Linux: `build/debug/libmath.so`
- macOS: `build/debug/libmath.dylib`

### Custom Output Directory

Use `--out-dir` to place build artifacts in a specific directory:

```bash
tml build math.tml --crate-type=lib --out-dir=dist/
```

This is useful when distributing a library alongside its source, or when
writing examples that should sit next to the compiled library.

## Generating the C Header

Add `--emit-header` to produce a C header file alongside the library:

```bash
tml build math.tml --crate-type=lib --emit-header --out-dir=dist/
```

This creates `dist/math.h`:

```c
#ifndef TML_MATH_H
#define TML_MATH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TML library: math — auto-generated, do not edit */

int32_t tml_add(int32_t a, int32_t b);
int32_t tml_multiply(int32_t x, int32_t y);
int32_t tml_factorial(int32_t n);

#ifdef __cplusplus
}
#endif

#endif /* TML_MATH_H */
```

Key properties of the generated header:

- All exported functions are prefixed with `tml_` to avoid name collisions
  with C functions of the same name.
- TML types are translated to C types (`I32` → `int32_t`, etc.).
- The `extern "C"` block makes the header safe to include from C++ code.
- Include guards prevent double-inclusion.

## Type Mapping in Exported Functions

| TML Parameter Type | C Signature Type |
|--------------------|-----------------|
| `I8` | `int8_t` |
| `I16` | `int16_t` |
| `I32` | `int32_t` |
| `I64` | `int64_t` |
| `U8` | `uint8_t` |
| `U16` | `uint16_t` |
| `U32` | `uint32_t` |
| `U64` | `uint64_t` |
| `F32` | `float` |
| `F64` | `double` |
| `Bool` | `bool` |
| `*U8` | `const char*` |
| `*T` | `T*` |
| `*Unit` | `void*` |

Complex TML types — `Str`, `Vec[T]`, `Maybe[T]`, structs with TML
runtime fields — do not map cleanly to C. For maximum interoperability,
limit your exported function signatures to primitive types and raw pointers.

## Using a TML Library from C

Build the library and header:

```bash
tml build math.tml --crate-type=lib --emit-header --out-dir=dist/
```

Write the C consumer:

```c
/* main.c */
#include <stdio.h>
#include "math.h"

int main(void) {
    int32_t sum     = tml_add(5, 3);
    int32_t product = tml_multiply(4, 7);
    int32_t fact    = tml_factorial(5);

    printf("5 + 3 = %d\n",   sum);       /* 8  */
    printf("4 * 7 = %d\n",   product);   /* 28 */
    printf("5! = %d\n",      fact);      /* 120 */
    return 0;
}
```

Compile and link:

```bash
# Windows
clang main.c -o main.exe dist/math.lib

# Linux
clang main.c -o main dist/libmath.a

# macOS
clang main.c -o main dist/libmath.a
```

Run:

```
5 + 3 = 8
4 * 7 = 28
5! = 120
```

## Exporting Structs

To share struct layouts between TML and C, use `@repr(C)` to guarantee that
the TML compiler lays out the struct exactly as a C compiler would — no
padding reordering, no field reordering.

```tml
// geometry.tml
mod geometry

@repr(C)
pub type Point {
    x: F64,
    y: F64,
}

@repr(C)
pub type Rect {
    top_left:     Point,
    bottom_right: Point,
}

pub func rect_area(r: Rect) -> F64 {
    let width:  F64 = r.bottom_right.x - r.top_left.x
    let height: F64 = r.bottom_right.y - r.top_left.y
    return width * height
}

pub func point_distance(a: Point, b: Point) -> F64 {
    let dx: F64 = b.x - a.x
    let dy: F64 = b.y - a.y
    return sqrt_f64(dx * dx + dy * dy)
}
```

Generated header excerpt:

```c
typedef struct {
    double x;
    double y;
} tml_Point;

typedef struct {
    tml_Point top_left;
    tml_Point bottom_right;
} tml_Rect;

double tml_rect_area(tml_Rect r);
double tml_point_distance(tml_Point a, tml_Point b);
```

The `@repr(C)` annotation is only needed for structs that appear in exported
function signatures and must have a predictable layout in C. Internal structs
that are not exported do not need it.

## A Complete Example: String Utilities Library

This example demonstrates a library that works with strings using raw
pointer types for C compatibility.

```tml
// strutil.tml
mod strutil

@extern("c")
func strlen(s: *U8) -> U64

@extern("c")
func memcmp(a: *Unit, b: *Unit, n: U64) -> I32

/// Return the length of a null-terminated C string.
pub func string_length(s: *U8) -> U64 {
    return strlen(s)
}

/// Return true if the two null-terminated C strings are equal.
pub func string_equal(a: *U8, b: *U8) -> Bool {
    let la: U64 = strlen(a)
    let lb: U64 = strlen(b)
    if la != lb {
        return false
    }
    return memcmp(a as *Unit, b as *Unit, la) == 0
}

/// Return true if `s` starts with `prefix`.
pub func string_starts_with(s: *U8, prefix: *U8) -> Bool {
    let ls: U64 = strlen(s)
    let lp: U64 = strlen(prefix)
    if lp > ls {
        return false
    }
    return memcmp(s as *Unit, prefix as *Unit, lp) == 0
}
```

Build:

```bash
tml build strutil.tml --crate-type=lib --emit-header --out-dir=dist/
```

Generated header (`dist/strutil.h`):

```c
#ifndef TML_STRUTIL_H
#define TML_STRUTIL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t tml_string_length(const char* s);
bool     tml_string_equal(const char* a, const char* b);
bool     tml_string_starts_with(const char* s, const char* prefix);

#ifdef __cplusplus
}
#endif

#endif /* TML_STRUTIL_H */
```

C consumer:

```c
#include <stdio.h>
#include "strutil.h"

int main(void) {
    printf("length: %llu\n",    tml_string_length("hello"));           /* 5  */
    printf("equal: %s\n",       tml_string_equal("abc", "abc")
                                    ? "true" : "false");                /* true */
    printf("starts_with: %s\n", tml_string_starts_with("hello", "he")
                                    ? "true" : "false");                /* true */
    return 0;
}
```

## Release Builds

By default, `tml build` produces a debug build. For a library you intend to
distribute, use the release flag:

```bash
tml build math.tml --crate-type=lib --emit-header --release --out-dir=dist/
```

Release builds enable optimizations and remove debug symbols. Link-time
optimization (`--lto`) can further reduce binary size and improve performance
when the library will always be statically linked.

## Cross-Compilation

Compile a library for a different target platform:

```bash
# Build a Linux ARM64 static library from Windows
tml build math.tml \
    --crate-type=lib \
    --emit-header \
    --target=aarch64-unknown-linux-gnu \
    --sysroot=/path/to/arm64-sysroot \
    --out-dir=dist/arm64/
```

The generated header is platform-independent. The `.a` or `.lib` file is
architecture-specific and must match the target you are linking on.

## Best Practices

1. **Keep exported signatures simple.** Primitive types and raw pointers
   translate reliably to C. Complex types (`Vec[T]`, `Maybe[T]`, closures)
   do not appear in C headers and should not be in public function signatures.

2. **Use `@repr(C)` on exported structs.** Without it, the compiler may
   reorder or pad fields differently from C, making the struct unusable from
   C code.

3. **Always generate the header.** Hand-writing C declarations for TML
   functions is error-prone. Let `--emit-header` do it automatically and
   treat the generated file as the source of truth.

4. **Prefix your API.** The `tml_` prefix on generated symbols reduces the
   chance of collision with the consumer's existing function names. If you
   want a different prefix, rename the TML functions accordingly.

5. **Document lifecycle and ownership.** If an exported function returns
   an allocated pointer, document what the caller must use to free it and
   when. C has no ownership system, so the API contract must be explicit.

6. **Version your API.** Once a C consumer links against your library, you
   cannot change exported function signatures without breaking binary
   compatibility. Plan the API carefully before publishing a stable release.

7. **Test from both sides.** Write TML unit tests for the library logic.
   Write a small C program that exercises the C API. Both are necessary:
   the TML tests verify correctness, the C tests verify that the ABI is
   correct and the header compiles cleanly.
