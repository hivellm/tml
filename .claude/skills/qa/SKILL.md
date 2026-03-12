---
name: qa
description: Run a quality audit on the codebase or a specific module. Use when the user says "qa", "quality", "audit", "qualidade", or wants to assess code quality, find issues, or create improvement tasks.
user-invocable: true
argument-hint: "[optional module or area — e.g. 'compiler/codegen', 'lib/core/str', 'stdlib']"
---

**Delegation**: Use the Agent tool to dispatch a `qa-code-analyst` agent with `model: sonnet` for this task.

## Quality Audit Workflow

### 1. Determine Scope

From `$ARGUMENTS`:
- **No arguments** → Full project audit
- **"compiler/codegen"** → Focus on codegen module
- **"lib/core/str"** → Focus on string module
- **"stdlib"** → All of lib/core + lib/std

### 2. Analyze

For each file in scope:
- Code quality: naming, structure, complexity
- Performance: unnecessary allocations, O(n²) where O(n) possible
- Documentation: missing doc comments on public APIs
- Test coverage: uncovered functions
- Consistency: patterns that differ from the rest of the codebase

### 3. Report

Present findings organized by severity:
- **Critical**: Bugs, UB, security issues
- **Warning**: Performance issues, missing error handling
- **Info**: Style inconsistencies, missing docs

### 4. Create Tasks

For actionable items, suggest creating Rulebook tasks via `/task-create`.
