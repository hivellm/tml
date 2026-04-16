## 1. Remove HeapValidate
- [ ] 1.1 In str_free.c: remove HeapValidate call on Windows path
- [ ] 1.2 After image range check: call mem_free directly if ptr is not in any image range
- [ ] 1.3 Keep null check and image range check (fast path ~3-5 ns)
- [ ] 1.4 Build runtime

## 2. Validation
- [ ] 2.1 Run compiler test suite — zero regressions
- [ ] 2.2 Run core test suite — zero crashes from invalid free
- [ ] 2.3 Run string_bench — verify no crashes on string deallocation

## 3. Tail (mandatory)
- [ ] 3.1 Update docs/analysis/string/ with new free cost
- [ ] 3.2 Test: concat + free cycle 100K iterations, zero crashes
- [ ] 3.3 Run tests and confirm pass
