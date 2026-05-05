# Proposal: phase24m_essential-c-residual-segv

## Why

phase24l shipped the typedef-arm `Shared.get_clone` fix and added the
`get_clone(this) -> T where T: Duplicate` method to
`lib/core/src/alloc/shared.tml`. That fix:

- closes the `c_essential_repro.c` minimal trigger (28-29/30 exit 0,
  matching the pre-existing baseline noise),
- preserves all phase24h regression baselines (sig_alone 10/10,
  `int (*p);` 30/30, `typedef void (*sig_t)(int);` 30/30),
- preserves the compiler suite at the 290/295 baseline,

but **does not close `essential.c × 5 = 0/5`**. With the typedef-arm
fix in place, `cc_driver essential.c -I … --emit=ast` still produces
a mix of exit-127 (uniform when the fix is wide enough) and exit-139
SIGSEGV (when narrower). Empty stderr on the exit-127 cases suggests
either a stack-overflow class crash or an unhandled access violation
that bypasses the runtime's panic handler.

Two fix variants tried in phase24l:

1. **Broader option (b)**: extend `.get_clone()` migration to ~40
   call sites across `compiler-tml/src/cc/types.tml`,
   `compiler-tml/src/cc/lower.tml`, and `compiler-tml/src/cc/parser.tml`
   (every `.get()` on `Shared[CType]` / `Shared[CDeclarator]` /
   `Shared[CExpr]` / `Shared[CStmt]` / `Shared[CBlockItem]` /
   `Shared[CInit]`). Result: minimal repro REGRESSED to 25/30 and
   essential.c unchanged. Each additional `.get_clone()` site at this
   level introduces refcount-bump leaks the consumer never decrements,
   mirroring the phase24k regression class. Reverted.

2. **Pure option (a)**: change `pub func get(this) -> T` to
   `pub func get(this) -> T where T: Duplicate { return (*this.ptr).value.duplicate() }`.
   Result: REGRESSED `lib/core` test suite — `cache_aligned_box`,
   `cache`, `cache_soavec_set`, `future_fuse` (and likely more) start
   failing K001 codegen errors with different signatures, indicating
   the language-level semantic change breaks monomorphizations of
   generic Shared users beyond cc/. Reverted.

Both fail-twice paths exhausted at the typedef-arm scope. The
residual essential.c gate requires either (a) deeper refcount
analysis of the language semantic of `Shared.get` for nested-handle
T (true option (a) with audit), or (c) a compiler-codegen-level fix
that emits the deep-clone automatically based on type information.
This task tracks that follow-up.

## What Changes

Three candidate paths, in order of structural preference:

(a) **Refined option (a)** — make `Shared.get` deep-clone for
    `T: Duplicate` AND audit/fix every callsite that breaks (the 4
    `lib/core` tests that regressed in phase24l attempt 3 are the
    starting points for the audit).

(b) **Compiler-codegen fix** — at TML codegen, when emitting
    `Shared[T]::get` and `T` has a `Duplicate` impl with nested
    `Shared` / `Heap` fields, automatically emit refcount bumps on
    those nested fields. This is option (c) from phase24l proposal.
    The fix lives in `compiler/src/codegen/llvm/expr/method.cpp` and
    runtime drop-glue emission. Wide blast radius but most
    transparent semantically.

(c) **HashMap.get specialization for Shared values** — make the
    HashMap-level lookup bump the refcount of `Shared[T]` values on
    return, so `HashMap.get` returns a proper owning handle (matching
    the contract documented in `CTypeEnv` comments). This narrows the
    fix to the actual aliasing site without changing `Shared.get`
    semantics. Limited blast radius (only HashMap users), but
    requires generic-trait-aware codegen.

## Impact

- Affected specs: none (semantic clarification — `Shared::get` /
  `HashMap::get` contract under nested Shared payloads).
- Affected code:
  - Option (a): `lib/core/src/alloc/shared.tml` + every regressed
    consumer (cache, future_fuse, etc.).
  - Option (b): `compiler/src/codegen/llvm/expr/method.cpp` plus
    runtime drop-glue emission.
  - Option (c): `lib/std/src/collections/hashmap.tml` plus codegen
    support for trait-aware specialization.
- Breaking change: SEMANTIC for option (a) — `Shared.get` deep-clones
  when T: Duplicate. Audit needed. NO at API surface for (b)/(c).
- User benefit: closes the residual `essential.c × 5 = 0/5` SIGSEGV;
  `tml cc essential.c` self-compiles deterministically; phase0z gate
  closes; phase24k archives.

## Source

- phase24l_shared-get-aliasing-deep-fix (typedef-arm fix shipped;
  diagnosed essential.c residual still active).
- phase24k_essential-cleanup-segv (originally diagnosed the
  `Shared.get` aliasing class).
- phase24g_heap-rc-or-borrow-language-fix (introduced the
  `Shared[T]` migration whose underlying `.get()` aliasing this
  task closes).
- phase0z_cc-driver-essential-c-residual (gate-blocker).

## Constraints

- Cannot regress the 290/295 compiler-suite baseline.
- Cannot regress `lib/core` test suite (especially the cache /
  future_fuse families that broke under phase24l attempt 3).
- Cannot regress `c_essential_repro.c` minimal trigger (current
  ≥ 28/30 with phase24l shipped).
- Cannot regress phase24h sig_alone / int*p / sig_t typedef
  reproducers (30/30 each).
