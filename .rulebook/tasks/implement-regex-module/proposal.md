# Proposal: Implement Regex Module

## Status

PROPOSED

## Why

TML currently has no regular expression support. Regex is a fundamental capability for any systems programming language — it's essential for text parsing, input validation, log analysis, configuration file processing, and data extraction. Without regex, users must write manual string parsing code for tasks that should be one-liners.

Every major language ships regex in its standard library (Rust's `regex` crate, Go's `regexp`, Python's `re`, C#'s `System.Text.RegularExpressions`). Adding `std::regex` to TML brings feature parity and makes TML practical for real-world text processing tasks.

## What Changes

### New Module: `std::regex`

A high-performance regex module exposed through TML's standard library with a C/C++ runtime backend.

### API Surface

**Core Types:**

```tml
use std::regex::{Regex, Match, Captures}

// Compile a pattern
let re = Regex.new(r"(\d{4})-(\d{2})-(\d{2})")

// Check if pattern matches
let found: Bool = re.is_match("2026-02-12")

// Find first match
let m: Maybe[Match] = re.find("date: 2026-02-12")

// Find all matches
let matches: List[Match] = re.find_all("2026-02-12 and 2025-01-01")

// Capture groups
let caps: Maybe[Captures] = re.captures("2026-02-12")

// Replace
let result: Str = re.replace("2026-02-12", "REDACTED")
let result: Str = re.replace_all("a1b2c3", "[$0]")

// Split
let parts: List[Str] = re.split("one,two,,three", ",+")
```

**Match Type:**

```tml
pub type Match {
    pub start: I64,   // Byte offset of match start
    pub end: I64,     // Byte offset of match end (exclusive)
}

impl Match {
    pub func text(this) -> Str          // Matched text
    pub func len(this) -> I64           // Match length
}
```

**Captures Type:**

```tml
pub type Captures {
    // Access capture groups by index
    pub func get(this, index: I64) -> Maybe[Match]
    pub func len(this) -> I64           // Number of groups
}
```

### Regex Syntax Support

The implementation will support a standard regex syntax subset:

| Feature | Syntax | Example |
|---------|--------|---------|
| Literals | `abc` | Matches "abc" |
| Character classes | `[a-z]`, `[^0-9]` | Character ranges |
| Predefined classes | `\d`, `\w`, `\s` | Digit, word, whitespace |
| Anchors | `^`, `$` | Start/end of line |
| Quantifiers | `*`, `+`, `?`, `{n,m}` | Repetition |
| Alternation | `a\|b` | Choice |
| Groups | `(...)` | Capture groups |
| Non-capturing | `(?:...)` | Group without capture |
| Escaping | `\.`, `\\` | Literal special chars |

### Runtime Backend

The regex engine will be implemented in C/C++ as part of `tml_runtime`, following the same opaque-handle FFI pattern used by `std::search`.

**Engine Choice:** A custom NFA/DFA-based engine (Thompson's construction) for:
- Guaranteed O(n) matching time (no catastrophic backtracking)
- Predictable performance characteristics
- No external dependencies (self-contained compiler goal)

**Alternative considered:** PCRE2 — rejected because it uses backtracking (exponential worst-case) and adds an external dependency.

### Architecture

```
lib/std/src/regex/
├── mod.tml           # Module declaration and re-exports
├── regex.tml         # Regex type and main API
├── match.tml         # Match and Captures types
└── pattern.tml       # Pattern syntax constants/helpers

compiler/runtime/regex/
├── regex_engine.cpp  # NFA/DFA regex engine implementation
└── regex.c           # C FFI wrapper functions

compiler/include/regex/
├── regex_engine.hpp  # C++ regex engine class header
└── nfa.hpp           # NFA state machine types

compiler/tests/regex/
├── regex_engine_test.cpp  # C++ unit tests for engine

lib/std/tests/regex/
├── regex_basic.test.tml       # Basic matching tests
├── regex_captures.test.tml    # Capture group tests
├── regex_replace.test.tml     # Replace/split tests
└── regex_edge_cases.test.tml  # Edge cases and error handling
```

## Impact

### Affected Specs
- `docs/specs/INDEX.md` — Add regex module to std library listing

### Affected Code
- `lib/std/src/mod.tml` — Add `pub mod regex` declaration
- `compiler/CMakeLists.txt` — Add regex runtime sources to build
- `compiler/runtime/` — New regex runtime directory
- `compiler/include/` — New regex engine headers
- `lib/std/src/regex/` — New TML module files
- `lib/std/tests/regex/` — New test files

### Breaking Changes
None. This is a purely additive feature.

### Benefits
- Enables text parsing, validation, and extraction use cases
- Feature parity with Rust, Go, Python, C# standard libraries
- O(n) guaranteed performance (no catastrophic backtracking)
- No external dependencies (self-contained)

## Risks

1. **Engine complexity** — A correct regex engine with NFA→DFA compilation is non-trivial (~2000-3000 lines of C++). Mitigation: start with a minimal syntax subset and expand incrementally.
2. **Unicode support** — Full Unicode character class support (e.g., `\p{L}`) adds significant complexity. Mitigation: defer Unicode categories to a later phase; support ASCII classes and UTF-8 literals initially.
3. **Performance tuning** — DFA state explosion on certain patterns. Mitigation: use lazy DFA (compile states on demand) with a state cache limit.

## Dependencies

- No external library dependencies (self-contained engine)
- Requires `std::collections::List` for result collections (already implemented)
- Requires `std::text` / `core::str` for string operations (already implemented)

## Success Criteria

1. `Regex.new()` compiles a regex pattern and returns a usable handle
2. `is_match()`, `find()`, `find_all()` work correctly for supported syntax
3. `captures()` correctly extracts numbered capture groups
4. `replace()` and `replace_all()` perform pattern-based substitution
5. `split()` splits strings by regex delimiter
6. All operations complete in O(n) time relative to input length
7. At least 60 TML-level tests pass covering all API methods
8. C++ engine has its own unit tests for correctness
9. No external dependencies required

## References

- Thompson's Construction: [Regular Expression Matching Can Be Simple And Fast](https://swtch.com/~rsc/regexp/regexp1.html)
- Rust regex crate design: [docs.rs/regex](https://docs.rs/regex)
- Go regexp package: [pkg.go.dev/regexp](https://pkg.go.dev/regexp)
- TML search module (FFI pattern reference): `lib/std/src/search/`
