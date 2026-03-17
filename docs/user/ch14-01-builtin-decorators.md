# Built-in Decorators

This chapter is a reference for all decorators built into the TML compiler. Decorators are
presented by category. Each entry shows the full syntax, the accepted arguments, and at least
one realistic example.

## Testing Decorators

### `@test`

Marks a function as a unit test. The function must return `I32` (returning `0` means pass) or
`Unit`. The test runner discovers all `@test` functions automatically.

```tml
@test
func test_addition() -> I32 {
    assert_eq(1 + 1, 2)
    return 0
}
```

An optional `name` argument overrides the display name shown in test output:

```tml
@test(name = "addition produces correct sum")
func test_add() -> I32 {
    assert_eq(3 + 4, 7)
    return 0
}
```

### `@bench`

Marks a function as a benchmark. The test runner executes the body repeatedly and reports the
average time per iteration.

```tml
@bench
func bench_sort() {
    var data = [5, 3, 8, 1, 9, 2, 7, 4, 6]
    data.sort()
}
```

Pass an integer argument to override the default iteration count (1000):

```tml
@bench(10000)
func bench_hash() {
    let _ = hash_string("benchmark input string")
}
```

### `@fuzz`

Marks a function as a fuzz target. The fuzzer provides random byte sequences as input.
The function receives `(data: ptr U8, len: U64)` and should exercise the code under test
without crashing or violating assertions:

```tml
@fuzz
func fuzz_json_parser(data: ptr U8, len: U64) {
    let result = JsonValue::parse_bytes(data, len)
    // Must not panic regardless of input
    let _ = result
}
```

### `@should_panic`

Used together with `@test`. The test passes only if the body panics. Without arguments, any
panic is accepted:

```tml
@test
@should_panic
func test_divide_by_zero() -> I32 {
    let _ = 10 / 0
    return 0
}
```

The `expected` argument requires the panic message to contain a specific substring:

```tml
@test
@should_panic(expected = "index out of bounds")
func test_bad_index() -> I32 {
    let arr = [1, 2, 3]
    let _ = arr[99]
    return 0
}
```

### `@ignore`

Skips a test during normal test runs. The test is still compiled; it is excluded from
execution unless the `--include-ignored` flag is passed to `tml test`:

```tml
@test
@ignore
func test_not_implemented_yet() -> I32 {
    unimplemented()
}
```

Provide a `reason` to document why the test is skipped:

```tml
@test
@ignore(reason = "requires network access")
func test_remote_endpoint() -> I32 {
    // ...
    return 0
}
```

---

## Performance Decorators

### `@inline`

Requests that the compiler inline the function at call sites. Without arguments, the compiler
is free to ignore the hint if inlining would be harmful:

```tml
@inline
func square(x: I32) -> I32 {
    return x * x
}
```

The `always` argument emits LLVM's `alwaysinline` attribute, which forces inlining even
in debug builds:

```tml
@inline(always)
func critical_path_helper(x: F64) -> F64 {
    return x * 1.4142135623730951
}
```

The `never` argument emits LLVM's `noinline` attribute, which prevents inlining even
when the optimizer would normally do so. Use this for error-handling paths or code that
should remain addressable:

```tml
@inline(never)
func cold_error_path(code: I32) {
    println("Fatal error code: {}", code)
    abort()
}
```

### `@cold`

Hints that the function is rarely called. The optimizer deprioritizes it during instruction
scheduling and branch layout, improving performance of the hot path by keeping cold code
out of the instruction cache:

```tml
@cold
func out_of_memory_handler() {
    println("Fatal: out of memory")
    abort()
}
```

### `@hot`

Hints that the function is on a critical execution path. The optimizer gives it higher
priority during inlining decisions and code layout:

```tml
@hot
func inner_loop_body(data: ref [F64], acc: mut ref F64) {
    loop i in 0 to data.len() {
        *acc += data[i]
    }
}
```

### `@simd`

Marks a function as a SIMD candidate. The compiler attempts to auto-vectorize the body
using the available SIMD instruction set:

```tml
@simd
func dot_product(a: [F64; 4], b: [F64; 4]) -> F64 {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]
}
```

---

## API Stability Decorators

### `@stable`

Documents that a public API is considered stable. An optional `since` argument records the
version in which the API became stable:

```tml
@stable(since = "1.0")
pub func parse_integer(s: Str) -> Outcome[I64, ParseError] {
    // ...
}
```

The compiler does not enforce stability rules automatically, but documentation generators
and linters use this information to produce stability badges and enforce stable-API-only
usage in dependent code.

