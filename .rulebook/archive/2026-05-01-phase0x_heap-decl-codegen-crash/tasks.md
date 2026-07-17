## 1. Reproduce
- [x] 1.1 Land minimal CDecl-shaped fixture as `compiler/tests/compiler/heap_decl_var_repro.test.tml`
- [x] 1.2 Confirm fixture crashes on current main
- [x] 1.3 Bisect minimum shape: `{ Str, InnerEnum(Str)|B, Str, I64 }` wrapped in 2-variant outer enum + Heap + List.push (steps A–S in test history). Removing any one of: 2nd outer Str, the I64, the inner enum, the Heap wrap, or the List.push makes it pass.

## 2. Root cause
- [x] 2.1 Emit LLVM IR — generated function takes `(ptr %value)`, body does `load %struct.MinDecl, ptr %value`. Caller passes `%struct.MinDecl %t14` (struct value). ABI mismatch: callee dereferences struct value as a pointer; first 8 bytes = i32 disc + pad = 0 → `load from null` → ACCESS_VIOLATION at 0x0.
- [x] 2.2 Identified culprit in `compiler/src/codegen/llvm/decl/impl.cpp:392-395` — rule "first non-self struct/enum param → ptr" applied at definition time. Sentinel-bisected the call path: the call lands in `gen_method_static_dispatch` (method.cpp::gen_method_call → gen_method_static_dispatch), not the call_user.cpp / call_generic_struct.cpp paths previously suspected.
- [x] 2.3 The fixup at `method_static_dispatch.cpp:1006-1020` was correct in shape but the FuncInfo lookup `functions_.find("Heap__MinDecl_new")` returned `end()`: generic instantiations are queued during arg processing (line 953-956) and only registered when `gen_impl_method_instantiation` actually runs as part of a subsequent pass. At arg-processing time the FuncInfo was missing and the conversion was bypassed.

## 3. Fix
- [x] 3.1 Added a fallback in `method_static_dispatch.cpp:1006-1029`: when the FuncInfo lookup misses AND `i == 0` AND the resolved arg type starts with `%struct.` or `%enum.`, mirror the impl.cpp:392-395 rule unconditionally (alloca + store + pass ptr). Scoped to the `func_sig` branch — the other two branches at 1081+ and 1162+ already handle their own cases through `pending_generic_impls_` lookups.
- [x] 3.2 Rebuilt compiler via `scripts\build.bat`
- [x] 3.3 Repro fixture passes (`compiler/tests/compiler/heap_decl_var_repro.test.tml`)
- [x] 3.4 Bug #7/#8/#9 regression tests still pass (`nested_constructor_push.test.tml`, `large_enum_by_value_duplicate.test.tml`)
- [x] 3.5 Compiler suite: 298/317 pass; the 19 failures are all pre-existing K001 self-host tests (lexer_basic, parser_basic, hir_types, infer_differential, c_preprocessor, parse_*, test_*) and one X002 timeout (builtins_imports). Same set on git stash without my fix → confirmed not regressions.

## 4. Re-enable downstream
- [x] 4.1 Added `test_parse_top_decl_int_x` to `compiler-tml/tests/native/c_parser.test.tml`
- [x] 4.2 Added `test_parse_translation_unit_int_x` to same file. All 5 tests in the suite pass.
- [x] 4.3 `tml cc .sandbox/int_x.c --emit=ast` exits 0 with output `cc_driver: parsed .sandbox/int_x.c`
- [x] 4.4 Phase24 Phase 4 (essential.c / mem.c self-compile) is now unblocked — the codegen-bug layer that was blocking it is fixed.

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update or create documentation covering the implementation — added `docs/patches/v0.3.39.md` and CHANGELOG.md row.
- [x] 5.2 Write tests covering the new behavior — `compiler/tests/compiler/heap_decl_var_repro.test.tml` plus `test_parse_top_decl_int_x` and `test_parse_translation_unit_int_x` in `compiler-tml/tests/native/c_parser.test.tml`.
- [x] 5.3 Run tests and confirm they pass — repro test PASS, c_parser suite PASS, compiler suite 298/317 (19 pre-existing failures, no regressions).
