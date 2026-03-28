# Tasks: Str Completeness

**Status**: Complete
**Priority**: HIGH
**Phase**: 7 — Rust Parity

## Phase 1: High priority parsing helpers
- [x] 1.1 `split_once(this, delim: Str) -> Maybe[(Str, Str)]`
- [x] 1.2 `rsplit_once(this, delim: Str) -> Maybe[(Str, Str)]`
- [x] 1.3 `strip_prefix(this, prefix: Str) -> Maybe[Str]`
- [x] 1.4 `strip_suffix(this, suffix: Str) -> Maybe[Str]`
- [x] 1.5 `splitn(this, n: I64, delim: Str) -> List[Str]`
- [x] 1.6 `is_ascii(this) -> Bool`
- [x] 1.7 `eq_ignore_ascii_case(this, other: Str) -> Bool`
- [x] 1.8 Tests — lib/core/tests/str/str_new_methods.test.tml (60 @test functions)

## Phase 2: Medium priority utility methods
- [x] 2.1 `rsplit(this, delim: Str) -> List[Str]`
- [x] 2.2 `replacen(this, pat: Str, repl: Str, count: I64) -> Str`
- [x] 2.3 `trim_matches(this, chars: Str) -> Str`
- [x] 2.4 `matches(this, pat: Str) -> List[Str]`
- [x] 2.5 `bytes(this) -> List[U8]`
- [ ] 2.6 `char_indices(this) -> List[(I64, Char)]` — BLOCKED: List[(I64, I32)] codegen bug (ACCESS_VIOLATION). Tuple-typed Lists crash the compiler.
- [x] 2.7 Tests — all functions tested via sandbox (str suite has pre-existing 100ms timeout)

## Notes
- All new functions have both module-level `pub func` and `impl Str {}` method wrappers
- str suite timeout is pre-existing (affects ALL core/str test files) — not caused by these changes
- `char_indices` removed entirely — will be unblocked when tuple List codegen is fixed
- `to` is a TML keyword — `replacen` uses `repl` parameter name instead
- Integer underflow guard required: `d_len > s_len` early return in all loop-bound calculations
