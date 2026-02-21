---
name: status
description: Project health dashboard showing test count, coverage, active tasks, and build status. Use when the user says "status", "dashboard", "estado do projeto", or wants a project overview.
user-invocable: true
allowed-tools: mcp__tml__project_coverage, mcp__tml__project_artifacts, mcp__tml__project_structure, mcp__rulebook__rulebook_task_list, Bash(git log *), Glob
argument-hint: no arguments needed
---

## Status Dashboard Workflow

Gather data from multiple sources in parallel, then present a unified dashboard.

### 1. Gather Data (in parallel)

Run these simultaneously:
- `mcp__tml__project_coverage` with `sort: "lowest"`, `limit: 5` — get coverage overview
- `mcp__tml__project_artifacts` with `kind: "executables"` — get build info
- `mcp__tml__project_structure` with `depth: 1` — get module counts
- `mcp__rulebook__rulebook_task_list` — get active tasks
- `git log --oneline -5` via Bash — get recent commits

### 2. Present Dashboard

Format as a clean dashboard:

```
## TML Project Status

### Build
- Compiler: <path> (<size>, <modified date>)
- Last commit: <hash> <message>

### Tests & Coverage
- Total tests: <count>
- Overall coverage: <percentage>
- Lowest coverage modules: <list top 5>

### Active Tasks
- <task-id>: <status>
- ...

### Recent Activity
- <last 5 commits>
```