# Tasks: Implement Regex Module

**Status**: Proposed (0%)

## Phase 1: C++ Regex Engine Core

- [ ] 1.1.1 Create `compiler/include/regex/nfa.hpp` with NFA state types (State, Fragment, StateList)
- [ ] 1.1.2 Create `compiler/include/regex/regex_engine.hpp` with RegexEngine class interface
- [ ] 1.1.3 Implement regex parser: tokenize pattern string into AST nodes
- [ ] 1.1.4 Implement character class parsing (`[a-z]`, `[^0-9]`, `\d`, `\w`, `\s`)
- [ ] 1.1.5 Implement quantifier parsing (`*`, `+`, `?`, `{n,m}`)
- [ ] 1.1.6 Implement alternation and grouping (`|`, `(...)`, `(?:...)`)
- [ ] 1.1.7 Implement Thompson's NFA construction from parsed AST
- [ ] 1.1.8 Implement NFA simulation for matching (multi-state traversal)
- [ ] 1.1.9 Implement capture group tracking during NFA simulation
- [ ] 1.1.10 Implement anchor support (`^`, `$`)

## Phase 2: C++ Engine — Advanced Features

- [ ] 2.1.1 Implement lazy DFA compilation from NFA for performance
- [ ] 2.1.2 Add DFA state cache with eviction (limit state explosion)
- [ ] 2.1.3 Implement `find()` — locate first match position in input
- [ ] 2.1.4 Implement `find_all()` — iterate all non-overlapping matches
- [ ] 2.1.5 Implement `replace()` — substitute first match with replacement
- [ ] 2.1.6 Implement `replace_all()` — substitute all matches
- [ ] 2.1.7 Implement `split()` — split input by regex delimiter
- [ ] 2.1.8 Support backreferences in replacement strings (`$0`, `$1`, etc.)
- [ ] 2.1.9 Handle escape sequences (`\.`, `\\`, `\n`, `\t`, `\r`)
- [ ] 2.1.10 Handle UTF-8 literal matching (multi-byte characters)

## Phase 3: C++ Engine Unit Tests

- [ ] 3.1.1 Create `compiler/tests/regex/regex_engine_test.cpp`
- [ ] 3.1.2 Test literal matching and concatenation
- [ ] 3.1.3 Test character classes and predefined classes (`\d`, `\w`, `\s`)
- [ ] 3.1.4 Test quantifiers (`*`, `+`, `?`, `{n}`, `{n,m}`)
- [ ] 3.1.5 Test alternation and grouping
- [ ] 3.1.6 Test anchors (`^`, `$`)
- [ ] 3.1.7 Test capture group extraction
- [ ] 3.1.8 Test `find_all()` with overlapping/adjacent matches
- [ ] 3.1.9 Test `replace()` and `replace_all()` with backreferences
- [ ] 3.1.10 Test `split()` with edge cases (empty segments, trailing delimiters)
- [ ] 3.1.11 Test invalid pattern handling (graceful error reporting)
- [ ] 3.1.12 Test O(n) performance guarantee (no catastrophic backtracking patterns)

## Phase 4: C FFI Wrapper

- [ ] 4.1.1 Create `compiler/runtime/regex/regex_engine.cpp` with opaque handle API
- [ ] 4.1.2 Implement `regex_create(pattern) -> void*` — compile pattern
- [ ] 4.1.3 Implement `regex_destroy(handle)` — free resources
- [ ] 4.1.4 Implement `regex_is_match(handle, input) -> int32_t`
- [ ] 4.1.5 Implement `regex_find(handle, input, out_start, out_end) -> int32_t`
- [ ] 4.1.6 Implement `regex_find_all(handle, input, out_starts, out_ends, max) -> int32_t`
- [ ] 4.1.7 Implement `regex_captures(handle, input, out_starts, out_ends, max_groups) -> int32_t`
- [ ] 4.1.8 Implement `regex_replace(handle, input, replacement) -> char*`
- [ ] 4.1.9 Implement `regex_replace_all(handle, input, replacement) -> char*`
- [ ] 4.1.10 Implement `regex_split(handle, input, out_parts, max) -> int32_t`
- [ ] 4.1.11 Implement `regex_error(handle) -> char*` — get compilation error message
- [ ] 4.1.12 Implement `regex_free_string(ptr)` — free returned strings

## Phase 5: CMake Build Integration

- [ ] 5.1.1 Add `compiler/include/regex/` headers to include paths
- [ ] 5.1.2 Add `compiler/src/regex/` (or inline in runtime) sources to `tml_runtime` or separate `tml_regex` library
- [ ] 5.1.3 Add `compiler/runtime/regex/regex_engine.cpp` to runtime build
- [ ] 5.1.4 Add `compiler/tests/regex/` tests to test build
- [ ] 5.1.5 Verify clean build with `scripts\build.bat`
- [ ] 5.1.6 Verify C++ tests pass with `scripts\test.bat`

## Phase 6: TML Standard Library Module

