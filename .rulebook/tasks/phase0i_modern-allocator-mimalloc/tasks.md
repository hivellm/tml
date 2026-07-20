# phase0i — Modern allocator (mimalloc) in the runtime

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` finding L-104
> (06-runtime-concurrency.md). Cheapest broad runtime win: one link-level
> change, no codegen change, benefits every allocation the codegen already
> over-emits; also de-serializes the heap for phase0l's threaded tests.

## Motivation

Codegen calls CRT `malloc` directly (closure envs, heap struct literals, string
concat, class instantiation) and `mem.c` is a thin malloc wrapper; the default
Windows CRT heap is lock-serialized and slow. Generated code is
allocation-heavier than Rust's (clone-on-read band-aids), compounding the gap.

## 1. Implementation
- [ ] 1.1 Vendor mimalloc; build it with zig cc into the runtime static lib;
  route `malloc/free/realloc` + `mem_alloc/mem_free/mem_realloc` through it
  (override or alias layer). Keep the LLVM `allockind`/`alloc-family`
  attributes on the declarations.
- [ ] 1.2 Build-flag fallback to CRT malloc for diagnosis.
- [ ] 1.3 Bench before/after and record here: alloc-heavy corpus (collection
  churn, Text transforms) + one full suite wall-clock.
- [ ] 1.4 Full quality gate; `mcp__tml__debug(check_leaks=true)` probes still
  clean (leak tooling must understand the new allocator or be pointed at its
  stats).

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation (runtime allocator note; fallback flag)
- [ ] 2.2 Write tests covering the new behavior (alloc/free/realloc smoke
  across sizes; cross-thread alloc/free fixture)
- [ ] 2.3 Run tests and confirm they pass
