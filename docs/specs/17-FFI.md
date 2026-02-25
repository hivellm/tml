# TML v1.0 — Foreign Function Interface

## 1. Overview

TML FFI enables bidirectional interoperability with C and other native libraries. This is essential for:
- **Importing**: Using C/C++ libraries from TML code
- **Exporting**: Creating C-compatible libraries from TML code
- System calls and OS APIs
- Performance-critical native code
- Platform-specific functionality

## 2. Exporting TML to C

### 2.1 Public Functions

Public functions are automatically exported with C linkage:

```tml
// math.tml
module math

// Exported as: int32_t tml_add(int32_t a, int32_t b)
pub func add(a: I32, b: I32) -> I32 {
    return a + b
}

// Exported as: int32_t tml_multiply(int32_t x, int32_t y)
pub func multiply(x: I32, y: I32) -> I32 {
    return x * y
}

// Not exported (private)
func helper(n: I32) -> I32 {
    return n * 2
}
```

### 2.2 Building C-Compatible Libraries

**Static Library:**
```bash
tml build mylib.tml --crate-type=lib
# Generates:
#   - build/debug/mylib.lib (Windows)
#   - build/debug/libmylib.a (Linux/macOS)
```

**Dynamic Library:**
```bash
tml build mylib.tml --crate-type=dylib
# Generates:
#   - build/debug/mylib.dll + mylib.lib (Windows)
#   - build/debug/libmylib.so (Linux)
#   - build/debug/libmylib.dylib (macOS)
```

**With Auto-Generated C Header:**
```bash
tml build mylib.tml --crate-type=lib --emit-header
# Generates:
#   - build/debug/mylib.lib + mylib.h (Windows)
#   - build/debug/libmylib.a + mylib.h (Linux/macOS)
```

**Custom Output Directory:**
```bash
tml build mylib.tml --crate-type=lib --emit-header --out-dir=examples/ffi
# Generates:
#   - examples/ffi/mylib.lib + mylib.h (Windows)
#   - examples/ffi/libmylib.a + mylib.h (Linux/macOS)
```

### 2.3 Auto-Generated C Headers

The `--emit-header` flag generates C-compatible headers:

**TML code (math.tml):**
```tml
module math

pub func add(a: I32, b: I32) -> I32 {
    return a + b
}

pub func factorial(n: I32) -> I32 {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}
```

**Generated header (math.h):**
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
int32_t tml_factorial(int32_t n);

#ifdef __cplusplus
}
#endif

#endif // TML_MATH_H
```

### 2.4 Type Mapping (TML → C)

Public function types are automatically mapped:

| TML Type | C Type | Header Type |
|----------|--------|-------------|
| `I8` | `int8_t` | `#include <stdint.h>` |
| `I16` | `int16_t` | `#include <stdint.h>` |
| `I32` | `int32_t` | `#include <stdint.h>` |
| `I64` | `int64_t` | `#include <stdint.h>` |
| `U8` | `uint8_t` | `#include <stdint.h>` |
| `U16` | `uint16_t` | `#include <stdint.h>` |
| `U32` | `uint32_t` | `#include <stdint.h>` |
| `U64` | `uint64_t` | `#include <stdint.h>` |
| `F32` | `float` | Built-in |
| `F64` | `double` | Built-in |
| `Bool` | `bool` | `#include <stdbool.h>` |
| `Str` | `const char*` | Built-in |
| `ref T` | `T*` | Built-in |
| `Ptr[T]` | `T*` | Built-in |

### 2.5 Using TML Libraries from C

**Compile TML library:**
```bash
tml build mylib.tml --crate-type=lib --emit-header --out-dir=examples/ffi
```

**C program (main.c in examples/ffi/):**
```c
#include <stdio.h>
#include "mylib.h"

int main() {
    int32_t result = tml_add(5, 3);
    printf("5 + 3 = %d\n", result);

    int32_t fact = tml_factorial(5);
    printf("5! = %d\n", fact);

    return 0;
}
```

