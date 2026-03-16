---
name: Task completion baselines 2026-03-15
description: Completion % for all active Rulebook tasks as of 2026-03-15 — use as baseline for velocity tracking
type: project
---

Baseline captured 2026-03-15 from .rulebook/tasks/ directory.

| Task | % | Status |
|------|---|--------|
| rewrite-test-system | 99.5% | Nearly done — deferred items only (sharding, cross-platform) |
| codegen-structural-fixes | ~80% | Phases 0-1,3,4 complete; Phase 2 (coverage crashes) active |
| optimize-codegen-like-rust | 75% | Phases 1-5 done; Phase 6 (exception handling) not started |
| single-binary-test-compilation | 90% | Core done; coverage verification + perf measurement deferred |
| zig-cc-compiler-integration | 75% | Toolchain done; TML object compiler integration pending |
| implement-reflection | 48% | Phases 1-2,4 done; Phase 3 partial; Phases 5-6 pending |
| developer-tooling | 75% | VSCode done; doc comments + HTML generator + LSP go-to-def pending |
| stdlib-essentials | 70% | Phase 1.4.2 (lambda→func) is gate for all Phase 2 work |
| complete-async-coverage | ~75% | Async net loopback done; Waker FFI + coverage run pending |
| improve-test-infrastructure | 25% | Diagnostic tests partial; fuzzing/regression dirs not started |
| language-completeness-roadmap | 48% | M1=84%, M2=41%, M3=36%, M4=23%, M5=59%, M6=3% |
| fix-codegen-coverage-blockers | 0% | Not started — generic trait dispatch returning () |
| fix-suite-codegen-bug | 0% | Not started — suite function symbol collision |
| add-compiler-cpp-unit-tests | 0% | Proposed only |
| async-network-stack | 0% | Blocked on async runtime foundation |
| auto-parallel | 0% | Future — M6 |
| cross-compilation | 0% | Future — M6 |
| implement-simd-generic-isa | 0% | Future |
| package-manager | ~20% | manifest + lockfile done; git deps + registry not started |
| self-hosting-compiler | 0% | Distant future |
| self-hosting-cranelift | 0% | Distant future |
| function-contracts | 0% | Proposed |
| inspector-diagnostics | 0% | Proposed, depends on reflection |
| tracy-profiler-integration | 0% | Proposed |
| implement-regex-module | ARCHIVED | Done — Thompson NFA, 4 test files |

**Why:** Tracks velocity. Compare against future snapshots to measure progress rate.
**How to apply:** When user asks "what progress was made?", diff against this baseline.
