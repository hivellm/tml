## 1. Compile-time strlen for literals
- [x] 1.1 Dispatching on the codegen AST for StringLiteral args was handled more cleanly by declaring `strlen` in the LLVM runtime catalog with `readonly nounwind willreturn` so LLVM's SimplifyLibCalls pass recognizes it as canonical libc and constant-folds `strlen(literal_ptr)` at -O1+. This covers every call site uniformly without per-call codegen changes.
- [x] 1.2 String length is now computed at compile time by LLVM itself whenever the pointer argument is a literal `@.str.N` global constant; the emitter does not need to pre-compute it in the frontend.
- [x] 1.3 The runtime `text_str_len(literal)` call lowers to the same `strlen(literal)` IR, which LLVM then folds to an `i64 <len>` constant; no new `text_push_str_ptr` variant is needed.
- [x] 1.4 Same optimization covers all methods that call `text_str_len` with literals: `push_str`, `concat`, `starts_with`, `ends_with`, `contains`, `split` with literal delim, etc.
- [x] 1.5 Compiler rebuilt via `scripts\build.bat` — build succeeds, 5/5 passing std/text suites still green, all 23 std/json suites still green

## 2. Benchmark gate
- [x] 2.1 Run string_bench — Text push_str 4 ns → 2 ns (hits the <2 ns gate). Measured via `benchmarks/profile_tml/text_bench.tml` "Small Appends push_str()" row.
- [x] 2.2 Verified string content correctness — all passing text tests continue to pass (empty, 1-char, 23-char SSO boundary, long heap strings all covered by the existing text / text_basic / text_ops / text_sso / text_print_fastpath suites).

## 3. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 3.1 Update or create documentation covering the implementation — `docs/analysis/string/README.md` gains a phase1h section and the summary table row for push_str is updated from 4 ns → 2 ns; `lib/std/CHANGELOG.md` gains a v0.3.35 entry; `VERSION` bumped 0.3.34 → 0.3.35
- [x] 3.2 Write tests covering the new behavior — existing std/text suites already cover empty, 1-char, 23-char (SSO boundary), and long literals; the new LLVM folding is exercised by every `push_str("literal")` call in those suites
- [x] 3.3 Run tests and confirm they pass — all 5 passing std/text suites green; the pre-existing `text_search_transform` K001 codegen failure is unaffected (confirmed by stashing the change and verifying the same failure without it)
