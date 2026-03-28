# Tasks: Core I/O Traits, ASCII Expansion, Random Trait

**Status**: Proposed
**Priority**: LOW
**Phase**: 7 — Rust Parity

## Phase 1: Core I/O Traits

- [ ] 1.1 Create `lib/core/src/io.tml` (or `io/mod.tml`)
- [ ] 1.2 `behavior Read` — `func read(mut this, buf: Buffer) -> Outcome[I64, IoError]`
- [ ] 1.3 `behavior Write` — `func write(mut this, buf: Buffer) -> Outcome[I64, IoError]`
- [ ] 1.4 `behavior BufRead: Read` — `func read_line(mut this) -> Outcome[Str, IoError]`
- [ ] 1.5 Move IoError/IoErrorKind from `std::io` to `core::io` (re-export from std)
- [ ] 1.6 Tests

## Phase 2: ASCII Expansion

- [ ] 2.1 `AsciiChar` enum — all 128 ASCII characters
- [ ] 2.2 `is_ascii_alphabetic(c: U8) -> Bool`
- [ ] 2.3 `is_ascii_digit(c: U8) -> Bool`
- [ ] 2.4 `is_ascii_alphanumeric(c: U8) -> Bool`
- [ ] 2.5 `is_ascii_whitespace(c: U8) -> Bool`
- [ ] 2.6 `is_ascii_punctuation(c: U8) -> Bool`
- [ ] 2.7 `is_ascii_control(c: U8) -> Bool`
- [ ] 2.8 `is_ascii_uppercase(c: U8) -> Bool` / `is_ascii_lowercase(c: U8) -> Bool`
- [ ] 2.9 `to_ascii_uppercase(c: U8) -> U8` / `to_ascii_lowercase(c: U8) -> U8`
- [ ] 2.10 Tests

## Phase 3: Core Random Trait

- [ ] 3.1 `behavior Random` in `core::random` — `func random(rng: mut ref Rng) -> Self`
- [ ] 3.2 `impl Random for I32`, `I64`, `F64`, `Bool`
- [ ] 3.3 Ensure `std::random::Rng` satisfies the core trait
- [ ] 3.4 Tests
