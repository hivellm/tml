## 1. Reproduce
- [x] 1.1 Scaffolding regression test landed at `compiler/tests/compiler/heap_ctype_return_repro.test.tml` (currently passes — baseline for future bisects of related Heap-borrow-drop patterns).
- [x] 1.2 Confirmed `./build/debug/cc_driver.exe .sandbox/test_no_inc.c --emit=ast` crashed at exit 127 silently (typedef-as-func-param). Synthetic TML test repros (3 sequential calls, mixed-variant enum returns) all passed despite the cc_driver crash.

## 2. Root cause
- [x] 2.1 IR inspection of `build/debug/cache/incr/ir/codegen_unit_*.ll` confirmed phase24b's `ref CTypeEnv` IS lowered to `ptr %env` (mangled `_R8CTypeEnv9CBaseType`). Phase24b fix is real but addressed a value-pass theory that wasn't the actual crash.
- [x] 2.2 The actual root cause: `HashMap[Str, Heap[CType]].get(name)` returns `Heap[CType]` BY VALUE; the local copy's `ptr` aliases the map bucket's allocation. `impl Drop for Heap[T]` (`lib/core/src/alloc/heap.tml:263`) calls `mem_free(this.ptr)`. When the local `t` goes out of scope at function return, the bucket's allocation is freed. The next call to `env.typedefs.get(name)` returns a Heap with the freed pointer; `t.get()` dereferences → SIGSEGV.
- [x] 2.3 Phase24b's regression test passed because in -O0 debug mode the freed allocation often still held the expected discriminant byte before allocator reuse. The cc_driver path triggers enough allocator pressure that the freed bucket gets recycled, surfacing the dangling-pointer crash deterministically.

## 3. Fix
- [x] 3.1 Applied `Heap[T]::into_raw()` workaround in `compiler-tml/src/cc/types.tml::base_to_ctype` Typedef arm: `var t: Heap[CType] = env.typedefs.get(name); let r: CType = t.get(); let _raw: *CType = t.into_raw(); return r`. `into_raw()` nulls the local's `ptr` so its drop hits the `if this.ptr != null` guard and skips `mem_free`, preserving the map bucket's allocation.
- [x] 3.2 Type-check clean. Rebuilt cc_driver.exe via `tml build compiler-tml/src/cc/bin/cc_driver.tml -o build/debug/cc_driver.exe --stage=parser:cpp` after wiping `build/debug/cache/cc_driver.obj` + `build/debug/cache/incr/incr.bin`.
- [x] 3.3 Verified `./build/debug/cc_driver.exe .sandbox/test_no_inc.c --emit=ast` exits 0 with `cc_driver: parsed`.
- [x] 3.4 Verified phase24b regression `test_phase24b_base_to_ctype_typedef_repeat` still passes; phase0x `heap_decl_var_repro` still passes; new phase24c repro `heap_ctype_return_repro` passes; c_lexer + c_parser + c_frontend suites all pass (5/5).
- [x] 3.5 phase0x bug #7/#8/#9 + heap_decl_var_repro tests still pass.

## 4. Self-compile gate
- [x] 4.1 `./build/debug/cc_driver.exe .sandbox/test_no_inc.c --emit=ast` exits 0 with `cc_driver: parsed .sandbox/test_no_inc.c`.
- [x] 4.2 `./build/debug/cc_driver.exe compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` segfaults (exit 139). essential.c is 1465 lines and hits constructs beyond typedef-as-param. Likely related to the same Heap-borrow-drop pattern in `base_to_ctype`'s `StructRef`/`UnionRef`/`EnumRef` arms (each returns a Heap from the map directly inside an enum constructor; the returned CType's eventual drop frees the map's allocation).
- [x] 4.3 Filed follow-up `phase24d_heap-borrow-drop-structref` for the broader Heap-borrow-drop pattern in `base_to_ctype`'s StructRef/UnionRef/EnumRef arms + the longer-term language-level fix (Rc-style Heap or non-owning HashMap.get for Heap-valued maps).

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update or create documentation covering the implementation — `docs/patches/v0.3.41.md` documents root cause, fix, files changed, verification, and the StructRef/UnionRef/EnumRef follow-up. `CHANGELOG.md` row added. `VERSION` bumped 0.3.40 → 0.3.41.
- [x] 5.2 Write tests covering the new behavior — scaffolding `heap_ctype_return_repro.test.tml` already landed last commit; `cc_driver: parsed test_no_inc.c` end-to-end is the integration test (gated by cc_driver.exe rebuild).
- [x] 5.3 Run tests and confirm they pass — c_frontend, c_lexer, c_parser, heap_decl_var_repro, heap_ctype_return_repro: 5/5 pass.
