## 1. Implementation
- [ ] 1.1 Confirm scope: does the regression reproduce with ONLY the other session's uncommitted `method_impl.cpp`/`method_static_dispatch.cpp` reverted (owner-side check)? Enumerate all `mut ref T` param body-usage sites across `lib/core` + `lib/std` that now fail (grep `: mut ref` funcs, type-check each)
- [ ] 1.2 Root-cause in the type checker / method-dispatch: where a `mut ref I64` binding used as an rvalue stopped auto-dereferencing (comparison/arithmetic) and where `x = x + 1` stopped writing through the ref — file:line evidence
- [ ] 1.3 Fix so `app_register`-style implicit-deref type-checks again (restore prior behavior); do NOT change the `.tml` call sites
- [ ] 1.4 Verify: `tml check lib/std/src/http/app/app.tml` and `lib/std/src/http/server/parse.tml` clean; re-run the ~111 previously-failing test files and confirm the count drops to the true residual
- [ ] 1.5 Commit the held `match`→`matched` fix in `parse.tml` once this clears (it is a correct committed-debt fix, currently blocked from a clean type-check by this regression)

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation — note in the relevant phase27 record + CHANGELOG
- [ ] 2.2 Write tests covering the new behavior — a `mut ref I64` implicit-deref regression fixture (param read as value + write-through)
- [ ] 2.3 Run tests and confirm they pass — determinism gate + affected suites green
