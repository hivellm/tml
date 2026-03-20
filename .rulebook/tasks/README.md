# Tasks — Priority Table

**Updated**: 2026-03-20
**Active**: 23 tasks | **Archived**: 5 (today) + previous
**Test baseline**: 1545/1599 passing (96.6%)

---

## Tier 1: High Priority (unblocks other tasks)

| # | Task | Status | Progress | Next step |
|---|------|--------|----------|-----------|
| 1 | **fix-struct-codegen-blockers** | 80% | 14/18 | Bug 4.4 closure+List crash (pre-existing) |
| 2 | **codegen-structural-fixes** | 80% | 28/40 | 4.3 inner iterator dispatch + 5.2-5.5 write tests |
| 3 | **fix-codegen-coverage-blockers** | 70% | 28/44 | Phase 1 generic trait dispatch -> () |

## Tier 2: Medium Priority (product features)

| # | Task | Status | Progress | Next step |
|---|------|--------|----------|-----------|
| 4 | **http-production-server** | 50% | 28/55 | 1.6 SO_RCVTIMEO fix, 2.2 chunked encoding |
| 5 | **http-production-framework** | 75% | 35/46 | 1.2 Hook.add/validate, 1.5 content-type parser |
| 6 | **complete-async-coverage** | 60% | 15/25 | Commit from_iter.test.tml, write missing tests |
| 7 | **developer-tooling** | 75% | 52/70 | Phase 1 doc comment preservation in lexer/parser |
| 8 | **zig-cc-compiler-integration** | 45% | 8/18 | 3.1 zig cc detection in compiler_setup.cpp |

## Tier 3: Low Priority (incremental improvements)

| # | Task | Status | Progress | Next step |
|---|------|--------|----------|-----------|
| 9 | **optimize-codegen-like-rust** | 75% | 28/33 | Phase 6 exception handling (invoke/cleanuppad) |
| 10 | **implement-reflection** | 48% | 33/70 | 3.1.4 get_field method |
| 11 | **zig-inspired-test-migration** | 60% | 12/20 | 1.7 fix library_decls_only for all functions |
| 12 | **package-manager** | 20% | 7/40 | Phase 1 git dependencies |
| 13 | **language-completeness-roadmap** | 48% | 70/172 | Tracking roadmap — update with recent progress |

## Tier 4: Future (not started, planning)

| # | Task | Status | Progress | Dependencies |
|---|------|--------|----------|-------------|
| 14 | **self-hosting-compiler** | 0% | 5/200+ | fix-struct-codegen-blockers + many others |
| 15 | **add-compiler-cpp-unit-tests** | 0% | 0/93 | No hard dependencies |
| 16 | **function-contracts** | 0% | 0/12 | Future language feature |
| 17 | **tracy-profiler-integration** | 0% | 0/50+ | None |
| 18 | **inspector-diagnostics** | 0% | 0/66+ | reflection + developer-tooling |
| 19 | **implement-simd-generic-isa** | 0% | 0/100+ | M6 roadmap |
| 20 | **simd-optimization** | 0% | 0/100+ | M6 roadmap |
| 21 | **auto-parallel** | 0% | 0/32 | M6 roadmap |
| 22 | **cross-compilation** | 0% | 0/85+ | M6 roadmap |
| 23 | **self-hosting-cranelift** | 0% | 0/10 | Cranelift backend must exist |

---

## Dependency Graph

```
fix-struct-codegen-blockers (Bug 1: GEP, Bug 2: ptr_read, Bug 3: mutation)
  |-> http-production-server (hooks/middleware re-enable)
  |-> http-production-framework (Phase 5 IOCP)
  |-> self-hosting-compiler (language readiness)

codegen-structural-fixes (4.3 inner iterator dispatch)
  |-> complete-async-coverage (iterator tests)
  |-> language-completeness-roadmap (M1 compiler bugs)

developer-tooling (Phase 1 doc comments)
  |-> inspector-diagnostics (depends on LSP + reflection)

implement-reflection (Phase 3 completion)
  |-> inspector-diagnostics
```

## Recently Archived (2026-03-20)

| Task | Reason |
|------|--------|
| async-network-stack | 44/44 items complete |
| refactor-async-use-existing-apis | 38 files refactored, blocked items -> fix-struct-codegen-blockers |
| user-docs-appendix-rewrite | 15/15 docs written |
| write-user-docs-oop | OOP chapter complete |
| write-user-docs-ownership | Ownership chapter complete |
