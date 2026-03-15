## Phase 0: Diagnostic Hygiene
- [x] 0.1 Mark NeverError::to_string, NeverError::debug_string with @no_coverage (already done — commit ebaa006b)
- [x] 0.2 Fix coverage key collision for From impls (already correct — uses "I32::from" qualified keys, convert module at 100%)
- [x] 0.3 Classify each of the 273 uncovered functions into: compiler / runtime / infra / tests-missing (commit cdc7594a)
- [x] 0.4 Update baseline: full suite 1408/1476 passing (2026-03-15)

## Phase 1: Codegen Representation Bugs (~52 functions) ✅ ALL FIXED
- [x] 1.1-1.4 BUG-MAYBE: FIXED — core/option 21/21, core/num 50/50
- [x] 1.5-1.7 BUG-OUTCOME: FIXED — std/zlib 12/12 (multi-param generic fix commit a28c74e2)
- [x] 1.8-1.10 BUG-GEP: FIXED — core/intrinsics 26/26
- [x] 1.11-1.13 BUG-CLOSURE: FIXED — core/cell 26/26 (closure MIR codegen commit a0551f9a)

## Phase 2: Coverage-Only Runtime Failures (~71 functions) ✅ MOSTLY FIXED
- [x] 2.1-2.3 CRASH-ALLOC: FIXED — current_ret_type_ override in let-stmt (commit cc2ad6a9)
  - Root cause: return codegen used let-stmt type hint instead of function return type
  - Added func_ret_type_ field, 8 files modified. core/alloc 37/37 passing.
- [x] 2.4-2.5 CRASH-ONCE: once_lock_get_or_init — coverage ABI fragility (pre-existing)
  - Closure return corrupted by tml_cover_func stack layout changes. @no_coverage candidate.
- [x] 2.6 CRASH-CAPTURE: Subsumed by CRASH-ALLOC fix
- [ ] 2.7-2.8 CRASH-FUTURE: core/future/future_poll — pre-existing IR type mismatch (Maybe__I32 vs ptr)

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
