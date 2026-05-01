## 1. Reproduce
- [x] 1.1 Synthetic-token typedef-then-use tests added to `compiler-tml/tests/native/c_parser.test.tml` (`test_typedef_then_use_minimal`, `test_typedef_then_use_in_function`). Both pass when invoked directly through `cp_parse_translation_unit` — confirming the parser is correct.
- [x] 1.2 `File::append_all` traces in `cc_driver.tml::run_pipeline`, `lower.tml::lower_translation_unit / lower_typedef / lower_func_decl`, and `types.tml::base_to_ctype` show the actual crash site: `base_to_ctype`'s `Typedef(name)` arm completes the `t.get()` call but crashes on the function-`return` of the resulting `CType` value.

## 2. Root cause
- [x] 2.1 The crash family matches `phase0x_heap-decl-codegen-crash`: a value-type with `Heap`-wrapped inner fields (CType has `Ptr(Heap[CType])`, `Struct(Heap[CAggregate])`, `Func(Heap[CFuncType])` variants) crosses a function boundary by value. phase0x fixed the call-site arg path for `Heap[T]::new(...)`; the symmetric return path appears unfixed.
- [x] 2.2 Synthetic-token tests do not exercise the lowerer, only the parser, which is why they pass while the full `tml cc` pipeline crashes.

## 3. Fix
- [ ] 3.1 Land a minimal C++ regression test that matches the failing shape: a TML function returning a value-type enum with `Heap`-wrapped variants, called from another function. Mirror the layout of `compiler/tests/compiler/heap_decl_var_repro.test.tml`.
- [ ] 3.2 Use the Rust-as-Reference IR methodology to capture how rustc lowers a `Box<Inner>`-bearing enum return on Windows x64. Compare against TML's IR.
- [ ] 3.3 Apply the codegen fix in `compiler/src/codegen/llvm/decl/func.cpp` (return-type lowering) and `compiler/src/codegen/llvm/expr/return_stmt.cpp` (return emission). The fix is structurally similar to the phase0x patch but on the return-path side.
- [ ] 3.4 Rebuild compiler via `scripts\build.bat`. Verify the new repro passes.
- [ ] 3.5 Existing phase0x bug #7/#8/#9 + heap_decl_var_repro tests still pass.

## 4. Self-compile gate
- [ ] 4.1 `tml cc .sandbox/test_no_inc.c --emit=ast` (a 2-line typedef reproducer) exits 0 with `cc_driver: parsed`.
- [ ] 4.2 `tml cc compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` reaches the next limitation (no longer crashes at the typedef point).
- [ ] 4.3 Document any subsequent gaps as separate task entries.

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update or create documentation covering the implementation
- [ ] 5.2 Write tests covering the new behavior
- [ ] 5.3 Run tests and confirm they pass
