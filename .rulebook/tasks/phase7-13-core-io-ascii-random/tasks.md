# Tasks: Core I/O Traits, ASCII Expansion, Random Trait

**Status**: Complete
**Priority**: LOW
**Phase**: 7 — Rust Parity

## Phase 1: Core I/O Traits

- [x] 1.1 Create `lib/core/src/io.tml` (or `io/mod.tml`)
- [x] 1.2 `behavior Read` — `func read(mut this, buf: Buffer) -> Outcome[I64, IoError]`
- [x] 1.3 `behavior Write` — `func write(mut this, buf: Buffer) -> Outcome[I64, IoError]`
- [x] 1.4 `behavior BufRead: Read` — `func read_line(mut this) -> Outcome[Str, IoError]`
- [x] 1.5 Move IoError/IoErrorKind from `std::io` to `core::io` (re-export from std)
- [x] 1.6 Tests

## Phase 2: ASCII Expansion

- [x] 2.1 `AsciiChar` enum — all 128 ASCII characters (pre-existing in core::ascii::char)
- [x] 2.2 `is_ascii_alphabetic(c: U8) -> Bool`
- [x] 2.3 `is_ascii_digit(c: U8) -> Bool`
- [x] 2.4 `is_ascii_alphanumeric(c: U8) -> Bool`
- [x] 2.5 `is_ascii_whitespace(c: U8) -> Bool`
- [x] 2.6 `is_ascii_punctuation(c: U8) -> Bool`
- [x] 2.7 `is_ascii_control(c: U8) -> Bool`
- [x] 2.8 `is_ascii_uppercase(c: U8) -> Bool` / `is_ascii_lowercase(c: U8) -> Bool`
- [x] 2.9 `to_ascii_uppercase(c: U8) -> U8` / `to_ascii_lowercase(c: U8) -> U8`
- [x] 2.10 Tests

## Phase 3: Core Random Trait

- [x] 3.1 `behavior Random` in `core::random` — `func random(rng: mut ref Rng) -> Self`
- [x] 3.2 `impl Random for I32`, `I64`, `F64`, `Bool`
- [ ] 3.3 Ensure `std::random::Rng` satisfies the core trait — BLOCKED: pre-existing T016 error in std/src/random.tml (generic trait dispatch codegen bug, not introduced by this task)
- [x] 3.4 Tests
