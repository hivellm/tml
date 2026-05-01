## 1. Reproduce
- [ ] 1.1 Land minimal CDecl-shaped fixture as `compiler/tests/compiler/heap_decl_var_repro.test.tml`
- [ ] 1.2 Confirm fixture crashes on current main
- [ ] 1.3 Bisect: which CVarDecl field shape triggers the crash (Str-only vs CBaseType vs CDeclarator vs full)

## 2. Root cause
- [ ] 2.1 Emit LLVM IR for the failing fixture (`tml emit-ir`)
- [ ] 2.2 Emit LLVM IR for the equivalent Rust `Box::new(Decl::Var(vd))`
- [ ] 2.3 Diff IR side-by-side; identify divergence (likely in heap init or drop glue)

## 3. Fix
- [ ] 3.1 Apply codegen fix in compiler C++ (specific file determined by Phase 2)
- [ ] 3.2 Rebuild compiler via `scripts\build.bat`
- [ ] 3.3 Repro fixture passes
- [ ] 3.4 Existing bug #7/#8/#9 regression tests still pass

## 4. Re-enable downstream
- [ ] 4.1 Add `test_parse_top_decl_int_x` to `compiler-tml/tests/native/c_parser.test.tml`
- [ ] 4.2 Add `test_parse_translation_unit_int_x` to same file
- [ ] 4.3 Run `tml cc .sandbox/int_x.c --emit=ast` end-to-end and confirm exit 0
- [ ] 4.4 Unblock phase24 Phase 4 (essential.c / mem.c self-compile)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with the codegen fix
- [ ] 5.2 Document the bug class in docs/patches/v0.3.x.md
- [ ] 5.3 Run full compiler + cc test suites and confirm zero regressions
