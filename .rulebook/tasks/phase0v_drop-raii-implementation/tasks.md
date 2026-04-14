## 1. Diagnosis
- [ ] 1.1 Read Rust's Drop trait docs and `std::mem::drop` semantics — understand scope-exit insertion rules (moved values excluded, panic unwind path)
- [ ] 1.2 Check `lib/core/src/` for any existing `Drop` behavior definition — confirm it does not exist yet
- [ ] 1.3 Identify the MIR scope-exit points: find where `ReturnInst`, block-end, `BreakInst`, and `ContinueInst` are emitted in the MIR builder

## 2. Implementation
- [ ] 2.1 Create `lib/core/src/drop.tml`: define `behavior Drop { func drop(self: mut Self) }` and export it
- [ ] 2.2 Implement `Drop` for `Heap[T]` in `lib/core/src/heap.tml`: `func drop(self: mut Heap[T]) { mem::free(self.ptr) }`
- [ ] 2.3 Implement `Drop` for `Text` in `lib/core/src/text.tml`: free internal buffer on drop
- [ ] 2.4 Implement `Drop` for `Buffer` in `lib/core/src/buffer.tml`: free internal allocation on drop
- [ ] 2.5 MIR pass — scope-exit drop insertion: at each scope boundary, collect all locals whose type implements `Drop`, emit `CallInst` to their `drop` method (in reverse declaration order, matching Rust)
- [ ] 2.6 Move tracking: after a value is moved (passed to a function by value), mark it as `moved` in the MIR — do NOT insert drop at its original scope exit
- [ ] 2.7 Double-free guard: if a variable has both a manual `mem::free` AND a `Drop` impl, emit a compiler warning at the manual free site

## 3. Benchmark Gate
- [ ] 3.1 Run the encoding benchmark again with `check_leaks=true` — confirm 0 leaks with automatic Drop (no manual free calls needed)
- [ ] 3.2 Run `mcp__tml__debug(file="benchmarks/profile_tml/encoding_bench.tml", check_leaks=true)` — must report 0 leaks
- [ ] 3.3 GATE: Any program using `Heap[T]`, `Text`, or `Buffer` must have 0 leaks when all locals go out of scope. Do NOT proceed if leaks remain.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions
- [ ] 4.2 Run `tml test --suite=compiler` — no regressions (compiler uses Heap[T] everywhere)
- [ ] 4.3 Write a test that intentionally moves a value and verifies the original binding is not dropped twice

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `feat(core): implement Drop behavior and automatic scope-exit drop insertion`
- [ ] 5.2 Write tests: Heap[T] drops on scope exit, Text drops on return, moved value is not double-dropped
- [ ] 5.3 Run tests and confirm they pass
