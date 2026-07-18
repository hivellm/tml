## 1. Implementation
- [x] 1.1 Confirm the exact call path with a temporary diagnostic — DONE. Diagnostic at `call_user.cpp:452` proved the callee is NON-generic (`func_sig->type_params=[]`): `[DIAG-FALLBACK] fn_name=str::replace bare=replace found_mod=core::runtime::mem n_param_types=2 func_sig=1 type_params=[] mangled=@tml_N4core7runtime3mem7replaceE_R1T1T`. Root is a bare-name module-scan COLLISION (str::replace mis-resolved to core::runtime::mem::replace), NOT a monomorphization gap. Evidence in `.sandbox/diag*.log`. Diagnostic removed.
- [x] 1.2 Implement fix in `call_user.cpp` — DONE, but the EVIDENCE-BACKED fix (not the hypothesized Fix B, which never fires — `type_params` empty). Two-phase module selection (`call_user.cpp` ~213–303): prefer the module whose `FuncSig` matches the type-checked `func_sig` (arity + generic-ness), then qualifier/current-module, else legacy order. Correct module → existing registered-symbol lookup (~430) resolves `core::str::replace`; fallback no longer reached. Diagnostics removed.
- [x] 1.3 Rebuild + verify 5 core/str K001s PASS — DONE. **core/str 27/32 → 33/33** (`--no-suite`; +1 new regression fixture). All 5 (`str_coverage2/advanced/transform/method/methods`) flip fail→pass. `.sandbox/str_final.log`.
- [x] 1.4 Regression sweep + IR evidence — DONE. Clean suites zero divergence: hash 14/14, borrow 12/12, alloc 44/44, mem 12/12 (generic `mem::replace[T]`/`take[T]` still monomorphize — guards the sig-match change), json 23/23. IR (`build/debug/str_method.test.ll`): dangling `mem::replace` ref → 0; call now `@tml_N4core3str7replace7replaceE_SSS` with a matching define; verifier clean (suite links + runs 33/33).
- [x] 1.5 Determinism gate — DONE. `determinism-gate.sh 10`: initial 26/28 (a `condmove_when` 7/10 + `condmove_taken` 9/10 dip); re-confirmed FLAKY (25/25 each in isolation; crashes are environmental resource contention, not output divergence); gate re-run **28/28** at/above floor. `.sandbox/determinism2.log`.
- [x] 1.6 GATE — MET. 5 core/str K001s fail→pass (27/32 → 33/33); zero regression on clean suites; determinism 28/28 at floor; IR shows a monomorphized/correct callee define, no literal-`T` dangling.

## 2. Tail (docs + tests)
- [x] 2.1 Update or create documentation covering the implementation — DONE. `docs/patches/v0.3.71.md` (accurate root cause: collision, not monomorphization); CHANGELOG row; VERSION 0.3.70 → 0.3.71; F-006 in `04-test-framework-performance.md` updated (core/str K001 collision class RESOLVED; genuine free-function monomorphization gap distinguished as a separate latent item for phase27a). Memory `shared-stdlib-fastpath-dedup-gap.md` corrected.
- [x] 2.2 Write tests covering the new behavior — DONE. New regression fixture `lib/core/tests/str/str_replace_name_collision.test.tml` exercises qualified (`str::replace`), bare-imported (`use core::str::{replace}` → bare `replace`), and method (`s.replace`) forms of the colliding call plus a no-match path. Passes. The 5 core/str tests serve as the acceptance set.
- [x] 2.3 Run tests + confirm pass — DONE (1.3 + 1.4 + 1.5 above).

## Note: root cause differs from proposal hypothesis
The proposal hypothesized a generic-free-function monomorphization gap (Fix B: mirror the
`Type::method` block at `call_user.cpp:595–632`). Item 1.1's diagnostic disproved this for the
5 core/str failures — `func_sig->type_params` is empty (non-generic `str::replace`). The actual
root is a bare-name module-scan collision; the fix is signature-aware module selection. The
genuinely-generic free-function monomorphization gap (Fix B) is a real but distinct latent issue,
only reproducible under the disabled fast-path (F-006), and is documented for phase27a rather than
implemented speculatively without a repro.
