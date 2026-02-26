# Proposal: Stdin Implementation & Parse Method Chaining Fix

**Date**: 2026-02-25
**Status**: Implemented

---

## 1. Feature: `std::io::stdin` Module

### Motivation

TML had no way to read interactive input from the terminal. Programs couldn't prompt users and wait for responses, making CLI tools impossible to build.

### Design Decisions

1. **Minimal C code** — Only 2 C functions added (`stdin_read_line`, `stdin_flush_stdout`), all higher-level logic in pure TML. Follows project rule of minimizing C/C++ code.

2. **Reuse existing file.c** — The stdin functions were added to `lib/std/runtime/file.c` since they share the same `<stdio.h>` dependencies and follow the same pattern as `file_read_line()`.

3. **Module location: `std::io::stdin`** — Placed under `std::io` (not `std::file`) since stdin is a stream, not a file. Converted `io.tml` from a single file to a directory (`io/mod.tml` + `io/stdin.tml`).

4. **Simple API without `Outcome`** — Follows the existing pattern of `File` which returns `Str` directly (not `Outcome[Str, IoError]`). Can evolve to use `Outcome` later.

5. **`stdin_flush_stdout()`** — Necessary so `prompt()` shows text before blocking. Without flushing, stdout buffering could hide the prompt.

### Files Modified

| File | Change |
|------|--------|
| `lib/std/runtime/file.c` | Added `stdin_read_line()` and `stdin_flush_stdout()` |
| `lib/std/runtime/file.h` | Added function declarations |
| `lib/std/src/io/mod.tml` | Converted from `io.tml`, added `pub mod stdin` |
| `lib/std/src/io/stdin.tml` | **New** — `read_line()` and `prompt()` functions |
| `compiler/src/cli/builder/helpers.cpp` | Added `std::io::stdin` to linker condition for `file.c` |

### Public API

```tml
use std::io::stdin

// Read a line from stdin (blocks until Enter)
pub func read_line() -> Str

// Print prompt then read a line
pub func prompt(message: Str) -> Str
```

### Usage Example

```tml
use std::io::stdin

func main() {
    let name = stdin::prompt("Your name: ")
    print("Hello, {name}!\n")
}
```

---

## 2. Bugfix: `Str.parse_*().unwrap()` Method Chaining

### Symptom

```tml
let s: Str = "42"
let val: I64 = s.parse_i64().unwrap()  // ERROR: "Unknown method: unwrap"
```

But these worked:
```tml
let parsed: Maybe[I64] = str::parse_i64("42")
let val: I64 = parsed.unwrap()          // OK (variable with explicit type)

let val: I64 = str::parse_i64("42").unwrap()  // OK (free function chaining)
```

### Root Cause

The codegen's method dispatch in `method.cpp` calls `infer_expr_type()` to determine the receiver's type before dispatching to `gen_maybe_method()`. For chained calls like `s.parse_i64().unwrap()`, the receiver of `.unwrap()` is the expression `s.parse_i64()`.

`infer_expr_type` delegates to `infer_expr_type_continued()` in `infer_methods.cpp` for method call expressions. This function has hardcoded return type mappings for known methods. However, **`Str.parse_i64()` and all other `Str.parse_*()` methods were missing** from the inference table.

Without knowing the return type of `s.parse_i64()`, the system returned `nullptr` as the receiver type for `.unwrap()`. The `if (named.name == "Maybe")` guard at line 889 of `method.cpp` was never reached, falling through to the "Unknown method" error at line 1306.

### Why Free Functions Worked

`str::parse_i64("42").unwrap()` worked because:
1. `str::parse_i64` is resolved via function signature lookup in the module registry
2. The function's return type `Maybe[I64]` is explicitly declared and accessible
3. `infer_expr_type` for `CallExpr` (not `MethodCallExpr`) has a different code path that looks up function signatures

### Fix

Added return type inference for all 13 `Str.parse_*()` methods in `compiler/src/codegen/llvm/expr/infer_methods.cpp`, inside the `PrimitiveKind::Str` block:

| Method | Returns |
|--------|---------|
| `parse_i8` | `Maybe[I8]` |
| `parse_i16` | `Maybe[I16]` |
| `parse_i32` | `Maybe[I32]` |
| `parse_i64` | `Maybe[I64]` |
| `parse_i128` | `Maybe[I128]` |
| `parse_u8` | `Maybe[U8]` |
| `parse_u16` | `Maybe[U16]` |
| `parse_u32` | `Maybe[U32]` |
| `parse_u64` | `Maybe[U64]` |
| `parse_u128` | `Maybe[U128]` |
| `parse_f32` | `Maybe[F32]` |
| `parse_f64` | `Maybe[F64]` |
| `parse_bool` | `Maybe[Bool]` |

### File Modified

| File | Change |
|------|--------|
| `compiler/src/codegen/llvm/expr/infer_methods.cpp` | Added `Str.parse_*` return type inference (13 methods) |

### Broader Implication

Any method whose return type isn't in `infer_methods.cpp` will break method chaining when the result is used as a receiver for `.unwrap()`, `.map()`, `.and_then()`, etc. Other `Str` methods that return complex types may also need similar treatment. The long-term fix is to have the type checker propagate semantic types to all expressions so the codegen doesn't need hardcoded inference tables.

### Verification

All 3 scenarios now pass:

```tml
// Scenario 1: explicit variable type (already worked)
let parsed: Maybe[I64] = str::parse_i64("42")
let val1: I64 = parsed.unwrap()           // OK

// Scenario 2: free function chaining (already worked)
let val2: I64 = str::parse_i64("99").unwrap()  // OK

// Scenario 3: method chaining (was broken, now fixed)
let s: Str = "123"
let val3: I64 = s.parse_i64().unwrap()    // OK
```
