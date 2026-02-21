---
name: coverage-gaps
description: Show modules with lowest test coverage and suggest what to test next. Use when the user says "coverage gaps", "lacunas", "what needs tests", or wants to prioritize testing.
user-invocable: true
allowed-tools: mcp__tml__project_coverage, Read
argument-hint: [optional limit e.g. 10] [optional threshold e.g. 50%]
---

## Coverage Gaps Workflow

### 1. Get Coverage Data

Use `mcp__tml__project_coverage` with:
- `sort`: "lowest" (show worst coverage first)
- `limit`: From `$ARGUMENTS` if specified, otherwise 20
- `refresh`: false (use existing data; user can run `/coverage` first for fresh data)

### 2. Filter and Analyze

From the results:
- Highlight modules with 0% coverage (completely untested)
- Highlight modules below 20% (critically undertested)
- Group by library (core vs std vs test)

### 3. Report

Show a prioritized list:
- Module name, function count, covered count, coverage %
- Suggest which modules to test next based on: (a) low coverage + high function count = highest impact, (b) 0% modules that are easy wins
- Estimate number of tests needed to reach 80% coverage per module