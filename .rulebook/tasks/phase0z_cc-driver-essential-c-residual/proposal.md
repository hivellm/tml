# Proposal: phase0z_cc-driver-essential-c-residual

## Why

phase24g + phase0y unlocked rebuilding `cc_driver` and improved sig_alone.c
determinism from 2/10 → 8/10. Empirical re-measurement of essential.c
self-compile remains stuck at 0/5: 4 of 5 runs SIGSEGV (exit 139), 1 of 5
exits cleanly with `cc_driver: parse failed` + 48504 memory leaks reported.

Two distinct failure modes coexist:

1. **Parse failure on essential.c**: `cc_driver` rejects some construct in
   `compiler/runtime/core/essential.c` (1465 lines, includes `<math.h>`,
   `<setjmp.h>`, `<stdarg.h>`; uses `setjmp`/`longjmp`, function-pointer
   typedefs, varargs, struct-in-struct, `_Atomic`-shaped patterns).
2. **Use-after-free during parser cleanup**: when output is redirected
   (`> /dev/null`), the same input that exits 1 cleanly under TTY
   segfaults. The 48504 leaks point to a Heap/Shared aliasing site
   phase24g + phase0y didn't catch — likely outside the CType/CDeclarator
   surface (different enum/struct in the parser pipeline).

Closing this unblocks `tml cc essential.c` self-compile and lifts
sig_alone.c determinism from 8/10 to 10/10.

## What Changes

1. Bisect essential.c: find the smallest input that reproduces (a)
   the parse failure deterministically, and (b) the SIGSEGV under
   redirected output.
2. Trace the parser path that fails. Likely candidates:
   - varargs handling (`va_list`, `__builtin_va_*`) — not yet in cc parser
   - GCC builtins (`__builtin_*`) — not yet in cc parser
   - `_Noreturn`, `_Atomic`, `_Alignas`, `_Static_assert` — partial support
   - Function-pointer typedefs at file scope (already covered by
     phase24b/24c regression tests, but essential.c may exercise an
     untested edge case)
   - `inline` / `static inline` function definitions
   - K&R-style declarations (unlikely but possible)
3. Trace the SIGSEGV: get a backtrace via `mcp__tml__debug
   --backtrace=true` on the cc_driver pipeline, or run cc_driver
   under WinDbg / cdb. Identify the use-after-free site.
4. Fix the parser gap (item 2) AND the use-after-free (item 3).
   These may be related (same alloc) or independent.

## Impact

- Affected specs: none (parser bugfix + memory safety fix).
- Affected code: `compiler-tml/src/cc/parser.tml`, possibly
  `compiler-tml/src/cc/preproc.tml`, possibly `compiler-tml/src/cc/lower.tml`.
  Possibly C-level: `compiler/src/codegen/llvm/expr/method_*.cpp` if
  the use-after-free is a generated-code bug rather than a TML-level one.
- Breaking change: NO.
- User benefit: `tml cc essential.c` exits 0 deterministically. Closes
  the structural blocker on self-compiling the C runtime via TML's
  C frontend.
