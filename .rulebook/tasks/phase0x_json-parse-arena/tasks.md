## 1. Diagnosis
- [x] 1.1 Locate JSON C runtime: `compiler/src/json/json_runtime.cpp` — uses global handle vector (`json_values`) with `alloc_json_handle`/`tml_json_free`. `json_allocator.hpp` has `JsonArena` class (C++) but NOT wired into FFI.
- [x] 1.2 Verify baseline: Tiny 2638 ns/op, Small 11238 ns/op — matches proposal numbers

## 2. C Runtime — Arena Allocator
- [x] 2.1 Added arena FFI to `json_runtime.cpp`: `JsonArena` struct with handle tracking, `tml_json_arena_create`, `tml_json_arena_parse`, `tml_json_arena_reset`, `tml_json_arena_destroy`
- [x] 2.2 `tml_json_arena_parse` calls `fast::parse_json_fast`, stores handle in arena tracking list. `tml_json_arena_reset` bulk-frees all handles in O(n).
- [x] 2.3 Build succeeds (18/18 targets)

## 3. TML Bindings
- [x] 3.1 Created `lib/std/src/json/arena.tml`: `JsonArena` type with `new(capacity)`, `parse(json)`, `reset()`, `drop()`
- [x] 3.2 FFI bindings: 4 `@extern` declarations matching C++ exports
- [x] 3.3 Type-check passed

## 4. Benchmark Gate
- [x] 4.1 Arena benchmark: Tiny arena 2563 ns/op (baseline 2638 ns/op, ~3% improvement)
- [x] 4.2 GATE NOT MET: arena wrapping doesn't address the root cause. Bottleneck is inside C++ `parse_json_fast` which uses `std::string`/`std::vector` standard allocators. The arena tracks handles but doesn't change the parser's allocation behavior. Deep parser integration (custom allocators) needed for the 10x improvement target.
- [x] 4.3 Note: C++ `JsonArena` class exists in `json_allocator.hpp` but isn't used by the fast parser. Future work: wire bump allocator into `parse_json_fast` internal allocations.

## 5. Validation
- [x] 5.1 4 arena regression tests pass (create/destroy, parse object, reset/reuse, multiple parses)
- [x] 5.2 Compiler tests: 55/55 pass (SSO + arena changes combined)

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 6.1 Arena infrastructure committed with SSO changes
- [x] 6.2 Tests: `lib/std/tests/json/arena_parse.test.tml` (4 tests)
- [x] 6.3 All tests pass
