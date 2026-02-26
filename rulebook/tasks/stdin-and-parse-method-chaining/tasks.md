# Tasks: Stdin Implementation & Parse Method Chaining Fix

**Status**: Complete (100%)

## Phase 1: Stdin Module (std::io::stdin)

- [x] 1.1 Add `stdin_read_line()` and `stdin_flush_stdout()` to `lib/std/runtime/file.c`
- [x] 1.2 Add function declarations to `lib/std/runtime/file.h`
- [x] 1.3 Convert `lib/std/src/io.tml` to directory `lib/std/src/io/mod.tml`
- [x] 1.4 Create `lib/std/src/io/stdin.tml` with `read_line()` and `prompt()` API
- [x] 1.5 Add `std::io::stdin` to linker condition in `compiler/src/cli/builder/helpers.cpp`
- [x] 1.6 Build and test stdin with piped input

## Phase 2: Bugfix — Str.parse_*().unwrap() Method Chaining

- [x] 2.1 Diagnose "Unknown method: unwrap" on `s.parse_i64().unwrap()`
- [x] 2.2 Identify root cause: missing return type inference for `Str.parse_*()` in `infer_methods.cpp`
- [x] 2.3 Add inference for 13 parse methods (parse_i8 through parse_f64, parse_bool) returning `Maybe[T]`
- [x] 2.4 Rebuild compiler and verify all 3 scenarios pass:
  - [x] 2.4.1 `let parsed: Maybe[I64] = str::parse_i64("42"); parsed.unwrap()` (already worked)
  - [x] 2.4.2 `str::parse_i64("99").unwrap()` (free function chaining — already worked)
  - [x] 2.4.3 `s.parse_i64().unwrap()` (method chaining — was broken, now fixed)
- [x] 2.5 Verify exponencial.tml works with clean syntax