**Compile C program:**
```bash
# Navigate to examples/ffi directory
cd examples/ffi

# Windows
clang main.c -o main.exe mylib.lib

# Linux/macOS
clang main.c -o main libmylib.a
```

**Or using default build directory:**
```bash
# Compile TML library
tml build mylib.tml --crate-type=lib --emit-header

# C program (main.c)
#include <stdio.h>
#include "build/debug/mylib.h"

int main() {
    // ...
}

# Compile
clang main.c -o main.exe build/debug/mylib.lib  # Windows
clang main.c -o main build/debug/libmylib.a      # Linux/macOS
```

### 2.6 Naming Convention

All exported TML functions use the `tml_` prefix to avoid naming conflicts:

```tml
pub func add(a: I32, b: I32) -> I32  // Exported as: tml_add
pub func process_data(x: F64) -> F64  // Exported as: tml_process_data
```

## 3. Importing C into TML

### 3.1 Extern Functions

```tml
module native

// Import C function
extern "C" func malloc(size: U64) -> *mut U8
extern "C" func free(ptr: *mut U8)
extern "C" func strlen(s: *const U8) -> U64

// With explicit library
extern "C" from "libm" func sin(x: F64) -> F64
extern "C" from "libm" func cos(x: F64) -> F64

// Windows API
extern "C" from "kernel32" func GetLastError() -> U32
extern "C" from "kernel32" func SetLastError(code: U32)
```

### 3.2 Extern Types

```tml
// Opaque type (size unknown, only used via pointer)
extern type FILE

// Sized extern type
extern type pthread_t = U64
extern type pthread_mutex_t = [U8; 40]  // platform-specific size

// Extern struct (C layout)
@repr(C)
extern type timespec {
    tv_sec: I64,
    tv_nsec: I64,
}
```

### 3.3 Extern Variables

```tml
// Global C variables
extern "C" var errno: I32
extern "C" var stdin: *mut FILE
extern "C" var stdout: *mut FILE
extern "C" var stderr: *mut FILE
```

## 4. Calling Conventions

### 4.1 Supported Conventions

| Convention | Syntax | Platform |
|------------|--------|----------|
| C (default) | `extern "C"` | All |
| System | `extern "system"` | Platform default |
| Stdcall | `extern "stdcall"` | Windows |
| Fastcall | `extern "fastcall"` | Windows |
| Win64 | `extern "win64"` | Windows x64 |
| SysV64 | `extern "sysv64"` | Linux/macOS x64 |

```tml
// Windows callback with stdcall
extern "stdcall" func WindowProc(
    hwnd: *mut Void,
    msg: U32,
    wparam: U64,
    lparam: I64
) -> I64
```

### 4.2 Variadic Functions

```tml
// C variadic functions
extern "C" func printf(format: *const U8, ...) -> I32
extern "C" func sprintf(buf: *mut U8, format: *const U8, ...) -> I32

// Calling variadic functions
lowlevel {
    printf(c"Hello %s, you are %d years old\n", name.as_ptr(), age)
}
```

## 5. Raw Pointers

### 5.1 Pointer Types

```tml
*const T    // immutable raw pointer
*mut T      // mutable raw pointer
*const Void // void pointer (opaque)
*mut Void   // mutable void pointer
```

### 5.2 Pointer Operations

```tml
lowlevel func pointer_ops() {
    var x: I32 = 42

    // Create pointers
    let p: *const I32 = ref x as *const I32
    let pm: *mut I32 = mut ref x as *mut I32

    // Dereference (lowlevel)
    let value: I32 = *p
    *pm = 100

    // Pointer arithmetic
    let arr: [I32; 5] = [1, 2, 3, 4, 5]
    let ptr: ptr I32 = arr.as_ptr()
    let third: I32 = *(ptr.offset(2))  // arr[2]

    // Null checks
    if ptr.is_null() {
        panic("null pointer")
    }

    // Cast between pointer types
    let void_ptr: *const Void = ptr as *const Void
    let back: *const I32 = void_ptr as *const I32
}
```

### 5.3 Pointer Methods

