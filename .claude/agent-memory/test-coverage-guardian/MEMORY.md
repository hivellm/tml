# Test Coverage Guardian — Persistent Memory

## New Test System Architecture (Phases 1-9 complete as of 2026-03-03)

**New test runner**: `compiler/src/testing/` (old `compiler/src/cli/tester/` deleted in Phase 9.1.1)
**Architecture**: Subprocess-only (Go model) — each suite compiles to EXE, runs as subprocess, streams NDJSON results
**Coverage**: No LLVM profiling. Subprocess writes covered function names via `tml_coverage_write_file` to temp file. Coordinator aggregates into `std::set<std::string>`. Generates HTML+JSON+JSONL from that set.
**Coverage achieved**: 81.0% (4575/5647 library functions)
**Cache**: `build/debug/.new-test-cache.json` — CRC32C hash of sources + compiler version + flags

## Blocker: coverage.c Cannot Be Deleted (Phase 9.1.2)

`lib/test/runtime/coverage.c` still needed because:
- `tml_cover_func` — inserted by codegen (`llvm_utils.cpp:130`) into all test binaries
- `tml_coverage_write_file` — called by dispatcher gen (`testing_dispatcher_gen.cpp:553`) AND old `generate.cpp:1833`
- `print_coverage_report/write_coverage_html/write_coverage_json` — called from `llvm_utils.cpp:142-144` for old inline-main test path

**To unblock**: Remove old inline-main test path from `generate.cpp` (has_test_functions branch ~line 1715-1840) and `llvm_utils.cpp` emit_coverage_report_calls. See details in rulebook memory ID `5ac4be69`.

## Key Files

| File | Role |
|------|------|
| `compiler/src/testing/testing_coordinator.cpp` | Main orchestration |
| `compiler/src/testing/testing_dispatcher_gen.cpp` | Generates LLVM IR dispatcher |
| `compiler/src/testing/testing_coverage.cpp` | Coverage reports (HTML/JSON/JSONL) |
| `compiler/src/codegen/llvm/core/llvm_utils.cpp:127` | Inserts tml_cover_func calls |
| `compiler/src/codegen/llvm/core/generate.cpp:1820+` | Old inline-main test path (still live) |
| `lib/test/runtime/coverage.c` | C runtime: lock-free func tracking + file write |
| `lib/test/runtime/test.c` | Assertion helpers (exit(1) = fine with subprocess model) |

## Phase 10/11 Status (as of 2026-03-03)

- 10.1.2: `test.c` assertions use `exit(1)` — correct for subprocess model (no update needed)
- 10.1.3/10.1.4: TML test framework self-tests all pass (mock/3, report/9, assertions/3, coverage/1)
- 11.1.3: CLAUDE.md updated — Project Structure + Test Commands sections corrected
- 11.1.5: Architectural decisions saved to rulebook memory (IDs: `6c082162`, `5ac4be69`)

## Test Framework Self-Test Suites

All pass with `mcp__tml__test`:
- `test/mock` (3 tests)
- `test/report` (9 tests)
- `test/assertions` (3 tests)
- `test/coverage` (1 test)

## Common Patterns

- Full test structured run: `mcp__tml__test with structured=true, no_cache=true` (returns 0 total — MCP timeout issue on full suite)
- Targeted suites work fine: `mcp__tml__test with suite="core/str", no_cache=true` → 21/21
- Coverage run by suite works: `mcp__tml__test with suite="core/str", coverage=true, no_cache=true` → 21/21, 282 functions covered
