## 1. Reproduce
- [ ] 1.1 Author a TML regression test in `compiler/tests/compiler/` that mirrors the failing shape: outer TML function with a `loop` capturing the return of an inner function returning a value-type enum with `Heap`-wrapped variants into a `let x: E` binding. Confirm iteration N=2 crashes pre-fix.
- [ ] 1.2 Confirm the existing repro `./build/debug/cc_driver.exe .sandbox/test_no_inc.c --emit=ast` still crashes (exit 127, no output) after the phase24b `ref CTypeEnv` fix has been built into cc_driver.exe.

## 2. Root cause
- [ ] 2.1 Trace via `File::append_all` shows the second `base_to_ctype` call (param 0 of `lower_func_decl`) reaches `return r` (with `r: CType::Int`, kind=6) but the caller's `let p_base: CType = ...` slot never resumes. Crash is on the SSA return + caller resume of a value-type enum across a function boundary.
- [ ] 2.2 Compare with rustc: write equivalent `fn outer() { for _ in 0..2 { let x: E = inner(); ... } }` in Rust, compile to LLVM IR, diff against TML's IR for the same shape. The Rust-as-Reference methodology (CLAUDE.md) should pinpoint the lowering difference.

## 3. Fix
- [ ] 3.1 Apply codegen fix in `compiler/src/codegen/llvm/decl/func.cpp` (return-type sret lowering) and/or `compiler/src/codegen/llvm/expr/return_stmt.cpp` (return instruction emission). Mirror the structural shape of the phase0x patch in `expr/method_static_dispatch.cpp` but on the return-path side.
- [ ] 3.2 Rebuild compiler via `scripts\build.bat`. Rebuild cc_driver.exe via `tml build compiler-tml/src/cc/bin/cc_driver.tml -o build/debug/cc_driver.exe --stage=parser:cpp` (after wiping `build/debug/cache/cc_driver.obj` + `build/debug/cache/incr/incr.bin`).
- [ ] 3.3 Verify `./build/debug/cc_driver.exe .sandbox/test_no_inc.c --emit=ast` exits 0 with `cc_driver: parsed`.
- [ ] 3.4 Verify the new minimal regression from 1.1 passes.
- [ ] 3.5 Verify phase24b regression (`test_phase24b_base_to_ctype_typedef_repeat`) and phase0x bug #7/#8/#9 + heap_decl_var_repro tests still pass.

## 4. Self-compile gate (was phase24b items 4.1 / 4.2)
- [ ] 4.1 `tml cc .sandbox/test_no_inc.c --emit=ast` exits 0 with `cc_driver: parsed`.
- [ ] 4.2 `tml cc compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` reaches the next limitation (no longer crashes at the typedef-as-param point).
- [ ] 4.3 Document any subsequent gaps as separate task entries.

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update or create documentation covering the implementation
- [ ] 5.2 Write tests covering the new behavior
- [ ] 5.3 Run tests and confirm they pass
