# Proposal: phase24l_shared-get-aliasing-deep-fix

## Why

Phase24k diagnosed but did not close the residual `essential.c × 5 = 0/5`
SIGSEGV. Root cause is now precisely understood: **`Shared.get(this) -> T`
in `lib/core/src/alloc/shared.tml:126` returns the inner `T` by bitwise
copy via `(*this.ptr).value`**. When `T` contains nested `Shared[...]`
fields (the typical case for recursive enums like `CType`), those nested
Shareds are aliased into the returned value WITHOUT bumping their
refcounts. When the returned value drops, its drop-glue decrements
those nested refcounts to 0 and frees the env's stored sub-allocations,
leaving every subsequent typedef lookup with dangling pointers.

Trigger pattern (3 lines, ~3% repro rate; 100% on essential.c):

```c
typedef void (*sig_t)(int);   // installs Shared[CType=Func(Shared[CFuncType])] in env
sig_t f(int sig, sig_t handler);  // first lookup: ret type. second lookup: param.
                                  // each lookup hands out a CType whose drop frees
                                  // the env's CFuncType backing. second lookup
                                  // operates on dangling pointer.
```

Bisection from `essential.c` reduced the trigger to `<math.h> +
<signal.h>` because `signal.h` includes both the function-pointer
typedef `__sighandler_t` AND a function decl `signal()` that uses it
twice (return type + parameter), reliably activating the aliasing.

Phase24k attempted two fixes inside `compiler-tml/src/cc/`:

1. `v.duplicate()` after `.get()`: `let v = ...get(); return v.duplicate()`.
   Result: minimal repro stayed near baseline; essential.c REGRESSED to
   30/30 crashes (vs 5/5 baseline). The added `let v` binding apparently
   causes TML codegen to emit additional drop-glue that double-decrements.

2. `expand_typedef_value(h: Shared[CType]) -> CType` helper: walks
   the variant manually, calling `inner.duplicate()` on each Shared
   field. Result: minimal repro fixed (0/30); but `essential_top50.c`
   with `-I` regressed (2/10 to 10/10) and essential.c remained 5/5.

Both fixes were reverted. Phase24k's deliverable is the diagnosis +
minimal reproducer fixture (`compiler-tml/tests/native/c_essential_repro.c`).
The fix needs to live deeper in the language semantics or codegen.

## What Changes

Choose one approach (likely (a) is least invasive):

(a) Make `Shared.get(this) -> T` recursively bump nested Shared
    refcounts when `T: Duplicate`. Concretely, change the impl to:
    `return (*this.ptr).value.duplicate()` when `T: Duplicate`,
    fall back to the bitwise copy otherwise. This requires the
    type system to support a conditional `where T: Duplicate`
    polymorphism on the impl, OR a new `get_clone(this) -> T where
    T: Duplicate` method that callers opt into.

(b) Audit and fix every `.get()` call site on `Shared[T]` where T
    has nested Shareds in `compiler-tml/src/cc/`. The audit list
    includes `types.tml::base_to_ctype` (typedef arm), every
    `apply_declarator` arm, every `lower_type` arm, every
    `Shared.get()` chain in `lower.tml`. The pattern is `let x =
    h.get(); use(x)` must become `let x = h.get().duplicate();
    use(x)` AND the `h` handle must outlive `x`. Note: phase24k
    proved that naive variants of this approach REGRESS — the
    audit must be very careful about TML drop-glue semantics and
    intermediate temporaries.

(c) Compiler-level codegen fix: detect at TML codegen whether
    `Shared[T].get()` returns a T with `Duplicate` impl, and emit
    an automatic `T::duplicate()` invocation. This is the most
    transparent option but requires invasive C++ codegen changes.

## Impact

- Affected specs: none (semantic clarification — `Shared::get` contract).
- Affected code: `lib/core/src/alloc/shared.tml` (option a/c), or
  every `.get()` call site in `compiler-tml/src/cc/` (option b),
  or `compiler/src/codegen/llvm/expr/method.cpp` plus runtime
  drop-glue emission (option c).
- Breaking change: NO at API surface. SEMANTIC change for option
  (a) — `Shared.get()` becomes a deep-clone for `T: Duplicate`,
  which adds refcount bumps that callers may not have accounted
  for. Audit needed.
- User benefit: closes phase24k essential.c residual SIGSEGV;
  closes phase0z gate; `tml cc essential.c` self-compiles
  deterministically.

## Source

- phase24k_essential-cleanup-segv (diagnosed but did not fix).
- phase24g_heap-rc-or-borrow-language-fix (introduced the
  Shared[T] migration that exposed the underlying `.get()`
  aliasing).
- phase0z_cc-driver-essential-c-residual (gate-blocker).

## Phase24k investigation summary

Trace via `File::append_all` checkpoints in `cc_driver.tml`,
`lower.tml::lower_translation_unit`, `lower_func_decl`, `lower_type`
showed crashes consistently at recursive `lower_type` calls when
processing the SECOND occurrence of a function-pointer typedef.
Pattern:

```
emit_ast_summary-start
after-parse-OK
after-c_lower
lower_tu before-i=1
  lower_func_decl name=f
  func params n=2
   param i=0 done            // int sig — no typedef, succeeds
   param i=1 after-decay
    lower_type-enter         // sig_t handler → Ptr[Func[(int)->void]]
    lt-Ptr-enter
    lower_type-enter         // recurse on Func
    lt-Func-enter
    lt-Func-after-get nparams=1
    lt-Func-param i=0 get-shared
    lt-Func-param after-shared-get
    lt-Func-param after-inner-get
    lower_type-enter         // recurse on int — CRASH (when discriminant
                             // reads dangling memory)
```

The first typedef use (return type) succeeded; on its CType drop, the
inner `Shared[CFuncType]` pointed to by the env's bucket was freed.
The second typedef use returned a CType with the now-dangling
`Shared[CFuncType]`, which crashes when `lower_type` recursively
descends into it.