### `@unstable`

Marks an API as experimental. Callers receive a compiler warning unless they explicitly
opt in:

```tml
@unstable(feature = "async_runtime", reason = "API may change before 2.0")
pub func spawn_async[T](task: Func() -> T) -> JoinHandle[T] {
    // ...
}
```

To use an `@unstable` API without warnings, callers annotate their module:

```tml
@allow(unstable_api)
mod my_experimental_module {
    use std::async::spawn_async
    // ...
}
```

### `@deprecated`

Marks a function or type as deprecated. The compiler emits a warning at every call site.
Without arguments, the warning is generic:

```tml
@deprecated
pub func old_parse(s: Str) -> I32 {
    // ...
}
```

The `message` argument customizes the warning text to guide users toward the replacement:

```tml
@deprecated(message = "Use parse_integer() from std::conv instead")
pub func atoi(s: Str) -> I32 {
    // ...
}
```

### `@must_use`

Requires callers to use the return value. The compiler emits a warning when the return
value is discarded:

```tml
@must_use
func compute_checksum(data: ref [U8]) -> U32 {
    // ...
}

func main() {
    let data = [1u8, 2, 3, 4]
    compute_checksum(ref data)  // warning: return value of @must_use function unused
}
```

The `message` argument provides context in the warning:

```tml
@must_use(message = "check Outcome for errors before proceeding")
func write_file(path: Str, content: Str) -> Outcome[Unit, IoError] {
    // ...
}
```

---

## FFI Decorators

### `@extern`

Declares a function implemented in another language. The first argument is the calling
convention. The function has no body:

```tml
@extern("c")
func strlen(s: ptr U8) -> U64

@extern("stdcall")
func MessageBoxA(hwnd: I32, text: ptr U8, caption: ptr U8, utype: I32) -> I32
```

An optional `name` parameter maps to a different symbol name when the TML identifier cannot
match the C symbol exactly:

```tml
@extern("c", name = "SDL_Init")
func sdl_init(flags: U32) -> I32
```

Supported calling conventions:

| String | Use |
|--------|-----|
| `"c"` | Standard C (cdecl) |
| `"stdcall"` | Windows API (Win32) |
| `"fastcall"` | x86 fast register calling |
| `"thiscall"` | C++ member functions (Windows) |

### `@link`

Specifies the library that provides the external symbol. Place it above `@extern`:

```tml
@link("openssl")
@extern("c")
func SSL_CTX_new(method: ptr Unit) -> ptr Unit

@link("user32")
@extern("stdcall")
func ShowWindow(hwnd: ptr Unit, cmd: I32) -> I32
```

On Windows, `@link("foo")` links against `foo.lib`. On Linux and macOS, it links against
`libfoo.so` or `libfoo.a`.

### `@no_mangle`

Exports a TML function with its exact name, without compiler name mangling. Required when
exposing TML code as a C-compatible shared library entry point:

```tml
@no_mangle
pub func tml_plugin_init() -> I32 {
    return 0
}
```

Combined with `pub`, `@no_mangle` makes the symbol visible to the linker and to foreign
callers via `dlsym` or `GetProcAddress`.

### `@repr`

Controls the memory layout of a struct or enum. The primary use is C interoperability,
where the layout must match the C compiler's layout exactly:

```tml
@repr(C)
type Vec3 {
    x: F32,
    y: F32,
    z: F32,
}
```

Additional modifiers refine the layout:

```tml
@repr(C, packed)           // Remove padding between fields
type PackedHeader {
    magic: U16,
    length: U32,
    flags: U8,
}

@repr(C, align(16))        // Align the struct to 16 bytes
type AlignedBuffer {
    data: [U8; 16],
}
```

Without `@repr(C)`, TML is free to reorder fields and add padding for performance.

---

## Memory Layout Decorators

### `@flags`

Applied to an enum to indicate that it represents a set of bit flags. The compiler assigns
powers-of-two values to variants automatically and enables bitwise combination:

```tml
@flags
type Permissions {
    Read,
    Write,
    Execute,
}

func check(p: Permissions) {
    if p & Permissions::Read != 0 {
        println("readable")
    }
}

let rw = Permissions::Read | Permissions::Write
```

Explicit values can be provided for full control:

```tml
@flags
type NetFlags {
    Ipv4    = 0x01,
    Ipv6    = 0x02,
    Tcp     = 0x04,
    Udp     = 0x08,
}
```

### `@thread_local`

Declares a module-level variable with thread-local storage. Each thread gets its own
independent copy initialized to the given value:

