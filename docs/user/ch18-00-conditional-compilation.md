# Conditional Compilation

Conditional compilation lets you include or exclude code based on compile-time conditions:
the target operating system, the CPU architecture, the build profile, or user-defined
symbols. This is the standard mechanism for writing code that runs correctly on multiple
platforms without duplicating entire modules.

TML provides two complementary systems: preprocessor directives (`#if`, `#ifdef`, etc.) and
the `@when` decorator. Both are evaluated before type checking begins. Code in a false branch
is discarded entirely — it is not type-checked, not compiled, and produces no binary output.

---

## Preprocessor Directives

### `#if` and `#endif`

The `#if` directive tests a condition. Everything between `#if` and `#endif` is included
only when the condition is true:

```tml
#if WINDOWS
func get_path_separator() -> Str { return "\\" }
#endif

#if LINUX or MACOS
func get_path_separator() -> Str { return "/" }
#endif
```

Conditions may use `and`, `or`, and `not`:

```tml
#if UNIX and not MACOS
    // Linux, FreeBSD, and other POSIX systems, excluding macOS
    use std::net::epoll::*
#endif
```

### `#elif`

`#elif` provides an alternative condition when the preceding `#if` (or `#elif`) was false:

```tml
#if WINDOWS
    func home_dir() -> Str {
        return env::get("USERPROFILE").unwrap_or("C:\\Users\\Default")
    }
#elif MACOS
    func home_dir() -> Str {
        return env::get("HOME").unwrap_or("/Users/guest")
    }
#elif LINUX
    func home_dir() -> Str {
        return env::get("HOME").unwrap_or("/home/guest")
    }
#else
    func home_dir() -> Str {
        return "/"
    }
#endif
```

### `#else`

`#else` provides a fallback for when all preceding conditions were false:

```tml
#ifdef DEBUG
    func log_level() -> Str { return "debug" }
#else
    func log_level() -> Str { return "info" }
#endif
```

### `#ifdef` and `#ifndef`

`#ifdef SYMBOL` is shorthand for `#if defined(SYMBOL)`. It tests whether a symbol is
defined at all, regardless of its value:

```tml
#ifdef FEATURE_EXPERIMENTAL
    use std::experimental::*
    func enable_experimental() { ... }
#endif
```

`#ifndef SYMBOL` is shorthand for `#if not defined(SYMBOL)`:

```tml
#ifndef PRODUCTION
    func dump_state() {
        println("Internal state: {}", DEBUG_STATE)
    }
#endif
```

---

## Predefined Symbols

The compiler defines the following symbols automatically based on the build environment.

### Operating System

| Symbol | Defined When |
|--------|-------------|
| `WINDOWS` | Target is Windows (any version) |
| `LINUX` | Target is Linux |
| `MACOS` | Target is macOS |
| `ANDROID` | Target is Android |
| `IOS` | Target is iOS |
| `FREEBSD` | Target is FreeBSD |
| `UNIX` | Target is any POSIX-conformant OS (Linux, macOS, FreeBSD, etc.) |
| `POSIX` | Same as `UNIX`; provided as an alias |

### CPU Architecture

| Symbol | Defined When |
|--------|-------------|
| `X86_64` | Target is 64-bit x86 |
| `X86` | Target is 32-bit x86 |
| `ARM64` | Target is 64-bit ARM (AArch64) |
| `ARM` | Target is 32-bit ARM |
| `WASM32` | Target is WebAssembly (32-bit) |
| `RISCV64` | Target is 64-bit RISC-V |

### Build Profile

| Symbol | Defined When |
|--------|-------------|
| `DEBUG` | Built with `tml build` (default) |
| `RELEASE` | Built with `tml build --release` |
| `TEST` | Built by the test runner (`tml test`) |

### Pointer Size

| Symbol | Defined When |
|--------|-------------|
| `PTR_64` | Pointer size is 8 bytes (64-bit target) |
| `PTR_32` | Pointer size is 4 bytes (32-bit target) |

### Endianness

| Symbol | Defined When |
|--------|-------------|
| `LITTLE_ENDIAN` | Target is little-endian (x86, ARM, most modern systems) |
| `BIG_ENDIAN` | Target is big-endian (some RISC-V, MIPS, PowerPC configurations) |

---

## User-Defined Symbols

Pass `-D` flags to `tml build` to define your own symbols:

```bash
tml build -DFEATURE_ANALYTICS -DAPI_VERSION=2
```

In source code, test them with `#ifdef` or `#if`:

```tml
#ifdef FEATURE_ANALYTICS
    use analytics::*

    func track_event(name: Str) {
        analytics::record(name)
    }
#else
    func track_event(name: Str) {
        // no-op
    }
#endif

#if API_VERSION == 2
    type ApiResponse { data: JsonValue, version: I32 }
#elif API_VERSION == 1
    type ApiResponse { result: Str }
#endif
```