```tml
extend *const T {
    func is_null(this) -> Bool
    func offset(this, count: I64) -> *const T
    func add(this, count: U64) -> *const T
    func sub(this, count: U64) -> *const T
    lowlevel func read(this) -> T
    lowlevel func read_volatile(this) -> T
}

extend *mut T {
    func is_null(this) -> Bool
    func offset(this, count: I64) -> *mut T
    func add(this, count: U64) -> *mut T
    func sub(this, count: U64) -> *mut T
    lowlevel func read(this) -> T
    lowlevel func write(this, value: T)
    lowlevel func read_volatile(this) -> T
    lowlevel func write_volatile(this, value: T)
}
```

## 6. Unsafe Blocks

### 6.1 Lowlevel Keyword

```tml
func safe_wrapper() -> I32 {
    // Low-level operations require explicit block
    lowlevel {
        let ptr: ptr U8 = malloc(100)
        if ptr.is_null() {
            return -1
        }

        // Do work with raw memory
        let result: I32 = process_buffer(ptr, 100)

        free(ptr)
        return result
    }
}
```

### 6.2 Lowlevel Functions

```tml
// Entire function is lowlevel
lowlevel func dangerous_operation(ptr: *mut I32, len: U64) -> I32 {
    // No need for lowlevel block inside
    var sum: I32 = 0
    loop i in 0 to len {
        sum += *(ptr.add(i))
    }
    return sum
}

// Calling lowlevel function
func caller() {
    lowlevel {
        let result: I32 = dangerous_operation(data.as_mut_ptr(), data.len())
    }
}
```

### 6.3 What Requires Lowlevel

| Operation | Lowlevel Required |
|-----------|-------------------|
| Raw pointer dereference | Yes |
| Calling extern functions | Yes |
| Calling lowlevel functions | Yes |
| Accessing mutable statics | Yes |
| Implementing lowlevel behaviors | Yes |
| Union field access | Yes |
| Inline assembly | Yes |

## 7. C String Interop

### 7.1 C String Literals

```tml
// Null-terminated C string literal
let msg: *const U8 = c"Hello, World!"

// Raw byte string
let bytes: *const U8 = b"binary data\x00\xFF"
```

### 7.2 CString Type

```tml
use std::ffi::CString

func use_c_string() {
    // Create from TML String
    let c_str: Outcome[CString, Error] = CString.new("Hello")!

    // Get raw pointer for FFI
    lowlevel {
        puts(c_str.as_ptr())
    }

    // Convert back
    let tml_str: String = c_str.to_string()
}

// From raw C string
lowlevel func from_c(ptr: *const U8) -> String {
    let c_str: CString = CString.from_ptr(ptr)
    return c_str.to_string()
}
```

### 7.3 CStr (borrowed)

```tml
use std::ffi::CStr

lowlevel func process_c_string(ptr: *const U8) {
    // Borrow without taking ownership
    let c_str: CStr = CStr.from_ptr(ptr)

    // Get length (scans for null)
    let len: U64 = c_str.len()

    // Convert to bytes slice
    let bytes: ref [U8] = c_str.as_bytes()

    // Try to convert to string (may fail if not UTF-8)
    when c_str.to_str() {
        Ok(s) -> println(s),
        Err(_) -> println("invalid UTF-8"),
    }
}
```

## 8. Type Mappings (C → TML)

### 8.1 Primitive Type Mapping

| TML Type | C Type | Size |
|----------|--------|------|
| `Bool` | `_Bool` / `bool` | 1 |
| `I8` | `int8_t` / `char` | 1 |
| `I16` | `int16_t` / `short` | 2 |
| `I32` | `int32_t` / `int` | 4 |
| `I64` | `int64_t` / `long long` | 8 |
| `U8` | `uint8_t` / `unsigned char` | 1 |
| `U16` | `uint16_t` / `unsigned short` | 2 |
| `U32` | `uint32_t` / `unsigned int` | 4 |
| `U64` | `uint64_t` / `unsigned long long` | 8 |
| `F32` | `float` | 4 |
| `F64` | `double` | 8 |
| `*const T` | `const T*` | ptr |
| `*mut T` | `T*` | ptr |
| `*const Void` | `const void*` | ptr |
| `*mut Void` | `void*` | ptr |