- [ ] 6.1.1 Create `lib/std/src/regex/mod.tml` with module declaration and re-exports
- [ ] 6.1.2 Create `lib/std/src/regex/regex.tml` with `Regex` type and FFI declarations
- [ ] 6.1.3 Implement `Regex.new(pattern: Str) -> Regex`
- [ ] 6.1.4 Implement `Regex.is_match(this, input: Str) -> Bool`
- [ ] 6.1.5 Implement `Regex.find(this, input: Str) -> Maybe[Match]`
- [ ] 6.1.6 Implement `Regex.find_all(this, input: Str) -> List[Match]`
- [ ] 6.1.7 Implement `Regex.captures(this, input: Str) -> Maybe[Captures]`
- [ ] 6.1.8 Implement `Regex.replace(this, input: Str, replacement: Str) -> Str`
- [ ] 6.1.9 Implement `Regex.replace_all(this, input: Str, replacement: Str) -> Str`
- [ ] 6.1.10 Implement `Regex.split(this, input: Str) -> List[Str]`
- [ ] 6.1.11 Implement `Regex.destroy(mut this)` for resource cleanup
- [ ] 6.1.12 Create `lib/std/src/regex/match.tml` with `Match` type (start, end, text)
- [ ] 6.1.13 Create `lib/std/src/regex/captures.tml` with `Captures` type (get, len)
- [ ] 6.1.14 Add `pub mod regex` to `lib/std/src/mod.tml`

## Phase 7: TML Tests

- [ ] 7.1.1 Create `lib/std/tests/regex/regex_basic.test.tml` — literal matching, is_match
- [ ] 7.1.2 Test character classes (`[a-z]`, `[^0-9]`, `\d`, `\w`, `\s`)
- [ ] 7.1.3 Test quantifiers (`*`, `+`, `?`, `{n}`, `{n,m}`)
- [ ] 7.1.4 Test anchors (`^`, `$`)
- [ ] 7.1.5 Test alternation (`a|b`) and grouping
- [ ] 7.1.6 Create `lib/std/tests/regex/regex_captures.test.tml` — capture group extraction
- [ ] 7.1.7 Test numbered capture groups (`$1`, `$2`)
- [ ] 7.1.8 Test non-capturing groups (`(?:...)`)
- [ ] 7.1.9 Test nested capture groups
- [ ] 7.1.10 Create `lib/std/tests/regex/regex_replace.test.tml` — replace and split
- [ ] 7.1.11 Test `replace()` with literal replacement
- [ ] 7.1.12 Test `replace_all()` with backreference substitution
- [ ] 7.1.13 Test `split()` basic and edge cases
- [ ] 7.1.14 Create `lib/std/tests/regex/regex_edge_cases.test.tml`
- [ ] 7.1.15 Test empty pattern, empty input
- [ ] 7.1.16 Test special characters in input (no false matches)
- [ ] 7.1.17 Test large input strings (performance sanity)
- [ ] 7.1.18 Test invalid regex pattern error handling

## Phase 8: Documentation

- [ ] 8.1.1 Add regex module documentation to `docs/specs/INDEX.md`
- [ ] 8.1.2 Add usage examples to `docs/specs/14-EXAMPLES.md`
- [ ] 8.1.3 Document regex syntax supported in module-level doc comments
- [ ] 8.1.4 Document performance characteristics (O(n) guarantee) in doc comments

## Phase 9: Coverage and Validation

- [ ] 9.1.1 Run `tml test --coverage` and verify regex module coverage
- [ ] 9.1.2 Verify no regressions in existing test suite
- [ ] 9.1.3 Verify clean build in both debug and release modes
- [ ] 9.1.4 Verify regex tests pass with `--no-cache` flag

## File Structure

```
compiler/include/regex/
├── regex_engine.hpp
└── nfa.hpp

compiler/runtime/regex/
└── regex_engine.cpp

compiler/tests/regex/
└── regex_engine_test.cpp

lib/std/src/regex/
├── mod.tml
├── regex.tml
├── match.tml
└── captures.tml

lib/std/tests/regex/
├── regex_basic.test.tml
├── regex_captures.test.tml
├── regex_replace.test.tml
└── regex_edge_cases.test.tml
```

## Dependencies

- Phase 4 depends on Phase 1-2 (engine must exist before FFI wrapper)
- Phase 5 depends on Phase 4 (build integration needs source files)
- Phase 6 depends on Phase 5 (TML module needs runtime linked)
- Phase 7 depends on Phase 6 (tests need TML module)
- Phase 3 can run in parallel with Phase 4-5 (C++ tests are independent)
- Phase 8-9 depend on Phase 7 (docs and validation after implementation)

## Deferred Items

- Unicode character categories (`\p{L}`, `\p{N}`, etc.) — future enhancement
- Named capture groups (`(?P<name>...)`) — future enhancement
- Lookahead/lookbehind assertions — future enhancement
- Regex compilation caching / global regex pool — future optimization
- `Regex` implementing `Display` behavior — after behavior system is mature
