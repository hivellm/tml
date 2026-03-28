# Tasks: Str Completeness

**Status**: Proposed
**Priority**: HIGH
**Phase**: 7 — Rust Parity

## Phase 1: High priority parsing helpers
- [ ] 1.1 `split_once(this, delim: Str) -> Maybe[(Str, Str)]`
- [ ] 1.2 `rsplit_once(this, delim: Str) -> Maybe[(Str, Str)]`
- [ ] 1.3 `strip_prefix(this, prefix: Str) -> Maybe[Str]`
- [ ] 1.4 `strip_suffix(this, suffix: Str) -> Maybe[Str]`
- [ ] 1.5 `splitn(this, n: I64, delim: Str) -> List[Str]`
- [ ] 1.6 `is_ascii(this) -> Bool`
- [ ] 1.7 `eq_ignore_ascii_case(this, other: Str) -> Bool`
- [ ] 1.8 Tests

## Phase 2: Medium priority utility methods
- [ ] 2.1 `rsplit(this, delim: Str) -> List[Str]`
- [ ] 2.2 `replacen(this, pat: Str, to: Str, count: I64) -> Str`
- [ ] 2.3 `trim_matches(this, chars: Str) -> Str`
- [ ] 2.4 `matches(this, pat: Str) -> List[Str]`
- [ ] 2.5 `bytes(this) -> List[U8]`
- [ ] 2.6 `char_indices(this) -> List[(I64, Char)]`
- [ ] 2.7 Tests
