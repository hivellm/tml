# Proposal: phase24k_essential-cleanup-segv

## Why

phase24g/h/i/j closed all known Heap-borrow-drop and small-enum dispatch
bug classes for the C-frontend AST. After phase24j shipped (CBlockItem /
CStmt / CExpr / CInit migrated to `Shared[T]`), the determinism profile is:

| Input | Pre-phase24j | Post-phase24j |
|-------|--------------|---------------|
| `sig_alone.c` × 10 | 10/10 | **10/10** |
| `int (*p);` × 30 | 30/30 | **30/30** |
| `typedef void (*sig_t)(int);` × 30 | 30/30 | **30/30** |
| Deep nested if/for/while function × 30 | n/a | **30/30** |
| 200-decl C file × 5 | n/a | **5/5** |
| `essential.c` × 5 | 0/5 SIGSEGV | **0/5 SIGSEGV** |

`essential.c` (1465 lines, `setjmp`/`longjmp` macros, complex preprocessor
expansion via `compiler/runtime/diagnostics/backtrace.h`, deeply nested
function bodies, large globals, function-pointer typedefs, file-scope
arrays of struct, enum types) is the only remaining input that
deterministically SIGSEGVs.

Bisection finding: even truncating essential.c to its top 250 lines
crashes intermittently. The trigger is content-shape, not size or
specific feature — multi-feature interactions in this file produce a
cleanup-time fault that smaller multi-feature programs (which we DO
test 30/30) don't trigger.

## What Changes

1. Reproduce with a minimum sub-set of essential.c (likely the
   `tml_crash_ctx_*` block + one function definition with `setjmp` macro
   expansion suffices).
2. `tml emit-ir` on the reproducer; identify the cleanup function whose
   IR has wrong ABI for `List[Shared[T]].duplicate` or
   `Maybe[Shared[T]].duplicate`.
3. Either (a) add more hand-rolled `dup_*` helpers to ast.tml in the
   pattern phase24j established, or (b) propose a compiler-level fix to
   the auto-derive of generic-Duplicate List/Maybe instantiation.

## Impact

- Affected specs: none
- Affected code: `compiler-tml/src/cc/ast.tml`, possibly
  `compiler-tml/src/cc/parser.tml`, possibly `compiler/src/codegen/llvm/`
- Breaking change: NO
- User benefit: closes phase0z gate. `tml cc essential.c` self-compiles.

## Source

- phase24j (this gate-blocker discovered at end of partial migration).
- phase0z (gated on essential.c × 5 = 5/5).
