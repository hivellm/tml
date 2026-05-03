# Proposal: phase24h_cc-funcptr-typedef-sigsegv

## Why

phase0z (`__declspec`/`__attribute__` skip) advanced essential.c past the
top-of-file MSVC export macro. The next blocker is `cc_driver` non-
deterministically SIGSEGVing on **any** function-pointer typedef or
parenthesized inner-declarator construct. Minimal repros (each crashes
non-deterministically — typically 60–80% of runs):

```c
// .sandbox/v9.c — variable with parenthesized pointer declarator
int (*p);

// .sandbox/just_funcptr_typedef.c — function-pointer typedef
typedef void (*tml_panic_hook_fn)(const char*);

// .sandbox/sig_alone.c — kernel of essential.c blocker
typedef void (*sig_t)(int);
```

The crash happens AFTER successful parse — in the cleanup / leak-
reporter path during process shutdown. Same ACCESS_VIOLATION shape as
phase24c/24d/24e/24f/24g (the Heap-borrow-drop bug class).

phase24f and phase24g already migrated `Heap[CType]`, `Heap[CDecl]`,
`Heap[CDeclarator]` to `Shared[T]`, eliminating the bulk of the bug
class. The remaining crash is in the partial-field-move pattern that
phase24g intentionally left in place:

- `cp_parse_direct` line 1246: `leaf = inner.decl` (struct field move
  out of `CParsedDeclarator`).
- `cp_parse_declarator_inner` line 1338: `var d: CDeclarator = direct.decl`
  (same pattern, returns from outer call).
- The Pointer-wrap loop line 1341: `d = CDeclarator::Pointer(Shared::new(d), ...)`
  moves `d` into `Shared::new`, then re-assigns `d` to the new
  Pointer variant.

Because the parenthesized-inner-declarator path goes through these
moves twice (once in the inner recursion, once in the outer), the
crash window is larger and the bug surfaces as a non-deterministic
SIGSEGV. Non-paren'd `int *p;` does the moves only once and survives.

The pattern observed in `funcptr_use_only.c`:

```c
typedef int (*hook_fn)(int);
static hook_fn x;        // sometimes "hook_fn not a typedef" parse error,
                         // sometimes SIGSEGV during parse, sometimes OK
```

implies the typedef name registered in `parser.typedefs: HashMap[Str, I64]`
gets a freed key buffer (the Str shares storage with the caller's
`name` local, which drops; the HashMap retains a dangling pointer).

essential.c hits this on line 170:
`typedef void (*tml_panic_hook_fn)(const char*);` and the use on
line 173 (`static tml_panic_hook_fn tml_panic_hook = NULL;`) sees
either a corrupted typedef table or crashes outright.

Closing this unblocks `tml cc essential.c` end-to-end.

## What Changes

1. Reproduce reliably on the minimal `int (*p);` and the typedef
   variants. Confirm 5/10 to 8/10 SIGSEGV rate per fresh build (current
   baseline post-phase0z).
2. Diagnose the exact partial-move drop site:
   - Trace `cp_parse_direct` and `cp_parse_declarator_inner` to verify
     which `CDeclarator` value ends up double-dropped or double-freed.
   - Likely candidates: `inner.decl` field-move (line 1246), the
     `Shared[CDeclarator]::new(d)` move-then-reassign loop (line 1341),
     or the `CParsedDeclarator` drop after partial-field move-out.
3. Apply the surgical fix. Most likely options:
   - (a) Mark `CParsedDeclarator` Drop as field-aware so partial moves
     do not double-drop. Requires TML codegen support for partial-move
     tracking on struct field moves.
   - (b) Replace partial-field moves with explicit local rebinds:
     `let d_local = inner.decl; <do work>; (do not drop inner explicitly)`.
     May still hit the same codegen issue.
   - (c) Migrate the remaining `CParsedDeclarator` and `CDeclarator`
     value-passing chains to `Shared[T]` (continuation of phase24g
     scope). Highest blast radius but most structural.
   - (d) TML compiler-side: fix codegen for partial-field moves so the
     drop chain on the source struct skips moved-out fields. Smallest
     surface, biggest leverage.
4. Bonus root cause fixes for the same class:
   - `parser.typedefs.set(name, ...)` then `name: name` on the next
     line. The partial fix in phase0z added `.duplicate()` on the set
     call but the underlying TML codegen for `set(this, key: K, ...)`
     should not double-drop the caller's `name` either way.
5. Verify `cc_driver compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` >= 9/10 exit 0.
   Verify `sig_alone.c` >= 9/10 exit 0.
   Verify compiler suite preserves baseline.

## Impact

- Affected specs: none (memory safety bug fix).
- Affected code: `compiler-tml/src/cc/parser.tml` (declarator
  paths); possibly `lib/core/src/alloc/shared.tml` if the bug is in
  `decrement_count` `let inner = *this.ptr` copy semantics; possibly
  the C++ TML compiler codegen if the issue is partial-field-move drop
  tracking.
- Breaking change: NO.
- User benefit: `tml cc essential.c` exits 0 deterministically; closes
  the structural blocker on the C runtime self-compile path.

## Source

- phase0z (`__declspec`/`__attribute__` parser fix) discovered this
  blocker as the next downstream issue.
- phase24c/24d/24e/24f/24g: same Heap-borrow-drop bug class lineage.
- Minimal repros under `.sandbox/v9.c`, `.sandbox/funcptr_only.c`,
  `.sandbox/sig_alone.c`, `.sandbox/just_funcptr_typedef.c`,
  `.sandbox/funcptr_use_only.c` (all crash non-deterministically with
  current `cc_driver.exe` post-phase0z).
