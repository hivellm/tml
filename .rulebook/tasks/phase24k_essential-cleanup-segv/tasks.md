## 1. Reproduce
- [ ] 1.1 Bisect essential.c down to a minimal reproducer (target ≤ 50 lines) that crashes ≥ 4/5 runs.
- [ ] 1.2 Save reproducer to `compiler-tml/tests/native/c_essential_repro.c` and document the trigger pattern.

## 2. Diagnose
- [ ] 2.1 `tml emit-ir` on the reproducer; locate cleanup-time IR with wrong ABI for `List[Shared[T]].duplicate` or `Maybe[Shared[T]].duplicate` calls.
- [ ] 2.2 Identify whether the bug is (a) more hand-rolled helpers needed in ast.tml, or (b) a compiler-level codegen fix for generic-Duplicate List/Maybe instantiation.

## 3. Fix
- [ ] 3.1 If (a): add `dup_*` helpers per phase24j pattern in `compiler-tml/src/cc/ast.tml` and route every reachable call site through them.
- [ ] 3.2 If (b): patch `compiler/src/codegen/llvm/expr/method.cpp` (or the generic-instantiation site) to emit correct ABI for `List[Shared[T]].duplicate` / `Maybe[Shared[T]].duplicate`. Land regression test in `compiler/tests/compiler/`.

## 4. Verify
- [ ] 4.1 `cc_driver essential.c -I compiler/runtime/include/c-stdlib --emit=ast` × 5 → 5/5 exit 0 (closes phase0z gate).
- [ ] 4.2 `cc_driver sig_alone.c --emit=ast` × 10 → 10/10 (preserves baseline).
- [ ] 4.3 phase24h regression repros (`int (*p);`, typedef variants) × 30 each → 30/30.
- [ ] 4.4 c_lexer, c_parser, c_frontend test suites pass.
- [ ] 4.5 Compiler suite ≥ 290/295 baseline preserved.

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 VERSION bump, CHANGELOG entry, `docs/patches/v0.3.52.md`.
- [ ] 5.2 Add regression test in `compiler-tml/tests/native/c_frontend.test.tml`.
- [ ] 5.3 Run all touched tests and confirm pass.
- [ ] 5.4 Archive phase0z if essential.c × 5 reaches 5/5.
