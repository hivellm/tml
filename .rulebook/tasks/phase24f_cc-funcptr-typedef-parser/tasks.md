## 1. Reproduce
- [ ] 1.1 Author a TML regression test in `compiler-tml/tests/native/c_parser.test.tml` that drives `cp_parse_translation_unit` on a synthetic CToken stream for `typedef void (*sig_t)(int);`. Confirm the typedef name extracted from the parsed CTypedefDef is `sig_t` deterministically across runs.
- [ ] 1.2 Confirm `./build/debug/cc_driver.exe .sandbox/sig_alone.c --emit=ast` is intermittent: instrument with `File::append_all` in `lower_typedef` showing the extracted name. Run 10× and capture the distribution of names (expected `sig_t`, observed mix of `(` / `int` / `sig_t`).

## 2. Bisect
- [ ] 2.1 Add `File::append_all` traces inside `cp_parse_declarator`'s parenthesized-declarator path. Identify the precise call where `tok.text` carries the wrong content.
- [ ] 2.2 Audit every `parser.tokens.get(p)` / `cp_peek` / `tok.text` site in the function-pointer declarator path. Apply the phase0v dangling-Str fix pattern — route every read through `cp_peek` and add `.duplicate()` at every Str escape site.

## 3. Fix
- [ ] 3.1 Apply the dangling-Str fix in `compiler-tml/src/cc/parser.tml`'s function-pointer declarator path.
- [ ] 3.2 Type-check clean. Rebuild cc_driver.exe.
- [ ] 3.3 Verify `./build/debug/cc_driver.exe .sandbox/sig_alone.c --emit=ast` exits 0 deterministically across 10 consecutive runs.
- [ ] 3.4 Verify the new regression from 1.1 passes.
- [ ] 3.5 Verify phase24e regression (5/5) still passes.

## 4. Self-compile gate
- [ ] 4.1 `./build/debug/cc_driver.exe compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` makes progress (no longer crashes at the function-pointer typedef point).
- [ ] 4.2 Document any subsequent gaps as separate task entries.

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update or create documentation covering the implementation
- [ ] 5.2 Write tests covering the new behavior
- [ ] 5.3 Run tests and confirm they pass
