---
name: tasks
description: List active Rulebook tasks with status and completion. Use when the user says "tasks", "tarefas", "list tasks", or wants to see what's in progress.
user-invocable: true
allowed-tools: mcp__rulebook__rulebook_task_list, Glob, Read
argument-hint: [optional --all to include archived]
---

## List Tasks Workflow

### 1. Get Tasks

Use `mcp__rulebook__rulebook_task_list` with:
- `includeArchived`: true if `$ARGUMENTS` contains "--all" or "all"
- `status`: filter if `$ARGUMENTS` contains "pending", "in-progress", "completed", or "blocked"

### 2. Enrich with Progress

For each active task, read its `tasks.md` file and count:
- Total checklist items (`- [ ]` + `- [x]`)
- Completed items (`- [x]`)
- Calculate completion percentage

### 3. Report

Show a table:
```
| Task ID | Status | Progress | Description |
|---------|--------|----------|-------------|
| task-id | status | XX% (N/M) | title |
```

Sort by: in-progress first, then pending, then blocked.