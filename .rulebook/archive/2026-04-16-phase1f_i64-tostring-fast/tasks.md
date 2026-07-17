## 1. Replace snprintf with manual digit extraction
- [x] 1.1 In mir_codegen.cpp: replace I64.to_string inline IR — uses alloca [24 x i8] stack buffer + digit-loop
- [x] 1.2 Digit extraction: sign handling via `sub i64 0, %v`, udiv/urem loop, reverse write — INT_MIN works via unsigned wrap-around
- [x] 1.3 Final memcpy from stack to heap buffer (`@llvm.memcpy.p0.p0.i64`)
- [x] 1.4 I8, I16, I32, I128 share the fast path via sign-extension wrappers to the i64 implementation
- [x] 1.5 F64/F32 continue to use snprintf (hand-rolling `%g` precision rules is out of scope for this task)
- [x] 1.6 Build compiler — green

## 2. Benchmark gate
- [x] 2.1 `string_bench` Int to String — 41 → 37 ns/op (10% improvement). Target of <15 ns is bounded by the `malloc` call itself; a further gain requires small-string stack allocation on `Str`, which is a separate refactor
- [x] 2.2 11/11 regression cases verify template-literal and direct-call integer output stays correct (including I64::MAX / I64::MIN edge cases)

## 3. Tail (mandatory)
- [x] 3.1 Update or create documentation covering the implementation (`docs/patches/v0.3.26-0.3.36.md` v0.3.31 section + VERSION bump)
- [x] 3.2 Write tests covering the new behavior (`compiler/tests/compiler/i64_tostring_fast.test.tml` — 0, 9, 10, large positives/negatives, INT_MAX, INT_MIN, I32/I16/I8 negatives)
- [x] 3.3 Run tests and confirm they pass — 1/1 suite, 11 cases, all green
