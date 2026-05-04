## 1. Bisect
- [x] 1.1 Run cc_driver on each minimal repro from proposal Phase 2 (basic VA_ARGS, named + VA_ARGS, `##` with empty VA, `##` with non-empty VA, exact RT_FATAL shape). Record exit code + parse error.
- [x] 1.2 Identify the smallest construct that triggers `expected expression`. Note the exact column/file location of the failure.

## 2. Diagnose
- [x] 2.1 Determine whether the failure is in preprocessor token-paste handling (`##` not recognized / not folding empty VA) or in the C parser receiving malformed expanded tokens. Bisect found the issue is more fundamental: function-like macros are not implemented at all — `pp_handle_directive::define` only routed to `pp_define_object`, treating `#define M(x) f(x)` as object-like with body `(x) f(x)`.
- [x] 2.2 If preprocessor: locate the `##` handling site in `compiler-tml/src/cc/preproc/{tokenize,macros}.tml` (or the substitution module). Located: `tokenize.tml::scan_punct` did not recognise `##`; `macros.tml::expand_macros` had a `FunctionLike` arm that was a stub.
- [x] 2.3 If C parser: locate where the expanded RT_FATAL call site enters the parser and what unexpected token it sees. Not applicable — the parser is downstream of the unfixed preprocessor.

## 3. Fix
- [x] 3.1 Implement the missing/broken token-paste handling. For `##__VA_ARGS__`: when VA is empty, drop the preceding comma; when VA is non-empty, paste normally. Also implemented function-like macro definition parsing, argument collection (with paren-depth tracking), substitution, plain `##` token pasting, and `##` punctuator recognition in the tokenizer. Used a flat `List[PpToken]` + offset table for `CollectedArgs` to dodge the Heap-borrow-drop pattern that breaks nested `List[List[PpToken]]`.
- [x] 3.2 Verify all minimal repros from item 1.1 now exit 0. All 7 (`r0`/`r0a`/`r1`–`r5`) pass deterministically across 10 consecutive runs.

## 4. Verify
- [x] 4.1 essential.c × 5 reaches 5/5 exit 0 (or surfaces the next-next blocker, document it). essential.c × 5 still SIGSEGVs at exit 139 — pre-existing baseline `Maybe[Heap[CBlockItem]]` cleanup-time crash gated on phase24g residual (refcounted `Heap[T]`). My fix doesn't worsen it.
- [x] 4.2 sig_alone.c × 10 preserves 10/10 (phase24h baseline). 10/10 confirmed.
- [x] 4.3 Compiler suite preserves baseline (290+/291 pass, only pre-existing failures). 313/322 (was 299/321 baseline; 14 net pass — c_lexer/c_parser/c_frontend now compile cleanly). 4 unique pre-existing K001 / Unknown-method failures remain (c_preprocessor, hir_types, infer_differential, mir_optimization_passes).
- [x] 4.4 c_lexer / c_parser / c_frontend / c_preproc test suites pass. All 4 green.

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update or create documentation covering the implementation (CHANGELOG row + docs/patches/v0.3.50.md).
- [x] 5.2 Write regression tests covering the new behavior in the appropriate test file (`compiler-tml/tests/native/c_preproc.test.tml` or `c_parser.test.tml`). Added 8 tests in c_preproc.test.tml (all 7 minimal repros + nested-parens-in-arg).
- [x] 5.3 Run tests and confirm they pass. c_preproc suite green; full suite passes 313/322.
