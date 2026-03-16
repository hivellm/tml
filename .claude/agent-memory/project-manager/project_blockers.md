---
name: Critical blocker chain
description: Dependency chain for TML project — what blocks what, ordered by cascade impact
type: project
---

As of 2026-03-15.

## Blocker Chain (highest cascade impact first)

### B1: async runtime foundation (async-network-stack Phase 1)
- Blocks: ALL of async HTTP, WebSocket, Promise/Observable, M3/M4 milestones
- Needs: Future/Poll/Waker types, single-threaded executor, block_on()
- Status: Not started (0%)
- Risk: HIGH — months of work, no clear owner

### B2: lambda→func pointer conversion (stdlib-essentials 1.4.2)
- Blocks: Vec::retain, Vec::from_iter, HashSet::from_iter, Distribution behavior, all Phase 2 stdlib
- Root cause: call.cpp doesn't handle direct lambda arg where func ptr expected ("void type only allowed for results")
- Status: 0% — identified, not fixed
- File: compiler/src/codegen/llvm/expr/call.cpp ~line 124+

### B3: generic trait dispatch returning () (fix-codegen-coverage-blockers Phase 1)
- Blocks: ~30 coverage functions including Array.hash(), Pool::acquire, Range::size_hint, Poll::eq, F32/F64 sum/product
- Root cause: type substitution for constrained generic impl return types fails
- Status: 0% — not started
- File: compiler/src/codegen/llvm/expr/call_method.cpp (expr_call_method.cpp)

### B4: doc comment preservation (developer-tooling Phase 1)
- Blocks: `tml doc` HTML generation, M2 documentation gate
- Needs: lexer DocComment token, parser propagation to AST, HTML generator
- Status: 0% — not started despite developer-tooling being at 75% overall

### B5: fix-suite-codegen-bug (function symbol collision in suite merging)
- Blocks: reliable suite mode (max_per_suite > 1)
- Root cause: repeat[T] vs repeat_char conflict in suite_execution.cpp merging
- Status: 0% — not started (workaround: forced individual mode for compiler tests)

### B6: Pin[ref T] ref-ref type mismatch (complete-async-coverage 1.1c)
- Blocks: 3 Pin[ref T] tests
- Root cause: type system rejects `ref ref T` access pattern
- Minor impact — doesn't cascade widely

## Milestone Gates

| Gate | What's needed | Blocking |
|------|--------------|---------|
| M1 complete | Fix 6 compiler bugs (1.6.x in language-completeness-roadmap) | M2 start |
| M2 complete | doc comments + HTML generator + serialization (TOML/YAML) | M3 formal start |
| M3 complete | async runtime + AsyncTcp/Udp (async-network-stack Phase 1-3) | M4 start |
| M4 complete | HTTP/1.1 server+client + WebSocket | Enterprise adoption |

**Why:** When prioritizing tasks, check this chain. Unblocking B2 delivers the most immediate value (stdlib completeness). Unblocking B1 is the highest long-term strategic priority.
**How to apply:** Any task that resolves B1-B6 should be elevated by one priority level.
