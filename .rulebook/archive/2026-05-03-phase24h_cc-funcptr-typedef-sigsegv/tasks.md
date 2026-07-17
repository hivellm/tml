## 1. Reproduce
- [x] 1.1 Confirmed `int (*p);` (`.sandbox/v9.c`) crashed 14/30 runs (~47%) against pre-fix `build/debug/cc_driver.exe`.
- [x] 1.2 Confirmed `typedef void (*sig_t)(int);` (`.sandbox/sig_alone.c`) crashed 16/30 runs (~53%).
- [x] 1.3 Confirmed `typedef int (*hook_fn)(int); static hook_fn x;` (`.sandbox/funcptr_use_only.c`) crashed and intermittently parse-errored ("hook_fn not a typedef") pre-fix.
- [x] 1.4 Landed regression tests in `compiler-tml/tests/native/c_parser.test.tml`: `test_paren_pointer_declarator_no_crash` (exercises `int (*p);`) and `test_funcptr_typedef_no_crash` (exercises `typedef void (*sig_t)(int);` + asserts `parser.typedefs.has("sig_t")`).

## 2. Diagnose
- [x] 2.1 Traced `cp_parse_direct` recursion path on `(*p)`: parser routes through the `LParen → is_inner=1` branch, recurses into `cp_parse_declarator_inner` with `p+1`, returns Ok with `Pointer(Shared[CDeclarator]::new(Ident("p")), ...)`, then partial-field-moves that variant out via `leaf = inner.decl`.
- [x] 2.2 Identified `leaf = inner.decl` (parser.tml:1246) and `var d = direct.decl` (parser.tml:1338) as the root-cause sites. Both move a `CDeclarator` enum value (containing `Shared[CDeclarator]` payloads) out of the source struct; the source struct's auto-generated drop then runs on the moved-out variant, double-decrementing the contained `Shared`.
- [x] 2.3 Investigated `Shared::decrement_count` (line 251 `let inner = *this.ptr`) — initial fix attempt to use direct field access only marginally changed crash rate (16/30 → 13/30); ruled out as the dominant cause. Reverted that change.
- [x] 2.4 `parser.typedefs.set(name, ...)` — already mitigated in phase0z via `name.duplicate()`. Not the dominant cause; the parenthesized-declarator path crashes WITHOUT touching the typedef table (e.g. `int (*p);`).

## 3. Fix
- [x] 3.1 Applied option (a) (surgical): added explicit `.duplicate()` at the two partial-field-move sites in `compiler-tml/src/cc/parser.tml`. Required `impl Duplicate for CDeclarator` to exist; added it manually in `compiler-tml/src/cc/ast.tml` (since `@auto(duplicate)` does not yet support enums). Also added `@auto(duplicate)` to `CFuncDeclPart`. Each variant arm forwards to `Shared::duplicate()` (refcount bump), `Maybe::duplicate()`, and `Heap::duplicate()` for non-Shared payloads.
- [x] 3.2 Audited other `.decl` field reads in `parser.tml` (lines 1412, 1416, 1473, 1695, 1700, 1702, 1703, 1711, 1730, 1734, 1736, 1737, 1739, 1759, 1889, 1892). These are all multi-read patterns that the pre-existing TML codegen handles via auto-duplicate (now backed by the new `Duplicate` impl). The two move-into-let sites (1246, 1338) were the only ones requiring explicit `.duplicate()` — they assign into a fresh local that survives past the source struct's drop scope.

## 4. Verify
- [x] 4.1 `int (*p);` exits 0 in 30/30 runs (was 16/30).
- [x] 4.2 `typedef void (*sig_t)(int);` exits 0 in 30/30 runs (was 14/30).
- [x] 4.3 `typedef int (*hook_fn)(int); static hook_fn x;` exits 0 in 30/30 runs.
- [x] 4.4 `cc_driver compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` advances past the funcptr typedef block (lines 170-173 of essential.c). Next blocker: `parse failed at compiler/runtime/core/essential.c:189:43: expected ';' after declaration` — a real parse error, not a crash. Documented for follow-up.
- [x] 4.5 c_parser, c_lexer, c_frontend test suites pass after rebuild.
- [x] 4.6 Compiler suite baseline preserved: 314/322 (same 8 pre-existing failures as v0.3.47 baseline: c_preprocessor K001, hir_types K001, infer_differential K001, mir_optimization_passes K001, closure_codegen X003, let_patterns X002, method_dispatch X003, regalloc_basic X003).

## 5. Tail (mandatory)
- [x] 5.1 Update or create documentation covering the implementation — VERSION bumped 0.3.47 → 0.3.48; CHANGELOG.md row added; `docs/patches/v0.3.48.md` written with root cause, fix, files changed, verification numbers (per-repro 30/30 and essential.c progress), and next blockers.
- [x] 5.2 Write tests covering the new behavior — `test_paren_pointer_declarator_no_crash` and `test_funcptr_typedef_no_crash` added in `compiler-tml/tests/native/c_parser.test.tml` (item 1.4); existing baseline verified (items 4.5/4.6).
- [x] 5.3 Run tests and confirm pass — c_parser passes after recompile; full compiler suite matches baseline (314/322 = same 8 pre-existing failures).
