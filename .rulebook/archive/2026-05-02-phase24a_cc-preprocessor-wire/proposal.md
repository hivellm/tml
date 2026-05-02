# Proposal: phase24a_cc-preprocessor-wire

## Why

`tml cc <file>.c --emit=ast` succeeds on raw C (no preprocessor
directives) — `int x;`, struct definitions, function bodies, typedefs
all parse cleanly after the codegen fix in phase0x. But the
phase24 Phase 4 acceptance gate (`tml cc essential.c` and
`tml cc mem.c`) needs `#include`, `#define`, `#ifdef`/`#ifndef`/`#endif`
handling — both runtime files start with `#include <stdio.h>` etc.
Today `cc_driver.tml::run_pipeline` calls `pp_tokenize_source` and
hands the token stream straight to the C lexer, with no macro
expansion or conditional inclusion. The parser sees a literal `#`
token and gives up.

The macro-expansion and conditional logic already exist in
`compiler-tml/src/cc/preproc/` — `macros.tml::expand_macros`,
`conditionals.tml::cond_*`, plus `predefined.tml` and the directive
parser stubs. They just are not wired into the driver. `#include`
file lookup is the only piece that requires net-new code.

This is the actual blocker for phase24 Phase 4. Without it,
self-compiling `essential.c` is impossible.

## What Changes

1. **Wire `expand_macros` + conditionals into `cc_driver.tml`.**
   After `pp_tokenize_source`, run the directive sweep: `#define`,
   `#undef`, `#if`/`#ifdef`/`#ifndef`/`#elif`/`#else`/`#endif`,
   `#include`. Then call `expand_macros` to rewrite identifier tokens
   that match registered macros.

2. **Implement `#include` lookup.** Search order:
   - `#include "foo.h"` — relative to the directory of the file being
     preprocessed, falling back to the system include path.
   - `#include <foo.h>` — system include path only.
   - System include path candidates: a project-bundled minimal C
     stdlib (preferred — keeps the build hermetic), Zig CC's bundled
     libc headers as fallback (already used elsewhere in the
     toolchain), and `-I <path>` arguments from the CLI.

3. **Bundle minimal stdlib stubs.** `essential.c` pulls in `<math.h>`,
   `<setjmp.h>`, `<signal.h>`, `<stdint.h>`, `<stdio.h>`, `<stdlib.h>`,
   `<string.h>`, `<windows.h>`/`<unistd.h>`, `<malloc.h>`, `<fcntl.h>`,
   `<io.h>`. We do not need full implementations — just typedefs
   (`size_t`, `int32_t`, `FILE*`, `jmp_buf`), function declarations
   the runtime uses (`printf`, `fputs`, `setjmp`, `_setjmp`, `signal`,
   `getenv`), and the handful of macros TML actually invokes
   (`stdout`, `stderr`, `EXIT_FAILURE`). Land under
   `compiler/runtime/include/c-stdlib/` so the build system can point
   at it as the system include path.

4. **Run end-to-end self-compile** of `essential.c`. The output `.obj`
   should match the Clang baseline byte-for-byte for symbol set,
   sizes, and alignments. This unlocks phase24 Phase 4.1.

5. **Same for `mem.c`** (phase24 Phase 4.2).

## Impact

- Affected specs: none (driver wiring + headers, no language
  semantics change).
- Affected code:
  - `compiler-tml/src/cc/bin/cc_driver.tml` — wire preprocessor.
  - `compiler-tml/src/cc/preproc/*.tml` — small additions for
    `#include` recursion (file IO, path resolution).
  - `compiler/runtime/include/c-stdlib/` — new bundled headers.
  - `compiler-tml/tests/native/c_preproc.test.tml` — regression suite
    for `#include`, `#define`, `#ifdef`.
  - `compiler/tests/compiler/cc_essential_self_compile.test.tml` —
    end-to-end acceptance gate.
- Breaking change: NO.
- User benefit: unblocks the C runtime self-compile gate. After this,
  the project no longer depends on Clang for its C runtime —
  `tml cc essential.c mem.c` becomes the canonical build path.
