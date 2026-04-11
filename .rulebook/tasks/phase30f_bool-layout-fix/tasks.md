## 1. LLVM type lowering
- [ ] 1.1 In struct type generation, emit Bool fields as `i8` instead of `i1`
- [ ] 1.2 On load from Bool struct field: emit `load i8` then `trunc i8 to i1`
- [ ] 1.3 On store to Bool struct field: emit `zext i1 to i8` then `store i8`
- [ ] 1.4 Keep Bool as `i1` in local variables and function parameters (no change)

## 2. Tail (mandatory)
- [ ] 2.1 Add test: struct with Bool field stores and loads correctly
- [ ] 2.2 Add test: struct with multiple Bool fields has correct layout (no overlap)
- [ ] 2.3 Verify existing tests still pass (layout change may affect serialization)
- [ ] 2.4 Update CHANGELOG.md
