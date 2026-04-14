# Proposal: C Preprocessor in TML

## Why

The TML C frontend (phase23b) needs a preprocessor before it can parse real C code. Every C source
file in the wild uses `#include`, `#define`, and `#if` — even TML's own runtime (`essential.c`,
`mem.c`) uses them. Without a preprocessor, the C parser would receive raw source with unresolved
macros, `#include` directives, and conditional blocks, making it unable to parse any real-world C.

The preprocessor also unblocks TML's path to full toolchain independence: once TML can preprocess
and parse its own C runtime, the dependency on Clang/GCC for that step is eliminated.

## What Changes

A standalone C preprocessor module is added under `compiler-tml/src/cc/preproc/`. It takes a C
source file path plus include paths and defines, and produces a flat `List[PpToken]` — a
preprocessed token stream ready for the C parser in phase23b. No AST is built; this is purely a
token-level transformation as specified in C11 §6.10.

### Include Handler (~1,000 LOC) — `compiler-tml/src/cc/preproc/mod.tml`

Handles `#include "file.h"` (user paths first, then system) and `#include <file.h>` (system only).
Detects circular includes by tracking a stack of in-progress file paths. Recognizes the canonical
include-guard pattern (`#ifndef GUARD / #define GUARD / ... / #endif`) and `#pragma once` to skip
re-including a guarded file.

### Macro Expander (~1,500 LOC) — `compiler-tml/src/cc/preproc/macros.tml`

Handles object-like macros (`#define NAME replacement`) and function-like macros
(`#define NAME(params) replacement`), including variadic macros with `...` / `__VA_ARGS__` /
`##__VA_ARGS__` / `__VA_OPT__`. The core correctness requirement is the blue-paint algorithm
(C11 §6.10.3.4): each token carries the set of macro names that caused it to be produced; a macro
whose name is in a token's blue set is not eligible for expansion during rescanning, preventing
both direct and indirect infinite recursion.

### Conditional Evaluator (~800 LOC) — `compiler-tml/src/cc/preproc/conditionals.tml`

Evaluates `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`. The `#if` expression evaluator
handles the full C11 constant expression subset: integer and character literals, `defined(NAME)`,
all arithmetic/comparison/logical/bitwise operators, and the ternary operator. All arithmetic is
64-bit signed per C11 §6.10.1. Macros in `#if` expressions are expanded before evaluation. Skip
mode (inactive branches) suppresses include processing and macro expansion, tracking only nesting
depth.

### Token Operations (~500 LOC) — `compiler-tml/src/cc/preproc/token_ops.tml`

Implements the `#` (stringification) and `##` (token pasting) operators. Stringification converts
argument tokens to a string literal with whitespace collapsed. Token pasting concatenates two tokens
and re-lexes the result; the blue-paint sets of both source tokens are unioned into the pasted
token. Implements `#line` (updates file/line tracking for `__FILE__`/`__LINE__`), `#error`
(emits compile error), and `#warning` (GCC/Clang extension, emits diagnostic and continues).

### Predefined Macros (~300 LOC)

Standard: `__FILE__`, `__LINE__`, `__DATE__`, `__TIME__`, `__STDC__` = 1,
`__STDC_VERSION__` = 201710L, `__STDC_HOSTED__` = 1.
TML-specific: `__TML__` = 1, `__TML_MAJOR__`, `__TML_MINOR__`, `__TML_PATCHLEVEL__`.
GCC compatibility (required by many headers): `__GNUC__` = 4, `__GNUC_MINOR__` = 2.
Platform: `_WIN32` + `_WIN64` on Windows; `__linux__` + `__unix__` on Linux;
`__APPLE__` + `__MACH__` on macOS.

## Key Decisions

**Blue-paint algorithm over simpler approach**: The naive "set a boolean while expanding" approach
fails for indirect recursion and token-pasting interactions. The blue-paint algorithm is specified
in C11 §6.10.3.4 and is what GCC and Clang implement. It is more complex but correct.

**Output is a flat token list, not a stream**: The C parser (phase23b) will consume the full
preprocessed token list. Producing a flat `List[PpToken]` is simpler than a streaming protocol and
avoids coupling the preprocessor to the parser's consumption speed.

**C11 as the baseline spec**: C11 §6.10 is the authoritative reference for all preprocessor
behavior. C17 makes no normative changes to the preprocessor. C23 additions (`__VA_OPT__`) are
included as they are already supported by GCC/Clang and needed for modern system headers.

**Prior art**: GNU cpp (100K+ LOC, full C standard), ucpp (5K LOC, minimal), mcpp (15K LOC,
standards-conformant). TML's implementation follows the mcpp approach — correct, not maximal.

## Risk

**Medium-High**: Macro expansion edge cases are the main risk. The interaction between `##` token
pasting and blue-paint rescanning is a known correctness trap. The `##__VA_ARGS__` empty-argument
comma suppression (a GNU extension used by real system headers) must be implemented even though it
is technically non-standard. Test against GCC -E output on TML's own C runtime files to catch
divergences before they cause parse failures in phase23b.

## Success Criteria

1. `tml cc -E essential.c` produces a token stream identical to `gcc -E essential.c`
   (ignoring whitespace tokens and `# line` markers)
2. `tml cc -E mem.c` likewise matches GCC output
3. The 20 most common system headers preprocess without infinite loops, crashes, or incorrect
   token counts
4. `#pragma once` and canonical include guards prevent double-inclusion correctly
5. `__VA_ARGS__` with empty argument list suppresses the preceding comma via `##__VA_ARGS__`

## Dependencies

- **Requires**: phase17c (self-hosting complete, so the preprocessor can be written in TML);
  phase13b (TML lexer patterns as reference for the C token scanning implementation)
- **Blocks**: phase23b (C frontend parser needs the preprocessed token stream as input)

## Estimated Size

~4,100 LOC TML across:
- `compiler-tml/src/cc/preproc/mod.tml` (~1,000 LOC — Preprocessor type, include handling)
- `compiler-tml/src/cc/preproc/macros.tml` (~1,500 LOC — macro expansion + blue-paint)
- `compiler-tml/src/cc/preproc/conditionals.tml` (~800 LOC — #if evaluation + skip mode)
- `compiler-tml/src/cc/preproc/token_ops.tml` (~500 LOC — # and ## operators, #line, #error)
- `compiler-tml/src/cc/preproc/predefined.tml` (~300 LOC — __FILE__, __LINE__, platform macros)
