## 1. Diagnosis
- [ ] 1.1 Locate JSON C runtime: find `json_arena`/`json_parse_fast` in `lib/std/runtime/json/` — map the malloc/free call sites
- [ ] 1.2 Verify baseline: run `benchmarks/profile_tml/json_bench.tml --stage=parser:cpp` and record ns/op for Parse Tiny/Small/Medium

## 2. C Runtime — Arena Allocator
- [ ] 2.1 Create `lib/std/runtime/json/json_arena.c`: bump allocator with `tml_json_arena_create`, `tml_json_arena_reset`, `tml_json_arena_destroy`
- [ ] 2.2 Add `tml_json_parse_arena(src, len, arena)` — routes node allocations into the arena instead of malloc
- [ ] 2.3 Wire into CMakeLists / build system so `json_arena.c` is compiled into the runtime

## 3. TML Bindings
- [ ] 3.1 Create `lib/std/src/json/arena.tml`: `JsonArena` type with `new(capacity)`, `parse(json)`, `reset()`, `drop()`
- [ ] 3.2 Export from `lib/std/src/json/mod.tml` (or equivalent module root)
- [ ] 3.3 Type-check: `tml check lib/std/src/json/arena.tml` — zero errors

## 4. Benchmark Gate
- [ ] 4.1 Update `benchmarks/profile_tml/json_bench.tml` to add arena variants for Parse Tiny/Small/Medium
- [ ] 4.2 Run benchmark — GATE: Parse Tiny ≤ 300 ns/op, Parse Small ≤ 2000 ns/op, ratio vs Rust < 2x
- [ ] 4.3 Run Rust reference: `.sandbox/rust_json_bench.exe` — record for ratio comparison

## 5. Validation
- [ ] 5.1 `tml test --suite=std` — no regressions (json tests must still pass)
- [ ] 5.2 `tml test --suite=compiler` — no regressions

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 6.1 Update CHANGELOG.md — version bump, perf entry
- [ ] 6.2 Write regression test: `lib/std/tests/json/arena_parse.test.tml` (arena create/parse/reset/drop correctness)
- [ ] 6.3 Run regression test — confirm passes
- [ ] Update or create documentation covering the implementation
- [ ] Write tests covering the new functionality
- [ ] Verify all tests pass