### 8.2 Platform-Specific Types

```tml
use std::ffi::c::*

// Platform-dependent sizes
type c_int = I32      // usually
type c_long = I64     // on 64-bit Unix, I32 on Windows
type c_size_t = U64   // on 64-bit
type c_ssize_t = I64

// Character types
type c_char = I8      // or U8 on some platforms
type c_wchar = I32    // or U16 on Windows
```

### 8.3 Struct Layout

```tml
// C-compatible struct layout
@repr(C)
type Point {
    x: F64,
    y: F64,
}

// Packed struct (no padding)
@repr(C, packed)
type PackedData {
    flag: U8,
    value: U32,  // no padding before
}

// Aligned struct
@repr(C, align(16))
type AlignedData {
    data: [U8; 64],
}
```

## 9. Unions

### 9.1 Union Declaration

```tml
// C-style union
@repr(C)
union Value {
    i: I64,
    f: F64,
    ptr: *mut Void,
}

// Usage (requires lowlevel)
func use_union() {
    var v: Value = Value { i: 42 }

    lowlevel {
        println(v.i.to_string())  // 42
        v.f = 3.14
        // Reading v.i now is undefined behavior!
    }
}
```

### 9.2 Tagged Union Pattern

```tml
@repr(C)
type TaggedValue {
    tag: U8,
    value: ValueUnion,
}

@repr(C)
union ValueUnion {
    int_val: I64,
    float_val: F64,
    str_ptr: *const U8,
}

const TAG_INT: U8 = 0
const TAG_FLOAT: U8 = 1
const TAG_STRING: U8 = 2

func get_int(tv: ref TaggedValue) -> Maybe[I64] {
    if tv.tag == TAG_INT {
        lowlevel { Just(tv.value.int_val) }
    } else {
        Nothing
    }
}
```

## 10. Callbacks

### 10.1 Function Pointers

```tml
// Function pointer type
type Comparator = func(*const Void, *const Void) -> I32

// C qsort
extern "C" func qsort(
    base: *mut Void,
    nmemb: U64,
    size: U64,
    compar: Comparator
)

// TML callback
func compare_i32(a: *const Void, b: *const Void) -> I32 {
    lowlevel {
        let va: I32 = *(a as *const I32)
        let vb: I32 = *(b as *const I32)
        return va - vb
    }
}

func sort_array() {
    var arr: [I32; 5] = [5, 2, 8, 1, 9]

    lowlevel {
        qsort(
            arr.as_mut_ptr() as *mut Void,
            5,
            size_of[I32](),
            compare_i32
        )
    }
}
```

### 10.2 Closures to C

```tml
// Closures cannot be directly passed to C
// Use trampoline pattern with user_data

extern "C" func set_callback(
    callback: func(*mut Void),
    user_data: *mut Void
)

type CallbackWrapper[F] {
    func_ptr: F,
}

func with_callback[F: Fn()](callback: F) {
    var wrapper: CallbackWrapper = CallbackWrapper { func_ptr: callback }

    lowlevel {
        set_callback(
            trampoline[F],
            mut ref wrapper as *mut Void
        )
    }
}

func trampoline[F: Fn()](user_data: *mut Void) {
    lowlevel {
        let wrapper: ref CallbackWrapper[F] = ref *(user_data as *const CallbackWrapper[F])
        (wrapper.func_ptr)()
    }
}
```

## 11. Library Linking

### 11.1 Link Directives

```tml
// Link to system library
@link(name = "m")
extern "C" {
    func sin(x: F64) -> F64
    func cos(x: F64) -> F64
}

// Static library
@link(name = "mylib", kind = "static")
extern "C" {
    func my_function() -> I32
}

// Dynamic library
@link(name = "ssl", kind = "dylib")
extern "C" {
    func SSL_new(ctx: *mut Void) -> *mut Void
}

// Framework (macOS)
@link(name = "CoreFoundation", kind = "framework")
extern "C" {
    func CFRetain(cf: *const Void) -> *const Void
}
```

