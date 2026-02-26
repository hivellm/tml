# Parallel Test Execution with Subprocess Architecture

**ID**: parallel-test-execution
**Category**: compiler/testing
**Complexity**: HIGH
**Status**: In Progress (Phase 3)

## What This Skill Does

Migrates TML test execution from **in-process DLL loading** (sequential) to **subprocess-based architecture** (N-way parallel) for 10-15x speedup.

## When to Use

- User asks to "speed up tests" or "make tests faster"
- User mentions test hangs or coverage deadlocks
- Working on test infrastructure improvements
- Implementing parallel subprocess execution

## Quick Start

Use Rulebook MCP tools to interact with Ralph:

```bash
# Initialize Ralph for this task (generates PRD)
mcp__rulebook__rulebook_ralph_init --task parallel-test-execution

# Run autonomous iteration loop (continues from Phase 3)
mcp__rulebook__rulebook_ralph_run --task parallel-test-execution --max-iterations 4

# Check current status
mcp__rulebook__rulebook_ralph_status --task parallel-test-execution

# View iteration history
mcp__rulebook__rulebook_ralph_get_iteration_history --task parallel-test-execution

# Run tests to verify parallelism works
tml test --no-cache --verbose
```

## Architecture

```
DLL Mode (Sequential - SLOW):          EXE Mode (Parallel - FAST):
Test Runner                             Test Launcher
├─ Load DLL #1 → Execute               ├─ [subprocess #1] ──┐
├─ Load DLL #2 → Execute               ├─ [subprocess #2] ──┼─ PARALLEL
├─ Load DLL #3 → Execute               ├─ [subprocess #3] ──┘
└─ ... (3,632 sequential loads)        └─ ... (454 suites × 8-16 concurrent)
```

## Key Implementation

### Phase 1: Async Launching ✅
- `launch_subprocess_async()` - non-blocking launch
- `wait_for_subprocess()` - blocking collect

### Phase 2: Coverage ✅
- Environment variable: `TML_COVERAGE_FILE`
- Coverage file aggregation across subprocesses

### Phase 3: Polling 🔄 (IN PROGRESS)
- `subprocess_is_done()` - non-blocking status check
- Rewrite suite_worker to poll ALL pending subprocesses
- Change from blocking wait to async polling

### Phase 4: Verification ⏳
- Measure 10-15x speedup
- Verify all tests pass
- Verify coverage accuracy

## Files to Modify

| File | Phase | Role |
|------|-------|------|
| exe_test_execution.cpp | 1,2,3 | Async subprocess functions |
| exe_suite_runner.cpp | 1,2,3 | Test orchestration loop |
| exe_test_runner.hpp | 1,2,3 | Function declarations |
| coverage.c | 2 | Coverage file writing |
| generate.cpp | 2 | LLVM IR code generation |
| run.cpp | 1 | Route coverage→exe_mode |

## Commands

```bash
# Initialize and generate PRD
ralph init --task parallel-test-execution

# Run autonomous loop with fresh context per iteration
ralph run --task parallel-test-execution --max-iterations 4 --verbose

# Check current phase and progress
ralph status --task parallel-test-execution --detailed

# View iteration history
ralph history --task parallel-test-execution --format markdown

# Test to verify parallelism
tml test --coverage --no-cache --verbose
```

## Success Criteria

- ✅ All 3,632 tests pass
- ✅ Coverage completes without hangs
- ✅ Real-time logs show which tests are running
- ✅ 10-15x speedup measured
- ✅ Coverage reports accurate

## References

- [PRD-TEST-PARALLEL-EXECUTION.md](../../docs/PRD-TEST-PARALLEL-EXECUTION.md)
- [.rulebook/ralph.json](../../.rulebook/ralph.json)
- [Rulebook Ralph](https://github.com/hivellm/rulebook#ralph)
