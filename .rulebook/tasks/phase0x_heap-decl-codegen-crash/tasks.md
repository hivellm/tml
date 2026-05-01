## 1. Reproduce
- [x] 1.1 Land minimal CDecl-shaped fixture as `compiler/tests/compiler/heap_decl_var_repro.test.tml`
- [x] 1.2 Confirm fixture crashes on current main
- [x] 1.3 Bisect minimum shape that crashes: `{ Str, InnerEnum(Str)|B, Str, I64 }` wrapped in 2-variant outer enum + Heap + List.push (steps A–S in test history). Removing any one of: 2nd outer Str, the I64, the inner enum, the Heap wrap, or the List.push makes it pass.

## 2. Root cause
- [x] 2.1 Emit LLVM IR — generated function takes `(ptr %value)`, body does `load %struct.MinDecl, ptr %value`. Caller passes `%struct.MinDecl %t14` (struct value). ABI mismatch: callee dereferences struct value as a pointer; first 8 bytes = i32 disc + pad = 0 → `load from null` → ACCESS_VIOLATION at 0x0.
- [x] 2.2 Identified culprit in `compiler/src/codegen/llvm/decl/impl.cpp:392-395` — rule "first non-self struct/enum param → ptr" applied at definition time, but the matching call-side fixup in `compiler/src/codegen/llvm/expr/call_user.cpp:897-922` builds its lookup key from the unmangled fn_name (e.g. `Heap_new`) which does not match the FuncInfo registered under `Heap__MinDecl_new` (impl.cpp:908). Lookup misses → fixup not applied → struct passed by value to ptr-expecting callee.
- [x] 2.3 Verified the call does not flow through `gen_call_user_function` (sentinel `emit_line` at entry was never emitted in the failing case). The actual codegen path for `Heap[T]::new` is elsewhere — likely a generic-class or struct-impl path. Need to find that path.

## 3. Fix
- [ ] 3.1 Identify the codegen path that emits `call %struct.Heap__MinDecl @...3newE(%struct.MinDecl %t14)` (audit `= call` sites in `compiler/src/codegen/llvm/expr/` for generic-class / generic-impl emitters)
- [ ] 3.2 Apply the same struct→ptr ABI fixup at that emission point, using either: (a) the registered mangled key `mangled_type_name + "_" + method`, or (b) reverse-lookup of `functions_` by `llvm_name == mangled`, or (c) unconditional rule for first arg when fn is `Type::method`.
- [ ] 3.3 Rebuild compiler via `scripts\build.bat`
- [ ] 3.4 Repro fixture passes
- [ ] 3.5 Existing bug #7/#8/#9 regression tests still pass

## 4. Re-enable downstream
- [ ] 4.1 Add `test_parse_top_decl_int_x` to `compiler-tml/tests/native/c_parser.test.tml`
- [ ] 4.2 Add `test_parse_translation_unit_int_x` to same file
- [ ] 4.3 Run `tml cc .sandbox/int_x.c --emit=ast` end-to-end and confirm exit 0
- [ ] 4.4 Unblock phase24 Phase 4 (essential.c / mem.c self-compile)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with the codegen fix
- [ ] 5.2 Document the bug class in docs/patches/v0.3.x.md
- [ ] 5.3 Run full compiler + cc test suites and confirm zero regressions
