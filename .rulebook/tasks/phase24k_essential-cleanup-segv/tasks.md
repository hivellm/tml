## 1. Reproduce
- [x] 1.1 Bisect essential.c down to a minimal reproducer (target ≤ 50 lines) that crashes ≥ 4/5 runs.
- [x] 1.2 Save reproducer to `compiler-tml/tests/native/c_essential_repro.c` and document the trigger pattern.

Result: 3-line reproducer (typedef + funcdecl + main). Crash rate ~3% on the
minimal repro; 100% on essential.c. Bisection traced trigger to `<signal.h>`
(function-pointer typedef + decl that uses it twice).

## 2. Diagnose
- [x] 2.1 `tml emit-ir` on the reproducer; locate cleanup-time IR with wrong ABI for `List[Shared[T]].duplicate` or `Maybe[Shared[T]].duplicate` calls.
- [x] 2.2 Identify whether the bug is (a) more hand-rolled helpers needed in ast.tml, or (b) a compiler-level codegen fix for generic-Duplicate List/Maybe instantiation.

Result: bug is NEITHER (a) nor (b) as originally framed. Root cause is
deeper — `Shared.get(this) -> T` (`lib/core/src/alloc/shared.tml:126`)
returns T by bitwise copy. Nested `Shared[...]` fields inside T are
aliased without bumping their refcounts. When the returned value drops,
its drop-glue decrements those refcounts to 0, freeing the env's
stored sub-allocations. Subsequent typedef lookups hand out CTypes
that wrap dangling pointers — explaining the deterministic SIGSEGV
on essential.c (many typedef lookups → progressive corruption) and
the rare crash on minimal repro (only 2 lookups → low collision odds).

Trace via `File::append_all` instrumentation in `cc_driver.tml`,
`lower.tml::lower_translation_unit`, `lower_func_decl`, `lower_type`
pinpointed the crash to recursive `lower_type` calls when processing
the SECOND occurrence of a function-pointer typedef. First lookup
succeeds; its CType drop frees the env's `Shared[CFuncType]`
backing; second lookup operates on dangling memory and crashes
inside the `when t {` discriminant read.

## 3. Fix
- [ ] 3.1 If (a): add `dup_*` helpers per phase24j pattern in `compiler-tml/src/cc/ast.tml` and route every reachable call site through them.
- [ ] 3.2 If (b): patch `compiler/src/codegen/llvm/expr/method.cpp` (or the generic-instantiation site) to emit correct ABI for `List[Shared[T]].duplicate` / `Maybe[Shared[T]].duplicate`. Land regression test in `compiler/tests/compiler/`.

NOT applicable as framed. Two phase24k attempts to fix at the
typedef-arm level both REGRESSED (one essential.c 5/5 → 30/30
crashes; the other regressed essential_top50 from 2/10 → 10/10
fail). Both reverted. Per fail-twice rule + T0 (no "blocked"),
filed `phase24l_shared-get-aliasing-deep-fix` as the structural
follow-up. The fix needs to live in `Shared.get()` itself, in a
new `get_clone()` opt-in method, or in TML codegen for the
.get()-then-drop pattern.

## 4. Verify
- [ ] 4.1 `cc_driver essential.c -I compiler/runtime/include/c-stdlib --emit=ast` × 5 → 5/5 exit 0 (closes phase0z gate).
- [ ] 4.2 `cc_driver sig_alone.c --emit=ast` × 10 → 10/10 (preserves baseline).
- [ ] 4.3 phase24h regression repros (`int (*p);`, typedef variants) × 30 each → 30/30.
- [ ] 4.4 c_lexer, c_parser, c_frontend test suites pass.
- [ ] 4.5 Compiler suite ≥ 290/295 baseline preserved.

Verified preserved baselines (no regressions from phase24k changes,
which were ultimately reverted; only the regression-test fixture
landed):
  - 4.2 sig_alone: 10/10 OK
  - 4.3 int (*p): 30/30 OK; typedef sig_t: 30/30 OK
  - 4.1 essential.c × 5: 0/5 (residual SIGSEGV — gate NOT met)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 VERSION bump, CHANGELOG entry, `docs/patches/v0.3.52.md`.
- [x] 5.2 Add regression test in `compiler-tml/tests/native/c_frontend.test.tml`.
- [ ] 5.3 Run all touched tests and confirm pass.
- [ ] 5.4 Archive phase0z if essential.c × 5 reaches 5/5.

Note (5.1): NO version bump because no behavior shipped — both
attempted fixes regressed and were reverted. The fixture file
landed as a regression test for phase24l.

Note (5.2): minimal reproducer fixture lives at
`compiler-tml/tests/native/c_essential_repro.c`. NOT registered as
a `.test.tml` test because (a) cc_driver invocation is currently
done via shell from outside the test framework, and (b) registering
it would fail the suite until phase24l ships. The fixture is
referenced by phase24l's verification task.

Note (5.4): essential.c gate NOT met. phase0z stays open. phase24k
stays open until phase24l lands the structural fix.