Symbols defined without a value (like `-DFEATURE_ANALYTICS`) are defined as `1`. Symbols
defined with a value (like `-DAPI_VERSION=2`) hold that integer or string value.

---

## Conditional Compilation with `@when`

The `@when` decorator conditions a single item on a target property. It is more concise
than a `#if`/`#endif` block when you are conditioning just one function or type:

```tml
@when(target_os = "windows")
func create_named_pipe(name: Str) -> Outcome[Pipe, IoError] {
    // Windows implementation using CreateNamedPipe
}

@when(target_os = "linux", target_os = "macos")
func create_named_pipe(name: Str) -> Outcome[Pipe, IoError] {
    // POSIX implementation using mkfifo
}
```

Multiple values for the same key are combined with OR. Multiple different keys are combined
with AND:

```tml
// Included only on 64-bit Linux
@when(target_os = "linux", target_arch = "x86_64")
func use_linux_64_path() { ... }
```

For conditions that span many items or contain complex logic, use `#if`/`#endif` instead.
For conditions on a single item with simple target checks, `@when` is cleaner.

---

## Practical Examples

### Platform-Specific File Paths

```tml
func config_dir() -> Str {
#if WINDOWS
    return env::get("APPDATA").unwrap_or("C:\\ProgramData")
#elif MACOS
    return env::get("HOME").unwrap_or("/tmp") + "/Library/Application Support"
#else
    return env::get("XDG_CONFIG_HOME").unwrap_or(
        env::get("HOME").unwrap_or("/tmp") + "/.config"
    )
#endif
}
```

### Debug Assertions

Assertions that only run in debug builds avoid overhead in release binaries while providing
helpful checks during development:

```tml
func insert_sorted(list: mut ref List[I32], value: I32) {
#ifdef DEBUG
    // Verify invariant before insert
    loop i in 1 to list.len() {
        assert(list[i - 1] <= list[i], "list must be sorted before insert")
    }
#endif

    let pos = list.partition_point(do(x) *x < value)
    list.insert(pos, value)
}
```

### Feature Flags

Feature flags let downstream consumers enable optional behavior at build time:

```tml
#ifdef ENABLE_PROFILING
    var PROFILER: Profiler = Profiler::new()

    func begin_section(name: Str) {
        PROFILER.begin(name)
    }

    func end_section() {
        PROFILER.end()
    }
#else
    func begin_section(name: Str) { }
    func end_section() { }
#endif
```

Build with `tml build -DENABLE_PROFILING` to get the real profiler; omit the flag for
zero-cost no-ops in production.

### Architecture-Specific Optimizations

```tml
func sum_f64_array(data: ref [F64]) -> F64 {
#if X86_64
    return sum_avx(data)     // use AVX2 SIMD on x86_64
#elif ARM64
    return sum_neon(data)    // use NEON SIMD on ARM64
#else
    var acc: F64 = 0.0
    loop i in 0 to data.len() {
        acc += data[i]
    }
    return acc
#endif
}
```

### Test-Only Code

Code that should only exist during testing can be conditioned on the `TEST` symbol:

```tml
#ifdef TEST
pub func test_helper_reset_global_state() {
    GLOBAL_COUNTER = 0
    GLOBAL_FLAG = false
}
#endif
```

This function is compiled only when running `tml test`. It does not appear in production
builds, so it cannot be called from production code accidentally.

---

## Interaction Between `#if` and `@when`

Both mechanisms condition code on the same underlying set of target properties, but they
operate at different granularities:

| | `#if`/`#endif` | `@when` |
|--|----------------|---------|
| Granularity | Arbitrary block of items | Single item |
| Conditions | Full boolean expressions with `and`, `or`, `not` | Key = value pairs |
| User symbols | Yes (`-DFOO`) | No |
| Nesting | Yes | No |
| Best for | Multiple items, complex logic, user symbols | Single function/type with simple target conditions |

Both are fully resolved before type checking. Neither has any runtime cost.

---

## Common Mistakes

**Forgetting `#endif`:** Every `#if`, `#ifdef`, and `#ifndef` must have a matching `#endif`.
A missing `#endif` is a compile error.

**Using `#if` inside function bodies:** Preprocessor directives are item-level constructs.
They cannot appear inside function bodies. To conditionally compile code within a function,
either extract the conditional parts into separate helper functions and use `@when`, or
restructure the function using `#if` at the module level.

**Relying on symbol values without checking:** `-DAPI_VERSION=2` defines `API_VERSION` as
`2`. Testing `#ifdef API_VERSION` is true; testing `#if API_VERSION == 2` checks the value.
Do not assume a defined symbol has any particular value.

---

## See Also

- [Built-in Decorators](ch14-01-builtin-decorators.md) — `@when` decorator reference
- [Foreign Function Interface](ch17-00-ffi.md) — conditional FFI bindings by platform
- [Testing](ch13-00-testing.md) — `TEST` symbol and test-only code patterns
