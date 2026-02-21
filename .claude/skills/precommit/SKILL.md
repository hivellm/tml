---
name: precommit
description: Run format, lint, and affected tests before committing. Use when the user says "precommit", "pre-commit", "antes de comitar", or wants to validate changes before commit.
user-invocable: true
allowed-tools: mcp__tml__format, mcp__tml__lint, mcp__tml__project_affected-tests, mcp__tml__test, Bash(git diff *), Glob
argument-hint: no arguments needed
---

## Pre-commit Validation Workflow

Run all quality checks sequentially. Stop on first failure.

### 1. Detect Changed Files

Run `git diff --name-only HEAD` via Bash to identify changed files.

### 2. Format Check

For each changed `.tml` file, run `mcp__tml__format` with `check: true`.

For each changed `.cpp`/`.hpp` file, note that the git pre-commit hook will check formatting.

If any `.tml` files need formatting, run `mcp__tml__format` with `check: false` to fix them.

### 3. Lint Check

For each changed `.tml` file, run `mcp__tml__lint`.

Report any warnings or errors.

### 4. Run Affected Tests

Use `mcp__tml__project_affected-tests` with `run: true` to detect and run affected tests.

### 5. Report

Summary:
- Format: OK / N files formatted
- Lint: OK / N warnings
- Tests: N passed, N failed
- Verdict: READY TO COMMIT or ISSUES FOUND