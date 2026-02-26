# Coverage Migration to EXE Architecture

**ID**: coverage-migration
**Category**: testing
**Complexity**: MEDIUM
**Status**: In Progress (Phase 3)
**Blocks**: parallel-test-execution (depends on coverage support)

## What This Skill Does

Enables LLVM coverage collection in EXE-based subprocess architecture without hangs or deadlocks.

## When to Use

- User asks to "run tests with coverage"
- Coverage tests hang or deadlock
- Need coverage reports for library functions
- Working on test coverage infrastructure

## Quick Start

Use Rulebook MCP tools to interact with Ralph:

```bash
# Initialize Ralph for this task (generates PRD)
mcp__rulebook__rulebook_ralph_init --task coverage-migration

# Run autonomous iteration loop (continues from Phase 3)
mcp__rulebook__rulebook_ralph_run --task coverage-migration --max-iterations 3

# Check current status
mcp__rulebook__rulebook_ralph_status --task coverage-migration

# View iteration history
mcp__rulebook__rulebook_ralph_get_iteration_history --task coverage-migration

# Run tests with coverage (uses EXE mode automatically)
tml test --coverage --no-cache --verbose

# View coverage report
cat build/coverage/coverage.json | jq '.summary'
open build/coverage/coverage.html
```

## Problem Solved

**Before**: DLL mode coverage causes hangs (LLVM profiling runtime conflict)
**After**: EXE mode - each subprocess has isolated LLVM runtime, coverage via temp files

## Architecture

```
Parent Process                          Subprocess #1
├─ Launch subprocess 1                  ├─ Run tests
├─ Set TML_COVERAGE_FILE                ├─ LLVM profiling active
├─ ...                                  ├─ Write to cov_suite1.txt
├─ Collect cov_suite1.txt               └─ Exit (clean isolation)
├─ Collect cov_suite2.txt
├─ Aggregate to coverage.json
└─ Generate coverage.html
```

## Implementation Phases

### ✅ Phase 1: Environment Variables (DONE)
- `TML_COVERAGE_FILE` passed to subprocess
- Coverage directory auto-created
- Each subprocess writes to unique temp file

### ✅ Phase 2: Runtime Writing (DONE)
- `tml_coverage_write_file()` function
- LLVM IR code calls coverage writer
- Format: one function name per line

### 🔄 Phase 3: Aggregation (IN PROGRESS)
- Collect all cov_*.txt files
- Aggregate covered functions into set
- Generate coverage.json report
- Generate coverage.html visualization
- Update coverage history

## Coverage File Format

**Subprocess Output** (build/coverage/cov_<suite>.txt):
```
std::string::from_str
std::string::split
core::fmt::format
core::error::Error
...
```

**Parent Aggregation** → coverage.json:
```json
{
  "summary": {
    "total_functions": 2543,
    "covered_functions": 1847,
    "coverage_percent": 72.6
  },
  "modules": [
    {
      "module": "core::str",
      "functions": 45,
      "covered": 42,
      "coverage_percent": 93.3
    },
    ...
  ]
}
```

## Commands

```bash
# Initialize
ralph init --task coverage-migration

# Run autonomous loop
ralph run --task coverage-migration --verbose

# Check progress
ralph status --task coverage-migration

# Run tests with coverage
tml test --coverage --no-cache --verbose

# View results
cat build/coverage/coverage.json | jq
open build/coverage/coverage.html
cat build/coverage/coverage_history.jsonl
```

## Output Files

| File | Purpose |
|------|---------|
| build/coverage/coverage.json | Machine-readable report |
| build/coverage/coverage.html | HTML visualization |
| build/coverage/coverage_history.jsonl | Historical tracking |
| build/coverage/cov_*.txt (temp) | Per-suite coverage |

## Success Criteria

- ✅ Coverage runs complete without hangs
- ✅ All 3,632 tests pass
- ✅ Coverage reports generated
- ✅ Reports are accurate (match expected functions)
- ✅ No process leaks

## References

- [PRD-TEST-PARALLEL-EXECUTION.md](../../docs/PRD-TEST-PARALLEL-EXECUTION.md#phase-2-coverage-support)
- [lib/test/runtime/coverage.c](../../lib/test/runtime/coverage.c)
- [exe_suite_runner.cpp](../../compiler/src/cli/tester/exe_suite_runner.cpp)
