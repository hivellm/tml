## 1. Replace snprintf with manual digit extraction
- [ ] 1.1 In mir_codegen.cpp: replace I64.to_string inline IR — use alloca [24 x i8] stack buffer
- [ ] 1.2 Implement digit extraction: handle sign, udivrem loop, write digits in reverse
- [ ] 1.3 Final memcpy from stack to heap buffer (caller expects heap-allocated Str)
- [ ] 1.4 Apply same pattern for I32, I16, I8 to_string
- [ ] 1.5 Apply same pattern for F64.to_string (replace snprintf with dtoa or similar)
- [ ] 1.6 Build compiler

## 2. Benchmark gate
- [ ] 2.1 Run string_bench — Int to String under 15 ns (from 41 ns)
- [ ] 2.2 Verify template literals with integers still produce correct output

## 3. Tail (mandatory)
- [ ] 3.1 Update docs/analysis/string/ with new numbers
- [ ] 3.2 Test: to_string for 0, -1, MAX_I64, MIN_I64
- [ ] 3.3 Run tests and confirm pass
