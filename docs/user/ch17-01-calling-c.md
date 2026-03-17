# Calling C from TML

TML can call any C function by declaring it with the `@extern` decorator. The
declaration tells the compiler the function's name, calling convention,
parameter types, and return type. No body is provided — the implementation
lives in the external library.

## Declaring an External Function

```tml
@extern("c")
func strlen(s: *U8) -> U64
```

The string argument to `@extern` specifies the calling convention. `"c"` is
the standard C calling convention and is correct for the vast majority of
libraries. The function declaration has no body.

## Calling the Declared Function

After declaring it, call the function exactly as you would any TML function:

```tml
@extern("c")
func strlen(s: *U8) -> U64

func main() {
    let s: *U8 = c"hello"
    let len: U64 = strlen(s)
    println("Length: ", len)  // Length: 5
}
```

The `c"..."` syntax produces a null-terminated C string literal (`*U8`).
This is the type expected by C functions that take `const char*`.

## Linking Libraries

By default, the TML linker links against the C runtime automatically.
For functions in additional libraries, add `@link` above `@extern`:

```tml
@link("m")
@extern("c")
func sqrt(x: F64) -> F64

@link("m")
@extern("c")
func cos(x: F64) -> F64

func main() {
    println(sqrt(2.0))  // 1.4142...
    println(cos(0.0))   // 1.0
}
```

The argument to `@link` is the library name without the `lib` prefix and
without the extension — it is equivalent to passing `-lm` to a C linker.

Multiple functions from the same library each need their own `@link`
decoration, or you can group them and use a module-level link annotation.

## Calling Conventions

| Convention | Argument | Use case |
|------------|----------|----------|
| Standard C | `"c"` | Most Unix and Windows C libraries |
| Windows stdcall | `"stdcall"` | Win32 API functions |
| Windows fastcall | `"fastcall"` | Performance-critical Windows code |
| C++ (mangled) | `"c++"` | C++ functions with `extern "C++"` linkage |
| C++ method | `"thiscall"` | MSVC C++ virtual method calls |

### Windows API Example

Windows API functions use the `stdcall` convention:

```tml
@link("kernel32")
@extern("stdcall")
func GetTickCount64() -> U64

@link("user32")
@extern("stdcall")
func MessageBoxA(hwnd: I32, text: *U8, caption: *U8, utype: I32) -> I32

func main() {
    let start: U64 = GetTickCount64()
    MessageBoxA(0, c"Hello from TML!", c"TML FFI", 0)
    let elapsed: U64 = GetTickCount64() - start
    println("Elapsed ms: ", elapsed)
}
```

## Custom Symbol Names

If the C symbol name conflicts with a TML keyword, or you want a more
descriptive TML name, use the `name` parameter:

```tml
@extern("c", name = "atoi")
func parse_int(s: *U8) -> I32

@extern("c", name = "strtod")
func parse_float(s: *U8, end: **U8) -> F64

func main() {
    let n: I32 = parse_int(c"123")
    println(n)  // 123
}
```

The `name` value is the exact symbol that will appear in the object file.
The TML name on the `func` line is what you call in TML code.

## Variadic Functions

C functions that accept a variable number of arguments (like `printf`) are
declared with `...` after the fixed parameters:

```tml
@extern("c")
func printf(format: *U8, ...) -> I32

func main() {
    printf(c"Hello, %s! You are %d years old.\n", c"Alice", 30)
}
```

> **Note:** Passing TML values of complex types (structs, enums, strings)
> as variadic arguments is unsupported. Only primitive types — integers,
> floats, and pointers — are safe to pass variadically.

## Type Mapping

When calling C functions, TML types correspond to C types as follows:

| TML Type | C type | Notes |
|----------|--------|-------|
| `I8` | `int8_t` | |
| `I16` | `int16_t` | |
| `I32` | `int32_t` / `int` | |
| `I64` | `int64_t` | |
| `U8` | `uint8_t` / `unsigned char` | |
| `U16` | `uint16_t` | |
| `U32` | `uint32_t` / `unsigned int` | |
| `U64` | `uint64_t` | |
| `F32` | `float` | |
| `F64` | `double` | |
| `Bool` | `bool` | Requires `<stdbool.h>` in C |
| `*U8` | `const char*` / `char*` | Use `c"..."` literals |
| `*T` | `T*` | Raw pointer to T |
| `*Unit` | `void*` | Opaque pointer |

TML does not automatically convert between `I32` and `I64`, or between
`U8` and `I32`. Always use the exact type the C function expects. When in
doubt, consult the C header.

## C String Literals

The `c"..."` syntax creates a null-terminated string literal with type `*U8`:

```tml
let greeting: *U8 = c"Hello, world!"
```

This is distinct from TML's `Str` type, which is a length-prefixed UTF-8
string managed by TML's runtime. Most C APIs expect null-terminated `char*`
strings, so use `c"..."` for string literals passed to C.

When you need to pass a TML `Str` to a C function, you must obtain a pointer
to its underlying bytes:

