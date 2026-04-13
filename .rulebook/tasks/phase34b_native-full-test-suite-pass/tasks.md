## 1. Implementation
- [ ] 1.1 Run the full 266-test compiler suite with `--backend=native`, redirect output to `.sandbox/native_suite.log`, read and triage all failures into codegen / ABI / linker buckets with a count per bucket
- [ ] 1.2 Fix codegen failures: for each unhandled instruction or expression form found in triage, add the missing branch in the appropriate `emit_*.tml` file; re-run the affected test after each fix before moving to the next
- [ ] 1.3 Fix ABI failures: correct calling-convention emission for multi-return structs (sret pointer convention on x86-64 Windows), variadic `printf`-style calls, and `@extern("c")` function declarations that pass or return structs by value
- [ ] 1.4 Fix linker failures: verify that every runtime symbol referenced by native-backend object files (`__tml_panic`, `__tml_alloc`, `__tml_list_grow`, etc.) is exported from `tml_compiler.dll` or present in the linked static runtime archive; add missing exports or link flags as needed
- [ ] 1.5 Confirm 266/266 tests pass under `--backend=native` with zero failures and zero timeouts; record the final pass count in this tasks.md
- [ ] 1.6 Add a benchmark entry to `docs/benchmarks.md`: compile a canonical 500-line TML program with `--backend=native` and `--backend=llvm`, record wall-clock compile time and output object size for both, compute the speedup ratio

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
