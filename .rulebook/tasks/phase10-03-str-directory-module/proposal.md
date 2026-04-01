# Proposal: Convert str.tml to Directory Module

## Status: PROPOSED

## Why

`lib/core/src/str.tml` (2,017 lines) contains 54 public functions for string manipulation. It's the largest TML library file and cannot be mechanically split because all functions are at the top level of the module.

## Key Insight: No Compiler Changes Needed

The TML module resolver already supports directory modules:
- `env_module_loading.cpp` tries `str.tml` first, then `str/mod.tml`
- `pub use` re-exports are fully implemented (proven by `std::sync/mod.tml`)
- `use core::str` will transparently resolve to `str/mod.tml`

## What Changes

Convert `str.tml` (single file) → `str/` directory with 7 submodule files + `mod.tml`.

| File | Functions | Lines |
|------|-----------|-------|
| `str/basic.tml` | len, is_empty, char_at, first_char, last_char, substring, substring_from, substring_to | ~250 |
| `str/search.tml` | contains, starts_with, ends_with, find, rfind, is_ascii, matches | ~200 |
| `str/split.tml` | split, split_whitespace, lines, split_once, rsplit_once, splitn, rsplit | ~300 |
| `str/replace.tml` | replace, replace_first, replacen | ~200 |
| `str/transform.tml` | trim, trim_start, trim_end, trim_matches, to_uppercase, to_lowercase, eq_ignore_ascii_case | ~300 |
| `str/parse.tml` | parse_i32, parse_i64, parse_f64, parse_bool | ~270 |
| `str/convert.tml` | chars, bytes, concat, concat_all, repeat, join, pad_left, pad_right, strip_prefix, strip_suffix | ~200 |
| `str/mod.tml` | pub use all submodules | ~20 |

## Dependency Note

Functions within str.tml call each other minimally:
- `trim` calls `trim_start`/`trim_end` — kept together in `transform.tml`
- All other functions are standalone (use lowlevel ptr ops, not TML-level calls)

Internal helpers like `is_whitespace` and `c_strlen`/`c_memcmp`/`c_memchr` (FFI declarations) need to be in a shared location — either duplicated per file or in `basic.tml` and imported by others.
