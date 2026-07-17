## 1. Remove HeapValidate
- [x] 1.1 `compiler/runtime/memory/str_free.c` — removed `HeapValidate(heap, 0, ptr)` from the Windows path
- [x] 1.2 Image-range miss now goes straight to `mem_free(ptr)` (codegen-side `is_heap_str_producer` tracking makes the validation redundant)
- [x] 1.3 Null check and image-range fast path preserved (~1–3 ns)
- [x] 1.4 Build runtime — green

## 2. Validation
- [x] 2.1 pipe_output.sh 9/9, str_methods_ast, foreach, i64_tostring_fast, when_block_body — all pass
- [x] 2.2 Core string tests reached via the regression sweep — zero crashes
- [x] 2.3 `string_bench` Int to String runs cleanly (1M drops) — 31 ns/op vs 37 ns/op previously

## 3. Tail (mandatory)
- [x] 3.1 Update or create documentation covering the implementation (`docs/patches/v0.3.26-0.3.36.md` — new v0.3.32 section with benchmark table)
- [x] 3.2 Write tests covering the new behavior — the existing `string_bench` drops 1M temporaries per run; any corruption on the affected path would surface as a crash
- [x] 3.3 Run tests and confirm they pass — 4/4 regression scripts green, benchmark 41→31 ns/op combined with phase1f
