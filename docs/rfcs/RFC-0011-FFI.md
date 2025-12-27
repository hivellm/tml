# RFC-0011: Foreign Function Interface (FFI)

## Status
Active - **Implemented in v0.1.0**

## Summary

This RFC defines the Foreign Function Interface (FFI) for TML, enabling seamless interoperability with C/C++ code through `@extern` and `@link` decorators.

## Motivation

TML needs to interface with existing C/C++ libraries and system APIs:

1. **System Calls** - Access OS functionality (file I/O, networking, etc.)
2. **Performance Libraries** - Use optimized C libraries (math, crypto, etc.)
3. **Legacy Code** - Integrate with existing C/C++ codebases
4. **Platform APIs** - Call Windows API, POSIX functions, etc.

## Specification

### 1. @extern Decorator

Declares a function implemented externally (C/C++) without a TML body.

#### 1.1 Basic Syntax

```tml
@extern(abi)
func function_name(params...) -> ReturnType
```

#### 1.2 With Custom Symbol Name

```tml
@extern(abi, name = "external_symbol")
func tml_function_name(params...) -> ReturnType
```

#### 1.3 Supported ABIs

| ABI | LLVM Convention | Description |
|-----|-----------------|-------------|
| `"c"` | (default) | Standard C calling convention |
| `"c++"` | (default) | C++ with name mangling |
| `"stdcall"` | `x86_stdcallcc` | Windows API convention |
| `"fastcall"` | `x86_fastcallcc` | Fast register-based calling |
| `"thiscall"` | `x86_thiscallcc` | C++ methods on Windows |

#### 1.4 Examples

```tml
// Standard C function
@extern("c")
func puts(s: Str) -> I32

// C function with different TML name
@extern("c", name = "atoi")
func string_to_int(s: Str) -> I32

// Windows API
@extern("stdcall")
func GetTickCount64() -> U64

// C++ with mangled name
@extern("c++", name = "_ZN3foo3barEi")
func foo_bar(x: I32) -> I32
```

### 2. @link Decorator

Specifies external libraries to link.

#### 2.1 Syntax

```tml
@link(library)
```

#### 2.2 Library Resolution

- **By name**: `@link("user32")` → passes `-luser32` to linker
- **By path**: `@link("./vendor/mylib.dll")` → passes path directly

#### 2.3 Examples

```tml
// Link Windows user32.dll
@link("user32")
@extern("stdcall")
func MessageBoxA(hwnd: I32, text: Str, caption: Str, utype: I32) -> I32

// Link math library (Unix)
@link("m")
@extern("c")
func sin(x: F64) -> F64

// Link custom library by path
@link("./libs/custom.dll")
@extern("c")
func custom_function(x: I32) -> I32
```

### 3. Type Mappings

TML types map to C types as follows:

| TML Type | C Type | LLVM Type |
|----------|--------|-----------|
| `I8` | `int8_t` | `i8` |
| `I16` | `int16_t` | `i16` |
| `I32` | `int32_t` | `i32` |
| `I64` | `int64_t` | `i64` |
| `U8` | `uint8_t` | `i8` |
| `U16` | `uint16_t` | `i16` |
| `U32` | `uint32_t` | `i32` |
| `U64` | `uint64_t` | `i64` |
| `F32` | `float` | `float` |
| `F64` | `double` | `double` |
| `Str` | `const char*` | `ptr` |
| `ptr T` | `T*` | `ptr` |
| `Bool` | `_Bool` / `bool` | `i1` |

### 4. Constraints

1. **No Body**: `@extern` functions MUST NOT have a body
2. **Valid ABI**: ABI must be one of the supported values
3. **Type Safety**: Parameter and return types must be FFI-compatible
4. **No Generics**: `@extern` functions cannot be generic

### 5. Code Generation

#### 5.1 LLVM IR Output

```llvm
; @extern("c") func puts(s: Str) -> I32
declare i32 @puts(ptr)

; @extern("stdcall") func MessageBoxA(...) -> I32
declare x86_stdcallcc i32 @MessageBoxA(i32, ptr, ptr, i32)

; @extern("c", name = "atoi") func string_to_int(s: Str) -> I32
declare i32 @atoi(ptr)
```

#### 5.2 Linker Flags

Libraries from `@link` are passed to the linker:
- `@link("user32")` → `-luser32`
- `@link("./vendor/lib.dll")` → `./vendor/lib.dll`

### 6. Complete Example

```tml
// ffi_example.tml - Windows MessageBox

@link("user32")
@extern("stdcall")
func MessageBoxA(hwnd: I32, text: Str, caption: Str, utype: I32) -> I32

@link("kernel32")
@extern("stdcall")
func GetTickCount64() -> U64

func main() -> I32 {
    let start: U64 = GetTickCount64()
    MessageBoxA(0, "Hello from TML!", "TML FFI", 0)
    let elapsed: U64 = GetTickCount64() - start
    return 0
}
```

## Implementation

### Files Modified

| File | Changes |
|------|---------|
| `include/tml/parser/ast.hpp` | Added `extern_abi`, `extern_name`, `link_libs` to FuncDecl |
| `src/parser/parser_decl.cpp` | Process @extern/@link decorators |
| `src/types/checker.cpp` | Validate @extern functions (no body, valid ABI) |
| `src/codegen/llvm_ir_gen_decl.cpp` | Emit LLVM `declare` with calling conventions |
| `src/cli/cmd_build.cpp` | Pass link flags to clang |

### Type Checker Validation

```cpp
if (func.extern_abi.has_value()) {
    const std::string& abi = *func.extern_abi;
    if (abi != "c" && abi != "c++" && abi != "stdcall" &&
        abi != "fastcall" && abi != "thiscall") {
        error("Invalid @extern ABI");
    }
    if (func.body.has_value()) {
        error("@extern function must not have a body");
    }
}
```

## Security Considerations

1. **Library Paths**: Paths with `..` could escape project directory (not yet validated)
2. **Symbol Hijacking**: External symbols could be malicious
3. **Type Mismatches**: Incorrect signatures can cause undefined behavior

## Future Work

1. **Automatic Header Parsing**: Generate TML bindings from C headers
2. **Safety Wrappers**: Generate safe wrappers around unsafe FFI calls
3. **ABI Validation**: Runtime checks for calling convention mismatches
4. **Varargs Support**: Support for variadic functions like `printf`
