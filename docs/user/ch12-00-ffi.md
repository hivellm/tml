# Foreign Function Interface (FFI)

TML provides seamless interoperability with C/C++ code through the `@extern` and `@link` decorators. This allows you to call system APIs, use optimized libraries, and integrate with existing codebases.

## Calling C Functions

Use the `@extern` decorator to declare a function implemented in C:

```tml
@extern("c")
func puts(s: Str) -> I32

func main() -> I32 {
    puts("Hello from C!")
    return 0
}
```

The `"c"` argument specifies the C calling convention. The function has no body - it's implemented externally.

## Linking Libraries

Use `@link` to specify which library contains the external function:

```tml
@link("m")  // Link libm (math library)
@extern("c")
func sqrt(x: F64) -> F64

@extern("c")
func cos(x: F64) -> F64

func main() -> I32 {
    let val: F64 = sqrt(2.0) + cos(0.0)
    return 0
}
```

## Windows API Example

Call Windows API functions using the `stdcall` convention:

```tml
@link("user32")
@extern("stdcall")
func MessageBoxA(hwnd: I32, text: Str, caption: Str, utype: I32) -> I32

@link("kernel32")
@extern("stdcall")
func GetTickCount64() -> U64

func main() -> I32 {
    let start: U64 = GetTickCount64()
    MessageBoxA(0, "Hello from TML!", "TML FFI", 0)
    return 0
}
```

## Custom Symbol Names

If the TML function name differs from the C symbol, use the `name` parameter:

```tml
@extern("c", name = "atoi")
func string_to_int(s: Str) -> I32

func main() -> I32 {
    let val: I32 = string_to_int("42")
    return val
}
```

## Supported Calling Conventions

| ABI | Use Case |
|-----|----------|
| `"c"` | Standard C functions (default) |
| `"c++"` | C++ with name mangling |
| `"stdcall"` | Windows API |
| `"fastcall"` | Performance-critical functions |
| `"thiscall"` | C++ methods on Windows |

## Type Mappings

| TML Type | C Type |
|----------|--------|
| `I8` | `int8_t` |
| `I16` | `int16_t` |
| `I32` | `int32_t` / `int` |
| `I64` | `int64_t` |
| `U8` | `uint8_t` |
| `U16` | `uint16_t` |
| `U32` | `uint32_t` |
| `U64` | `uint64_t` |
| `F32` | `float` |
| `F64` | `double` |
| `Str` | `const char*` |
| `ptr T` | `T*` |
| `Bool` | `bool` |

## Common Patterns

### Environment Variables

```tml
@extern("c")
func getenv(name: Str) -> Str

func main() -> I32 {
    let home: Str = getenv("HOME")
    return 0
}
```

### File Operations

```tml
@extern("c")
func fopen(path: Str, mode: Str) -> ptr I8

@extern("c")
func fclose(file: ptr I8) -> I32

@extern("c")
func fprintf(file: ptr I8, fmt: Str) -> I32
```

### Memory Allocation

```tml
@extern("c")
func malloc(size: I64) -> ptr I8

@extern("c")
func free(ptr: ptr I8)

@extern("c")
func memcpy(dest: ptr I8, src: ptr I8, n: I64) -> ptr I8
```

## Best Practices

1. **Type Safety**: Ensure TML types match C types exactly
2. **Null Checks**: C functions may return null pointers
3. **Memory Management**: Be careful with C memory allocation
4. **Error Handling**: Check return values for errors
5. **Documentation**: Document external dependencies

## Limitations

- `@extern` functions cannot be generic
- Variadic functions (like `printf`) are not yet supported
- Struct layout must match C exactly for interop