```tml
@extern("c")
func puts(s: *U8) -> I32

func print_str(s: Str) {
    // Get a pointer to the string's bytes.
    // The string must remain alive for the duration of the C call.
    let ptr: *U8 = s.as_ptr()
    puts(ptr)
}
```

> **Important:** The pointer returned by `as_ptr()` is only valid while the
> `Str` it came from is alive and not mutated. Do not store this pointer or
> pass it to a C function that will use it after the TML function returns.

## Working with Opaque Pointers

Many C APIs use opaque handle types — pointers to structs whose internal
layout is hidden. Declare these as `*Unit` in TML:

```tml
// SQLite example
@link("sqlite3")
@extern("c")
func sqlite3_open(filename: *U8, db: **Unit) -> I32

@link("sqlite3")
@extern("c")
func sqlite3_close(db: *Unit) -> I32

@link("sqlite3")
@extern("c")
func sqlite3_exec(
    db: *Unit,
    sql: *U8,
    callback: *Unit,
    arg: *Unit,
    errmsg: **U8
) -> I32

func main() {
    var db: *Unit = null

    let rc: I32 = sqlite3_open(c"test.db", ref mut db)
    if rc != 0 {
        println("Failed to open database")
        return
    }

    sqlite3_exec(db, c"CREATE TABLE IF NOT EXISTS t (id INTEGER);",
                 null, null, null)

    sqlite3_close(db)
}
```

## A Complete Example: Calling libz

The following example compresses and decompresses data using zlib — a real
C library available on most platforms.

```tml
@link("z")
@extern("c")
func compress(
    dest:       *U8,
    dest_len:   *U64,
    source:     *U8,
    source_len: U64
) -> I32

@link("z")
@extern("c")
func uncompress(
    dest:       *U8,
    dest_len:   *U64,
    source:     *U8,
    source_len: U64
) -> I32

@link("z")
@extern("c")
func compressBound(source_len: U64) -> U64

func main() {
    let original: *U8 = c"Hello, zlib! This is a test string for compression."
    let src_len: U64 = 51

    // Allocate output buffer for the compressed data.
    let bound: U64    = compressBound(src_len)
    let compressed: *U8 = mem_alloc(bound as I64) as *U8

    var compressed_len: U64 = bound
    let rc: I32 = compress(compressed, ref mut compressed_len, original, src_len)

    if rc == 0 {
        println("Compressed ", src_len, " bytes to ", compressed_len, " bytes")
    } else {
        println("Compression failed: ", rc)
    }

    // Decompress back to verify.
    let decompressed: *U8 = mem_alloc(src_len as I64) as *U8
    var out_len: U64 = src_len
    uncompress(decompressed, ref mut out_len, compressed, compressed_len)

    mem_free(compressed as *Unit)
    mem_free(decompressed as *Unit)
}
```

This example shows:
- Multiple `@link`/`@extern` declarations for functions from the same library.
- Passing `ref mut` to get a mutable pointer parameter (`*U64` out-parameter).
- Casting `*Unit` from `mem_alloc` to the required pointer type.
- Manual memory management for buffers passed to C functions.

## Common Patterns

### Environment Variables

```tml
@extern("c")
func getenv(name: *U8) -> *U8

func main() {
    let home: *U8 = getenv(c"HOME")
    if home != null {
        // Use home...
    }
}
```

### File Operations

```tml
@extern("c")
func fopen(path: *U8, mode: *U8) -> *Unit

@extern("c")
func fclose(file: *Unit) -> I32

@extern("c")
func fprintf(file: *Unit, format: *U8, ...) -> I32

func main() {
    let f: *Unit = fopen(c"output.txt", c"w")
    if f != null {
        fprintf(f, c"Value: %d\n", 42)
        fclose(f)
    }
}
```

### Memory Functions

```tml
@extern("c")
func malloc(size: U64) -> *Unit

@extern("c")
func free(ptr: *Unit)

@extern("c")
func memcpy(dest: *Unit, src: *Unit, n: U64) -> *Unit

@extern("c")
func memset(dest: *Unit, c: I32, n: U64) -> *Unit
```

## Best Practices

1. **Match types exactly.** C does not automatically promote `I32` to `I64`.
   Mismatched types produce subtle bugs that appear as garbage values or
   crashes.

2. **Check return codes.** Most C functions signal errors through return values
   or `errno`. TML has no way to know a C function failed unless you check.

3. **Respect C ownership semantics.** If a C function returns a pointer to
   heap-allocated memory that you must free, call the corresponding C free
   function. Do not pass such pointers to TML's `mem_free`.

4. **Never pass TML-managed pointers to C free.** TML's allocator and C's
   `malloc`/`free` may use different heaps. Memory allocated by `mem_alloc`
   must be freed by `mem_free`, and memory allocated by C's `malloc` must be
   freed by C's `free`.

5. **Wrap FFI calls.** Define TML wrapper functions around raw `@extern`
   declarations. The wrapper validates inputs, handles error codes, and
   provides a TML-idiomatic API to the rest of your code.

6. **Keep `@extern` declarations in a dedicated module.** Grouping all FFI
   declarations in a single module (for example, `mod ffi`) makes it easy to
   audit foreign calls and update them when the C API changes.
