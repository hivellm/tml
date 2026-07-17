# Tasks: Convert str.tml to Directory Module

**Status**: Done — 15/15
**Priority**: Medium
**Risk**: MEDIUM (module resolver is proven, but 54 functions need correct re-export)
**Result**: str.tml 2,017 lines → 8 files in str/ directory (7 submodules + mod.tml)

## Phase 1: Setup + basic.tml — 3/3

- [x] 1.1 Create `lib/core/src/str/` directory
- [x] 1.2 Create `str/basic.tml`: len, is_empty, char_at, first_char, last_char, substring, substring_from, substring_to + FFI helpers
- [x] 1.3 Create minimal `str/mod.tml` with `pub use` for basic

## Phase 2: Search + Split — 3/3

- [x] 2.1 Create `str/search.tml`: contains, starts_with, ends_with, find, rfind, is_ascii, matches
- [x] 2.2 Create `str/split.tml`: split, split_whitespace, lines, split_once, rsplit_once, splitn, rsplit
- [x] 2.3 Update `str/mod.tml`, check passes

## Phase 3: Replace + Transform — 3/3

- [x] 3.1 Create `str/replace.tml`: replace, replace_first, replacen
- [x] 3.2 Create `str/transform.tml`: trim, trim_start, trim_end, trim_matches, to_uppercase, to_lowercase, eq_ignore_ascii_case
- [x] 3.3 Update `str/mod.tml`, check passes

## Phase 4: Parse + Convert — 3/3

- [x] 4.1 Create `str/parse.tml`: parse_i32, parse_i64, parse_f64, parse_bool
- [x] 4.2 Create `str/convert.tml`: chars, bytes, concat, concat_all, repeat, join, pad_left, pad_right, strip_prefix, strip_suffix
- [x] 4.3 Update `str/mod.tml` with all 7 submodules + impl Str block

## Phase 5: Finalize — 3/3

- [x] 5.1 Renamed `str.tml` → `str.tml.bak` (can be deleted after verification)
- [x] 5.2 `mcp__tml__test suite="core/str"` — 24/24 pass
- [x] 5.3 `mcp__tml__test suite="std/collections"` — 94/94 pass (cross-module OK)

## Notes

- Submodules use `use core::str::basic::len` etc. for cross-submodule dependencies
- FFI declarations (`@extern("strlen")` etc.) duplicated per submodule — linker deduplicates
- `impl Str` block stays in mod.tml, uses `str::funcname()` which resolves via pub use re-exports
- No compiler changes needed — directory module resolver already supported
