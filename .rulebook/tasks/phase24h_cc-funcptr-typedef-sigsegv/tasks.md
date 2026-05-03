## 1. Reproduce
- [ ] 1.1 Confirm `int (*p);` (`.sandbox/v9.c`) crashes 60-80% of runs against current `build/debug/cc_driver.exe` (post-phase0z).
- [ ] 1.2 Confirm `typedef void (*sig_t)(int);` (`.sandbox/sig_alone.c`) crashes at similar rate.
- [ ] 1.3 Confirm `typedef int (*hook_fn)(int); static hook_fn x;` (`.sandbox/funcptr_use_only.c`) sometimes parses with `hook_fn not a typedef` error and sometimes SIGSEGVs.
- [ ] 1.4 Land synthetic TML regression test in `compiler-tml/tests/native/c_parser.test.tml` exercising `cp_parse_translation_unit` on a function-pointer typedef CToken stream.

## 2. Diagnose
- [ ] 2.1 Trace `cp_parse_direct` recursion path on `(*p)` (or `(*name)(...)` for the typedef case).
- [ ] 2.2 Identify which struct-field move (`leaf = inner.decl` line 1246, `var d = direct.decl` line 1338, or the `Shared::new(d)` move-then-reassign loop at 1341) triggers the dangling reference.
- [ ] 2.3 Verify whether `Shared::decrement_count` line 251 (`let inner: SharedInner[T] = *this.ptr`) double-drops contained pointers via the local `inner` copy.
- [ ] 2.4 Verify whether `HashMap[Str, V].set(name, v)` callee-side drop of the `name` parameter frees the caller's Str buffer (the partial workaround in phase0z calls `name.duplicate()` but the underlying convention is still buggy).

## 3. Fix
- [ ] 3.1 Apply the surgical fix at the identified site. Pick ONE of options (a)/(b)/(c)/(d) from proposal.md after diagnosis.
- [ ] 3.2 Audit other call sites with the same pattern in `compiler-tml/src/cc/parser.tml` and apply the same fix.

## 4. Verify
- [ ] 4.1 `int (*p);` exits 0 in 10/10 runs.
- [ ] 4.2 `typedef void (*sig_t)(int);` (`sig_alone.c`) exits 0 in 10/10 runs.
- [ ] 4.3 `typedef int (*hook_fn)(int); static hook_fn x;` exits 0 in 10/10 runs.
- [ ] 4.4 `cc_driver compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` makes progress past the funcptr typedef block (line 170-173 of essential.c). Document the next blocker or confirm parse succeeds end-to-end.
- [ ] 4.5 c_parser, c_lexer, c_frontend test suites pass.
- [ ] 4.6 Compiler suite baseline preserved (>= 312/319 per phase0z baseline).

## 5. Tail (mandatory)
- [ ] 5.1 Bump VERSION, update CHANGELOG, write `docs/patches/v0.3.x.md` with root cause, fix, files changed, and verification numbers.
- [ ] 5.2 Tests written (1.4) and existing baseline verified.
- [ ] 5.3 Run all tests and confirm pass.
