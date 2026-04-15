## 1. Diagnosis
- [ ] 1.1 Reproduce: `tml check compiler/tests/compiler/bool_i32_comparison.test.tml 2>&1 | cat` — confirm hang
- [ ] 1.2 Identify the blocking call: add `fflush(stdout); fflush(stderr);` after each major pipeline step; find where output stops
- [ ] 1.3 Check if the issue is CRT buffering or a blocking Windows console API call (`GetConsoleMode`, `ReadConsoleInput`, etc.)

## 2. Implementation
- [ ] 2.1 Add `setvbuf(stdout, nullptr, _IONBF, 0)` and `setvbuf(stderr, nullptr, _IONBF, 0)` at the top of `main()` / `tml_main()`
- [ ] 2.2 If CRT buffering fix is insufficient: audit all diagnostic/log output paths for blocking console API calls; replace with `fputs`/`fprintf` or direct `WriteFile`
- [ ] 2.3 Verify the MCP server `mcp__tml__check` captures output correctly (pipe capture)

## 3. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 3.1 Update or create documentation covering the implementation
- [ ] 3.2 Write tests covering the new behavior — `tml check 2>&1 | cat` exits with output within 10s
- [ ] 3.3 Run tests and confirm they pass
