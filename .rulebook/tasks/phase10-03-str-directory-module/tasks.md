# Tasks: Convert str.tml to Directory Module

**Status**: Proposed — 0/15
**Priority**: Medium
**Risk**: MEDIUM (module resolver is proven, but 54 functions need correct re-export)
**Target**: str.tml 2,017 lines → 7 files, each <350 lines

## Phase 1: Setup + basic.tml — 0/3

- [ ] 1.1 Create `lib/core/src/str/` directory
- [ ] 1.2 Create `str/basic.tml`: move len, is_empty, char_at, first_char, last_char, substring, substring_from, substring_to + shared helpers (is_whitespace, c_strlen, c_memcmp, c_memchr)
- [ ] 1.3 Create minimal `str/mod.tml` with `pub use` for basic, verify `mcp__tml__check` passes

## Phase 2: Search + Split — 0/3

- [ ] 2.1 Create `str/search.tml`: contains, starts_with, ends_with, find, rfind, is_ascii, matches
- [ ] 2.2 Create `str/split.tml`: split, split_whitespace, lines, split_once, rsplit_once, splitn, rsplit
- [ ] 2.3 Update `str/mod.tml`, verify check passes

## Phase 3: Replace + Transform — 0/3

- [ ] 3.1 Create `str/replace.tml`: replace, replace_first, replacen
- [ ] 3.2 Create `str/transform.tml`: trim, trim_start, trim_end, trim_matches, to_uppercase, to_lowercase, eq_ignore_ascii_case
- [ ] 3.3 Update `str/mod.tml`, verify check passes

## Phase 4: Parse + Convert — 0/3

- [ ] 4.1 Create `str/parse.tml`: parse_i32, parse_i64, parse_f64, parse_bool
- [ ] 4.2 Create `str/convert.tml`: chars, bytes, concat, concat_all, repeat, join, pad_left, pad_right, strip_prefix, strip_suffix
- [ ] 4.3 Update `str/mod.tml` with all 7 submodules, verify check passes

## Phase 5: Finalize — 0/3

- [ ] 5.1 Delete `lib/core/src/str.tml`
- [ ] 5.2 Run `mcp__tml__test suite="core/str"` — all 24 tests must pass
- [ ] 5.3 Run `mcp__tml__test suite="std/collections"` — verify cross-module str usage works
