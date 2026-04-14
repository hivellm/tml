# Proposal: phase24a_fix-k001-str-len-symbol

## Why

The `core::str::len()` function symbol `@tml_N4core3str3lenE_S` is not emitted in generated LLVM IR, causing a K001 undefined-value error at link time. This blocks **all** string-dependent benchmarks and any user code that calls `str.len()`, `str.contains()`, `str.find()`, `str.split()`, `str.trim()`, or any method that internally uses `len()`.

**Benchmark impact**: 29+ core modules (str, simd, fmt) and 80+ std modules (text, json, url, mime, http) are completely unbenmarkable. This is the single highest-impact codegen bug — fixing it unblocks ~17% of the entire standard library.

**Error reproduced by**:
```
tml run benchmarks/profile_tml/string_bench.tml --stage=parser:cpp
tml run benchmarks/profile_tml/text_bench.tml --stage=parser:cpp
```

**Error message**:
```
K001: use of undefined value '@tml_N4core3str3lenE_S'
  %t566 = call i64 @tml_N4core3str3lenE_S(ptr %this)
```

## What Changes

1. Trace why `core::str::len()` is not being emitted in LLVM IR — likely a missing entry in the symbol table, incorrect dead-code elimination, or the function body not being generated during MIR→LLVM emission
2. Fix the codegen path so `core::str::len()` (and any other `core::str` methods with the same issue) emit valid LLVM function definitions
3. Verify string_bench.tml and text_bench.tml compile and run successfully
4. Run the full test suite to confirm no regressions

## Impact
- Affected specs: core::str, core::fmt, std::text, std::json, std::net::url, std::net::mime
- Affected code: `compiler/src/mir/thir_mir_builder.cpp`, `compiler/src/codegen/instructions.cpp`, possibly `compiler/src/query/query_core.cpp`
- Breaking change: NO
- User benefit: String operations work in compiled TML programs; ~80+ modules unblocked for benchmarking
