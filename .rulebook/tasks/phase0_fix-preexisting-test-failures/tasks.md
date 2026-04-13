# Tasks: Fix Pre-Existing Test Failures

**Status**: Planned (0/10)
**Priority**: HIGH — blocks 100% pass rate
**Current**: 257/264 pass (7 failures)

---

## Phase 1: K001 Crashes (2 items)

- [ ] 1.1 Fix doc_generation crash (X003 exit -1073741784): investigate stack overflow in doc comment codegen, likely recursive type handling or large string buffer
- [ ] 1.2 Fix maybe_inference crash (X003 exit -1073740791): trace the heap corruption (0xAB debug fill pattern), likely CacheEntry or large struct in HashMap value slot

## Phase 2: Runtime Timeouts (2 items)

- [ ] 2.1 Fix infer_differential timeout: profile the 10 tests, identify which are slow, optimize hot paths or split into smaller test files
- [ ] 2.2 Fix mir_passes timeout: profile MIR optimization pass tests, reduce input size or increase timeout threshold

## Phase 3: Codegen Timeouts (2 items)

- [ ] 3.1 Fix parser_basic codegen timeout (X002 >30s): the full TML parser chain (~8K lines across 7 files) exceeds the codegen time limit; split into smaller compilation units or increase timeout for parser tests
- [ ] 3.2 Fix test_frontend_sim codegen timeout (X002 >30s): same root cause as parser_basic — full frontend simulation imports the entire parser

## Phase 4: Transient Linker Error (1 item)

- [ ] 4.1 Fix destructuring_let linker permission denied (N001): add retry logic or file lock detection before LLD invocation

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
