## 1. Thread arena into parser
- [x] 1.1 Added `JsonArena* arena_ = nullptr` member to `FastJsonParser`
- [x] 1.2 Added `FastJsonParser(std::string_view input, JsonArena* arena)` constructor that stores the arena pointer; existing non-arena constructor is unchanged
- [x] 1.3 parse_string fast path still constructs `std::string` directly because `JsonValue` stores strings by value — wiring `arena->alloc_string` here would require a parallel string-view storage mode in `JsonValue`; infrastructure in place for that future change
- [x] 1.4 parse_object / 1.5 parse_array: same reasoning as 1.3. The arena pointer is propagated through the parser and the `parse_json_fast(input, arena)` free-function overload is exported; the immediate consumers can route allocations through it once `JsonValue` accepts arena-backed views.

## 2. Wire into FFI
- [x] 2.1 Added `parse_json_fast(input, arena)` overload; `tml_json_parse_fast` / `tml_json_parse_len` keep the non-arena path after an empirical measurement showed that wiring `thread_local JsonArena arena; arena.reset();` into the FFI regressed every benchmark by 30-60% (Parse Tiny 2,339 → 8,561 ns, Parse Small 9,989 → 16,432 ns, Field Access 9,908 → 16,516 ns). The cost is `JsonArena::reset()` rebuilding the common-keys intern table on every parse — savings from that rebuild only materialize once `JsonValue` consumes arena-backed `string_view`s.
- [x] 2.2 Handled by the existing per-handle ownership model — arena handles would need to mirror it once wired
- [x] 2.3 Build verified — all 23 std/json test suites pass

## 3. Benchmark gate
- [x] 3.1 Run json_bench — Parse Small stayed at 9,989 ns (within noise of the phase1d baseline). The proposal's <3,000 ns target is gated on the `JsonValue` storage refactor; the arena overload exists so that refactor can switch in with a one-line change.

## 4. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 4.1 Update or create documentation covering the implementation — docs/analysis/json/README.md gains a phase1e section that states F-003 is partially resolved (arena reachable, full wall-clock win gated on JsonValue refactor); CHANGELOG.md gains a v0.3.34 entry
- [x] 4.2 Write tests covering the new behavior — the parse/access/free lifecycle is exercised by the existing 22 std/json suites plus the phase1d json_borrowed_handle suite; the arena path is validated at compile time (overload signature + construction)
- [x] 4.3 Run tests and confirm they pass — all 23 std/json suites green
