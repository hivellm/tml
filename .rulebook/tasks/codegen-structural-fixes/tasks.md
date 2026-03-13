## Phase 0: Diagnostic Hygiene
- [x] 0.1 Mark NeverError::to_string, NeverError::debug_string with @no_coverage (already done — commit ebaa006b)
- [x] 0.2 Fix coverage key collision for From impls (already correct — uses "I32::from" qualified keys, convert module at 100%)
- [x] 0.3 Classify each of the 273 uncovered functions into: compiler / runtime / infra / tests-missing (commit cdc7594a)
- [ ] 0.4 Update baseline numbers after hygiene cleanup

## Phase 1: Codegen Representation Bugs (~52 functions)
- [ ] 1.1 BUG-MAYBE: repro mínima — Maybe[I32] emitting i32 instead of struct (IN PROGRESS — agent running)
- [ ] 1.2 BUG-MAYBE: emit IR, compare with Rust equivalent
- [ ] 1.3 BUG-MAYBE: fix enum/optional lowering
- [ ] 1.4 BUG-MAYBE: verify std_lowlevel suite compiles and passes
- [ ] 1.5 BUG-OUTCOME: repro mínima — Outcome[Bytes, ZlibError] type mismatch
- [ ] 1.6 BUG-OUTCOME: fix generic enum monomorphization
- [ ] 1.7 BUG-OUTCOME: verify zlib_zstd suite passes
- [ ] 1.8 BUG-GEP: repro mínima — invalid GEP on scalar
- [ ] 1.9 BUG-GEP: fix place vs value in field projection
- [ ] 1.10 BUG-GEP: verify intrinsics_array_ops passes
- [ ] 1.11 BUG-CLOSURE: repro mínima — struct with Fn field unsized alloca
- [ ] 1.12 BUG-CLOSURE: fix Fn type layout in struct context
- [ ] 1.13 BUG-CLOSURE: verify cell/lazy suite passes

## Phase 2: Coverage-Only Runtime Failures (~71 functions)
- [ ] 2.1 CRASH-ALLOC: confirm alloc suites pass without coverage
- [ ] 2.2 CRASH-ALLOC: identify coverage harness init conflict
- [ ] 2.3 CRASH-ALLOC: fix harness init order or allocator interaction
- [ ] 2.4 CRASH-ONCE: reproduce once_lock failure in coverage mode
- [ ] 2.5 CRASH-ONCE: fix atomics/init ordering under instrumentation
- [ ] 2.6 CRASH-CAPTURE: reproduce capture crash, check panic hook interaction
- [ ] 2.7 CRASH-FUTURE: reproduce future_fuse/async_lazy_future crashes
- [ ] 2.8 CRASH-FUTURE: fix async executor + coverage interaction

## Phase 3: Link/Infrastructure (~52 functions)
- [ ] 3.1 LINK-OPENSSL: diff link lines normal vs coverage mode
- [ ] 3.2 LINK-OPENSSL: create minimal repro with single EVP call
- [ ] 3.3 LINK-OPENSSL: fix link order or missing library
- [ ] 3.4 LINK-CRYPTO: verify cipher suites pass after OpenSSL fix
- [ ] 3.5 INFRA-NET: evaluate net_tls/tcp_timeout — real fix vs @no_coverage
- [ ] 3.6 INFRA-THREAD: evaluate thread/scope — real fix vs @no_coverage

## Phase 4: Associated Type Constraints (~30 functions)
- [ ] 4.1 TY-ASSOC: repro mínima — where I::Item = ref T
- [ ] 4.2 TY-ASSOC: fix type projection normalization before codegen
- [ ] 4.3 TY-ASSOC: verify iter/adapters (cloned, copied, flatten, intersperse, peekable)

## Phase 5: Write Missing Tests (only after Phases 1-3 clean)
- [ ] 5.1 Smoke test: array/ascii — confirm compile+link+run
- [ ] 5.2 Smoke test: collections/buffer — confirm compile+link+run
- [ ] 5.3 Write array/ascii tests (9 functions)
- [ ] 5.4 Write collections/buffer tests (67 uncovered functions)
- [ ] 5.5 Write remaining partial module tests (fmt/rt, json gaps, pool, alloc/layout)

## Verification
- [ ] 6.1 Run full coverage — confirm improvement over 95.02%
- [ ] 6.2 Update coverage-blockers-report.md with final numbers
- [ ] 6.3 Commit all changes
