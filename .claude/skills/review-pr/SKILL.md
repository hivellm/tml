---
name: review-pr
description: Review a pull request or the current branch changes. Use when the user says "review", "review pr", "revise", or wants code review feedback.
user-invocable: true
allowed-tools: Bash(git *), Bash(gh *), Read, Grep, Glob
argument-hint: [optional PR number or branch name]
---

## PR Review Workflow

### 1. Get the Changes

If `$ARGUMENTS` contains a PR number:
- `gh pr view $ARGUMENTS`
- `gh pr diff $ARGUMENTS`

If no arguments, review current branch vs main:
- `git log main..HEAD --oneline`
- `git diff main...HEAD --stat`
- `git diff main...HEAD`

### 2. Analyze Changes

For each changed file, check:

**Correctness**
- Logic errors, off-by-one, null/undefined access
- Missing error handling
- Race conditions in concurrent code

**TML Project Specific**
- New C/C++ code when TML would suffice (VIOLATION of minimize C++ rule)
- Tests that were simplified or commented out (VIOLATION)
- Direct cmake usage instead of build scripts (VIOLATION)
- Missing MCP tool usage for test/build operations (VIOLATION)

**Style**
- Follows existing code patterns
- Consistent naming conventions
- No unnecessary complexity

**Tests**
- Are new features covered by tests?
- Do existing tests still pass?

### 3. Report

Provide structured feedback:
- **Summary**: One-line description of what the PR does
- **Approval status**: Approve / Request Changes / Comment
- **Issues found**: List with severity (critical/warning/nit)
- **Suggestions**: Improvements that aren't blocking