# Proposal: Build std::intern — String Interning Module

**Task**: phase12b_string-interning
**Status**: Planned
**Priority**: P1
**Estimated effort**: 3–5 days
**Risk**: Low

## Problem

The TML self-hosting compiler will use string interning extensively for symbol table management:
every identifier (function name, type name, variable name) encountered during lexing or parsing
needs to be stored once and referenced by a cheap integer handle throughout the rest of the
pipeline. Without a dedicated interning module, each compiler subsystem would independently
maintain a `HashMap[Str, I64]` + `List[Str]` pair, duplicating the pattern and risking
consistency bugs when two subsystems intern the same name independently and get different handles.

The standard library currently has no interning type. A search of the existing stdlib (`std::sync`,
`std::collections`, `core::str`) confirms no `Interner`, `InternedStr`, or equivalent. This is a
gap that needs filling before phase12e (AST serializers), which will use `InternedStr` as the
compact representation for symbol names in the binary format — serializing a 4-byte index instead
of a length-prefixed string for every identifier reference in the AST.

## Proposed Solution

Implement `std::intern` as a ~200 LOC module containing two public types:

**`InternedStr`** — an opaque handle wrapping `I64`. The index is the position of the string in
the interner's backing `List[Str]`. The type is intentionally not `Str` to prevent accidental use
as a string without lookup; callers must call `get()` to recover the original text.

**`Interner`** — the owning table. Backed by two fields: `strings: List[Str]` (the authoritative
ordered sequence of interned strings, index = `InternedStr` value) and `index: HashMap[Str, I64]`
(reverse map from string content to position). The `HashMap` is the hot path for deduplication;
the `List` is the hot path for lookup by handle.

Core operations:
- `intern(ref mut self, s: Str) -> InternedStr` — check `index` map first; if present return
  existing handle; otherwise append to `strings`, insert into `index`, return new handle.
- `get(ref self, id: InternedStr) -> Str` — direct index into `strings` list; panics on
  out-of-bounds (callers should only ever hold handles produced by `intern`).
- `len(ref self) -> I64` — number of unique strings interned so far.
- `contains(ref self, s: Str) -> Bool` — non-inserting membership check.

No locking. The `Interner` is designed for single-threaded use within one pipeline stage. If
shared across threads (e.g., in a future parallel lexer), the caller wraps it in `Mutex[Interner]`.

## Key Decisions

**`InternedStr` as `I64`, not a struct.** A newtype wrapper struct would require extractvalue
in codegen for every comparison, adding overhead. An `I64` alias with a distinct type name is
sufficient for type safety at the TML level while remaining a zero-cost primitive at IR level.
Decision: use `I64` internally, exposed as `InternedStr` via the public API.

**`HashMap` + `List` instead of a single `HashMap[Str, I64]`.** A `HashMap` alone supports
`intern()` and `contains()` but makes `get(InternedStr)` O(n) (must scan values). The paired
`List` makes `get()` O(1). The memory overhead of storing each string twice (once as key, once
in the list) is acceptable — compiler symbol tables are small (< 100K entries) and strings are
short-lived.

**No `remove()` or mutable `get()`.** Interners are append-only. Removing a string would
invalidate all handles issued after the removed index, breaking the invariant that
`get(intern(s)) == s` holds for the lifetime of the interner.

**Benchmark threshold: < 5% overhead vs raw HashMap.** The interning abstraction adds one `List`
append and two `HashMap` lookups per unique string. This should be negligible. If benchmarks
show > 5% overhead, the implementation is wrong and must be fixed (not the threshold lowered).

## Files to Create/Modify

**Created**:
- `lib/std/src/intern/mod.tml` — module declaration with `pub use intern::Interner`,
  `pub use intern::InternedStr`
- `lib/std/src/intern/interner.tml` — `Interner` type implementation (~180 LOC)
- `lib/std/tests/intern/basic.test.tml` — unit tests: intern, get, dedup, contains, len,
  edge cases (empty string, single char), stress test with 10K strings

**Modified**:
- `lib/std/src/mod.tml` — add `pub mod intern` to the std module declaration

## Success Criteria

- `mcp__tml__check lib/std/src/intern/interner.tml` reports no type errors
- All tests in `lib/std/tests/intern/` pass
- `intern(s)` called twice with the same `s` returns equal `InternedStr` values
- `get(intern(s)) == s` holds for all test inputs including empty string
- Stress test: 10,000 unique strings interned and retrieved without error
- Benchmark: interning overhead < 5% vs a raw `HashMap[Str, I64]` baseline

## Dependencies

**Blocks**: phase12e (AST serializers use `InternedStr` as the compact representation for
symbol names in the binary format).

**Depends on**: Nothing. `std::collections::HashMap` and `std::collections::List` are already
implemented and tested. This task can start immediately in parallel with any other phase12 work.
