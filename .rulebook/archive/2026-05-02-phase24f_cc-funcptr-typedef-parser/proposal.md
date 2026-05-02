# Proposal: phase24f_cc-funcptr-typedef-parser

## Why

After phase24e shipped CType deep-clone via `impl Duplicate for
CType`, every typedef / struct / union / enum tag reference works
end-to-end through `cc_driver`. The remaining blocker for
`essential.c` self-compile is a parser bug specific to bare
function-pointer typedef declarations:

```c
typedef void (*sig_t)(int);
```

`./build/debug/cc_driver.exe .sandbox/sig_alone.c --emit=ast`
crashes intermittently at exit 127 with no output. The
intermittency depends on heap layout — sometimes the run prints
`cc_driver: parsed` and exits 0, sometimes crashes silently.

`File::append_all` traces in `lower_typedef` reveal the typedef
name extracted from the parser is wrong: sometimes `(`, sometimes
`int`, never `sig_t`. The parser's declarator-name extraction is
reading the wrong token in the function-pointer declarator path
`(*sig_t)(int)`.

The pattern matches the earlier dangling-Str class
(phase0x / earlier phase24 sessions): a `Str` borrowed from the
token stream is freed before the declarator-name extraction reads
it back. When the freed buffer happens to still hold valid bytes,
the parser sees garbage that may parse OK; when the buffer was
recycled, the parser crashes downstream.

`signal.h` from the bundled C stdlib uses
`typedef void (*__sighandler_t)(int);`, so `essential.c` (which
includes `<signal.h>` transitively) hits this crash on every run.
That's the first hard barrier on the path to phase24 Phase 4
(`tml cc essential.c` self-compile).

## What Changes

1. **Reproduce reliably.** Author a small TML test that drives
   `cp_parse_translation_unit` on a synthetic CToken stream
   matching the function-pointer typedef shape. Confirm whether
   the bug is in parser (intermittent declarator-name extraction)
   or in the lexer producing CToken.text that aliases freed
   buffers.
2. **Audit `cp_parse_declarator`'s function-pointer path** —
   specifically the recursive parenthesized-declarator handling
   for `(*name)(...)`. Identify every `tok.text` / `tok.file`
   site that may carry a Str from a dropped-token borrow. Apply
   the dangling-Str fix pattern from phase0v / commit `546a2cfc`
   — route every read through `cp_peek` and add `.duplicate()` at
   every escape site.
3. **Verify** `./build/debug/cc_driver.exe .sandbox/sig_alone.c
   --emit=ast` exits 0 deterministically across 10 consecutive
   runs.
4. **Verify** `./build/debug/cc_driver.exe
   compiler/runtime/core/essential.c
   -I compiler/runtime/include/c-stdlib --emit=ast` makes
   progress. Document the next limitation as a separate task
   entry.

## Impact

- Affected specs: none (parser-level fix, no language change).
- Affected code:
  - `compiler-tml/src/cc/parser.tml` — likely
    `cp_parse_declarator`, `cp_parse_paren_declarator`, or
    similar function-pointer-handling paths.
- Breaking change: NO.
- User benefit: unblocks `tml cc` on real-world C sources that
  use function-pointer typedefs (signal handlers, callbacks,
  vtables). Last known blocker on the path to `essential.c` self-
  compile.
