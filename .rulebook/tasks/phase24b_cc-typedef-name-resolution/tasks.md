## 1. Reproduce
- [ ] 1.1 Land minimum reproducer at `compiler-tml/tests/native/c_parser.test.tml`: tokenise `typedef int int32_t; int32_t add(int32_t a, int32_t b) { return a + b; }` and call `cp_parse_translation_unit`. Confirm the test crashes / silent-exits on current main.
- [ ] 1.2 Bisect: shrink the reproducer until the smallest failing shape is found. Probable suspects (in order of priority): `cp_parse_specifiers` not checking `parser.typedefs` for the second `int32_t`; `cp_at_decl_specifier` returning false for the typedef name; `CBaseType::Typedef(Str)` payload dangling between registration and first use.

## 2. Root cause
- [ ] 2.1 Add `File::append_all` instrumentation around the suspect functions (println loses output on SIGSEGV — direct file appends survive). Capture the exact path through `cp_parse_specifiers` → `cp_parse_declarator` for the `int32_t add(...)` line.
- [ ] 2.2 Determine whether the failure is a missing typedef lookup or a Str payload lifetime issue. The phase0x dangling-Str pattern was specific to `cp_dup_token` callers — verify whether the typedef HashMap retains valid keys across the declaration boundary.

## 3. Fix
- [ ] 3.1 Apply the parser change in `compiler-tml/src/cc/parser.tml`. Either ensure `cp_at_decl_specifier` consults the typedef map for `Ident` tokens (current code at line 213 already does this — but only for the active declaration; check whether subsequent declarations re-read it), or fix the `CBaseType::Typedef` capture site to `.duplicate()` the name into the variant payload.
- [ ] 3.2 Verify the reproducer test from item 1.1 now passes.
- [ ] 3.3 Existing c_parser, c_lexer, c_frontend, c_preproc test suites continue to pass.

## 4. Self-compile gate
- [ ] 4.1 `tml cc compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` exits 0 (or fails on a different, downstream limitation — file the next blocker if so).
- [ ] 4.2 Same for `compiler/runtime/memory/mem.c`.
- [ ] 4.3 Document remaining gaps (likely `windows.h` types and Microsoft-specific `__declspec` attributes) for the follow-up phase24c task that lands `windows.h` headers and the attribute parser.

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update or create documentation covering the implementation
- [ ] 5.2 Write tests covering the new behavior
- [ ] 5.3 Run tests and confirm they pass
