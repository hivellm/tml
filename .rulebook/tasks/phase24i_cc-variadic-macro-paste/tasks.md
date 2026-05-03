## 1. Bisect
- [ ] 1.1 Run cc_driver on each minimal repro from proposal Phase 2 (basic VA_ARGS, named + VA_ARGS, `##` with empty VA, `##` with non-empty VA, exact RT_FATAL shape). Record exit code + parse error.
- [ ] 1.2 Identify the smallest construct that triggers `expected expression`. Note the exact column/file location of the failure.

## 2. Diagnose
- [ ] 2.1 Determine whether the failure is in preprocessor token-paste handling (`##` not recognized / not folding empty VA) or in the C parser receiving malformed expanded tokens.
- [ ] 2.2 If preprocessor: locate the `##` handling site in `compiler-tml/src/cc/preproc/{tokenize,macros}.tml` (or the substitution module).
- [ ] 2.3 If C parser: locate where the expanded RT_FATAL call site enters the parser and what unexpected token it sees.

## 3. Fix
- [ ] 3.1 Implement the missing/broken token-paste handling. For `##__VA_ARGS__`: when VA is empty, drop the preceding comma; when VA is non-empty, paste normally.
- [ ] 3.2 Verify all minimal repros from item 1.1 now exit 0.

## 4. Verify
- [ ] 4.1 essential.c × 5 reaches 5/5 exit 0 (or surfaces the next-next blocker, document it).
- [ ] 4.2 sig_alone.c × 10 preserves 10/10 (phase24h baseline).
- [ ] 4.3 Compiler suite preserves baseline (290+/291 pass, only pre-existing failures).
- [ ] 4.4 c_lexer / c_parser / c_frontend / c_preproc test suites pass.

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update or create documentation covering the implementation (CHANGELOG row + docs/patches/v0.3.50.md).
- [ ] 5.2 Write regression tests covering the new behavior in the appropriate test file (`compiler-tml/tests/native/c_preproc.test.tml` or `c_parser.test.tml`).
- [ ] 5.3 Run tests and confirm they pass.
