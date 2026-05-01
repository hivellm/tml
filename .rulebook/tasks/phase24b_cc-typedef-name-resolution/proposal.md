# Proposal: phase24b_cc-typedef-name-resolution

## Why

After phase24a wired the C preprocessor (directive sweep, macro
expansion, `#include` resolution, bundled stdlib headers) into
`cc_driver`, the next blocker for `tml cc compiler/runtime/core/essential.c`
is the C17 parser's handling of typedef names. A two-line
reproducer crashes the parser silently:

```c
typedef int int32_t;
int32_t add(int32_t a, int32_t b) { return a + b; }
```

`tml cc test.c --emit=ast` returns no output and exits non-zero.
Adding `--emit=tokens` shows the preprocessor output is correct
(34 tokens with `int32_t` appearing as an `Ident`), so the failure
is downstream — almost certainly in `cp_parse_specifiers` /
`cp_parse_top_decl` where the C grammar's typedef-name vs
identifier disambiguation lives. `essential.c` declares 30+
typedefs and uses them as parameter / variable / return types
throughout, so this single bug blocks the entire phase24
self-compile gate.

## What Changes

1. **Diagnose the typedef-as-type crash.** Bisect the failing path
   in `compiler-tml/src/cc/parser.tml`; the parser already maintains
   `parser.typedefs: HashMap[Str, I64]` and registers names from
   `typedef` declarations in `cp_parse_top_decl`, so the two
   suspects are: (a) `cp_at_decl_specifier` / `cp_parse_specifiers`
   not consulting the typedef map when the next token is an
   `Ident`, or (b) the `CBaseType::Typedef(Str)` registration
   suffering a dangling-Str bug similar to phase0x.

2. **Fix the parser** so a previously-typedef'd identifier in a
   declaration position is recognised as a type specifier and
   produces `CBaseType::Typedef(name)`.

3. **Add a regression test** that exercises typedef-then-use
   directly (independent of `#include`). Land under
   `compiler-tml/tests/native/c_parser.test.tml`.

4. **Re-run phase24a Phase 5**: with this fix landed,
   `tml cc compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast`
   should reach further into the file before hitting the next
   limitation.

## Impact

- Affected specs: none (parser bugfix, no language change).
- Affected code:
  - `compiler-tml/src/cc/parser.tml` (the actual parser fix).
  - `compiler-tml/tests/native/c_parser.test.tml` (regression).
- Breaking change: NO.
- User benefit: unblocks phase24 Phase 4 — `tml cc` becomes
  usable on the project's own C runtime files. Without this fix
  the entire bundled stdlib in
  `compiler/runtime/include/c-stdlib/` is unreachable past
  `stddef.h`.
