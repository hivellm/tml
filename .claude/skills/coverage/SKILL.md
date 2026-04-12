---
name: coverage
description: Run project-scoped test coverage report. Use when the user says "coverage", "cobertura", "cv", or wants to see test coverage for a specific project.
user-invocable: true
argument-hint: "[project-path]"
---

## Run Coverage Report

Run the `tml cv` command to generate a project-scoped coverage report.

Parse `$ARGUMENTS`:
- **path**: Project path (optional, defaults to `compiler-tml`)

### Steps

1. Determine the project path from arguments or default to `compiler-tml`

2. Run the coverage command via Bash:
   ```
   ./build/debug/bin/tml.exe cv <path> 2>&1
   ```

3. Report the results: total modules, test files, @test functions, coverage %, untested modules.

### Options

- `--quick` — skip module-to-test mapping, only count files
- `--verbose` — show per-file results
