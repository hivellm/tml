# ARCHIVED — See .rulebook/tasks/archive/2026-03-21-codegen-structural-fixes/

## Phase 0: Diagnostic Hygiene
- [x] 0.1 Mark NeverError::to_string, NeverError::debug_string with @no_coverage (commit ebaa006b)
- [x] 0.2 Fix coverage key collision for From impls (uses "I32::from" qualified keys, convert at 100%)
- [x] 0.3 Classify each of the 273 uncovered functions into: compiler / runtime / infra / tests-missing (commit cdc7594a)
- [x] 0.4 Update baseline: full suite 1408/1477 passing (2026-03-16)

## Phase 1: Codegen Representation Bugs (~52 functions) — COMPLETE
- [x] 1.1-1.4 BUG-MAYBE: FIXED — core/option 21/21, core/num 50/50
- [x] 1.5-1.7 BUG-OUTCOME: FIXED — std/zlib 12/12 (multi-param generic fix commit a28c74e2)
- [x] 1.8-1.10 BUG-GEP: FIXED — core/intrinsics 26/26
- [x] 1.11-1.13 BUG-CLOSURE: FIXED — core/cell 26/26 (closure MIR codegen commit a0551f9a)

## Phase 2: Coverage-Only Runtime Failures (~71 functions) — MOSTLY FIXED
- [x] 2.1-2.3 CRASH-ALLOC: FIXED — current_ret_type_ override in let-stmt (commit cc2ad6a9)
- [x] 2.4-2.5 CRASH-ONCE: once_lock_get_or_init — pre-existing coverage ABI fragility, @no_coverage candidate
- [x] 2.6 CRASH-CAPTURE: subsumed by CRASH-ALLOC fix
- [x] 2.7-2.8 CRASH-FUTURE: FIXED — Pin dispatch (4 bugs) + cross-module generic field resolution (lookup_struct re-exports + pub use). future_ready_value test passes.
- [x] 2.9 BUG-NESTED-GENERIC: Poll[Outcome[I64, MyError]] lost inner type — FIXED: expected_enum_type_ propagation in call.cpp for single-type-param outer enums

## Phase 3: Link/Infrastructure (~52 functions) — COMPLETE
- [x] 3.1-3.3 LINK-OPENSSL: link failures are compile-time LLD failures (std_zlib, std_json, std_http, std_io, std_search, std_profiler) — infrastructure-dependent, not coverage failures
- [x] 3.4 LINK-CRYPTO: crypto tests pass in normal (non-coverage) mode
- [x] 3.5 INFRA-NET: std/net/tls_cert_verify fails — real network dependency, @no_coverage candidate
- [x] 3.6 INFRA-THREAD: thread/scope passes (verified via full suite run 2026-03-16)

## Phase 4: Associated Type Constraints (~30 functions) — PARTIAL
- [x] 4.1 TY-ASSOC: where I::Item = ref T implemented (commit f220fb7c)
- [x] 4.2 TY-ASSOC: core/iter 52/52 passing — basic associated type constraints working
- [x] 4.3 TY-ASSOC: inner iterator dispatch — FIXED by fold[B] monomorphization fix (cloned, copied, flatten, intersperse, peekable all pass)
- [x] 4.4 TY-ASSOC: constructor monomorphization — FIXED: infer_expr_type for generic function calls now resolves type params from arguments (infer.cpp)
- [x] 4.5 TY-ASSOC: verified iter/adapters pass — all 5 adapters 1/1

## Phase 5: Write Missing Tests — DONE
- [x] 5.1 Smoke test: array/ascii — 5 test files, all pass (23/23 in core/array suite)
- [x] 5.2 Smoke test: collections/buffer — 18 test files, 73/73 pass
- [x] 5.3 Array/ascii tests — covered in existing test files
- [x] 5.4 Buffer tests — added 3 new files (core ops, slice/str, endian), 73/73 collections pass
- [x] 5.5 fmt 39/45 (6 pre-existing Unit type failures), json 16/18 (2 LLD link), no new gaps found

## Verification
- [ ] 6.1 Run full coverage — confirm improvement over 95.02% baseline
- [ ] 6.2 Update coverage-blockers-report.md with final numbers
- [ ] 6.3 Commit all changes

## Current Failures Summary (2026-03-20) — 1545/1599 passing
- [ ] KNOWN: core/array/array_zip_eq — pre-existing
- [ ] KNOWN: core/future/future_poll — pre-existing async IR mismatch
- [ ] KNOWN: std/net/tls_cert_verify — real network dependency
- [ ] KNOWN: test/report/report_color — pre-existing
- [ ] KNOWN: [compile] 10 LLD link failures (std_zlib x2, std_json x2, std_http, std_io, std_search, std_profiler, compiler_compiler x2) — infrastructure/OpenSSL dependency
