## Status: 0/22 items complete

## Phase 1: SSE2 Instructions
- [ ] 1.1 Encode SSE2 scalar float ops: ADDSD, SUBSD, MULSD, DIVSD, SQRTSD, UCOMISD (64-bit doubles)
- [ ] 1.2 Encode SSE2 scalar float ops: ADDSS, SUBSS, MULSS, DIVSS, SQRTSS, UCOMISS (32-bit floats)
- [ ] 1.3 Encode SSE2 vector load/store: MOVDQU, MOVDQA, MOVSD, MOVSS, MOVAPD (128-bit XMM)
- [ ] 1.4 Encode conversion instructions: CVTSI2SD, CVTSD2SI, CVTSI2SS, CVTSS2SI, CVTSS2SD, CVTSD2SS

## Phase 2: SSE4.2 and AVX2
- [ ] 2.1 Encode SSE4.2 string ops: PCMPESTRI, PCMPESTRM, PCMPISTRI, PCMPISTRM (for SIMD string search)
- [ ] 2.2 Encode SSE4.1/4.2 integer: PMULLD, PBLENDW, PINSRQ, PEXTRQ, PCMPGTQ
- [ ] 2.3 Encode AVX2 256-bit integer: VPADDD, VPSUBD, VPMULLD, VPAND, VPOR, VPXOR (YMM registers)
- [ ] 2.4 Add CPUID feature detection at startup: set flags for SSE2, SSE4.1, SSE4.2, AVX2 — emit instructions only when flag is set

## Phase 3: Atomic Instructions
- [ ] 3.1 Encode `LOCK CMPXCHG r64, [mem]` (compare-and-swap — basis for all lock-free structures)
- [ ] 3.2 Encode `LOCK XADD r64, [mem]` (atomic fetch-add — used in reference counting)
- [ ] 3.3 Encode `MFENCE`, `SFENCE`, `LFENCE` (memory barriers for sequentially consistent atomics)

## Phase 4: Peephole Optimizations
- [ ] 4.1 Eliminate redundant MOVs: `MOV rA, rB; MOV rB, rA` → keep first, drop second (if rA not modified between)
- [ ] 4.2 Strength reduction: `IMUL r64, 2` → `LEA r64, [r64+r64]`; `IMUL r64, 4/8` → `SHL r64, 2/3`
- [ ] 4.3 LEA for multiply: `r = a * 3` → `LEA r, [a + a*2]`; `r = a * 5` → `LEA r, [a + a*4]`
- [ ] 4.4 Zero-register idiom: `XOR r64, r64` instead of `MOV r64, 0` (1 byte shorter, no REX prefix needed for 32-bit form)

## Phase 5: Constant Propagation at MachIR Level
- [ ] 5.1 Track VirtualRegs holding compile-time constants through the MachIR; fold constant arithmetic (e.g., `ADD vr1=3, vr2=4` → `vr3=7` as imm)
- [ ] 5.2 Propagate constants into branch conditions: `CMP vr=5, 3; JL label` → unconditional `JMP label` or eliminate dead branch

## Phase 6: Benchmark
- [ ] 6.1 Benchmark native backend vs `tml build --backend=llvm -O0` on 5 programs: fibonacci, sort, string concat, HTTP parse, JSON parse
- [ ] 6.2 Benchmark native backend vs `tml build --backend=llvm -O2` on same programs — target: native within 2x of LLVM -O2
- [ ] 6.3 Report instruction count diff (use `perf stat` or RDPMC-based counter) for each benchmark

## Phase 7: Default Flag
- [ ] 7.1 Change `--backend=native` to be the default on Windows x86_64 (LLVM still available via `--backend=llvm`)
- [ ] 7.2 Update CLI help text and docs/readme.md to reflect new default backend

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