### 11.2 Build Configuration

```toml
# tml.toml

[build]
# Link libraries
links = ["m", "pthread"]

# Library search paths
lib-dirs = ["/usr/local/lib", "vendor/lib"]

# Include paths (for bindgen)
include-dirs = ["/usr/local/include"]

[target.x86_64-linux]
links = ["dl", "rt"]

[target.x86_64-windows]
links = ["kernel32", "user32"]

[target.aarch64-macos]
frameworks = ["Foundation", "Security"]
```

## 12. Inline Assembly

### 12.1 Basic Assembly

```tml
// Inline assembly (platform-specific)
lowlevel func get_cpu_id() -> U32 {
    var result: U32 = 0

    asm! {
        "cpuid",
        out("eax") result,
        in("eax") 1,
        options(nostack, preserves_flags)
    }

    return result
}
```

### 12.2 Assembly Syntax

```tml
asm! {
    template,
    operands...,
    options(...)
}

// Operands
in(reg) value        // input in register
out(reg) var         // output to variable
inout(reg) var       // input and output
lateout(reg) var     // output after all inputs consumed

// Options
pure                 // no side effects
nomem                // doesn't access memory
readonly             // only reads memory
nostack              // doesn't use stack
preserves_flags      // doesn't modify flags
```

## 13. Platform Conditionals

### 13.1 Conditional Compilation

```tml
@when(target_os = "linux")
func platform_specific() {
    // Linux-only code
}

@when(target_os = "windows")
func platform_specific() {
    // Windows-only code
}

@when(target_os = "macos")
func platform_specific() {
    // macOS-only code
}

@when(target_arch = "x86_64")
func optimized_routine() {
    // x86_64 optimizations
}

@when(target_arch = "aarch64")
func optimized_routine() {
    // ARM64 optimizations
}
```

### 13.2 Platform Modules

```tml
module mylib.platform

@when(target_os = "linux")
public use linux.*

@when(target_os = "windows")
public use windows.*

@when(target_os = "macos")
public use macos.*

// Unified interface
public behavior Platform {
    func get_home_dir() -> Outcome[String, Error]
    func get_temp_dir() -> String
    func spawn_process(cmd: String) -> Outcome[Process, Error]
}
```

## 14. Safety Guidelines

### 14.1 Safe Wrappers

```tml
// Always provide safe wrappers for FFI

// Lowlevel C function
extern "C" func dangerous_alloc(size: U64) -> *mut U8

// Safe wrapper
pub func safe_alloc(size: U64) -> Outcome[Heap[U8], AllocError] {
    if size == 0 {
        return Err(AllocError.ZeroSize)
    }

    lowlevel {
        let ptr: ptr U8 = dangerous_alloc(size)
        if ptr.is_null() {
            return Err(AllocError.OutOfMemory)
        }
        return Ok(Heap.from_raw(ptr))
    }
}
```

### 14.2 RAII for FFI Resources

```tml
// Wrap C resources in RAII types

extern "C" func fopen(path: *const U8, mode: *const U8) -> *mut FILE
extern "C" func fclose(file: *mut FILE) -> I32

pub type SafeFile {
    handle: *mut FILE,
}

extend SafeFile {
    pub func open(path: String, mode: String) -> Outcome[This, IoError] {
        let c_path: CString = CString.new(path)!
        let c_mode: CString = CString.new(mode)!

        lowlevel {
            let handle: ptr FILE = fopen(c_path.as_ptr(), c_mode.as_ptr())
            if handle.is_null() {
                return Err(IoError.from_errno())
            }
            return Ok(This { handle: handle })
        }
    }
}

extend SafeFile with Disposable {
    func drop(this) {
        lowlevel {
            fclose(this.handle)
        }
    }
}
```

---

*Previous: [16-COMPILER-ARCHITECTURE.md](./16-COMPILER-ARCHITECTURE.md)*
*Next: [18-ABI.md](./18-ABI.md) — Application Binary Interface*