```tml
@thread_local
var REQUEST_COUNTER: I32 = 0

func handle_request() {
    REQUEST_COUNTER += 1
    println("Thread has handled {} requests", REQUEST_COUNTER)
}
```

### `@interior_mutable`

Documents that a type contains interior mutability — fields that can be mutated through
a shared reference. This is informational; it does not change codegen, but it suppresses
the `interior_mutability` lint and documents the design intent:

```tml
@interior_mutable
type SharedCounter {
    value: Cell[I32],
}

extend SharedCounter {
    func increment(this) {
        this.value.set(this.value.get() + 1)
    }
}
```

---

## Diagnostic Decorators

### `@allow`

Suppresses a specific compiler warning or lint for the annotated item and its children:

```tml
@allow(unused_variable)
func demo() {
    let x = 42   // would normally warn: x is never used
    println("demo called")
}
```

Multiple warnings can be listed:

```tml
@allow(unused_variable, dead_code)
mod legacy_compat {
    // old API kept for compatibility
}
```

### `@deny`

Elevates a specific warning to a hard error for the annotated item and its children:

```tml
@deny(unsafe_code)
mod safe_module {
    // any lowlevel block here will fail to compile
}
```

### `@doc`

Attaches a documentation string to the item. The `tml doc` tool uses these strings to
generate HTML documentation:

```tml
@doc("Returns the number of UTF-8 scalar values in the string.")
pub func char_count(s: Str) -> U64 {
    // ...
}
```

For multi-line documentation, use `///` doc comments above the item instead; they are
equivalent to `@doc` but easier to read for long descriptions:

```tml
/// Returns the number of UTF-8 scalar values in the string.
///
/// This function iterates over the entire string and counts codepoints.
/// It is O(n) in the length of the string.
pub func char_count(s: Str) -> U64 {
    // ...
}
```

---

## Contract Decorators

### `@pre`

Declares a precondition that must hold when the function is called. In debug builds, the
condition is checked at the call site and panics if violated. In release builds, the
condition is used as an optimization hint (treated as `assume`):

```tml
@pre(n > 0)
func factorial(n: I32) -> I64 {
    if n == 1 { return 1 }
    return n as I64 * factorial(n - 1)
}
```

### `@post`

Declares a postcondition that must hold when the function returns. The `result` keyword
refers to the return value:

```tml
@pre(x >= 0.0)
@post(result >= 0.0)
func sqrt(x: F64) -> F64 {
    // ...
}
```

### `@invariant`

Declares an invariant on a type that must hold at the end of every method call. Applied to
the type definition:

```tml
@invariant(this.len >= 0)
@invariant(this.len <= this.capacity)
type DynamicArray[T] {
    ptr:      ptr T,
    len:      I32,
    capacity: I32,
}
```

---

## Conditional Decorator

### `@when`

Includes or excludes an item based on a compile-time condition. The item is compiled only
if the condition is true; otherwise it is as if the item does not exist:

```tml
@when(target_os = "windows")
func get_temp_dir() -> Str {
    return env::get("TEMP").unwrap_or("C:\\Temp")
}

@when(target_os = "linux", target_os = "macos")
func get_temp_dir() -> Str {
    return "/tmp"
}
```

Recognized keys for `@when`:

| Key | Values |
|-----|--------|
| `target_os` | `"windows"`, `"linux"`, `"macos"`, `"android"`, `"freebsd"` |
| `target_arch` | `"x86_64"`, `"x86"`, `"arm64"`, `"arm"`, `"wasm32"`, `"riscv64"` |
| `target_feature` | `"sse2"`, `"avx2"`, `"neon"`, etc. |
| `build` | `"debug"`, `"release"`, `"test"` |
| `ptr_size` | `"32"`, `"64"` |

Multiple values for the same key are combined with OR. Multiple different keys are combined
with AND:

```tml
// Included on x86_64 Linux or x86_64 macOS
@when(target_arch = "x86_64", target_os = "linux", target_os = "macos")
func use_avx2_path() { ... }
```

For more complex conditions, use the `#if` preprocessor directive instead.

---

## See Also

- [Derive Macros](ch14-02-derive.md) — the `@derive` decorator in full detail
- [Custom Decorators](ch14-03-custom-decorators.md) — defining your own decorators
- [Conditional Compilation](ch18-00-conditional-compilation.md) — `#if`, `#ifdef`, `#elif`
- [Foreign Function Interface](ch17-00-ffi.md) — `@extern`, `@link`, `@repr` with full examples
